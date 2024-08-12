#pragma once
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<sys/epoll.h>
#include<string>
#include<iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include"MESSAGES/message.pb.h"
#include "print_level.h"
using namespace std;

class Client
{
    public:
    int client;
    int epoll_fd;
    char send_buffer[1024];
    char receive_buffer[2048];
    epoll_event events[128];
    PrintL pLevel;

    int sock_create();

    int sock_bind(int port);

    int sock_connect(string host,int port);

    int send_to_server(char send_buffer[1024]);

    int epoll_initialisation();
    
    void close_client();
};