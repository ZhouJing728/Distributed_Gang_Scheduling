#include "strategies.h"
using namespace std;

Strategy strategy;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//**This scheduling method does not preserve previous scheduling. All jobs on all processors
//  are rescheduled each round.
//**Each task gets five seconds of run time for all clients. repeate for six times.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
vector<vector<task>> Strategy::roundRobin(vector<Job_gang> job_list,int sum_cpu){
    
    hypperperiode_ms=0;

    vector<vector<task>> ousterhaut_table;
    int table_row = sum_cpu;
    ousterhaut_table.resize(table_row);

    task empty_task;
    empty_task.set_duration_ms(5000);
    empty_task.set_task_id("empty");

    wait_for_processors = false;
 
   for(int i =0;i<2;i++)
   {
        for(vector<Job_gang>::iterator it = job_list.begin();it!=job_list.end();it++)
        {
            task task;
            task.set_duration_ms(5000);

            Job_gang job = *it;
            task.set_path(job.job_path());

            if(job.requested_processors()>table_row)
            {
                printf("**There aren't enough processors for job with id %d now.** \n   ---> Wating for more free processors~\n",job.job_id());
                wait_for_processors = true;
                continue;
            }
            hypperperiode_ms=hypperperiode_ms+5000;
            
            int r;
            for(r=0;r<job.requested_processors();r++)
            {
                int job_id = job.job_id();
                char buffer[128];
                sprintf(buffer,"%d%d",r,job_id);
                task.set_task_id(buffer);
                ousterhaut_table[r].push_back(task);
            }
            for(r=r;r<table_row;r++)
            {
                ousterhaut_table[r].push_back(empty_task);
            }

        }
   }
    
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