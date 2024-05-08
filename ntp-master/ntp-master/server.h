/* created by Jing*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h> /* gettimeofday() */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> /* for time() and ctime() */

//#define UTC_NTP 2208988800U /* 1970 - 1900 */

	void request_process_loop(int fd);
	int ntp_server(struct sockaddr_in bind_addr);
	void wait_wrapper();
	int ntp_reply(
		int socket_fd,
		struct sockaddr *saddr_p,
		socklen_t saddrlen,
		unsigned char recv_buf[],
		uint32_t recv_time[]);
	void log_ntp_event(char *msg);
	void log_request_arrive(uint32_t *ntp_time);
	int die(const char *msg);
	void gettime64(uint32_t ts[]);
