#include "client.h"
#include "MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

#define laucher_port 1235
#define server_port 1234

Client laucher;
string host = "127.0.0.1";
int job_createAndsend();
int job_initialise();

int main()
{
    cout<<"welcome to laucher, please first connect to the Global Scheduler(Server)."<<endl;

    if(laucher.sock_create()<0)
    {
        cout<<"failed to create socket"<<endl;
        return -1;
    }

    if(laucher.sock_bind(laucher_port)<0)
    {
        cout<<"failed to bind socket"<<endl;
        return -1;
    }

    if(laucher.sock_connect(host,server_port)<0)
    {
        cout<<"failed to connect server"<<endl;
        return -1;
    }

    // while(1)
    // {
    //     cout<<"you can create a new gang now."<<endl;

    //     if(job_createAndsend()<0)
    //     {
    //         cout<<"failed to create and send a job_gang message"<<endl;
    //         break;
    //     }
    // }
    if(job_initialise()<0)
    {
        cout<<"failed to initialise and send job message"<<endl;
        return -1;
    }

    while(1);
    laucher.close_client();//unreachable unless errors happened

    return 0;

}

int job_initialise()
{
    Job_gang job1,job2;
    int requested_processors1=1,requested_processors2=2;
    int job_id1=2,job_id2=1;
    string path1="./JOBS/Job1",path2="./JOBS/Job2";

    job1.set_job_id(job_id1);
    job1.set_job_path(path1);
    job1.set_requested_processors(requested_processors1);
    memset(laucher.send_buffer,'\0',1024);

    job1.SerializePartialToArray(laucher.send_buffer,1024);

    if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

    job2.set_job_id(job_id2);
    job2.set_job_path(path2);
    job2.set_requested_processors(requested_processors2);
    memset(laucher.send_buffer,'\0',1024);

    job2.SerializePartialToArray(laucher.send_buffer,1024);

    if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

    return 0;

}

int job_createAndsend()
{
    Job_gang job;
    int requested_processors;
    int job_id;
    string path;

    cout<<"please enter the number of processors you need for this gang:"<<endl;
    cin>>requested_processors;
    job.set_requested_processors(requested_processors);
    cout<<"please enter the id of this Gang_job"<<endl;
    cin>>job_id;
    job.set_job_id(job_id);
    cout<<"please enter the path of the job:"<<endl;
    cin>>path;
    job.set_job_path(path);

    // laucher.client_to_server.set_type(0);
    // laucher.client_to_server.set_allocated_job_gang(&job);

    memset(laucher.send_buffer,'\0',1024);

    job.SerializePartialToArray(laucher.send_buffer,1024);

    if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

    return 0;

}