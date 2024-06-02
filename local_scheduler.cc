#include <list>
#include <sys/timerfd.h>
#include <sys/time.h>
#include<sys/signalfd.h>
#include <signal.h>
#include<sys/wait.h>
#include<map>
#include<sstream>
#include<string>
#include<iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include "client.h"
#include "ntp_client-Gaa/src/ntp_client.h"
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
WSADATA xwsa_data;
WSAStartup(MAKEWORD(2, 0), &xwsa_data);
#endif // defined(_WIN32) || defined(_WIN64)
using namespace std;


#define server_port 1234
#define CGROUP_PATH "../cgroup"

string server = "127.0.0.1";

int ntp_timerfd;
//events number from epoll_Wait()

int task_timerfd;

int sigchildfd;

int shmid;

char* shmaddr;

int read_number;
//the position for next to be execute task in schedule
int position;
//size for current schedule tasks
int size;
//for interaive calculating start time of task
struct timeval given_time;

map<int,string>pid_to_taskid;

string id_endTask_lastSched = "empty";
//ids of tasks that has run on this local. these task don't need to execute again and get a new pid
vector<string> taskids_exist_local;

Client local_scheduler;
//has been taken from the list, the currently executed scheduling
schedule schedule_current;
//New schedule that may be received during the execution of the current schedule
schedule schedule_new;
//true, when schedule_current not finished
bool dispatching=false;
//schedule received during execution of current schedule
bool new_schedule=false;
//standard server time - client time
x_int64_t deviation_in_microsecond=0;

/************************************************/
//ntp responsible parameters
int xit_iter = 0;
//xopt_args_t   xopt_args;
xntp_cliptr_t xntp_this = X_NULL;
xtime_vnsec_t xtm_vnsec = XTIME_INVALID_VNSEC;
xtime_vnsec_t xtm_ltime = XTIME_INVALID_VNSEC;
xtime_descr_t xtm_descr = { 0 };
xtime_descr_t xtm_local = { 0 };
x_ccstring_t host = "127.0.0.1";
x_int16_t port = 123;
x_uint32_t xut_tmout = 3000;


string serializeMap(const std::map<int, std::string>& myMap) {
    ostringstream oss;
    for (const auto& pair : myMap) {
        oss<< pair.first << ":" << pair.second << ";";
    }
    return oss.str();
}

map<int,string> deserializeMap(const std::string& str) {
    std::map<int, std::string> myMap;
    std::istringstream iss(str);
    std::string item;
    while (std::getline(iss, item, ';')) {
        if (!item.empty()) {
            size_t pos = item.find(':');
            int key = std::stoi(item.substr(0, pos));
            std::string value = item.substr(pos + 1);
            myMap[key] = value;
        }
    }
    return myMap;
}


