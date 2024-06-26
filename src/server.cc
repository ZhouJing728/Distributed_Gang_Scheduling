#include"../include/server.h"
using namespace std;//cause <iostream> 


Server::Server()
{
    laucher=-1;
    read_number=-1;
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("../config.ini", pt);
    laucher_port=pt.get<int>("port_laucher.value");
}

int Server::sock_create()
{
    // first step ->create socket for server
    server = socket(AF_INET,SOCK_STREAM,0);

    if(server==-1)
    {
        pLevel.P_ERR("fail to create socket\n");
        return -1;
    }else{
        pLevel.P_NOPRT("successfully to create socket\n");
    }
    return 0;
}

int Server::sock_bindAndListen(int port)
{
    //second step bind ->server's ip&port for communication to socket created in fist step
    sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(port);

    //set server in multiplexing mode,allow resues of local address and port
    int opt=1;
    if(setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))!=0)
    {
        pLevel.P_ERR("setopt:\n");
        return -1;
    }

    if(bind(server,(struct sockaddr *)&server_addr,sizeof(server_addr))!=0)
    {
        pLevel.P_ERR("bind failed\n");
        return -1;
    }else{
        pLevel.P_NOPRT("successfully to bind\n");
    }
    
    //Third step ->Set socket to listening mode
    
    if(listen(server,SOMAXCONN)!=0)
    {
        pLevel.P_ERR("failed to listen\n");
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
        pLevel.P_ERR("epoll_create\n");
        return -1;
    }
    epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=server;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server,&ev)<0)
    {
        pLevel.P_ERR("epoll_ctl\n");
        return -1;
    }

    return 0;
}

int Server::new_connection(int i)
{
    if((int)clients.size()>=max_client)
    {
        pLevel.P_NOPRT("|---Has reached the max num of clients. new connection is reject!---|\n");
        return 0;
    }

    int socklen=sizeof(struct sockaddr_in);
    sockaddr_in client_addr;
    int server_accept=accept(events[i].data.fd,(struct sockaddr*)&client_addr,(socklen_t*)&socklen);

    if(server_accept<0)
    {
        pLevel.P_ERR(" server accept failed\n");
        return -1;
    }

    if(client_addr.sin_port == htons(laucher_port))
    {
        laucher = server_accept;
        pLevel.P_NOPRT("laucher has been connected\n");
    }else{
        clients.push_back(server_accept);
        pLevel.P_NOPRT("%s has been connected as the %d client\n",inet_ntoa(client_addr.sin_addr),clients.size());
    }

    // add to list of epoll
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_accept;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_accept,&ev)<0)
    {
        pLevel.P_ERR("epoll add failed\n");
        return -1;
    }
    
    return 0;
}

