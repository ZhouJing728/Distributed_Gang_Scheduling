#include "../include/strategies.h"
using namespace std;

Strategy strategy;
Strategy::Strategy()
{
    hypperperiode_ms=0;
    lastTaskDuration_ms=0;
    wait_for_processors=false;
}

vector<vector<task>> Strategy:: get_scheduleTable(strategies strategy,vector<Job_gang> job_list,int sum_cpu,int duration_ms)
{
    vector<vector<task>> table;
    switch (strategy)
    {
    case Roundrobin:
        table=roundRobin(job_list,sum_cpu,duration_ms);
        break;
    case Infini_pair:
        table=infini_pair(job_list,sum_cpu,duration_ms);
        break;
    default:
        //other strategy in future;
        break;
    }
    return table;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//**This scheduling method does not preserve previous scheduling. All jobs on all processors
//  are rescheduled each round.
//**Each task gets five seconds of run time for all clients. repeate for twice.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
vector<vector<task>> Strategy::roundRobin(vector<Job_gang> job_list,int sum_cpu,int duration){
    
    hypperperiode_ms=0;

    vector<vector<task>> ousterhaut_table;
    int table_row = sum_cpu;
    ousterhaut_table.resize(table_row);

    task task;
    task.set_relevant_swtichtime_ms(duration);
    //task.set_task_id("empty");

    wait_for_processors = false;
    lastTaskDuration_ms = duration;
 
    for(vector<Job_gang>::iterator it = job_list.begin();it!=job_list.end();it++)
    {
        //task.set_duration_ms(5000);

        Job_gang job = *it;
        task.set_path(job.job_path());

        if(job.requested_processors()>table_row)
        {
            printf("**There aren't enough processors for job with id %d now.** \n   ---> Wating for more free processors~\n",job.job_id());
            wait_for_processors = true;
            continue;
        }
        //hypperperiode_ms=hypperperiode_ms+5000;
        
        int r;
        for(r=0;r<job.requested_processors();r++)
        {
            int job_id = job.job_id();
            char buffer[128];
            sprintf(buffer,"%d",job_id);
            task.set_task_id(buffer);
            ousterhaut_table[r].push_back(task);
        }
        for(r=r;r<table_row;r++)
        {
            task.set_task_id("empty");
            ousterhaut_table[r].push_back(task);
        }
        hypperperiode_ms=hypperperiode_ms+duration;
        task.set_relevant_swtichtime_ms(task.relevant_swtichtime_ms()+duration);

    }
  
    return ousterhaut_table;

}

vector<vector<task>> Strategy::infini_pair(vector<Job_gang> job_list,int sum_cpu,int duration_ms)
{
    vector<vector<task>> ousterhaut_table;
    int table_row = sum_cpu;
    ousterhaut_table.resize(table_row);


    task task;
    task.set_path("./Job1");
    task.set_relevant_swtichtime_ms(duration_ms);
    for(int i=0;i<sum_cpu;i++)
    {
        char buffer[128];
        sprintf(buffer,"%d",0);
        task.set_task_id(buffer);
        ousterhaut_table[i].push_back(task);
    }
    hypperperiode_ms=duration_ms;
    wait_for_processors = false;
    lastTaskDuration_ms = duration_ms;

    int iter=0;
   
    for(vector<Job_gang>::iterator it = job_list.begin();it!=job_list.end();it++)
    {
        if(iter>=table_row-1)
        {
            printf("**There aren't enough compute node to test now.** \n ");
            wait_for_processors = true;
            break;
        }
        iter++;

        Job_gang job = *it;
        int job_id = job.job_id();
        char buffer[128];
        sprintf(buffer,"%d",job_id);
        task.set_task_id(buffer);
        task.set_path(job.job_path());
        task.set_relevant_swtichtime_ms(task.relevant_swtichtime_ms()+duration_ms);
        
        ousterhaut_table[0].push_back(task);
        ousterhaut_table[iter].push_back(task);

        int r;
        for(r=1;r<sum_cpu;r++)
        {
            if(r!=iter)
            {
                task.set_task_id("empty");
                ousterhaut_table[r].push_back(task);
            }
        }
        hypperperiode_ms=hypperperiode_ms+duration_ms;
    }

    task.set_path("./Job2");
    task.set_relevant_swtichtime_ms(hypperperiode_ms+duration_ms);
    for(int i=0;i<sum_cpu;i++)
    {
        char buffer[128];
        sprintf(buffer,"%d",1);
        task.set_task_id(buffer);
        ousterhaut_table[i].push_back(task);
    }
    hypperperiode_ms=hypperperiode_ms+duration_ms;
    
    return ousterhaut_table;

}

int Strategy:: get_hyperperiode_ms()
{
    return hypperperiode_ms;
}

bool Strategy::get_wait_for_processors()
{
    return wait_for_processors;
}

int Strategy:: get_lastTaskDuration_ms()
{
    return lastTaskDuration_ms;
}