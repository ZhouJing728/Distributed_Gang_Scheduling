#include "server.h"
#include <list>
#include "MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

//******GLOBAL VARIBLES*******//

Server global_scheduler;

typedef struct jobs
{
    int job_id;
    int requested_processors;
    string job_path;
}job;

list <job> job_list;

//job array[4][2];

job **ousterhaut_table = new job *[2];//MATRIX FOR SCHED FOR ALL PROCESSORS (assume we only have two local scheduler now

job sched[4];//ARRAY FOR TRANSMIT TO SINGLE LOCAL(4 timeslice as a round)

// int client_pos = 0;

// int clients[3]= {-1,-1,-1};//-1 for clients have not connected yet. clients[0] is the fd of the first came. laucher could also be here

// int laucher;

int global_scheduler_port = 1234;
//********FUNCTIONS********//

int acceptNewJob(int fd);

void finishJob(int fd);

int handle_event();

int readByevent(int i);

int send_processes(int i);



int main()
{
    
    if(global_scheduler.sock_create()<0)
    {
        cout<<"sock create failed"<<endl;
        return -1;
    }
    if(global_scheduler.sock_bindAndListen(global_scheduler_port)<0)
    {
        cout<<"sock bind and listen failed"<<endl;
        return -1;
    }

    if(global_scheduler.epoll_initialisation()<0)
    {
        cout<<"epoll initialisation failed"<<endl;
    }

    while (1)
    {
        switch (global_scheduler.read_number=epoll_wait(global_scheduler.epoll_fd,global_scheduler.events,global_scheduler.max_event,-1))//time out -1 for infinite waiting
        {
            case 0:
                cout<<"time out"<<endl;
                break;//go out of this switch but still in while loop..

            case -1:
                cout<<"epoll_Wait error"<<endl;
                close(global_scheduler.server);
                return -1;
        
            default:
                if(handle_event()<0)
                {
                    cout<<"failed to handle event"<<endl;
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
    //FIRST SITUATION: event of server->new connection
        if(global_scheduler.events[i].data.fd==global_scheduler.server&&(global_scheduler.events[i].events&EPOLLIN))
        {
            if(global_scheduler.new_connection(i)<0)return -1;
                      
        }else
        {
    //SECOND SITUATION: read from client (job finished) or laucher (new job)
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
        cout<<"illegal fd triggered Epollin!"<<endl;
        return -1;
    }
    
    memset(global_scheduler.read_buffer,'\0',sizeof(global_scheduler.read_buffer));
    size_t size = read(fd,global_scheduler.read_buffer,sizeof(global_scheduler.read_buffer));
    if(size>0)
    {//************JOB FROM LAUCHER*************//
        if(fd=global_scheduler.laucher)
        {
            if(acceptNewJob(fd)<0)return -1;
            return 0;
    
        }else{//*********JOB FINISH FROM LOCAL**************//
            
            finishJob(fd);
            return 0;
        }

    }else if(size==0)//end of file
    {
        cout<<"client close.."<<endl;
        epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_DEL,fd,NULL);
        close(fd);
        //free(mem);
        return -1;
    }else//error
    {
        cout<<"read failed"<<endl;
        return -1;
    }
    

}

int acceptNewJob(int fd)
{
    Job_gang job_gang;
    job_gang.ParseFromArray(global_scheduler.read_buffer,1024);
    job job_accept;
    job_accept.job_id=job_gang.job_id();
    job_accept.job_path=job_gang.job_path();
    job_accept.requested_processors=job_gang.requested_processors();
    if(job_accept.requested_processors>2)
    {
        cout<<"received wrong job, discarded it~"<<endl;
        return -1;
    }
    job_list.push_back(job_accept);
    cout<<"received a new job, added to job list"<<endl;
    //sched();
    //send();
    return 0;

}

void finishJob(int fd)
{
    Message_from_Local message;
    message.ParseFromArray(global_scheduler.read_buffer,1024);
    //if(save and get Status())  //return 1 when this job is finished at all locals
    //{
         //delete(job_id);
         //SCHED();
         //SEND();
    //}
   cout<<"job "<<message.job_id()<<" has finished in local "<<message.local_id()<<endl;
}

//job list
//scheduling matrix
//strategy

//receive(laucher,client)
//transfer scheduling
//int new_connection(int i)
//{
    
    // if(client_pos>=2)
    // {
    //     cout<<"excessive connection request"<<endl;
    //     return -1;
    // }

    // int socklen=sizeof(struct sockaddr_in);
    // global_scheduler.server_accept=accept(global_scheduler.events[i].data.fd,(struct sockaddr*)&global_scheduler.client_addr,(socklen_t*)&socklen);

    // if(global_scheduler.server_accept<0)
    // {
    //     cout<<"accept failed"<<endl;
    //     return -1;
    // }

    // // add to list of epoll
    // global_scheduler.ev.events = EPOLLIN;
    // global_scheduler.ev.data.fd = global_scheduler.server_accept;

    // if(epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_ADD,global_scheduler.server_accept,&global_scheduler.ev)<0)
    // {
    //     cout<<"epoll add failed"<<endl;
    //     return -1;
    // }

    // clients[client_pos] = global_scheduler.server_accept;

    // client_pos++;

    // global_scheduler.client_num++;

    // cout<<inet_ntoa(global_scheduler.client_addr.sin_addr)<<"has been connected"<<endl;

    // return 0;

//}