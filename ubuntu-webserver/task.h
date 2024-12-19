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
    bool write();
    bool read();
    HTTP_RESULT parseRead();
    LINE_STATE parse_line();
    char* get_line();
    HTTP_RESULT parse_request_line(char* text);
    HTTP_RESULT parse_request_header(char* text);
    HTTP_RESULT parse_request_content(char* text);

    bool makeResponse(HTTP_RESULT result);
    HTTP_RESULT Get_File();
    bool add_Response(const char* Format,...);

    void process();
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
    bool m_linger;

    CHECK_STATE m_check_state;
    int m_check_index;
    int m_start_line;

    char* m_url;
    METHOD m_method;
    char* m_version;
    int m_content_length;
    char* m_host;
    char m_real_file[200];
    struct stat m_file_stat;
    char* m_file_address;
}




#endif