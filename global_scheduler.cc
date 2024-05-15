#include "server.h"
#include<vector>
#include <sys/timerfd.h>
#include "SCHED_STRATEGY/strategies.h"
#include "MESSAGES/message.pb.h"
/*******************************************/
/* from ntp server*************************/
#include <sys/time.h> /* gettimeofday() */
#include <sys/wait.h>
#include <time.h> /* for time() and ctime() */

#define UTC_NTP 2208988800U /* 1970 - 1900 */
#include "ntp-master/ntp-master/server.h"
/* end from ntp server*/
/********************************************/
using namespace std;
using namespace Message::protobuf;

//******GLOBAL VARIBLES*******//

Server global_scheduler;

int ntp_fd;

int timer_fd;

bool need_schedule = false;

struct itimerspec send_timer;

start_time next_Starttime;
//queue<Job_gang> job_queue;

vector<Job_gang> job_list;

vector<vector<task>> ousterhaut_table;//MATRIX FOR SCHED FOR ALL PROCESSORS (assume we only have two local scheduler now

//Job_gang sched[4];//ARRAY FOR TRANSMIT TO SINGLE LOCAL(4 timeslice as a round)

int global_scheduler_port = 1234;
//********FUNCTIONS********//

int acceptNewJob(int fd);

void finishJob(int fd);

int handle_event();

int readByevent(int i);

//int send_schedules_periodically();

/*triggered every hyperperiode(60s) after first trigger(30s after the this program started)*/
int epoll_timer();

int timer_handler();

int get_and_send_schedule();

int epoll_ntpServer();

void ntpServer();
/*initialise the first nst as current_time + one hyperperiode(1 min)*/
void initialise_nst();
/*nst =last_nst + one hyperperiode (1min)*/
void update_nst();

/********************************************/
/*from ntp server*/
void ntpServer_reply();

void request_process_loop(int fd);
int ntp_server(struct sockaddr_in bind_addr);
void wait_wrapper();
int ntp_reply(
	int socket_fd,
	struct sockaddr *saddr_p,
	socklen_t saddrlen,
	unsigned char recv_buf[],
	uint32_t recv_time[]);
void log_ntp_event(const char *msg);
void log_request_arrive(uint32_t *ntp_time);
int die(const char *msg);
void gettime64(uint32_t ts[]);


int main()
{
    
    if(global_scheduler.sock_create()<0)
    {
        cout<<"sock create failed"<<endl;
        return -1;
    }
    if(global_scheduler.sock_bindAndListen(global_scheduler_port)<0)
    {
        cout<<"sock bind and listen failed"<<endl;
        return -1;
    }

    if(global_scheduler.epoll_initialisation()<0)
    {
        cout<<"epoll initialisation failed"<<endl;
        return -1;
    }
   if(epoll_ntpServer()<0)
   {
        cout<<"epoll_ntpServer failed"<<endl;
        return -1;
   }

   if(epoll_timer()<0)
   {
        cout<<"epoll_timer failed"<<endl;
        return -1;
   }

   initialise_nst();
   printf("SUCCESSFULLY INITIALISED NST\n");

    while (1)
    {
        switch (global_scheduler.read_number=epoll_wait(global_scheduler.epoll_fd,global_scheduler.events,global_scheduler.max_event,-1))//time out -1 for infinite waiting
        {
            case 0:
                cout<<"time out"<<endl;
                break;//go out of this switch but still in while loop..

            case -1:
                cout<<"epoll_Wait error"<<endl;
                close(global_scheduler.server);
                return -1;
        
            default:
                if(handle_event()<0)
                {
                    cout<<"failed to handle event"<<endl;
                    close(global_scheduler.server);
                    return -1;
                }
        }
    }
    return 0;
}

int handle_event()
{
    for(int i=0;i<global_scheduler.read_number;i++)
    {
        int fd_temp = global_scheduler.events[i].data.fd;
        //Fist situation: send_timer is triggered
        if(fd_temp == timer_fd&&(global_scheduler.events[i].events&EPOLLIN))
        {
            //string s ="\n==============================\n""TIMER TRIGGERED,BEGIN SCHEDULE\n""==============================\n";
            //const char* cs = s.c_str();
            // log_ntp_event("\n==============================\n"
            //         "TIMER TRIGGERED,BEGIN SCHEDULE\n"
            //         "==============================\n");
            //log_ntp_event(cs);
            printf("\n==============================\n""TIMER TRIGGERED,BEGIN SCHEDULE\n""==============================\n");
            if(timer_handler()<0)
            {
                printf("-----Schedule Generate and send process failed------\n");
                return -1;
            }
        //Second situation: ntp client requests for time
        }else if(fd_temp == ntp_fd)
        {
            request_process_loop(ntp_fd);
        //Third SITUATION: event of server->new connection    
        }else if(fd_temp==global_scheduler.server&&(global_scheduler.events[i].events&EPOLLIN))
        {
            if(global_scheduler.new_connection(i)<0)return -1;
                      
        }else
        {
        //Fourth SITUATION: read from client (job finished) or laucher (new job)
            if(readByevent(i)<0)return -1;
        }   
    }
    return 0;
}

