#include "client.h"
#include <list>
using namespace std;


#define server_port 1234

string host = "127.0.0.1";

Client local_scheduler;

typedef struct schedule_local{
    struct tm *start_time;
    task tasks[4];
}schedule_local;


list<schedule_local> schedule_list;

bool new_schedule;//initialise as false in main()

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// tcpcli_receive() receive schedules, set the newschedule flag, 
// translate time structure into tm, which can be used by functions of <time.h >
// and push the schedule into schedule_list
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void tcpcli_receive()
{
    if(recv(local_scheduler.client,local_scheduler.receive_buffer,1024,0)<=0)
    {
        cout<<"failed to receive from server, connection closed"<<endl;
        local_scheduler.close_client();
    }else
    {
        new_schedule = true;

        schedule schedule;
        schedule.ParseFromArray(local_scheduler.receive_buffer,1024);
        schedule_local schedule_local;
        start_time time =schedule.start_time();
        schedule_local.start_time->tm_hour= time.hour();
        schedule_local.start_time->tm_min= time.min();
        schedule_local.start_time->tm_sec= time.sec();
        for(int i =0;i++;i<4){
            task task = schedule.tasks(i);
            schedule_local.tasks[i] =task;
        }
        schedule_list.push_back(schedule_local);
    }
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
        
    }else if(local_scheduler.sock_connect(host,server_port)<0)
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

