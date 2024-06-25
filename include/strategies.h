#include <iostream>
#include<vector>
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

    // Strategy()
    // {
    //     ousterhaut_table.resize(2);
    //     //hypperperiode_ms = 0;
    // }

    //vector<task> schedule_tasks;

    int hypperperiode_ms=0;

    int max_last_task_duration_ms=0;

    bool wait_for_processors = false;

    //vector<vector<task>> ousterhaut_table;

    /* (five) seconds for each job(task at a local node) in turn*/
    vector<vector<task>> roundRobin(vector<Job_gang> job_list,int sum_cpu);

    int get_hyperperiode_ms();

    int get_max_ltd_ms();

    bool get_wait_for_processors();

};