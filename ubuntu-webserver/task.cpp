#include "task.h"
#include <iostream>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
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
    m_user_count = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(recvBuf,0,sizeof(recvBuf));
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

/*
    方法：parseMsg
    描述：解析客户端请求的报文数据
    参数：无
    返回值：void
    by liuyingen 2024.12.12
*/
void task::parseMsg()
{
}

/*
    方法：write
    描述：非阻塞的写,不停的写,直到数据全部写完
    参数：无
    返回值：bool
    by liuyingen 2024.12.12
*/
bool task::write()
{
    int temp = 0;
    int bytes_have_send = 0;    //已发送的字节
    int bytes_to_send = m_write_idx;    //需要发送的字节数

    if(bytes_to_send == 0){
        //没有要发送的数据，这一次响应结束。重新关注读事件，准备处理下一个请求
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(true) {
        
    }
}

/*
    方法：read
    描述：非阻塞的读,不停的读,直到数据全部读完
    参数：无
    返回值：bool
    by liuyingen 2024.12.12
*/
bool task::read()
{
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;
    while(true) {
        //从recvBuf + m_read_idx 位置开始读取数据往后保存,数据读取大小是READ_BUFFER_SIZE - m_read_idx
        bytes_read = recv(m_sockfd, recvBuf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == 0){
            //对方关闭连接
            return false;
        }else if(bytes_read == -1){
            if( errno == EAGAIN || errno == EWOULDBLOCK){
                //没有数据可读
                break;
            }
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}
