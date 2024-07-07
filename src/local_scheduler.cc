#include <list>
#include <sys/timerfd.h>
#include <sys/time.h>
#include<sys/signalfd.h>
#include <signal.h>
#include<sys/wait.h>
#include<map>
#include<sstream>
#include<deque>
#include<string>
#include<iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include "../include/client.h"
#include "../lib/ntp_client-Gaa/src/ntp_client.h"
#include "../MESSAGES/message.pb.h"
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
WSADATA xwsa_data;
WSAStartup(MAKEWORD(2, 0), &xwsa_data);
#endif // defined(_WIN32) || defined(_WIN64)
using namespace std;
using namespace Message::protobuf;


struct timeinterval{
    vector<int> cpus;//it could be multiple cpus switch task at same time
                    //at least the first tasks
    long int timeinterval_ms;
};

struct task_local{
    string task_id;
    string path;
};

vector<vector<task_local>> tasksets_local;

vector<timeinterval> timeinervals;

string CGROUP_PATH;

long int ntp_request_interval_sec;

long int ntp_request_interval_nsec;

int ntp_timerfd;

int task_timerfd;

int sigchildfd;

int num_cpu;

int read_number;
//the position for next to be execute task in schedule
vector<int> position;

int index_timeInterval;
//for interaive calculating start time of task
struct timeval ST;

map<int,string>pid_to_taskid;

