#pragma once
#include <iostream>
#include<vector>
#include "../MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

/*****************************************************************
 * For now, we have only ROUND ROBIN strategy, which can only (re)
 * generate one schedule for all local_nodes. it doesn't consider 
 * resuse of a free cpu slice after some jobs finished(we could always
 * store that info in Global_scheduler, and some future strategies 
 * could use that info).
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

    Strategy();

    //vector<vector<task>> ousterhaut_table;
    vector<vector<task>> get_scheduleTable(strategies strategy,vector<Job_gang> job_list,int sum_cpu);

    int get_hyperperiode_ms();

    int get_lastTaskDuration_ms();

    bool get_wait_for_processors();

    private:

    int hypperperiode_ms;

    int lastTaskDuration_ms;

    bool wait_for_processors;
    /* (five) seconds for each job(task at a local node) in turn*/
    vector<vector<task>> roundRobin(vector<Job_gang> job_list,int sum_cpu);

};