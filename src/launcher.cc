#include "../include/client.h"
#include "../MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

Client laucher;
int job_createAndsend();
int job_initialise();
int latency_gang(int id);
int normal_gang(int id, int num);

int main()
{
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("../config.ini", pt);
    
    int server_port = pt.get<int>("port_globalscheduler.value");
    int laucher_port = pt.get<int>("port_laucher.value");
    string server_ip =pt.get<string>("ip_globalscheduler.value");
    string test_method = pt.get<string>("test_method.value");
    int num_ib_pairs=pt.get<int>("num_ib_pairs.value");
    int num_normal_gangs=pt.get<int>("num_normal_gangs.value");
    int num_normal_gangs_requested_cores=pt.get<int>("num_normal_gangs_requested_cores.value");

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

    if(laucher.sock_connect(server_ip,server_port)<0)
    {
        cout<<"failed to connect server"<<endl;
        return -1;
    }
    
    if(test_method=="ibbench")
    {
        for(int job_id=2;job_id<2+num_ib_pairs;job_id++)
        {
            if(latency_gang(job_id)<0)
            {
                printf("failed to send the %d th latancy test gang!\n",job_id);
                return -1;
            }
        }
    }else if(test_method=="normal")
    {
        for(int job_id=0;job_id<num_normal_gangs;job_id++)
        {
            if(normal_gang(job_id,num_normal_gangs_requested_cores)<0)
            {
                printf("failed to send the %d th normal test gang!\n",job_id);
                return -1;
            }
        }
    }else{
        printf("invalid given test method, sample gangs would be sent by default~\n");
        if(job_initialise()<0)
        {
            cout<<"failed to initialise and send job message"<<endl;
            return -1;
        }

        Job_gang job3;
        job3.set_job_id(3);
        job3.set_job_path("./Job3");
        printf("+++++++++++++++++++++++++++++++++++++\n for test purpose(new job later || requested processor num),\n we have predifiend Job3..\n");
        printf("please enter the processor number that you need\n");
        int num;
        cin>>num;
        job3.set_requested_processors(num);
        memset(laucher.send_buffer,'\0',1024);

        job3.SerializePartialToArray(laucher.send_buffer,1024);

        if(laucher.send_to_server(laucher.send_buffer)<0)return -1;
    }

    while(1);
    
    laucher.close_client();//unreachable unless errors happened

    return 0;

}

int latency_gang(int id)
{
    Job_gang job;
    job.set_job_id(id);
    job.set_job_path("../test/ibbench.sh");
    job.set_requested_processors(2);
    memset(laucher.send_buffer,'\0',1024);

    job.SerializePartialToArray(laucher.send_buffer,sizeof(laucher.send_buffer));

   if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

   return 0;
}

int normal_gang(int id,int num_cores)
{
    Job_gang job;
    job.set_job_id(id);
    job.set_job_path("./Job1");
    job.set_requested_processors(num_cores);
    memset(laucher.send_buffer,'\0',sizeof(laucher.send_buffer));

    job.SerializePartialToArray(laucher.send_buffer,sizeof(laucher.send_buffer));

   if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

   return 0;
}

int job_initialise()
{
    Job_gang job0,job1,job2;
    int requested_processors0=1,requested_processors1=1,requested_processors2=2;
    int job_id0=0,job_id1=1,job_id2=2;
    string path0="./Job0",path1="./Job1",path2="./Job2";

    job0.set_job_id(job_id0);
    job0.set_job_path(path0);
    job0.set_requested_processors(requested_processors0);
    memset(laucher.send_buffer,'\0',sizeof(laucher.send_buffer));

    job0.SerializePartialToArray(laucher.send_buffer,sizeof(laucher.send_buffer));

    if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

    job1.set_job_id(job_id1);
    job1.set_job_path(path1);
    job1.set_requested_processors(requested_processors1);
    memset(laucher.send_buffer,'\0',sizeof(laucher.send_buffer));

    job1.SerializePartialToArray(laucher.send_buffer,sizeof(laucher.send_buffer));

    if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

    job2.set_job_id(job_id2);
    job2.set_job_path(path2);
    job2.set_requested_processors(requested_processors2);
    memset(laucher.send_buffer,'\0',sizeof(laucher.send_buffer));

    job2.SerializePartialToArray(laucher.send_buffer,sizeof(laucher.send_buffer));

    if(laucher.send_to_server(laucher.send_buffer)<0)return -1;

    return 0;

}
