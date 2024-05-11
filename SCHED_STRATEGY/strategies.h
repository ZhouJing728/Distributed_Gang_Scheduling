#include <iostream>
//#include <queue>
// #include"../jobQueue.h"
// #include"../taskQueue.h"
#include<vector>
#include <cstring>
#include "../MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

/*****************************************************************
 * For now, the strategy can only generate one schedule for all
 * local_nodes. it doesn't consider cpu number or resuse of a free
 * cpu.
 * 
 * for now task id = job id. but in the furture it should be distinct
 * in different machines.
******************************************************************/

class Strategy{

    public:

    enum strategies {
        Roundrobin,
        Other

    };

    // typedef struct jobs
    // {
    // int job_id;
    // int requested_processors;
    // string job_path;
    // }job;

    vector<task> schedule_tasks;

    /* (five) seconds for each job(task at a local node) in turn*/
    vector<task> roundRobin(vector<Job_gang> job_list);

};