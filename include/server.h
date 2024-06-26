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
#include "print_level.h"
using namespace std;

class Server
{
    public:
    int server;
    int laucher_port;
    int read_number;
    int epoll;
    int epoll_fd;
    // maximun number of clients that allowed to connect the server.
    int max_client;
    //fd of connected clients, the sequece is first connect first in.
    vector<int> clients;
    int laucher;
    char send_buffer[1024];
    char read_buffer[1024];
    epoll_event events[128];
    PrintL pLevel;

    Server();

    int sock_create();

    int sock_bindAndListen(int port);

    int epoll_initialisation();

    int new_connection(int i);

    int readByevent(int i);


};

