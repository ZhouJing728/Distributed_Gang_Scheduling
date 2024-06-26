#include<vector>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include "../include/server.h"
#include "../include/strategies.h"
#include "MESSAGES/message.pb.h"
/*******************************************/
/* from ntp server*************************/
#include <sys/time.h> /* gettimeofday() */
#include <sys/wait.h>
#include <time.h> /* for time() and ctime() */

#define UTC_NTP 2208988800U /* 1970 - 1900 */
//#include "ntp-master/ntp-master/server.h"
/* end from ntp server*/
/********************************************/
using namespace std;
using namespace Message::protobuf;

//******GLOBAL VARIBLES*******//

Server global_scheduler;

Strategy mysched;

boost::property_tree::ptree pt;

int ntp_fd;

int timer_fd;

int hyperperiode_ms;
//true,when still has schedule to be send or job status changed
bool need_schedule = false;
//true, when new clients connect,
//reset to false,when this changed has been seen by schedule stratrgy
bool clinets_status_changed = false;

struct itimerspec send_timer;

start_time next_Starttime;
//queue<Job_gang> job_queue;

vector<Job_gang> job_list;

vector<vector<task>> ousterhaut_table;//MATRIX FOR SCHED FOR ALL PROCESSORS (assume we only have two local scheduler now

map<int,int>taskid_finish;
//Job_gang sched[4];//ARRAY FOR TRANSMIT TO SINGLE LOCAL(4 timeslice as a round)

int global_scheduler_port;

/********************************************/
/*from ntp server*/
extern "C"{
    void ntpServer_reply();

    void request_process_loop(int fd);
    int ntp_server(struct sockaddr_in bind_addr);
    void wait_wrapper();
    int ntp_reply(
        int socket_fd,
        struct sockaddr *saddr_p,
        socklen_t saddrlen,
        unsigned char recv_buf[],
        uint32_t recv_time[]);
    void log_ntp_event(const char *msg);
    void log_request_arrive(uint32_t *ntp_time);
    int die(const char *msg);
    void gettime64(uint32_t ts[]);
}

//********FUNCTIONS********//

int acceptNewJob(int fd);

int finishJob(int fd);

int get_rpn_by_id(int id);

void remove_job_by_id(int id);

int handle_event();

int readByevent(int i);

//int send_schedules_periodically();

/*triggered every hyperperiode(60s) after first trigger(30s after the this program started)*/
int epoll_timer();

int timer_handler();

int get_and_send_schedule();

int epoll_ntpServer();

void ntpServer();
/*initialise the first nst as current_time + one hyperperiode(1 min)*/
void initialise_nst();
/*nst =last_nst + one hyperperiode (1min)*/
int update_nst();


int main()
{
    boost::property_tree::ini_parser::read_ini("../config.ini", pt);
    global_scheduler_port= pt.get<int>("port_globalscheduler.value");
    global_scheduler.max_client=pt.get<int>("max_local_num.value");


    if(global_scheduler.sock_create()<0)
    {
        global_scheduler.pLevel.P_ERR("sock create failed\n");
        return -1;
    }
    if(global_scheduler.sock_bindAndListen(global_scheduler_port)<0)
    {
        global_scheduler.pLevel.P_ERR("sock bind and listen failed\n");
        return -1;
    }

    if(global_scheduler.epoll_initialisation()<0)
    {
        global_scheduler.pLevel.P_ERR("epoll initialisation failed\n");
        return -1;
    }
   if(epoll_ntpServer()<0)
   {
        global_scheduler.pLevel.P_ERR("epoll_ntpServer failed\n");
        return -1;
   }

   if(epoll_timer()<0)
   {
        global_scheduler.pLevel.P_ERR("epoll_timer failed\n");
        return -1;
   }

   initialise_nst();
   global_scheduler.pLevel.P_NODE("SUCCESSFULLY INITIALISED NST\n");

    while (1)
    {
        switch (global_scheduler.read_number=epoll_wait(global_scheduler.epoll_fd,global_scheduler.events,128,-1))//time out -1 for infinite waiting
        {
            case 0:
                global_scheduler.pLevel.P_NODE("time out\n");
                break;//go out of this switch but still in while loop..

            case -1:
                global_scheduler.pLevel.P_ERR("epoll_Wait error\n");
                close(global_scheduler.server);
                return -1;
        
            default:
                if(handle_event()<0)
                {
                    global_scheduler.pLevel.P_ERR("failed to handle event\n");
                    close(global_scheduler.server);
                    return -1;
                }
        }
    }
    return 0;
}

