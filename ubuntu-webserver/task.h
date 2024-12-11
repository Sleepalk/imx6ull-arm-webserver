#ifndef TASK_H
#define TASK_H

#include <sys/stat.h>
#include <netinet/in.h>
#include "commonMsg.h"

/*
    类名：task
    描述：任务类，处理客户端的请求，解析HTTP报文，组装报文请求
    by liuyingen 2024.12.11
*/

class task{
public:
    task();
    ~task();
    void init(int sockfd, const sockaddr_in& addr);
    void init();
    void close_connect();

public:
    int m_epollfd;


private:
    static m_user_count = 0;
    int m_sockfd = -1;
}




#endif