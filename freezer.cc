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

#define MINIMUM_TIME_UNIT 5
#define CGROUP_PATH "./cgroup"
#define PROGRAM_0_PATH "./JOBS/Job0"
#define PROGRAM_1_PATH "./JOBS/Job1"
#define PROGRAM_2_PATH "./JOBS/Job2"
/*
assumu we got a Sched array

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


int main() {
    // create n subgroups for n Gang in Cgroup

    /*
    check if path already exist
    no---->mkdir("path",mode?)
    */


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

    sleep(MINIMUM_TIME_UNIT*2);
    write_to_cgroup_file(CGROUP_PATH"/1/freezer.state", "FROZEN");

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
    return 0;
}
