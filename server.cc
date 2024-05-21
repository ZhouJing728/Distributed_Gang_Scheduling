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

// void Server::handle_event()
// {
//     for(int i=0;i<read_number;i++)
//     {
//     //FIRST SITUATION: event of server->new connection
//         if(events[i].data.fd==server&&(events[i].events&EPOLLIN))
//         {
//             if(new_connection(i)<0)continue;
                      
//         }else
//         {
//             //SECOND SITUATION: read from client
//             if(events[i].events&EPOLLIN)
//             {
//                 if(readByevent(i)<0)continue;
                
//             //THIRD SITUATION : reply after read
//             }else if(events[i].events&EPOLLOUT)
//             {
//                 if(reply(i)<0)cout<<"reply failed"<<endl;

//             }
//         }   
//     }
// }

int Server::new_connection(int i)
{
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
    }else if((left_child>0)&&(right_child>0))
    {
        cout<<"already two client has connected! new connection close...."<<endl;
        close(server_accept);
        return -1;
    }else if(left_child>0)
    {
        right_child = server_accept;
        right_free = true;
        cout<<"new client comes as right child for this server"<<endl;
    }else{
        left_child = server_accept;
        left_free = true;
        cout<<"new client comes as left child for this server"<<endl;
    }

    // add to list of epoll
    ev.events = EPOLLIN;
    ev.data.fd = server_accept;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_accept,&ev)<0)
    {
        cout<<"epoll add failed"<<endl;
        return -1;
    }

    client_num++;

    cout<<inet_ntoa(client_addr.sin_addr)<<"has been connected"<<endl;
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

// int Server::reply(int i)
// {
//     databuf_p mem = (databuf_p)events[i].data.ptr;
//     int fd = mem->fd;
//     memset(send_buffer,'\0',1024);

//     //cout<<"please enter the reply:";
//     //cin>>send_buffer;
//     server_to_client.set_type(1);
//     cout<<"please enter the scheduling order(7 number):"<<endl;
//     int order[7];
//     for(int i=0;i<7;i++)
//     {
//         cin>>order[i];
//     }

//     for(int i=0;i<7;i++)
//     {
//         scheduling.add_data(order[i]);
//     }

//     cout<<"the first number in order is:"<<order[0]<<endl;
//     cout<<"you have inputed (first number for example):"<<scheduling.data(0)<<endl;

//     scheduling.set_processor(fd);

//     server_to_client.set_allocated_scheduling(&scheduling);

//     server_to_client.SerializeToArray(send_buffer,1024);

//     write(fd,send_buffer,1024);

//     ev.events=EPOLLIN;    
//     ev.data.fd=fd;

//     if(epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev)<0)
//     {
//         cout<<"epoll modify by reply failed"<<endl;
//         return -1;
//     }

//     return 0;
// }
