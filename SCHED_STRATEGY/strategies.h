#include <iostream>
#include <list>
#include <cstring>
using namespace std;

class Strategy{

    enum strategies {
        Roundrobin,
        Other

    };

    typedef struct jobs
    {
    int job_id;
    int requested_processors;
    string job_path;
    }job;

    int roundRobin(list <job> joblist);

};