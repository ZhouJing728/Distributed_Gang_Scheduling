#include <list>
#include <sys/timerfd.h>
#include "client.h"
#include "ntp_client-Gaa/src/ntp_client.h"
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
WSADATA xwsa_data;
WSAStartup(MAKEWORD(2, 0), &xwsa_data);
#endif // defined(_WIN32) || defined(_WIN64)
using namespace std;


#define server_port 1234

string server = "127.0.0.1";

int ntp_timerfd;
//events number from epoll_Wait()
int read_number;

Client local_scheduler;
//has been taken from the list, the currently executed scheduling
schedule schedule_current;
//New schedules that may be received during the execution of the current schedule
vector<schedule> schedule_new;
//true, when schedule_current not finished
bool dispatching=false;
//schedule received during execution of current schedule
bool new_schedule=false;

x_int64_t deviation_in_microsecond;

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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// tcpcli_receive() receive schedules, 
//* set the newschedule flag
//* push the schedule into schedule_list
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
int tcpcli_receive()
{
    if(recv(local_scheduler.client,local_scheduler.receive_buffer,1024,0)<=0)
    {
        cout<<"failed to receive from server, connection closed"<<endl;
        return -1;
    }
    
    new_schedule = true;

    schedule schedule_received;

    schedule_received.ParseFromArray(local_scheduler.receive_buffer,1024);

    schedule_new.push_back(schedule_received);

    return 0;
    
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//tcpcli() create a tcp socket and connect to server, it keeps receiving schedules and saving it.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
void *tcpcli(void *arg){

    printf("====================\n",
            "==TCP CLIENT START==\n",
            "====================\n");

    if(local_scheduler.sock_create()<0)
    {
        cout<<"failed to create socket"<<endl;
        
    }else if(local_scheduler.sock_connect(server,server_port)<0)
    {
        cout<<"failed to connect server"<<endl;
        
    }else
    {
        while (1)
        {
            tcpcli_receive();
        }
      
    }
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
        printf("epoll_timer create failed!\n");
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
        printf("send_timer settime failed!\n");
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
    xtm_vnsec = ntpcli_req_time(xntp_this,xut_tmout);
    if (XTMVNSEC_IS_VALID(xtm_vnsec))
    {
        printf("arrived 1\n");
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
                printf("-----Schedule Generate and send process failed------\n");
                return -1;
            }
        
        //SECOND SITUATION: receive from server    
        }else if(fd_temp==local_scheduler.client&&(local_scheduler.events[i].events&EPOLLIN))
        {
            if(tcpcli_receive()<0)return -1;
            printf("successfull to receive and store schedules\n");

                      
        }else
        {
        //Third SITUATION: task switch check timer
        
        }   
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

    if(local_scheduler.sock_connect()<0)
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
    printf("========================\n",
        "==TCP&NTP CLIENT START==\n",
        "========================\n");

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