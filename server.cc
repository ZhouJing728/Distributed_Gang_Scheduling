#include<stdio.h>
#include<iostream>
#include<cstring>
#include<stdlib.h>
#include<sys/fcntl.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<sys/types.h>
#include <arpa/inet.h>
#include<sys/epoll.h>
#include"MESSAGES/message.pb.h"
#include"server.h"
using namespace std;//cause <iostream> 
using namespace Message::protobuf;
//MESSAGE_TYPE 0 FOR JOB_GANG, 1 FOR SCHEDULING

int Server::sock_create()
{
    // first step ->create socket for server
    server = socket(AF_INET,SOCK_STREAM,0);

    if(server==-1)
    {
        cout<<"fail to create socket"<<endl;
        return -1;
    }else{
        cout<<"successfully to create socket"<<endl;
    }
    return 0;
}

int Server::sock_bindAndListen(int port)
{
    
    //second step bind ->server's ip&port for communication to socket created in fist step
    
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(port);

    //set server in multiplexing mode,allow resues of local address and port

    if(setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))!=0)
    {
        cout<<"setopt:"<<endl;
        return -1;
    }

    if(bind(server,(struct sockaddr *)&server_addr,sizeof(server_addr))!=0)
    {
        cout<<"bind failed"<<endl;
        return -1;
    }else{
        cout<<"successfully to bind"<<endl;
    }
    
    //Third step ->Set socket to listening mode
    
    if(listen(server,SOMAXCONN)!=0)
    {
        cout<<"failed to listen"<<endl;
        close(server);
        return -1;
    }

    return 0;
}

int Server::epoll_initialisation()
{
    epoll_fd = epoll_create(128);//maximum size for listening, since linux 2.6.8 is ignored,but need to be greater than 0

    if(epoll_fd<0)
    {
        cout<<"epoll_create"<<endl;
        return -1;
    }

    ev.events=EPOLLIN;
    ev.data.fd=server;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server,&ev)<0)
    {
        cout<<"epoll_ctl"<<endl;
        return -1;
    }

    return 0;
}

int Server::new_connection(int i)
{
    if((int)clients.size()>=max_client)
    {
        cout<<"|---Has reached the max num of clients. new connection is reject!---|";
        return 0;
    }

    int socklen=sizeof(struct sockaddr_in);
    server_accept=accept(events[i].data.fd,(struct sockaddr*)&client_addr,(socklen_t*)&socklen);

    if(server_accept<0)
    {
        cout<<"accept failed"<<endl;
        return -1;
    }

    if(client_addr.sin_port == htons(1235))
    {
        laucher = server_accept;
        cout<<"laucher has been connected"<<endl;
    }else{
        clients.push_back(server_accept);
        cout<<inet_ntoa(client_addr.sin_addr)<<"has been connected as the"<<clients.size()<<" client"<<endl;
    }

    // add to list of epoll
    ev.events = EPOLLIN;
    ev.data.fd = server_accept;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_accept,&ev)<0)
    {
        cout<<"epoll add failed"<<endl;
        return -1;
    }
    
    return 0;
}

int Server::readByevent(int i)
{
    int fd = events[i].data.fd;

    if(fd<0)
    {
        cout<<i<<endl;
        cout<<"illegal fd triggered Epollin!"<<endl;
        return -1;
    }

    databuf_p mem = (databuf_p)malloc(sizeof(databuf_t));

    if(!mem)
    {
        cout<<"malloc failed!"<<endl;
        return -1;
    }

    mem->fd=fd;
    memset(mem->buf,'\0',sizeof(mem->buf));
    size_t size = read(mem->fd,mem->buf,sizeof(mem->buf));
    
    if(size>0)// normal display
    {
        cout<<size<<endl;
        cout<<inet_ntoa(client_addr.sin_addr)<<"client has sent: \n"<<mem->buf<<endl;
        ev.events=EPOLLOUT;//wait to be written in next loop
        ev.data.ptr=mem;
        if(epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev)<0)
        {
            cout<<"epoll modify by read failed"<<endl;
            return -1;
        }//modify the current fd
        return 0;

    }else if(size==0)//end of file
    {
        cout<<"client close.."<<endl;
        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
        close(fd);
        free(mem);
        return -1;
    }else//error
    {
        cout<<"read failed"<<endl;
        return -1;
    }
}