vector<string> id_endTask_lastSched;
//ids of tasks that has run on this local. these task don't need to execute again and get a new pid
vector<vector<string>> taskids_exist_local;

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
x_cstring_t host;
x_int16_t port=123;
x_uint32_t xut_tmout = 3000;


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
        local_scheduler.pLevel.P_ERR("ntpcli_open() return X_NULL, errno : %d\n", errno);
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
        local_scheduler.pLevel.P_ERR("ntp_timer create failed!\n");
        return -1;
    }
    struct itimerspec timer;
    //---------------------------------------//
    //triggered every 60s after first trigger
    //(30s after the this program started)
    timer.it_interval.tv_sec=60;//this doesn't work
    timer.it_interval.tv_nsec=0;
    timer.it_value.tv_nsec=ntp_request_interval_nsec;
    timer.it_value.tv_sec=ntp_request_interval_sec;

    if(timerfd_settime(ntp_timerfd,0,&timer,NULL)<0)
    {
        local_scheduler.pLevel.P_ERR("ntp_timer settime failed!\n");
        return -1;
    }
    epoll_event ev;
    ev.events= EPOLLIN;
    ev.data.fd = ntp_timerfd;
    if(epoll_ctl(local_scheduler.epoll_fd,EPOLL_CTL_ADD,ntp_timerfd,&ev)<0)
    {
        local_scheduler.pLevel.P_ERR("epoll_ctl_Add timer failed!\n");
        return -1;
    }
    local_scheduler.pLevel.P_NODE("SUCCESSFULLY CREATE AND ADD TIMER TO EPOLL\n");
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//ntp_timer_handler() request time deviation for once, save it in deviation in microsecons
int ntp_timer_handler()
{
    uint64_t exp;
    if(read(ntp_timerfd,&exp,sizeof(uint64_t))<0)
    {
        local_scheduler.pLevel.P_ERR("failed read from timer\n");
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
        local_scheduler.pLevel.P_ERR("\n[%d] %s:%d : errno = %d\n",
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
        local_scheduler.pLevel.P_ERR("task_timer create failed!\n");
        return -1;
    }
    struct itimerspec timer;
    timer.it_interval.tv_nsec=0;
    timer.it_interval.tv_sec=0;
    timer.it_value.tv_nsec=0;
    timer.it_value.tv_sec=0;
    
    if(timerfd_settime(task_timerfd,0,&timer,NULL)<0)
    {
        local_scheduler.pLevel.P_ERR("task_timer settime failed!\n");
        return -1;
    }
    epoll_event ev;
    ev.events= EPOLLIN;
    ev.data.fd = task_timerfd;
    if(epoll_ctl(local_scheduler.epoll_fd,EPOLL_CTL_ADD,task_timerfd,&ev)<0)
    {
        local_scheduler.pLevel.P_ERR("epoll_ctl_Add task timer failed!\n");
        return -1;
    }
    local_scheduler.pLevel.P_NODE("SUCCESSFULLY CREATE AND ADD TIMER TO EPOLL\n");
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

bool compareTimeInterval(const timeinterval&a,const timeinterval &b) {
    return a.timeinterval_ms< b.timeinterval_ms;
}

void insert_timeIntervals(task task,int cpu)
{

    for (auto &interval : timeinervals) {
        if (interval.timeinterval_ms == task.relevant_swtichtime_ms()) {
            interval.cpus.insert(interval.cpus.end(), cpu);
            return;
        }
    }
    timeinterval ti;
    ti.cpus.push_back(cpu);
    ti.timeinterval_ms=task.relevant_swtichtime_ms();
    timeinervals.push_back(ti);
    std::sort(timeinervals.begin(), timeinervals.end(), compareTimeInterval);

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//organize_current_schedule() set the dispatching flag, reset the position from 0, set timer for first task start
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int organize_current_schedule()
{
    //size = schedule_current.tasksets(0).tasks_size();
    dispatching = true;
    index_timeInterval = 0;
    for(int i=0;i<num_cpu;i++)position[i] = 0;

    /***********the start time of first task is the start time of schedule********************/
    struct itimerspec timer;

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    ST.tv_sec=schedule_current.start_time().sec();
    ST.tv_usec=schedule_current.start_time().ms()*1000;

    long long diff_usec = time_diff_microseconds(current_time,ST);

    if(diff_usec<0)
    {
        local_scheduler.pLevel.P_ERR("start time is already passed %lld usec!\n",diff_usec);
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
    local_scheduler.pLevel.P_NODE("successful to set the first timer of a schedule\n");
    if(new_schedule){
        for(int i=0;i<num_cpu;i++)tasksets_local[i].clear();
        timeinervals.clear();
        for(int i=0;i<schedule_current.tasksets_size();i++)
        {
            for(int j=0;j<schedule_current.tasksets(i).tasks_size();j++)
            {
                task task=schedule_current.tasksets(i).tasks(j);
                task_local tl;
                tl.path=task.path();
                tl.task_id=task.task_id();
                tasksets_local[i].push_back(tl);

                insert_timeIntervals(task,i);
            }
            local_scheduler.pLevel.P_DBG("taskset of %d taskset is %d\n",i,tasksets_local[i].size());
        }
    }
    new_schedule=false;
    return 0;
}

void write_to_cgroup_file(const char *file_path, const char *value) {
    int fd = open(file_path, O_WRONLY);
    if (fd == -1) {
        local_scheduler.pLevel.P_ERR("Error opening file\n");
        exit(EXIT_FAILURE);
    }

    if (write(fd, value, strlen(value)) == -1) {
        local_scheduler.pLevel.P_ERR("Error writing to file\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void add_pid_to_cgroup(const char *tasks_file_path, pid_t pid) {

    int fd;

    fd = open(tasks_file_path, O_WRONLY);
    if (fd == -1) {
        local_scheduler.pLevel.P_ERR("Error opening tasks file\n");
        exit(EXIT_FAILURE);
    }
    //write pid to task file 
    if (dprintf(fd, "%d", pid) == -1) {
        local_scheduler.pLevel.P_ERR("Error writing to tasks file\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//check_task_initialised() returns true, if this task has been executed at least once before position
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
bool check_task_initialised(int cpu, string id)
{
    vector<string> taskids_exist_cpu =taskids_exist_local[cpu];
    for(vector<string>::iterator it= taskids_exist_cpu.begin();it!=taskids_exist_cpu.end();it++)
    {
        if(*it == id)return true;
    }
    return false;
}

bool check_file_empty(string id)
{
    char buffer[128];
    sprintf(buffer,"%s/%s/%s",CGROUP_PATH.c_str(),id.c_str(),"tasks");
    FILE *file = fopen(buffer, "r");
    if (file == NULL) {
        local_scheduler.pLevel.P_ERR("fopen failed\n");
        return -1; 
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        local_scheduler.pLevel.P_ERR("fseek failed!\n");
        fclose(file);
        return -1; 
    }

    long file_size = ftell(file);
    if (file_size == -1) {
        local_scheduler.pLevel.P_ERR("ftell failed!\n");
        fclose(file);
        return -1; 
    }

    fclose(file);

    return file_size == 0;

}

/**********from second task, the start time = last start time + last duration************/
int timer_update()
{
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    struct timeval given_time;
    given_time.tv_sec=ST.tv_sec+timeinervals[index_timeInterval].timeinterval_ms/1000;
    given_time.tv_usec= ST.tv_usec+(timeinervals[index_timeInterval].timeinterval_ms%1000)*1000;
    long long diff_usec = time_diff_microseconds(current_time,given_time);
    if(diff_usec<0)
    {
        local_scheduler.pLevel.P_ERR("start time(not the first) is already passed!\n");
        local_scheduler.pLevel.P_DBG("current time is %lld usec later than next start time\n",diff_usec);
        return -1;
    }
    
    struct itimerspec timer;
    
    int64_t sum_ns = diff_usec*1000LL-deviation_in_microsecond*1000LL;
    timer.it_value.tv_sec = sum_ns/1000000000LL;
    timer.it_value.tv_nsec = sum_ns%1000000000LL;
    timer.it_interval.tv_nsec = 0;
    timer.it_interval.tv_sec =0;

    if(timerfd_settime(task_timerfd,0,&timer,NULL)<0)return -1;
    index_timeInterval++;
    local_scheduler.pLevel.P_NODE("succeed to update the timer\n");
    return 0;
}

int task_switch(int cpu)
{
    if(position[cpu]==0)
    {
        if(id_endTask_lastSched[cpu]!="empty")
        {
            char buffer[128];
            sprintf(buffer,"%s/%s/%s",CGROUP_PATH.c_str(),id_endTask_lastSched[cpu].c_str(),"freezer.state");
            write_to_cgroup_file(buffer, "FROZEN");
            local_scheduler.pLevel.P_NODE("====================\nLAST TASK WITH ID %s OF LAST SCHEDULE HAS BEEN FROZEN\n====================\n",id_endTask_lastSched[cpu].c_str());
        }else{
            local_scheduler.pLevel.P_NODE("====================\nLAST TASK TIME SLICE OF LAST SCHEDULE IS EMPTY\n====================\n");
        }
    }
    /**********frozen the last task, if last task is empty,(no task for last time slice),skip this step*********/
    //vector<task_local>::iterator it = tasksets_local[cpu].begin();
    if(position[cpu]>0){
        //vector<task_local>::iterator it = tasksets_local[cpu].begin();
        //for(int i=0;i<position[cpu]-1;i++)it++;
        vector<task_local>taskset =tasksets_local[cpu];        
        task_local last_task = taskset[position[cpu]-1];
        if(last_task.task_id!="empty")
        {
            char buffer[128];
            string id = last_task.task_id.substr(1);
            sprintf(buffer,"%s/%s/%s",CGROUP_PATH.c_str(),id.c_str(),"freezer.state");
            write_to_cgroup_file(buffer, "FROZEN");
            local_scheduler.pLevel.P_NODE("====================\nTask %s at position %d has been frozen\n",id.c_str(),position[cpu]-1);
        }
    }
    /*********thaw the next task********************/
    //it++;
    task_local next_task = tasksets_local[cpu][position[cpu]];
    if(next_task.task_id!="empty")
    {
        char state[128];
        string id = next_task.task_id.substr(1);
        sprintf(state,"%s/%s/%s",CGROUP_PATH.c_str(),id.c_str(),"freezer.state");
        //cout<<state<<endl;
        write_to_cgroup_file(state, "THAWED");
        local_scheduler.pLevel.P_NODE("Task %s IS THAEWD AT POSITION %d NOW\n",id.c_str(),position[cpu]);
        print_current_time();
        //*****if not be executed before, it needs to get pid and store it ****//
        if(!check_task_initialised(cpu,id))
        {
            taskids_exist_local[cpu].push_back(id);
            pid_t pid = fork();
            if (pid == -1) {
                local_scheduler.pLevel.P_ERR("Error forking process\n");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {//child process
                prctl(PR_SET_PDEATHSIG,SIGKILL);

                cpu_set_t set;
                CPU_ZERO(&set);
                CPU_SET(cpu,&set);

                if (sched_setaffinity(0, sizeof(set), &set) == -1) {
                    local_scheduler.pLevel.P_ERR("sched_setaffinity!\n");
                    exit(EXIT_FAILURE);
                }

                const char* path = next_task.path.c_str();
                execl(path, path, NULL);
                local_scheduler.pLevel.P_ERR("Error executing program!\n");
                exit(EXIT_FAILURE);
            }else//father process here continue
            {
                char tasks[128];
                sprintf(tasks,"%s/%s/%s",CGROUP_PATH.c_str(),id.c_str(),"tasks");
                add_pid_to_cgroup(tasks,pid);
                pid_to_taskid.insert(pair<int,string>(pid,id));
            }
        }
    }else/*********if next task is empty, skip the next step*******************/
    {
        local_scheduler.pLevel.P_NODE("Next task time slice is empty!\n====================\n");
    }

    if((schedule_current.tasksets(cpu).tasks(position[cpu]).task_id()!="empty")&&(position[cpu]==schedule_current.tasksets(cpu).tasks_size()-1))
    {
        id_endTask_lastSched[cpu]=next_task.task_id.substr(1);
        return 0;
    }
    position[cpu]++;
    return 0;
}

int task_timer_handler()
{
    uint64_t exp;
    if(read(task_timerfd,&exp,sizeof(uint64_t))<0)
    {
        local_scheduler.pLevel.P_ERR("failed read from task timer\n");
        return -1;
    }

    for(long unsigned int i=0;i<timeinervals[0].cpus.size();i++)
    {
        if(task_switch(timeinervals[0].cpus[i])<0)
        local_scheduler.pLevel.P_ERR("failed to handle task timer for cpu\n");
    }
    
    /*********current schedule finished********************/
    /*must be the last task of all tasksets cause the current shceduler will be cleaned*/
    /*the last task still in list,because we need its switch time to update timer */
    if(index_timeInterval==(int)timeinervals.size()-1)
    {
        if(new_schedule)
        {
            local_scheduler.pLevel.P_NODE("THERE IS NEW SCHEDULE DETECTED!\n");
            schedule_current.Clear();
            schedule_current.mutable_start_time()->CopyFrom(schedule_new.start_time());
            schedule_current.mutable_tasksets()->CopyFrom(schedule_new.tasksets());
            schedule_new.Clear();
            if(organize_current_schedule()<0)return -1;
            local_scheduler.pLevel.P_NODE("successfully organized the new schedule\n");
        }else{
            start_time nst;
            //start time for repeated schedule = last schedule'start time + last task's deadline
            int i = timeinervals.size();
            int64_t given_time_us = ST.tv_sec*1000000LL+ST.tv_usec+timeinervals[i-1].timeinterval_ms*1000LL;
            nst.set_sec(given_time_us/1000000LL);
            nst.set_ms((given_time_us%1000000LL)/1000LL);
            schedule_current.clear_start_time();
            schedule_current.mutable_start_time()->CopyFrom(nst);
            if(organize_current_schedule()<0)return -1;
            local_scheduler.pLevel.P_NODE("successfully organized the repeated schedule!\n");
        }
        return 0;//now need to update timer,cause current schedule'last deadline will be updated as new schedule's first start
    }

    if(timer_update()<0)
    {
        local_scheduler.pLevel.P_ERR("ntp_timer settime failed!\n");
        cout<<"failed to update timer with task position:"<<index_timeInterval<<endl;
        return -1;
    }
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
            local_scheduler.pLevel.P_NODE("\n==============================\n""REQUSTING TIME DEVIATION\n""==============================\n");
            if(ntp_timer_handler()<0)
            {
                local_scheduler.pLevel.P_ERR("-----request time deviation failed------\n");
                return -1;
            }
        
        //SECOND SITUATION: receive from server    
        }else if(fd_temp==local_scheduler.client&&(local_scheduler.events[i].events&EPOLLIN))
        {
            memset(local_scheduler.receive_buffer,'\0',sizeof(local_scheduler.receive_buffer));
            if(recv(local_scheduler.client,local_scheduler.receive_buffer,1024,0)<=0)
            {
                local_scheduler.pLevel.P_ERR("failed to receive from server, connection closed!\n");
                return -1;
            }
            new_schedule = true;
            if(dispatching)
            {
                schedule_new.Clear();
                schedule_new.ParseFromArray(local_scheduler.receive_buffer,1024);
                local_scheduler.pLevel.P_NODE("*******a new schedule is stored in schedule_new********<----\n");
                local_scheduler.pLevel.P_DBG("size of new schedule's first taskset is: %d",schedule_new.tasksets(0).tasks_size());
                print_current_time();
            }else{
                schedule_current.Clear();
                schedule_current.ParseFromArray(local_scheduler.receive_buffer,1024);
                local_scheduler.pLevel.P_NODE("a new schedule is stored in schedule_current\n");
                print_current_time();
                if(organize_current_schedule()<0)return -1;
                local_scheduler.pLevel.P_NODE("successfully organized current schedule\n");
            }
         //THIRD SITUATION : SOME CHID PROCESS HAS ENDED             
        }else if(fd_temp == sigchildfd)
        {
            struct signalfd_siginfo fdsi;
            ssize_t s = read(sigchildfd, &fdsi, sizeof(fdsi));
            if (s != sizeof(fdsi)) {
                local_scheduler.pLevel.P_ERR("read!\n");
                exit(1);
            }

            if (fdsi.ssi_signo == SIGCHLD) {
                pid_t pid;
                int status;
                pid = waitpid(-1, &status, WNOHANG); 
                local_scheduler.pLevel.P_NODE("Child process with PID %d has exited.\n",pid);
            
                memset(local_scheduler.send_buffer,'\0',1024);

                // for (std::map<int, string>::iterator it = pid_to_taskid.begin(); it != pid_to_taskid.end(); ++it) 
                // {
                //     cout << "Key: " << it->first << ", Value: " << it->second << std::endl;
                // }
                map<int,string>::iterator iter = pid_to_taskid.find(pid);
                if(iter!= pid_to_taskid.end())
                {
                    strcpy(local_scheduler.send_buffer,iter->second.c_str());
                    local_scheduler.send_to_server(local_scheduler.send_buffer);
                }else{
                    local_scheduler.pLevel.P_ERR("Didn't find the correct pid!\n");
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
        local_scheduler.pLevel.P_ERR("sigprocmask\n");
        exit(1);
    }

    sigchildfd = signalfd(-1, &mask, 0);
    if (sigchildfd == -1) {
        local_scheduler.pLevel.P_ERR("signalfd!\n");
        exit(1);
    }

    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = sigchildfd;

    if (epoll_ctl(local_scheduler.epoll_fd, EPOLL_CTL_ADD, sigchildfd, &event) == -1) {
        local_scheduler.pLevel.P_ERR("epoll_ctl\n");
        exit(1);
    }
    return 0;
}

int main()
{
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("../config.ini", pt);
    string str=pt.get<string>("ip_ntpServer.value");
    host=str.c_str();
    string ip=pt.get<string>("ip_ntpServer.value");
    int server_port = pt.get<int>("port_globalscheduler.value");
    CGROUP_PATH = pt.get<string>("path_cgroup.value");
    string server=pt.get<string>("ip_globalscheduler.value");
    ntp_request_interval_nsec=pt.get<long int>("ntp_request_interval.nsec");
    ntp_request_interval_sec=pt.get<long int>("ntp_request_interval.sec");
    num_cpu=pt.get<int>("num_cpus.value");
    for(int i=0;i<num_cpu;i++)
    {
        id_endTask_lastSched.push_back("empty");
        position.push_back(0);
    }

    tasksets_local.resize(num_cpu);
    
    taskids_exist_local.resize(num_cpu);
    
    if(local_scheduler.sock_create()<0)return -1;
    
    local_scheduler.pLevel.P_NODE("successfull create tcp socket\n");

    if(local_scheduler.sock_connect(server,server_port)<0)return -1;
   
    local_scheduler.pLevel.P_NODE("successfull connect to tcp server\n");

    if(local_scheduler.epoll_initialisation()<0)return -1;
    
    local_scheduler.pLevel.P_NODE("successfull initialise epoll\n");
    
    if(ntp_client_initialisation()<0)return -1;
    
    local_scheduler.pLevel.P_NODE("successfull initialise ntpclient\n");

   if(ntp_timer()<0)
   {
        local_scheduler.pLevel.P_ERR("ntp_timer failed\n");
        return -1;
   }

   if(task_timer_initialise()<0)
   {
        local_scheduler.pLevel.P_ERR("task_timer initialise failed\n");
        return -1;
   }

   if(epoll_sigchild()<0)
   {
        local_scheduler.pLevel.P_ERR("failed to create sigchild fd and add it in epoll\n");
        return -1;
   }

    printf("========================\n==TCP&NTP CLIENT START==\n========================\n");

    while (1)
    {
        switch (read_number=epoll_wait(local_scheduler.epoll_fd,local_scheduler.events,128,-1))//time out -1 for infinite waiting
        {
            case 0:
                local_scheduler.pLevel.P_ERR("time out!\n");
                break;//go out of this switch but still in while loop..

            case -1:
                local_scheduler.pLevel.P_ERR("epoll_Wait error\n");
                close(local_scheduler.client);
                return -1;
        
            default:
                if(handle_event()<0)
                {
                    local_scheduler.pLevel.P_ERR("failed to handle event!\n");
                    close(local_scheduler.client);
                    return -1;
                }
        }
    }
    return 0;
}