int handle_event()
{
    for(int i=0;i<global_scheduler.read_number;i++)
    {
        int fd_temp = global_scheduler.events[i].data.fd;
        //Fist situation: send_timer is triggered
        if(fd_temp == timer_fd&&(global_scheduler.events[i].events&EPOLLIN))
        {
            global_scheduler.pLevel.P_NODE("\n==============================\n""TIMER TRIGGERED,BEGIN SCHEDULE\n""==============================\n\n");
            if(timer_handler()<0)
            {
                global_scheduler.pLevel.P_ERR("-----Schedule Generate and send process failed------\n");
                return -1;
            }
        //Second situation: ntp client requests for time
        }else if(fd_temp == ntp_fd)
        {
            request_process_loop(ntp_fd);
        //Third SITUATION: event of server->new connection    
        }else if(fd_temp==global_scheduler.server&&(global_scheduler.events[i].events&EPOLLIN))
        {
            if(global_scheduler.new_connection(i)<0)return -1;
            clinets_status_changed =true;
            //the connection of laucher doesn't matter,cause laucher comes before every schedule method.
            //after schedule this flag will be reset. and this flag only make sense with still wait for processors flag.
                      
        }else
        {
        //Fourth SITUATION: read from client (job finished) or laucher (new job)
            if(readByevent(i)<0)return -1;
        }   
    }
    return 0;
}

int readByevent(int i)
{
    int fd = global_scheduler.events[i].data.fd;//server_accept for that client

    if(fd<0)
    {
        cout<<i<<endl;
        global_scheduler.pLevel.P_ERR("illegal fd triggered Epollin!\n");
        return -1;
    }
    
    memset(global_scheduler.read_buffer,'\0',sizeof(global_scheduler.read_buffer));
    size_t size = read(fd,global_scheduler.read_buffer,sizeof(global_scheduler.read_buffer));
    if(size>0)
    {//************JOB FROM LAUCHER*************//
        if(fd==global_scheduler.laucher)
        {
            if(acceptNewJob(fd)<0)return -1;
            return 0;
    
        }else{//*********JOB FINISH FROM LOCAL**************//
            
            if(finishJob(fd)<0)return -1;
            return 0;
        }

    }else if(size==0)//end of file
    {
        global_scheduler.pLevel.P_NODE("client close..\n");
        epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_DEL,fd,NULL);
        close(fd);
        return -1;
    }else//error
    {
        global_scheduler.pLevel.P_ERR("read failed\n");
        return -1;
    }
    

}

int acceptNewJob(int fd)
{
    Job_gang job_accept;
    job_accept.ParseFromArray(global_scheduler.read_buffer,1024);

    if(job_accept.requested_processors()>global_scheduler.max_client||(job_accept.requested_processors()==0))
    {
        global_scheduler.pLevel.P_WRN("received wrong job, discarded it~\n");
        return -1;
    }
    job_list.push_back(job_accept);

    char path[128];
    sprintf(path,"../cgroup/%d",job_accept.job_id());

    struct stat st;
    if((stat(path,&st)<0)&&mkdir(path,0755)<0)
    {
        perror("mkdir");
        return -1;
    }

    need_schedule = true;

    global_scheduler.pLevel.P_NODE("received a new job, added to job list\n");

    return 0;

}

int get_rpn_by_id(int id)
{
    for(auto it=job_list.begin();it!=job_list.end();it++)
    {
        if(it->job_id()==id)
        return it->requested_processors();
    }
    return -1;
}

void remove_job_by_id(int id)
{
    for(auto it=job_list.begin();it!=job_list.end();it++)
    {
        if(it->job_id()==id)
        job_list.erase(it);
    }
}

int finishJob(int fd)
{
    int id=atoi(global_scheduler.read_buffer);
    taskid_finish[id]++;
    int num = get_rpn_by_id(id);
    if(num<0)
    {
        global_scheduler.pLevel.P_ERR("Don't find task with id %d in joblist!\n",id);
        return -1;
    }
    global_scheduler.pLevel.P_NODE("Job with id %d has finished in %d processors\n",id,num);

    if(taskid_finish[id]==num)
    {
        remove_job_by_id(id);
        global_scheduler.pLevel.P_NODE("Job with id %d has finished, and removed from joblist!\n",id);
        need_schedule=true;
    }
    return 0;

    
}

