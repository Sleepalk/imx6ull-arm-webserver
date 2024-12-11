#include "task.h"
#include <iostream>
#include <string.h>
#include <sys/epoll.h>
#include <fcntl.h>

task::task(){

}

task::~task(){

}

void set_nonblocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL);
} 

void addfd(int epollfd, int sockfd, bool one_shot){
    epoll_event event;
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLRDHUP;    //检测读事件和远端半关闭事件
    if(one_shot){
        event.events |= EPOLLONESHOT;       //单次触发事件，读事件被触发之后，从epoll中移除，避免多个线程调用该文件描述符
    }
    epll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
    //设置文件描述符非阻塞
    set_nonblocking(sockfd);
}

void removefd(int epollfd, int sockfd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);      //关闭连接
}

void modfd(int epollfd, int sockfd, int ev){
    epoll_event event;
    event.data.fd = sockfd;
    event.events = ev | EPOLLONESHOT | EPOLLHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
}

/*
    方法：init
    描述：初始化客户端连接，把客户端文件描述符添加到epoll中
    参数：
        sockfd      //客户端文件描述符
        addr        //客户端地址
    返回值：void
    by liuyingen 2024.12.11
*/
void task::init(int sockfd, const sockaddr_in& addr){
    m_sockfd = sockfd;

    //设置地址端口复用
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCK, SO_REUSEADDR | SO_REUSEPORT, &reuse, sizeof(reuse));

    addfd(m_epollfd, sockfd, true);
    m_user_count++;

    init();
}

/*
    方法：init
    描述：初始化一些字符串，便于处理和解析数据
    参数：
        无
    返回值：void
    by liuyingen 2024.12.11
*/
void task::init(){

}

/*
    方法：close_connect
    描述：关闭客户端连接
    参数：
        无
    返回值：void
    by liuyingen 2024.12.11
*/
void task::close_connect(){
    if(m_sockfd != -1){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
    
}