void print_current_time() {
    struct timeval tv;
    struct tm *tm_info;
    char buffer[30];
    char usec_buffer[21];

    gettimeofday(&tv, NULL);

    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", tm_info);

    snprintf(usec_buffer, 21, "%06ld", tv.tv_usec);

    printf("CURRENT TIME :%s.%s\n", buffer, usec_buffer);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//ntp_client_initialisation() initialise some parameter, config ntp server infomation
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int ntp_client_initialisation()
{
    xntp_this = ntpcli_open();

    if (X_NULL == xntp_this)
    {
        printf("ntpcli_open() return X_NULL, errno : %d\n", errno);
        return -1;
    }

    ntpcli_config(xntp_this,host,port);
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// ntp_timer() set a timer that times out every minute for requesting a time deviation.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int ntp_timer()
{
    ntp_timerfd = timerfd_create(CLOCK_REALTIME,0);
    if(ntp_timerfd<0)
    {
        printf("ntp_timer create failed!\n");
        return -1;
    }
    struct itimerspec timer;
    //---------------------------------------//
    //triggered every 60s after first trigger
    //(30s after the this program started)
    timer.it_interval.tv_sec=60;//this doesn't work
    timer.it_interval.tv_nsec=0;
    timer.it_value.tv_nsec=0;
    timer.it_value.tv_sec=30;//this works

    if(timerfd_settime(ntp_timerfd,0,&timer,NULL)<0)
    {
        printf("ntp_timer settime failed!\n");
        return -1;
    }
    local_scheduler.ev.events= EPOLLIN;
    local_scheduler.ev.data.fd = ntp_timerfd;
    if(epoll_ctl(local_scheduler.epoll_fd,EPOLL_CTL_ADD,ntp_timerfd,&local_scheduler.ev)<0)
    {
        printf("epoll_ctl_Add timer failed!\n");
        return -1;
    }
    printf("SUCCESSFULLY CREATE AND ADD TIMER TO EPOLL\n");
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//ntp_timer_handler() request time deviation for once, save it in deviation in microsecons
int ntp_timer_handler()
{
    uint64_t exp;
    if(read(ntp_timerfd,&exp,sizeof(uint64_t))<0)
    {
        cout<<"failed read from timer"<<endl;
        return -1;
    }

    xtm_vnsec = ntpcli_req_time(xntp_this,xut_tmout);
    if (XTMVNSEC_IS_VALID(xtm_vnsec))
    {
        xtm_ltime = time_vnsec();
        xtm_descr = time_vtod(xtm_vnsec);
        xtm_local = time_vtod(xtm_ltime);
        xit_iter++;
        printf("============================================\n");
        printf("\n[%d] %s:%d : \n",
                xit_iter ,
                host,
                port);
        printf("\tNTP response : [ %04d-%02d-%02d %d %02d:%02d:%02d.%03d ]\n",
                xtm_descr.ctx_year  ,
                xtm_descr.ctx_month ,
                xtm_descr.ctx_day   ,
                xtm_descr.ctx_week  ,
                xtm_descr.ctx_hour  ,
                xtm_descr.ctx_minute,
                xtm_descr.ctx_second,
                xtm_descr.ctx_msec  );

        printf("\tLocal time   : [ %04d-%02d-%02d %d %02d:%02d:%02d.%03d ]\n",
                xtm_local.ctx_year  ,
                xtm_local.ctx_month ,
                xtm_local.ctx_day   ,
                xtm_local.ctx_week  ,
                xtm_local.ctx_hour  ,
                xtm_local.ctx_minute,
                xtm_local.ctx_second,
                xtm_local.ctx_msec  );

        deviation_in_microsecond = ntpcli_req_time_by_Jing(xntp_this,xut_tmout);
        printf("Jing's deviation:%lld us\n",deviation_in_microsecond/10LL);
        printf("\tDeviation    : %lld us\n",
                ((x_int64_t)(xtm_ltime - xtm_vnsec)) / 10LL);
        printf("============================================\n");
        return 0;       
    }
    else
    {
        printf("\n[%d] %s:%d : errno = %d\n",
                xit_iter + 1,
                host,
                port,
                errno);
        return -1;
    }

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//task_timer() create timerfd for every tasks in current schedule with their start time, 
//and put them into epoll 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int task_timer_initialise()
{
    task_timerfd = timerfd_create(CLOCK_REALTIME,0);
    if(task_timerfd<0)
    {
        printf("task_timer create failed!\n");
        return -1;
    }
    struct itimerspec timer;
    timer.it_interval.tv_nsec=0;
    timer.it_interval.tv_sec=0;
    timer.it_value.tv_nsec=0;
    timer.it_value.tv_sec=0;
    
    if(timerfd_settime(task_timerfd,0,&timer,NULL)<0)
    {
        printf("task_timer settime failed!\n");
        return -1;
    }
    local_scheduler.ev.events= EPOLLIN;
    local_scheduler.ev.data.fd = task_timerfd;
    if(epoll_ctl(local_scheduler.epoll_fd,EPOLL_CTL_ADD,task_timerfd,&local_scheduler.ev)<0)
    {
        printf("epoll_ctl_Add task timer failed!\n");
        return -1;
    }
    printf("SUCCESSFULLY CREATE AND ADD task TIMER TO EPOLL\n");
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//second time value - first time value;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~`//
long long time_diff_microseconds(struct timeval start, struct timeval end) {
    long long start_usec = start.tv_sec * 1000000LL + start.tv_usec;
    long long end_usec = end.tv_sec * 1000000LL + end.tv_usec;
    return end_usec - start_usec;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//organize_current_schedule() set the dispatching flag, reset the position from 0, set timer for first task start
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int organize_current_schedule()
{
    size = schedule_current.tasks_size();
    dispatching = true;
    position = 0;

    /***********the start time of first task is the start time of schedule********************/
    struct itimerspec timer;

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    given_time.tv_sec=schedule_current.start_time().sec();
    given_time.tv_usec=schedule_current.start_time().ms()*1000;

    long long diff_usec = time_diff_microseconds(current_time,given_time);

    if(diff_usec<0)
    {
        cout<<"start time is already passed!"<<endl;
        return -1;
    }
    //*****sum_ns = given server time - current server time*****************//
    //************= given server time - (current client time + deviation)***// 
    //************= diff_usec -deviation************************************//
    int64_t sum_ns = diff_usec*1000LL-deviation_in_microsecond*1000LL;
    timer.it_value.tv_sec = sum_ns/1000000000LL;
    timer.it_value.tv_nsec = sum_ns%1000000000LL;
    timer.it_interval.tv_nsec = 0;
    timer.it_interval.tv_sec =0;

    //printf("timer sec:%ld, nsec:%ld\n",timer.it_value.tv_sec,timer.it_value.tv_nsec);

    if(timerfd_settime(task_timerfd,0,&timer,NULL)<0)return -1;
    cout<<"successful to set the first timer of a schedule"<<endl;

    return 0;
}

void write_to_cgroup_file(const char *file_path, const char *value) {
    int fd = open(file_path, O_WRONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    if (write(fd, value, strlen(value)) == -1) {
        perror("Error writing to file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void add_pid_to_cgroup(const char *tasks_file_path, pid_t pid) {

    int fd;

    fd = open(tasks_file_path, O_WRONLY);
    if (fd == -1) {
        perror("Error opening tasks file");
        exit(EXIT_FAILURE);
    }
    //write pid to task file 
    if (dprintf(fd, "%d", pid) == -1) {
        perror("Error writing to tasks file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//check_task_initialised() returns true, if this task has been executed at least once before position
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
bool check_task_initialised(string id)
{
    // for(int i=0;i<position;i++)
    // {
    //     task taski = schedule_current.tasks(i);
    //     task task_current = schedule_current.tasks(position);
    //     if(taski.task_id()==task_current.task_id())return true;
    // }
    // return false;
    for(vector<string>::iterator it= taskids_exist_local.begin();it!=taskids_exist_local.end();it++)
    {
        if(*it == id)return true;
    }
    return false;
}

bool check_file_empty(string id)
{
    char buffer[128];
    sprintf(buffer,"%s/%s/%s",CGROUP_PATH,id.c_str(),"tasks");
    FILE *file = fopen(buffer, "r");
    if (file == NULL) {
        printf("fopen failed");
        return -1; 
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        printf("fseek failed");
        fclose(file);
        return -1; 
    }

    long file_size = ftell(file);
    if (file_size == -1) {
        printf("ftell failed");
        fclose(file);
        return -1; 
    }

    fclose(file);

    return file_size == 0;

}

/**********from second task, the start time = last start time + last duration************/
int timer_update(int position)
{
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    task task = schedule_current.tasks(position);
    given_time.tv_sec=given_time.tv_sec+task.duration_ms()/1000;
    given_time.tv_usec= given_time.tv_usec+(task.duration_ms()%1000)*1000;
    long long diff_usec = time_diff_microseconds(current_time,given_time);
    if(diff_usec<0)
    {
        cout<<"start time(not the first) is already passed!"<<endl;
        return -1;
    }
    
    struct itimerspec timer;
    
    int64_t sum_ns = diff_usec*1000LL-deviation_in_microsecond*1000LL;
    timer.it_value.tv_sec = sum_ns/1000000000LL;
    timer.it_value.tv_nsec = sum_ns%1000000000LL;
    timer.it_interval.tv_nsec = 0;
    timer.it_interval.tv_sec =0;

    if(timerfd_settime(task_timerfd,0,&timer,NULL)<0)return -1;
    cout<<"successful to set the timer"<<endl;
    return 0;
}

int task_timer_handler()
{
    uint64_t exp;
    if(read(task_timerfd,&exp,sizeof(uint64_t))<0)
    {
        cout<<"failed read from task timer"<<endl;
        return -1;
    }

    if(position==0)
    {
        if(id_endTask_lastSched!="empty")
        {
            char buffer[128];
            sprintf(buffer,"%s/%s/%s",CGROUP_PATH,id_endTask_lastSched.c_str(),"freezer.state");
            write_to_cgroup_file(buffer, "FROZEN");
            printf("====================\nLAST TASK WITH ID %s OF LAST SCHEDULE HAS BEEN FROZEN\n====================\n",id_endTask_lastSched.c_str());
        }else{
            printf("====================\nLAST TASK TIME SLICE OF LAST SCHEDULE IS EMPTY\n====================\n");
        }
    }
    /**********frozen the last task, if last task is empty,(no task for last time slice),skip this step*********/
    if(position>0){
        task last_task = schedule_current.tasks(position-1);
        if(last_task.task_id()!="empty")
        {
            char buffer[128];
            string id = last_task.task_id().substr(1);
            sprintf(buffer,"%s/%s/%s",CGROUP_PATH,id.c_str(),"freezer.state");
            write_to_cgroup_file(buffer, "FROZEN");
            printf("====================\nTask %s at position %d has been frozen\n",id.c_str(),position-1);
        }
    }
    /*********if next task is empty, skip the next step*******************/
    task next_task = schedule_current.tasks(position);
    if(next_task.task_id()=="empty")
    {
        if(timer_update(position)<0)
        {
            cout<<"failed to update timer with task position:"<<position<<endl;
            return -1;
        }   
        printf("Next task time slice is empty!\n====================\n");
        print_current_time();
        position++;
        return 0;
    }

    /*********thaw the next task********************/
    char state[128];
    string id = next_task.task_id().substr(1);
    sprintf(state,"%s/%s/%s",CGROUP_PATH,id.c_str(),"freezer.state");
    //cout<<state<<endl;
    write_to_cgroup_file(state, "THAWED");
    printf("Task %s IS THAEWD AT POSITION %d NOW\n",id.c_str(),position);
    print_current_time();
    //*****if not be executed before, it needs to get pid and store it ****//
    if(!check_task_initialised(id))
    {
        taskids_exist_local.push_back(id);
        pid_t pid = fork();
        if (pid == -1) {
            perror("Error forking process");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {//child process
            prctl(PR_SET_PDEATHSIG,SIGKILL);
            char tasks[128];
            sprintf(tasks,"%s/%s/%s",CGROUP_PATH,id.c_str(),"tasks");
            cout<<"task pid is"<<getpid()<<endl;
            add_pid_to_cgroup(tasks,getpid());

            string serializedMap(shmaddr);
            map<int, string> receivedMap = deserializeMap(serializedMap);

            receivedMap.insert(pair<int,string>(getpid(),id));
            serializedMap = serializeMap(receivedMap);
            copy(serializedMap.begin(), serializedMap.end(), shmaddr);
            shmaddr[serializedMap.size()] = '\0';
            shmdt(shmaddr);

            const char* path = next_task.path().c_str();
            execl(path, path, NULL);
            perror("Error executing program ");
            exit(EXIT_FAILURE);
        }//father process here continue
    }

    /*********current schedule finished********************/
    if(position==schedule_current.tasks_size()-1)
    {
        if(new_schedule)
        {
            printf("THERE IS NEW SCHEDULE DETECTED!\n");
            new_schedule=false;
            id_endTask_lastSched= id;
            schedule_current.Clear();
            cout<<"current task size after clear is:"<<schedule_current.tasks_size()<<endl;
            //schedule_current.CopyFrom(schedule_new);
            schedule_current.mutable_start_time()->CopyFrom(schedule_new.start_time());
            schedule_current.mutable_tasks()->CopyFrom(schedule_new.tasks());
            cout<<"current task size after copy is:"<<schedule_current.tasks_size()<<endl;
            schedule_new.Clear();
            if(organize_current_schedule()<0)return -1;
            cout<<"successfully swtiched to new schedule!"<<endl;
        }else{
            id_endTask_lastSched= id;
            start_time nst;
            //start time for repeated schedule = last task'start time + last task's duration
            int64_t given_time_us = given_time.tv_sec*1000000LL+given_time.tv_usec+schedule_current.tasks(position).duration_ms()*1000LL;
            nst.set_sec(given_time_us/1000000LL);
            nst.set_ms((given_time_us%1000000LL)/1000LL);
            schedule_current.clear_start_time();
            schedule_current.mutable_start_time()->CopyFrom(nst);
            if(organize_current_schedule()<0)return -1;
            cout<<"successfully swtiched to new schedule!"<<endl;
        }
        return 0;
    }

    if(timer_update(position)<0)
    {
        cout<<"failed to update timer with task position:"<<position<<endl;
        return -1;
    }

    position++;
    return 0;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int handle_event()
{
    for(int i=0;i<read_number;i++)
    {
        int fd_temp = local_scheduler.events[i].data.fd;
        //Fist situation: ntp_timer is triggered,it is time to request a deviation!
        if(fd_temp == ntp_timerfd)
        {
            printf("\n==============================\n""REQUSTING TIME DEVIATION\n""==============================\n");
            if(ntp_timer_handler()<0)
            {
                printf("-----request time deviation failed------\n");
                return -1;
            }
        
        //SECOND SITUATION: receive from server    
        }else if(fd_temp==local_scheduler.client&&(local_scheduler.events[i].events&EPOLLIN))
        {
            memset(local_scheduler.receive_buffer,'\0',sizeof(local_scheduler.receive_buffer));
            if(recv(local_scheduler.client,local_scheduler.receive_buffer,1024,0)<=0)
            {
                cout<<"failed to receive from server, connection closed"<<endl;
                return -1;
            }
            if(dispatching)
            {
                new_schedule = true;
                schedule_new.Clear();
                schedule_new.ParseFromArray(local_scheduler.receive_buffer,1024);
                cout<<"*******a new schedule is stored in schedule_new********<----"<<endl;
                cout<<"task size of new schedule is : "<<schedule_new.tasks_size()<<endl;
                print_current_time();
            }else{
                schedule_current.Clear();
                schedule_current.ParseFromArray(local_scheduler.receive_buffer,1024);
                cout<<"a new schedule is stored in schedule_current"<<endl;
                cout<<"task size of first current schedile is:"<<schedule_current.tasks_size();
                print_current_time();
                if(organize_current_schedule()<0)return -1;
                cout<<"successfully organized current schedule"<<endl;
            }
         //THIRD SITUATION : SOME CHID PROCESS HAS ENDED             
        }else if(fd_temp == sigchildfd)
        {
            struct signalfd_siginfo fdsi;
            ssize_t s = read(sigchildfd, &fdsi, sizeof(fdsi));
            if (s != sizeof(fdsi)) {
                perror("read");
                exit(1);
            }

            if (fdsi.ssi_signo == SIGCHLD) {
                pid_t pid;
                int status;
                pid = waitpid(-1, &status, WNOHANG); 
                std::cout << "Child process with PID " << pid << " has exited.\n";
            
                memset(local_scheduler.send_buffer,'\0',1024);

                string serializedMap(shmaddr);
                map<int, string> receivedMap = deserializeMap(serializedMap);
                for (std::map<int, string>::iterator it = receivedMap.begin(); it != receivedMap.end(); ++it) 
                {
                    cout << "Key: " << it->first << ", Value: " << it->second << std::endl;
                }
                map<int,string>::iterator iter = receivedMap.find(pid);
                if(iter!= receivedMap.end())
                {
                    strcpy(local_scheduler.send_buffer,iter->second.c_str());
                    local_scheduler.send_to_server(local_scheduler.send_buffer);
                }else{
                    cout<<"Don't find the correct pid"<<endl;
                }
           }
        }else
        {
        //Fourth SITUATION: task switch check timer
        if(task_timer_handler()<0)return -1;
        }   
    }
    return 0;
}

int epoll_sigchild()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        perror("sigprocmask");
        exit(1);
    }

    sigchildfd = signalfd(-1, &mask, 0);
    if (sigchildfd == -1) {
        perror("signalfd");
        exit(1);
    }

    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = sigchildfd;

    if (epoll_ctl(local_scheduler.epoll_fd, EPOLL_CTL_ADD, sigchildfd, &event) == -1) {
        perror("epoll_ctl");
        exit(1);
    }
    return 0;
}

int set_shared_memory()
{
    key_t key = 1234;
    shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid < 0) {
        cout << "shmget failed" <<endl;
        return -1;
    }

    shmaddr = (char*)shmat(shmid, nullptr, 0);
    if (shmaddr == (char*)-1) {
        cout<< "shmat failed" <<endl;
        return -1;
    }
    return 0;
}

int main()
{
    
    if(local_scheduler.sock_create()<0)
    {
        cout<<"sock create failed"<<endl;
        return -1;
    }
    cout<<"successfull create tcp socket"<<endl;

    if(local_scheduler.sock_connect(server,1234)<0)
    {
        cout<<"sock connect failed"<<endl;
        return -1;
    }
    cout<<"successfull connect to tcp server"<<endl;

    if(local_scheduler.epoll_initialisation()<0)
    {
        cout<<"epoll initialisation failed"<<endl;
        return -1;
    }
    cout<<"successfull initialise epoll"<<endl;
    
    if(ntp_client_initialisation()<0)
    {
        cout<<"ntp_client initialsation failed"<<endl;
    }
    cout<<"successfull initialise ntpclient"<<endl;

   if(ntp_timer()<0)
   {
        cout<<"ntp_timer failed"<<endl;
        return -1;
   }

   if(task_timer_initialise()<0)
   {
        cout<<"task_timer initialise failed"<<endl;
        return -1;
   }

   if(epoll_sigchild()<0)
   {
        cout<<"failed to create sigchild fd and add it in epoll"<<endl;
        return -1;
   }

   if(set_shared_memory()<0)return-1;

    printf("========================\n==TCP&NTP CLIENT START==\n========================\n");

    while (1)
    {
        switch (read_number=epoll_wait(local_scheduler.epoll_fd,local_scheduler.events,128,-1))//time out -1 for infinite waiting
        {
            case 0:
                cout<<"time out"<<endl;
                break;//go out of this switch but still in while loop..

            case -1:
                cout<<"epoll_Wait error"<<endl;
                close(local_scheduler.client);
                return -1;
        
            default:
                if(handle_event()<0)
                {
                    cout<<"failed to handle event"<<endl;
                    close(local_scheduler.client);
                    return -1;
                }
        }
    }
    return 0;
}