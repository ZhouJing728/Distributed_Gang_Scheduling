#include"../include/client.h"

Client client;

int Client::sock_create()
{
    
    if ( (client = socket(AF_INET,SOCK_STREAM,0))==-1) 
    { 
        pLevel.P_ERR("fail to create socket\n");
        return -1; 
    }else{
        pLevel.P_NOPRT("succeed to create socket\n");
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
        pLevel.P_ERR("bind failed\n");
        return -1;
    }else{
        pLevel.P_NOPRT("succeed to bind socket to given addr\n");
    }
    return 0;
}

int Client::sock_connect(string host,int port)
{
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port); // server's port
    server_addr.sin_addr.s_addr=inet_addr(host.c_str());//server's ip

    if (connect(client, (struct sockaddr *)&server_addr,sizeof(server_addr)) != 0)  // send request to server for connection
    { 
        pLevel.P_ERR("failed to connect with server\n");
        close(client); 
        return -1; 
    }else{
        pLevel.P_NOPRT("successfully to connect\n");
    }
    return 0;
}
 
int Client::send_to_server(char send_buffer[1024])//for each send a new buffer, don't need to reset
{
    if(send(client,send_buffer,1024,0)<=0)
    {
        pLevel.P_ERR("failed to send, connection closed.\n");
        return -1;
    }else{
        pLevel.P_NOPRT("successfully sent!\n");
    }
    return 0;
}

int Client::epoll_initialisation()
{
    epoll_fd = epoll_create(128);//maximum size for listening, since linux 2.6.8 is ignored,but need to be greater than 0

    if(epoll_fd<0)
    {
        pLevel.P_ERR("epoll_create\n");
        return -1;
    }

    epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=client;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client,&ev)<0)
    {
        pLevel.P_ERR("epoll_ctl\n");
        return -1;
    }

    return 0;
}

void Client::close_client()
{
    close(client);
    
}

