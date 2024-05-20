#include "strategies.h"
using namespace std;

Strategy strategy;

vector<vector<task>> Strategy::roundRobin(vector<Job_gang> job_list,bool left_free,bool right_free){

    ousterhaut_table[0].clear();
    ousterhaut_table[1].clear();
    int queueSize = job_list.size();
    cout<<"job list has size:"<<queueSize<<endl;
    
    for(int i =0; i<6;i++)
    {
   
        for(vector<Job_gang>::iterator it = job_list.begin();it!=job_list.end();it++)
        {
            task task;
            task.set_duration_ms(5);
            hypperperiode_ms=hypperperiode_ms+5;
            Job_gang job = *it;
            task.set_path(job.job_path());
            if(job.requested_processors()==1)
            {
                if(left_free)
                {
                    int job_id = job.job_id();
                    char buffer[128];
                    sprintf(buffer,"0%d",job_id);
                    printf("task id is %s ",buffer);
                    task.set_task_id(buffer);
                    ousterhaut_table[0].push_back(task);
                }
                if(right_free)//put a empty task,let right run nothing for the same time slice
                {
                    task.set_task_id("empty");
                    ousterhaut_table[1].push_back(task);
                }
            }else if((job.requested_processors()==2)||(left_free&&right_free))
            {
                int job_id = job.job_id();
                char left_buffer[128];
                char right_buffer[128];
                sprintf(left_buffer,"0%d",job_id);
                sprintf(right_buffer,"1%d",job_id);
                task.set_task_id(left_buffer);
                ousterhaut_table[0].push_back(task);
                task.set_task_id(right_buffer);
                ousterhaut_table[1].push_back(task);
                
            }
        }
    }
    return ousterhaut_table;

}

int get_hyperperiode_ms()
{
    return strategy.hypperperiode_ms;
}