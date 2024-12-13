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
    void parseMsg();
    bool write();
    bool read();
public:
    static int m_epollfd;
    static int m_user_count; 
    static int READ_BUFFER_SIZE = 2048;
    static int WRITE_BUFFER_SIZE = 1024;
private:
    int m_sockfd = -1;
    char recvBuf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_write_idx;
    struct iovec m_iv[2];
    int m_iv_count;
}




#endif