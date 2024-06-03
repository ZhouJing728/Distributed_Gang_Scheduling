#pragma once
#include <stdio.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "MESSAGES/message.pb.h"
using namespace std;
using namespace Message::protobuf;

class Server
{
    public:
    int server;
    int server_port;
    int server_accept;
    int epoll;
    int epoll_fd;
    int opt = 1;
    int max_event = 128;
    int read_number = -1;
    // maximun number of clients that allowed to connect the server.
    int max_client;
    int left_child = -1;// always for the first came true client
    int right_child = -1;
    //fd of connected clients, the sequece is first connect first in.
    vector<int> clients;
    int laucher = -1;
    bool left_free=false;
    bool right_free=false;

    char send_buffer[1024];
    char read_buffer[1024];
    sockaddr_in server_addr;
    sockaddr_in client_addr;
    epoll_event ev;
    epoll_event events[128];


    typedef struct databuf
    {
        int fd;
        char buf[1024]; // used as receive buffer
    } databuf_t, *databuf_p;


    int sock_create();

    int sock_bindAndListen(int port);

    int epoll_initialisation();

    int new_connection(int i);

    int readByevent(int i);


};