int readByevent(int i)
{
    int fd = global_scheduler.events[i].data.fd;//server_accept for that client

    if(fd<0)
    {
        cout<<i<<endl;
        cout<<"illegal fd triggered Epollin!"<<endl;
        return -1;
    }
    
    memset(global_scheduler.read_buffer,'\0',sizeof(global_scheduler.read_buffer));
    size_t size = read(fd,global_scheduler.read_buffer,sizeof(global_scheduler.read_buffer));
    if(size>0)
    {//************JOB FROM LAUCHER*************//
        if(fd=global_scheduler.laucher)
        {
            if(acceptNewJob(fd)<0)return -1;
            return 0;
    
        }else{//*********JOB FINISH FROM LOCAL**************//
            
            finishJob(fd);
            return 0;
        }

    }else if(size==0)//end of file
    {
        cout<<"client close.."<<endl;
        epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_DEL,fd,NULL);
        close(fd);
        //free(mem);
        return -1;
    }else//error
    {
        cout<<"read failed"<<endl;
        return -1;
    }
    

}

int acceptNewJob(int fd)
{
    Job_gang job_accept;
    job_accept.ParseFromArray(global_scheduler.read_buffer,1024);

    if(job_accept.requested_processors()>2||(job_accept.requested_processors()==0))
    {
        cout<<"received wrong job, discarded it~"<<endl;
        return -1;
    }
    job_list.push_back(job_accept);

    need_schedule = true;

    cout<<"received a new job, added to job list"<<endl;

    return 0;

}

void finishJob(int fd)
{
    Message_from_Local message;
    message.ParseFromArray(global_scheduler.read_buffer,1024);
    //if(save and get Status())  //return 1 when this job is finished at all locals
    //{
         //delete(job_id);
         //SCHED();
         //SEND();
    //}
   cout<<"job "<<message.task_id()<<" has finished  "<<endl;
}

void ntpServer()
{
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));

    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
    //signal(SIGCHLD,wait_wrapper);
	ntp_fd=ntp_server(bind_addr);
    //request_process_loop(ntp_fd)
	/* nicht erreichbar: */
}

int epoll_timer()
{
    timer_fd = timerfd_create(CLOCK_REALTIME,0);
    if(timer_fd<0)
    {
        printf("epoll_timer create failed!\n");
        return -1;
    }
    //struct itimerspec send_timer;
    //---------------------------------------//
    //triggered every 60s after first trigger
    //(30s after the this program started)
    send_timer.it_interval.tv_sec=60;//this doesn't work
    send_timer.it_interval.tv_nsec=0;
    send_timer.it_value.tv_nsec=0;
    send_timer.it_value.tv_sec=30;//this works

    if(timerfd_settime(timer_fd,0,&send_timer,NULL)<0)
    {
        printf("send_timer settime failed!\n");
        return -1;
    }
    global_scheduler.ev.events= EPOLLIN;
    global_scheduler.ev.data.fd = timer_fd;
    if(epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_ADD,timer_fd,&global_scheduler.ev)<0)
    {
        printf("epoll_ctl_Add timer failed!\n");
        return -1;
    }
    printf("SUCCESSFULLY CREATE AND ADD TIMER TO EPOLL\n");
    return 0;
}

int epoll_ntpServer()
{
     ntpServer();

    global_scheduler.ev.events=EPOLLIN;
    global_scheduler.ev.data.fd=ntp_fd;

    if(epoll_ctl(global_scheduler.epoll_fd,EPOLL_CTL_ADD,ntp_fd,&global_scheduler.ev)<0)
    {
        cout<<"epoll_ctl_add ntp_Fd failed"<<endl;
        return -1;
    }
    printf("SUCCESSFULLY TO ADD NTP SERVER TO EPOLL\n");
    return 0;
}

void initialise_nst()
{
    time_t current_time;
    timeval* tv;
    time(&current_time);
    printf("got current time\n");
    gettimeofday(tv,NULL);
    printf("got current time in us\n");
    tm* timeinfo = localtime(&current_time);
    printf("converted time structure\n");
    next_Starttime.set_hour(timeinfo->tm_hour);
    next_Starttime.set_min(timeinfo->tm_min+1);
    next_Starttime.set_sec(timeinfo->tm_sec);
    printf("arrived here\n");
    next_Starttime.set_ms(0);
}

