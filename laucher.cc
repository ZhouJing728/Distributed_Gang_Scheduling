#include "client.h"
#include "MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

#define port 1235

Client laucher;
int job_createAndsend();

int main()
{
    cout<<"welcome to laucher, please first connect to the Global Scheduler(Server)."<<endl;

    if(laucher.sock_create()<0)
    {
        cout<<"failed to create socket"<<endl;
        return -1;
    }

    if(laucher.sock_bind(port)<0)
    {
        cout<<"failed to bind socket"<<endl;
        return -1;
    }

    if(laucher.sock_connect()<0)
    {
        cout<<"failed to connect server"<<endl;
        return -1;
    }

    while(1)
    {
        cout<<"you can create a new gang now."<<endl;

        if(job_createAndsend()<0)
        {
            cout<<"failed to create and send a job_gang message"<<endl;
            break;
        }
    }

    laucher.close_client();//unreachable unless errors happened

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