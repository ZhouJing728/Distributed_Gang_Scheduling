#include "strategies.h"


Strategy strategy;

typedef struct jobs
    {
    int job_id;
    int requested_processors;
    string job_path;
    }job;


queue<task> Strategy::roundRobin(queue<Job_gang> jobQueue){

    int queueSize = jobQueue.size();
    for( int i = 0;i<queueSize;i++)
    {
        printf("in RR functions:\n");
        Job_gang job = jobQueue.front();
        printf("successfully dequeue job message\n");
        cout<<"with job_id:"<< job.job_id()<<endl;
        task task;
        task.set_duration_ms(5);
        task.set_paths(job.job_path());
        task.set_task_id(job.job_id());
        strategy.schedule_tasks.push(task);
        jobQueue.pop();//pop from begin
        jobQueue.push(job);//push back to end

    }
    printf("task size in strategy function is %d\n",strategy.schedule_tasks.size());
    return strategy.schedule_tasks;

}