void update_nst()
{
    if(next_Starttime.min()<59)
    {
        next_Starttime.set_min(next_Starttime.min()+1);
    }else if(next_Starttime.hour()<23){
        next_Starttime.set_hour(next_Starttime.hour()+1);
        next_Starttime.set_min(0);
    }else{
        next_Starttime.set_hour(0);
        next_Starttime.set_min(0);
    }
}

int timer_handler()
{
    if(!need_schedule)
    {
        printf("there is no new job received or finished \n");
    }else{
        printf("trying to get schedule using RR\n");

        if(get_and_send_schedule()<0)
        {
            printf("failed to get and send schedules\n");
            update_nst();
            return -1;
        }

        printf("SUCCESSFULLY SEND SCHEDULE OR NO CLIENTS YET\n");
    }

    update_nst();
    printf("SUCCESSFULLY UPDATED NST\n");
    
    return 0;
    
}

int get_and_send_schedule()
{
    if(!(global_scheduler.left_free||global_scheduler.right_free))
    {
        printf("there is no clients available now, will reschedule and retry in next round~\n");
        return 0;
    }
    Strategy mysched;
    ousterhaut_table.clear();
    ousterhaut_table = mysched.roundRobin(job_list,global_scheduler.left_free,global_scheduler.right_free);

    printf("successfully got schedule_Tasks(without start time)\n");

    schedule common_sched;
    //schedule_temp schedule_Temp;
    common_sched.mutable_start_time()->CopyFrom(next_Starttime);
    printf("successfully set nst\n");

    vector<task>left = ousterhaut_table[0];

    for(vector<task>::iterator it = left.begin();it!=left.end();it++)
    {
        task* common_task = common_sched.add_tasks();
        *common_task = *it;
    }
    memset(global_scheduler.send_buffer,'\0',1024);
    printf("reset send buffer\n");
    common_sched.SerializePartialToArray(global_scheduler.send_buffer,1024);
    printf("send buffer for left child is ready\n");
    if(write(global_scheduler.left_child,global_scheduler.send_buffer,1024)<0)
    {
        printf("failed to send schedule to left child\n");
        return -1;
    }

    common_sched.clear_tasks();
    vector<task>right = ousterhaut_table[1];
    for(vector<task>::iterator it = right.begin();it!=right.end();it++)
    {
        task* common_task = common_sched.add_tasks();
        *common_task = *it;
    }
    memset(global_scheduler.send_buffer,'\0',1024);
    printf("reset send buffer\n");
    common_sched.SerializePartialToArray(global_scheduler.send_buffer,1024);
    printf("send buffer for right child is ready\n");
    if(write(global_scheduler.right_child,global_scheduler.send_buffer,1024)<0)
    {
        printf("failed to send schedule to right child\n");
        return -1;
    }

    need_schedule = false;

    return 0;

}

// int send_schedules_periodically()
// {

//     if(global_scheduler.right_child>0)
//     {
//         if(write(global_scheduler.left_child,global_scheduler.send_buffer,1024)<0)
//         {
//             printf("failed to send schedule to left child\n");
//             return -1;
//         }
    
//         if(write(global_scheduler.right_child,global_scheduler.send_buffer,1024)<0)
//         {
//             printf("failed to send schedule to right child\n");
//             return -1;
//         }
//     }else if(global_scheduler.left_child>0)
//     {
//         if(write(global_scheduler.left_child,global_scheduler.send_buffer,1024)<0)
//         {
//             printf("failed to send schedule to left child\n");
//             return -1;
//         }
//     }else
//     {
//         printf("there is no clients available now, will reschedule and retry in next round~\n");
//         return 0;
//     }

//     return 0;
// }


/****************************************************/
/******************from ntp server*******************/
void gettime64(uint32_t ts[])
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	ts[0] = tv.tv_sec + UTC_NTP;
	ts[1] = (4294*(tv.tv_usec)) + ((1981*(tv.tv_usec))>>11);
}


int die(const char *msg)
{
	if (msg) {
		fputs(msg, stderr);
	}
	exit(-1);
}


void log_request_arrive(uint32_t *ntp_time)
{
	time_t t; 

	if (ntp_time) {
		t = *ntp_time - UTC_NTP;
	} else {
		t = time(NULL);
	}
	printf("A request comes at: %s", ctime(&t));
}


void log_ntp_event(const char *msg)
{
	puts(msg);
}


