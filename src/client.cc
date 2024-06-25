#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<string>
#include<iostream>
#include"../MESSAGES/message.pb.h"
#include"../include/client.h"
using namespace std;
using namespace Message::protobuf;
//MESSAGE_TYPE 0 FOR JOB_GANG, 1 FOR SCHEDULING

Client client;

int Client::sock_create()
{
    
    if ( (client = socket(AF_INET,SOCK_STREAM,0))==-1) 
    { 
        pLevel.P_ERR("fail to create socket");
        return -1; 
    }else{
        pLevel.P_NODE("succeed to create socket");
    }
    return 0;
}

int Client::sock_bind()
{
    cout<<"please give the client a port number"<<endl;
    cin>>client_port;
    struct sockaddr_in client_addr;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(client_port);
    
    if(bind(client,(struct sockaddr *)&client_addr,sizeof(client_addr))<0)
    {
        cout<<"bind failed"<<endl;
        return -1;
    }else{
        cout<<"successfully to bind"<<endl;
    }
    return 0;
}

int Client::sock_bind(int port)
{
    struct sockaddr_in client_addr;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    
    if(bind(client,(struct sockaddr *)&client_addr,sizeof(client_addr))<0)
    {
        cout<<"bind failed"<<endl;
        return -1;
    }else{
        cout<<"successfully to bind"<<endl;
    }
    return 0;
}

int Client::sock_connect()
{
    memset(&server_addr,0,sizeof(server_addr));

    cout<<"please enter the ip address of Server:"<<endl;
    string ip_input;
    cin>>ip_input;
    ip = ip_input.c_str();
    cout<<"please enter the port number of the Server:"<<endl;
    cin>>server_port;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port); // server's port
    server_addr.sin_addr.s_addr=inet_addr(ip);//server's ip

    if (connect(client, (struct sockaddr *)&server_addr,sizeof(server_addr)) != 0)  // send request to server for connection
    { 
        cout<<"failed to connect with server"<<endl;
        close(client); 
        return -1; 
    }else{
        cout<<"successfully to connect"<<endl;
    }

    return 0;
}

int Client::sock_connect(string host,int port){

    memset(&server_addr,0,sizeof(server_addr));
    ip = host.c_str();
    server_port = port;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port); // server's port
    server_addr.sin_addr.s_addr=inet_addr(ip);//server's ip

    if (connect(client, (struct sockaddr *)&server_addr,sizeof(server_addr)) != 0)  // send request to server for connection
    { 
        cout<<"failed to connect with server"<<endl;
        close(client); 
        return -1; 
    }else{
        cout<<"successfully to connect"<<endl;
    }

    return 0;
}
 
int Client::send_to_server(char send_buffer[1024])//for each send a new buffer, don't need to reset
{
    if(send(client,send_buffer,1024,0)<=0)
    {
        cout<<"failed to send, connection closed."<<endl;
        return -1;
    }else{
        cout<<"successfully sent! "<<endl;
    }
    return 0;
}

int Client::epoll_initialisation()
{
    epoll_fd = epoll_create(128);//maximum size for listening, since linux 2.6.8 is ignored,but need to be greater than 0

    if(epoll_fd<0)
    {
        cout<<"epoll_create"<<endl;
        return -1;
    }

    ev.events=EPOLLIN;
    ev.data.fd=client;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client,&ev)<0)
    {
        cout<<"epoll_ctl"<<endl;
        return -1;
    }

    return 0;
}
// int Client::send_to_server()//use send_buffer of client structure,need to reset 
// {
//     memset(send_buffer,'\0',sizeof(send_buffer));

//     cout<<"please enter your message to server (no spaces):";
//     cin>>send_buffer;

//     if(send(client,send_buffer,1024,0)<=0)
//     {
//         cout<<"failed to send, connection closed."<<endl;
//         return -1;
//     }else{
//         cout<<"you have sent : "<<send_buffer<<endl;
//     }
//     return 0;
// }

// int Client::receive_from_server()
// {
//     if(recv(client,receive_buffer,1024,0)<=0)
//     {
//         cout<<"failed to receive from server, connection closed"<<endl;
//         return -1;
//     }else{
    
//         //cout<<"server :"<<receive_buffer<<endl;
//         server_to_client.ParseFromArray(receive_buffer,1024);
//         if(server_to_client.type()!=1)
//         {
//             cout<<"received wrong message!(not scheduling)"<<endl;
//             return -1;
//         }
//         scheduling = server_to_client.scheduling();
//         cout<<"Server send to fd:"<<scheduling.processor()<<endl;
//         cout<<"the new order is:"<<endl;
//         for(int i =0;i<7;i++)
//         {
//             cout<<scheduling.data(i)<<endl;
//         }
//     }
//     return 0;
    
// }

void Client::close_client()
{
    close(client);
    
}

// int main()
// {
  
//     if(client.createAndconnect()<0)
//     {
//         cout<<"failed to create socket and connect to server"<<endl;
//         return -1;
//     }
 
//     // 3th step connect with server 
//     while(1)
//     {
        
//         if(client.send_to_server()<0)
//         {
//             cout<<"failed to send, connection closed."<<endl;
//             break;
//         }

//         if(client.receive_from_server()<0)
//         {
//             cout<<"failed to receive from server, connection closed"<<endl;
//             break;
//         }
//     }
//     //4th close fd;
  
//     client.close_client();
//     return 0;
// }
