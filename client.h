#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<sys/epoll.h>
#include<string>
#include<iostream>
#include"MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

class Client
{
    public:
    int client;
    int server_port=1234;
    int client_port;
    int epoll_fd;
    sockaddr_in server_addr;
    const char* ip;
    char send_buffer[1024];
    char receive_buffer[1024];
    epoll_event ev;
    epoll_event events[128];

    schedule scheduling;

    int sock_create();

    int sock_bind();

    int sock_bind(int port);

    int sock_connect();//give addr from command line OR define it

    int sock_connect(string host,int port);

    int send_to_server(char send_buffer[1024]);

    int epoll_initialisation();
    
    void close_client();
};