int ntp_reply(
	int socket_fd,
	struct sockaddr *saddr_p,
	socklen_t saddrlen,
	unsigned char recv_buf[],
	uint32_t recv_time[])
{
	/* Assume that recv_time is in local endian ! */
	unsigned char send_buf[48];
	uint32_t *u32p;

 	/* do not use 0xC7 because the LI can be `unsynchronized` */
	if ((recv_buf[0] & 0x07/*0xC7*/) != 0x3) {
		/* LI VN Mode stimmt nicht */
        string s ="Invalid request: found error at the first byte";
        const char* cs= s.c_str();
		log_ntp_event(cs);
		return 1;
	}

	/* füllt LI VN Mode aus
	   	LI   = 0
		VN   = Version Nummer aus dem Client
		Mode = 4
	 */
	send_buf[0] = (recv_buf[0] & 0x38) + 4;

	/* Stratum = 1 (primary reference)
	   Reference ID = 'LOCL"

	       (falscher) Bezug auf der lokalen Uhr.
	 */
	/* Stratum */
	send_buf[1] = 0x01;
	/* Reference ID = "LOCL" */
	*(uint32_t*)&send_buf[12] = htonl(0x4C4F434C);

	/* Copy Poll */
	send_buf[2] = recv_buf[2];

	/* Precision in Microsecond ( from API gettimeofday() ) */
	send_buf[3] = (signed char)(-6);  /* 2^(-6) sec */

	/* danach sind alle Werte DWORD aligned  */
	u32p = (uint32_t *)&send_buf[4];
	/* zur Vereinfachung , Root Delay = 0, Root Dispersion = 0 */
	*u32p++ = 0;
	*u32p++ = 0;

	/* Reference ID ist vorher eingetragen */
	u32p++;

	/* falscher Reference TimeStamp,
	 * wird immer vor eine minute synchronisiert,
	 * damit die Überprüfung in Client zu belügen */
	gettime64(u32p);
	*u32p = htonl(*u32p - 60);   /* -1 Min.*/
	u32p++;
	*u32p = htonl(*u32p);   /* -1 Min.*/
	u32p++;

	/* Originate Time = Transmit Time @ Client */
	*u32p++ = *(uint32_t *)&recv_buf[40];
	*u32p++ = *(uint32_t *)&recv_buf[44];

	/* Receive Time @ Server */
	*u32p++ = htonl(recv_time[0]);
	*u32p++ = htonl(recv_time[1]);

	/* zum Schluss: Transmit Time*/
	gettime64(u32p);
	*u32p = htonl(*u32p);   /* -1 Min.*/
	u32p++;
	*u32p = htonl(*u32p);   /* -1 Min.*/

	if ( sendto( socket_fd,
		     send_buf,
		     sizeof(send_buf), 0,
		     saddr_p, saddrlen)
	     < 48) {
		perror("sendto error");
		return 1;
	}

	return 0;
}


void request_process_loop(int fd)
{
	struct sockaddr src_addr;
	socklen_t src_addrlen = sizeof(src_addr);
	unsigned char buf[48];
	uint32_t recv_time[2];
	//pid_t pid;

	//while (1) {
		while (recvfrom(fd, buf,
				48, 0,
				&src_addr,
				&src_addrlen)
			< 48 );  /* invalid request */

		gettime64(recv_time);
		/* recv_time in local endian */
		log_request_arrive(recv_time);

		//pid = fork();
		//if (pid == 0) {
			/* Child */
			ntp_reply(fd, &src_addr , src_addrlen, buf, recv_time);
			//exit(0);
		//} else if (pid == -1) {
			//perror("fork() error");
			//die(NULL);
		//}
		/* return to parent */
	//}
}


int ntp_server(struct sockaddr_in bind_addr)
{
	int s;
	struct sockaddr_in sinaddr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("Can not create socket.");
		die(NULL);
	}

	memset(&sinaddr, 0, sizeof(sinaddr));
	sinaddr.sin_family = AF_INET;
	sinaddr.sin_port = htons(123);
	sinaddr.sin_addr.s_addr = bind_addr.sin_addr.s_addr;

	if (0 != bind(s, (struct sockaddr *)&sinaddr, sizeof(sinaddr))) {
		perror("Bind error");
		die(NULL);
	}
    string str = "\n========================================\n""= Server started, waiting for requests =\n""========================================\n";
    const char* cs = str.c_str();
	// log_ntp_event(	"\n========================================\n"
	// 		"= Server started, waiting for requests =\n"
	// 		"========================================\n");
    log_ntp_event(cs);

	return s;
	//request_process_loop(s);
	//close(s);
}
