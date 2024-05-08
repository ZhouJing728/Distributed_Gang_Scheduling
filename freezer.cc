#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>//mkdir
#include <sys/types.h>
#include <sys/prctl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "ntp_client-Gaa/src/ntp_client.c"
#include "ntp_client-Gaa/src/xtime.c"//time.h also in there

#define MINIMUM_TIME_UNIT 5
#define NUM_THREADS 2
#define CGROUP_PATH "./cgroup"
#define PROGRAM_0_PATH "./JOBS/Job0"
#define PROGRAM_1_PATH "./JOBS/Job1"
#define PROGRAM_2_PATH "./JOBS/Job2"

x_int64_t deviation_in_microsecond = 0;//Local time - Server time
/*
assumu we got a Sched array  at start time 10:00

(content)|  JOB 1  |  JOB1   |  JOB2   |  JOB1
(index)  |  TIME1  |  TIME2  |  TIME3  |  TIME4  (SINCE START TIME)
*/

void write_to_cgroup_file(const char *file_path, const char *value) {
    int fd = open(file_path, O_WRONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    if (write(fd, value, strlen(value)) == -1) {
        perror("Error writing to file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void add_pid_to_cgroup(const char *tasks_file_path, pid_t pid) {

    int fd;

    fd = open(tasks_file_path, O_WRONLY);
    if (fd == -1) {
        perror("Error opening tasks file");
        exit(EXIT_FAILURE);
    }
    //write pid to task file 
    if (dprintf(fd, "%d", pid) == -1) {
        perror("Error writing to tasks file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}


void *pthread_freezer(void *arg) {
    // create n subgroups for n Gang in Cgroup

    /*
    check if path already exist
    no---->mkdir("path",mode?)
    */

    time_t current_time;
    struct tm *timeinfo;//only second
    struct timespec delay;

    // Wait untill start time
    time(&current_time);
    //timeinfo = localtime(&current_time);
    timeinfo->tm_hour = 18;
    timeinfo->tm_min = 20;
    timeinfo->tm_sec = 0;
    // if the start time already passed, break for this time.
    if (current_time >= mktime(timeinfo)) {
        //timeinfo->tm_mday += 1;
       printf("Time out -- start time\n");
    }
    delay.tv_sec = mktime(timeinfo) - current_time;
    delay.tv_nsec = -deviation_in_microsecond*1000;

    printf("Waiting until %d:%d:%d\n",
                            timeinfo->tm_hour,
                            timeinfo->tm_min,
                            timeinfo->tm_sec);

    nanosleep(&delay, &delay);

    printf("It's %d:%d:%d, time to start the task!\n",
            timeinfo->tm_hour,
            timeinfo->tm_min,
            timeinfo->tm_sec);


    //pid for child/parent process
    pid_t pid1, pid2;

    // initialise the state of all cgroups: FROZEN
    write_to_cgroup_file(CGROUP_PATH"/1/freezer.state", "FROZEN");

    write_to_cgroup_file(CGROUP_PATH"/2/freezer.state", "FROZEN");

    /*
    CHECK IF THE NEIGHBORING TASKS OF THE ARRAY ARE THE SAME
    YES---->DOUBLE EXECUTION TIME
    NO----->JUST RUN ONE MINUMUM TIME UNIT
    */

    // run first task for double time 
    write_to_cgroup_file(CGROUP_PATH"/1/freezer.state", "THAWED");
    pid1 = fork();
    if (pid1 == -1) {
        perror("Error forking process 1");
        exit(EXIT_FAILURE);
    } else if (pid1 == 0) {//child process
        prctl(PR_SET_PDEATHSIG,SIGKILL);
        add_pid_to_cgroup(CGROUP_PATH"/1/tasks",getpid());
        execl(PROGRAM_1_PATH, PROGRAM_1_PATH, NULL);
        perror("Error executing program 1");
        exit(EXIT_FAILURE);
    }
    //parent process
    //check if the next task still the same task, then double the sleep time

    //===========================================================================//
    //===============TEST OF SWITCH SLEEP() TO NANOSLEEP();======================//
    //============================================================================//

    time(&current_time);
    timeinfo->tm_sec=timeinfo->tm_sec+2*MINIMUM_TIME_UNIT;
    delay.tv_sec = mktime(timeinfo) - current_time;
    delay.tv_nsec = -deviation_in_microsecond*1000;
    nanosleep(&delay, &delay);
    //sleep(MINIMUM_TIME_UNIT*2);
    write_to_cgroup_file(CGROUP_PATH"/1/freezer.state", "FROZEN");

    //=============================================================================//
    //==============RESULT : WORKS !!!=============================================//
    //============================================================================//

    //run second task for one time unit 
    write_to_cgroup_file(CGROUP_PATH"/2/freezer.state", "THAWED");
    pid2 = fork();
    if (pid2 == -1) {
        perror("Error forking process 2");
        exit(EXIT_FAILURE);
    } else if (pid2 == 0) {//child process
        prctl(PR_SET_PDEATHSIG,SIGKILL);
        add_pid_to_cgroup(CGROUP_PATH"/2/tasks",getpid());
        execl(PROGRAM_2_PATH, PROGRAM_2_PATH, NULL);
        perror("Error executing program 2");
        exit(EXIT_FAILURE);
    }
    int two =(int) waitpid(pid2,NULL,WNOHANG);
    printf("%s\n","zero means not finish");
    printf("%d\n",two);
    sleep(MINIMUM_TIME_UNIT);
    write_to_cgroup_file(CGROUP_PATH"/2/freezer.state", "FROZEN");

    // run task one again for one unit
    write_to_cgroup_file(CGROUP_PATH"/1/freezer.state", "THAWED");

    sleep(MINIMUM_TIME_UNIT);

    write_to_cgroup_file(CGROUP_PATH"/1/freezer.state", "FROZEN");

  
    //rmdir(CGROUP_PATH);


    /*
    ~~~~~~~~~~~~TEST~~~~~~~~~~~~
    IF WE CAN GET A MESSAGE, WHEN A TASK FINISHED WITHIN ITS PERMITTED DURATION??

    LET'S TRY JOB 0 WITH 5 SECONDS LOOP
    GIVE IT TWO TIME UNIT(10 SECONDS)
    
    */
   pid_t pid0;
   write_to_cgroup_file(CGROUP_PATH"/0/freezer.state", "THAWED");
   pid0 = fork();
    if (pid0 == -1) {
        perror("Error forking process 0");
        exit(EXIT_FAILURE);
    } else if (pid0 == 0) {//child process
        prctl(PR_SET_PDEATHSIG,SIGKILL);
        add_pid_to_cgroup(CGROUP_PATH"/0/tasks",getpid());
        printf("%s\n","job 0's pid is");
        printf("%d\n",getpid());
        execl(PROGRAM_0_PATH, PROGRAM_0_PATH, NULL);
        perror("Error executing program 0");
        exit(EXIT_FAILURE);
    }
    //int zero =(int) waitpid(pid0,NULL,0);
    sleep(10);
    write_to_cgroup_file(CGROUP_PATH"/0/freezer.state", "FROZEN");
    int zero =(int) waitpid(pid0,NULL,WNOHANG);
    printf("%s\n","zero means job 0 not finish, or it shows the pid number it collects ");
    printf("%d\n",zero);
    //signal(SIGCHLD,handler())

    /*
    ~~~~~~~~~~~~~~RESULT~~~~~~~~~~~

    WE COULD KNOW IF THE TASK FINISHED WITH IN ITS DURATION!!!
    
    AND IT SHOULD NOT INFLUNCE THE START TIME OF NEXT TIME UNIT 

    CAUSE WE USE WNOHANG(NON BLOCKING MODE)

    BUT MICROSECOND?? HMMMM....

    */
    //return 0;
}

void *pthread_ntp(void *arg){

    int xit_iter = 0;
    //xopt_args_t   xopt_args;
    xntp_cliptr_t xntp_this = X_NULL;

    xtime_vnsec_t xtm_vnsec = XTIME_INVALID_VNSEC;
    xtime_vnsec_t xtm_ltime = XTIME_INVALID_VNSEC;
    xtime_descr_t xtm_descr = { 0 };
    xtime_descr_t xtm_local = { 0 };

    #if defined(_WIN32) || defined(_WIN64)
        WSADATA xwsa_data;
            WSAStartup(MAKEWORD(2, 0), &xwsa_data);
    #endif // defined(_WIN32) || defined(_WIN64)

    xntp_this = ntpcli_open();

    if (X_NULL == xntp_this)
    {
        printf("ntpcli_open() return X_NULL, errno : %d\n", errno);
        //break;
    }

    x_ccstring_t host = "127.0.0.1";
    x_int16_t port = 123;
    x_uint32_t xut_tmout = 3000;

    ntpcli_config(xntp_this,host,port);

    while(1){
        xtm_vnsec = ntpcli_req_time(xntp_this,xut_tmout);
        if (XTMVNSEC_IS_VALID(xtm_vnsec))
        {
            xtm_ltime = time_vnsec();
            xtm_descr = time_vtod(xtm_vnsec);
            xtm_local = time_vtod(xtm_ltime);
            xit_iter++;
            printf("============================================\n");
            printf("\n[%d] %s:%d : \n",
                    xit_iter ,
                    host,
                    port);
            printf("\tNTP response : [ %04d-%02d-%02d %d %02d:%02d:%02d.%03d ]\n",
                    xtm_descr.ctx_year  ,
                    xtm_descr.ctx_month ,
                    xtm_descr.ctx_day   ,
                    xtm_descr.ctx_week  ,
                    xtm_descr.ctx_hour  ,
                    xtm_descr.ctx_minute,
                    xtm_descr.ctx_second,
                    xtm_descr.ctx_msec  );

            printf("\tLocal time   : [ %04d-%02d-%02d %d %02d:%02d:%02d.%03d ]\n",
                    xtm_local.ctx_year  ,
                    xtm_local.ctx_month ,
                    xtm_local.ctx_day   ,
                    xtm_local.ctx_week  ,
                    xtm_local.ctx_hour  ,
                    xtm_local.ctx_minute,
                    xtm_local.ctx_second,
                    xtm_local.ctx_msec  );

            deviation_in_microsecond = ((x_int64_t)(xtm_ltime - xtm_vnsec)) / 10LL;
            printf("\tDeviation    : %lld us\n",
                    deviation_in_microsecond);
            printf("============================================\n");
                    
        }
        else
        {
            printf("\n[%d] %s:%d : errno = %d\n",
                    xit_iter + 1,
                    host,
                    port,
                    errno);
        }

        sleep(60);
    }
    
}

int main(){

    pthread_t threads[NUM_THREADS];
    int thread_args[NUM_THREADS];
    int i;

    if (pthread_create(&threads[0], NULL, pthread_ntp, NULL) != 0) {
        perror("pthread_ntp_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&threads[1], NULL, pthread_freezer, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    return 0;

}