#include "strategies.h"


Strategy strategy;

vector<task> Strategy::roundRobin(vector<Job_gang> job_list){

    int queueSize = job_list.size();
    cout<<"job list has size:"<<queueSize<<endl;
    
    printf("in RR functions:\n");
    for(vector<Job_gang>::iterator it = job_list.begin();it!=job_list.end();it++)
    {
        Job_gang job = *it;
        printf("successfully dequeue job message,");
        cout<<"with job_id:"<< job.job_id()<<endl;
        task task;
        task.set_duration_ms(5);
        //task.set_paths(job.job_path());
        task.set_task_id(job.job_id());
        strategy.schedule_tasks.push_back(task);
    }
    
    printf("task size in strategy function is %ld\n",strategy.schedule_tasks.size());
    return strategy.schedule_tasks;

}