void ntpServer()
{
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));

    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
	ntp_fd=ntp_server(bind_addr);

}

int epoll_timer()
{
    timer_fd = timerfd_create(CLOCK_REALTIME,0);
    if(timer_fd<0)
    {
        global_scheduler.pLevel.P_ERR("epoll_timer create failed!\n");
        return -1;
    }
    //struct itimerspec send_timer;
    //---------------------------------------//
    //triggered every 20s after first trigger
    //(10s after the this program started)
    send_timer.it_interval.tv_sec=20;
    send_timer.it_interval.tv_nsec=0;
    send_timer.it_value.tv_nsec=0;
    send_timer.it_value.tv_sec=10;//this works

    if(timerfd_settime(timer_fd,0,&send_timer,NULL)<0)
    {
        global_scheduler.pLevel.P_ERR("send_timer settime failed!\n");
        return -1;
    }
    epoll_event ev;
    ev.events= EPOLLIN;
    ev.data.fd = timer_fd;
    if(epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_ADD,timer_fd,&ev)<0)
    {
        global_scheduler.pLevel.P_ERR("epoll_ctl_Add timer failed!\n");
        return -1;
    }
    global_scheduler.pLevel.P_NODE("SUCCESSFULLY CREATE AND ADD TIMER TO EPOLL\n");
    return 0;
}

long long time_diff_microseconds(struct timeval start, struct timeval end) {
    long long start_usec = start.tv_sec * 1000000LL + start.tv_usec;
    long long end_usec = end.tv_sec * 1000000LL + end.tv_usec;
    return end_usec - start_usec;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//send timer need to be triggered (about)10s before new nst.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int update_timer()
{
    struct itimerspec timer;

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    struct timeval given_time;
    given_time.tv_sec=next_Starttime.sec();
    given_time.tv_usec=next_Starttime.ms()*1000;

    global_scheduler.pLevel.P_NODE("NST time: %ld seconds and %lld microseconds\n", given_time.tv_sec, given_time.tv_usec/1000LL);

    long long diff_usec = time_diff_microseconds(current_time,given_time);

    global_scheduler.pLevel.P_NODE("Time difference: %lld milliseconds\n", diff_usec/1000LL);

    if(diff_usec<0)
    {
        global_scheduler.pLevel.P_ERR("start time is already passed!\n");
        return -1;
    }
    int64_t sum_ns=diff_usec*1000LL-10*1000*1000*1000LL;
    timer.it_value.tv_sec = sum_ns/(1000LL*1000LL*1000LL);
    timer.it_value.tv_nsec = sum_ns%(1000LL*1000LL*1000LL);
    timer.it_interval.tv_nsec = 0;
    timer.it_interval.tv_sec =0;

    if(timerfd_settime(timer_fd,0,&timer,NULL)<0)
    {
        global_scheduler.pLevel.P_ERR("send_timer settime failed!\n");
        return -1;
    }
    global_scheduler.pLevel.P_NODE("timer has been updated!\n");
    return 0;
}

int epoll_ntpServer()
{
    ntpServer();
    epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=ntp_fd;

    if(epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_ADD,ntp_fd,&ev)<0)
    {
        global_scheduler.pLevel.P_ERR("epoll_ctl_add ntp_Fd failed\n");
        return -1;
    }
    global_scheduler.pLevel.P_NODE("SUCCESSFULLY TO ADD NTP SERVER TO EPOLL\n");
    return 0;
}

//current time +20s
void initialise_nst()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    next_Starttime.set_sec(tv.tv_sec+20);
    next_Starttime.set_ms(tv.tv_usec/1000);
}

int update_nst()
{
    hyperperiode_ms = mysched.get_hyperperiode_ms();
    //*******none schedule yet or has not been sent(no client)********//
    if(!hyperperiode_ms)
    {
        next_Starttime.set_sec(next_Starttime.sec()+20);//to be more determinted
    }else{//********has repeated schedule or new schedule********//
        int64_t final_ms= next_Starttime.ms()+hyperperiode_ms+next_Starttime.sec()*1000LL;

        next_Starttime.set_sec(final_ms/1000LL);
        next_Starttime.set_ms(final_ms%1000LL);

        if(update_timer()<0)return -1;//send timer need to be triggered 10s before nst
    }
    global_scheduler.pLevel.P_NODE("nst has been updated\n");
    struct tm *tm_info;
    struct timeval tv;
    char buffer[30];
    char usec_buffer[21];
    tv.tv_sec=next_Starttime.sec();
    tv.tv_usec=next_Starttime.ms()*1000LL;
    tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(usec_buffer, 21, "%06ld", tv.tv_usec);
    global_scheduler.pLevel.P_NODE("NST TIME :%s.%s(us)\n", buffer, usec_buffer);
    return 0;
}

int timer_handler()
{
    uint64_t exp;
    if(read(timer_fd,&exp,sizeof(uint64_t))<0)
    {
        global_scheduler.pLevel.P_ERR("failed read from timer\n");
        return -1;
    }
    //NO NEW RECEIVED OR DELETED JOBS
    if(!need_schedule)
    {
        //There are jobs in job list that wait for more clients 
        //&& clients status changed
        if(mysched.get_wait_for_processors()&&clinets_status_changed)
        {
            if(get_and_send_schedule()<0)
            {
                global_scheduler.pLevel.P_ERR("failed to get and send schedules\n");
                update_nst();
                return -1;
            }
            global_scheduler.pLevel.P_NODE("SUCCESSFULLY SEND SCHEDULE\n");
        }else
        {
            global_scheduler.pLevel.P_NODE("there is no new job received or finished \n");
        }
    //NEW JOB STATUS
    }else{
        global_scheduler.pLevel.P_NODE("trying to get schedule using RR\n");

        if(get_and_send_schedule()<0)
        {
            global_scheduler.pLevel.P_ERR("failed to get and send schedules\n");
            update_nst();
            return -1;
        }
        global_scheduler.pLevel.P_NODE("SUCCESSFULLY SEND SCHEDULE OR NO CLIENTS YET\n");
    }

    if(update_nst()<0)return -1;
    global_scheduler.pLevel.P_NODE("SUCCESSFULLY UPDATED NST\n");
    
    return 0;
    
}

int get_and_send_schedule()
{
    if(global_scheduler.clients.size()==0)
    {
        global_scheduler.pLevel.P_NODE("there is no clients available now, will reschedule and retry in next round~\n");
        need_schedule=true;
        return 0;
    }
    ousterhaut_table.clear();
    int num_cpu_pro_local = pt.get<int>("num_cpus.value");
    int sum_cpu =global_scheduler.clients.size()*num_cpu_pro_local;
    ousterhaut_table =mysched.get_scheduleTable(mysched.Roundrobin,job_list,sum_cpu);

    clinets_status_changed = false;
    need_schedule = false;

    global_scheduler.pLevel.P_NODE("successfully got schedule_Tasks(without start time)\n");

    schedule common_sched;

    common_sched.mutable_start_time()->CopyFrom(next_Starttime);

    global_scheduler.pLevel.P_NODE("successfully set nst\n");

    for(int client = 0; client<(int)global_scheduler.clients.size();client++)
    {
        common_sched.clear_tasksets();
        vector<tasks_set_pro_cpu>tasksets;
        for(int i=client*num_cpu_pro_local;i<(client+1)*num_cpu_pro_local;i++)
        {
            vector<task> tasks = ousterhaut_table[i];
            tasks_set_pro_cpu taskset;
            for(vector<task>::iterator it = tasks.begin();it!=tasks.end();it++)
            {
                task* common_task = taskset.add_tasks();
                *common_task = *it;
            }
            tasksets.push_back(taskset);
        }
        common_sched.mutable_tasksets()->CopyFrom({tasksets.begin(),tasksets.end()});

        memset(global_scheduler.send_buffer,'\0',1024);

        global_scheduler.pLevel.P_NODE("reset send buffer\n");
        common_sched.SerializePartialToArray(global_scheduler.send_buffer,1024);
        if(write(global_scheduler.clients[client],global_scheduler.send_buffer,1024)<0)
        {
            global_scheduler.pLevel.P_ERR("failed to send schedule to client -%d-\n",client);
            return -1;
        }
        global_scheduler.pLevel.P_NODE("~~SCHEDULE TO CLIENT -%d- HAS SENT~~\n",client);
    }

    return 0;

}
