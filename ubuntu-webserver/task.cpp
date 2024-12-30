#include "task.h"
#include <iostream>
#include<sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include <strings.h>

const char* doc_root = "/home/sleepwalk/ubuntu-webServer/resources";

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request\n";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

int task::m_epollfd = -1;
int task::m_user_count = 0;

int set_nonblocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) {return -1;}
    flags |= O_NONBLOCK;
    if(fcntl(sockfd,F_SETFL,flags) == -1)
        return -1;
    return 0;
} 

void addfd(int epollfd, int sockfd, bool one_shot){
    epoll_event event;
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLRDHUP;    //检测读事件和远端半关闭事件
    if(one_shot){
        event.events |= EPOLLONESHOT;       //单次触发事件，读事件被触发之后，从epoll中移除，避免多个线程调用该文件描述符
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
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

task::task()
{
}

task::~task()
{
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
void task::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    //m_file_address = addr;

    //设置地址端口复用
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &reuse, sizeof(reuse));

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
    //m_user_count = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(recvBuf,0,sizeof(recvBuf));
    m_linger = false;   //默认不保持连接
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;   //请求行
    m_check_index = 0;  //当前解析字符的下标
    m_start_line = 0;   //当前正在解析的行的起始位置
    m_url = 0;          //解析到的url
    m_method = METHOD::GET;     //请求的方式，默认为GET
    m_version = 0;      //解析到的HTTP版本
    m_content_length = 0;       //请求消息的总长度
    m_host = 0;         //主机名
    memset(m_real_file,0,sizeof(m_real_file));
    //memset(m_file_stat,0,sizeof(m_file_stat));
    bzero(m_real_file, 200);
    //m_file_address = 0;
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
    方法：parseRead
    描述：使用主从状态机，解析客户端请求的报文数据,返回解析过程中可能出现的结果
    参数：无
    返回值：HTTP_RESULT
    by liuyingen 2024.12.16
*/
HTTP_RESULT task::parseRead()
{
    LINE_STATE line_state = LINE_OK;
    HTTP_RESULT ret = NO_REQUEST;
    char* text = 0;
    while((( m_check_state == CHECK_STATE_REQUESTBODY) && (line_state == LINE_OK)) || ((line_state = parse_line()) == LINE_OK)){
        //解析到了一行完整数据 或者 解析到了请求体, 都说明数据完整

        //拿到刚刚解析到的一行数据
        text = get_line();
        m_start_line = m_check_index;

        std::cout << "get a line: " << text << std::endl; 

        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE :{
            //请求行
            ret = parse_request_line(text);
            if(ret == HTTP_RESULT::BAD_REQUEST){
                return HTTP_RESULT::BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_REQUESTHEAD :{
            //请求头
            ret = parse_request_header(text);
            if(ret == HTTP_RESULT::BAD_REQUEST){
                return HTTP_RESULT::BAD_REQUEST;
            }else if(ret == HTTP_RESULT::GET_REQUEST){
                return Get_File();
            }
            break;
        }
        case CHECK_STATE_REQUESTBODY :{
            //请求体
            ret = parse_request_content(text);
            if(ret == HTTP_RESULT::BAD_REQUEST){
                return HTTP_RESULT::BAD_REQUEST;
            }else if(ret == HTTP_RESULT::GET_REQUEST){
                return Get_File();
            }
            line_state = LINE_OPEN;
            break;
        }
        default:
            break;
        }
    }
    return HTTP_RESULT::NO_REQUEST;
}

/* 解析具体的一行 */
LINE_STATE task::parse_line()
{
    char curTemp;
    for(;m_check_index < m_read_idx; ++m_check_index){
        curTemp = recvBuf[m_check_index];
        if(curTemp == '\r'){
            if((m_check_index + 1) == m_read_idx){
                return LINE_OPEN;
            }
            else if(recvBuf[m_check_index + 1] == '\n'){
                recvBuf[m_check_index++] = '\0';
                recvBuf[m_check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(curTemp == '\n'){
            if( ( m_check_index > 1) && ( recvBuf[ m_check_index - 1 ] == '\r' ) ) {
                recvBuf[ m_check_index-1 ] = '\0';
                recvBuf[ m_check_index++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/* 返回一行数据，\0 结束 */
char *task::get_line()
{
    return recvBuf + m_start_line;
}

/*
    方法：parse_request_line
    描述：解析请求行,获得请求方法，目标URL，HTTP版本
    参数：text          //解析的文本,   GET HTTP://192.168.181.129:10000/index.html HTTP/1.1
    返回值：HTTP_RESULT     //解析出来的HTTP处理结果，供组装请求使用
    by liuyingen 2024.12.17
*/
HTTP_RESULT task::parse_request_line(char* text)
{
    // GET /index.html HTTP/1.1
    m_url = strpbrk(text," \t");
    if (! m_url) { 
        return HTTP_RESULT::BAD_REQUEST;
    }
    *m_url++ = '\0';        //  GET\0/index.html\tHTTP/1.1 , m_url执行完++后指向/index.html\tHTTP/1.1
    char* curMethod = text;
    if(strcasecmp(curMethod,"GET") == 0){
        m_method = METHOD::GET;
    }else{
        return HTTP_RESULT::BAD_REQUEST;
    }
    m_version = strpbrk(m_url," \t");
    if(!m_version){
        return HTTP_RESULT::BAD_REQUEST;
    }
    *m_version++ = '\0';        // /index.html\0HTTP/1.1
    if(strcasecmp(m_version,"HTTP/1.1") != 0){
        return HTTP_RESULT::BAD_REQUEST;
    }
    /**
     * http://192.168.110.129:10000/index.html
    */
    if(strncasecmp(m_url,"http://",7) == 0){
        m_url += 7;
        m_url = strchr(m_url,'/');  //192.168.110.129:10000/index.html查找/的位置并返回该位置的指针,此时m_url = /index.html
    }
    if(!m_url || m_url[0] != '/'){
        return HTTP_RESULT::BAD_REQUEST;
    }

    //更新主状态机
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTHEAD;   //请求头
    return HTTP_RESULT::NO_REQUEST;
}

/*
    方法：parse_request_header
    描述：解析请求头
    参数：text          //解析的文本
    返回值：HTTP_RESULT     //解析出来的HTTP处理结果，供组装请求使用
    by liuyingen 2024.12.17
*/
HTTP_RESULT task::parse_request_header(char *text)
{
    if(text[0] == '\0'){
        if(m_content_length != 0){
            //请求体里面有内容
            m_check_state = CHECK_STATE::CHECK_STATE_REQUESTBODY;
            return HTTP_RESULT::NO_REQUEST;
        }
        return HTTP_RESULT::GET_REQUEST;
    }else if(strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0){
            m_linger = true;
        }
        //return HTTP_RESULT::NO_REQUEST;
    }else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
        //return HTTP_RESULT::NO_REQUEST;
    }else if(strncasecmp(text, "Content-Length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
        //return HTTP_RESULT::NO_REQUEST;
    }else{
        printf("unknow header line: %s\n", text);
        //return HTTP_RESULT::NO_REQUEST;
    }
    return HTTP_RESULT::NO_REQUEST;
}

/*
    方法：parse_request_content
    描述：解析请求体
    参数：text              //解析的文本
    返回值：HTTP_RESULT     //解析出来的HTTP处理结果，供组装请求使用
    by liuyingen 2024.12.17
*/
HTTP_RESULT task::parse_request_content(char *text)
{
    if((m_check_index + m_content_length) <= m_read_idx){
        text[m_content_length] = '\0';
        return HTTP_RESULT::GET_REQUEST;
    }
    return HTTP_RESULT::NO_REQUEST;
}

/*
    方法：makeResponse
    描述：组装响应请求
    参数：
        result      //parseRead返回的结果，根据返回的结果组装不同的请求
    返回值：bool
    by liuyingen 2024.12.16
*/
bool task::makeResponse(HTTP_RESULT result)
{
    switch (result)
    {
    case BAD_REQUEST:{
        //客户端请求语法出错
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        if(!add_content(error_400_form)){
            return false;
        }
        break;
    }
    case NO_RESOURCE:{
        //服务器没有资源
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form)){
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST:{
        //客户端对资源没有足够的权限
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form)){
            return false;
        }
        break;
    }
    case FILE_REQUEST:{
        //文件请求
        add_status_line(200, ok_200_title);
        add_headers(m_file_stat.st_size);
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;
        return true;
    }
    case INTERNAL_ERROR:{
        //服务端内部错误
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form)){
            return false;
        }
        break;
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

/*
    方法：Get_File
    描述：解析到完整的请求头或者请求体之后，组装好请求文件的路径，然后判断文件的属性，如果文件对其他用户可读，
        开始映射文件地址，返回FILE_REQUEST文件请求，供组装函数MakeResponse调用
    参数: 无
    返回值：HTTP_RESULT
    by liuyingen 2024.12.18
*/
HTTP_RESULT task::Get_File()
{
    strcpy(m_real_file,doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, 200-1-len);
    if(stat(m_real_file, &m_file_stat) < 0){
        //没有资源
        return HTTP_RESULT::NO_RESOURCE;
    }

    //检查文件是否有可以被其他用户读取的权限
    if(!(m_file_stat.st_mode & S_IROTH)){
        return HTTP_RESULT::FORBIDDEN_REQUEST;
    }

    //判断文件是否是目录
    if(S_ISDIR(m_file_stat.st_mode)){
        return HTTP_RESULT::BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
    //映射文件地址
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return HTTP_RESULT::FILE_REQUEST;
}

/*
    方法：add_Response
    描述：向写缓冲区中写入数据
    参数:
        Format      //可变长参数
    返回值：bool
    by liuyingen 2024.12.18
*/
bool task::add_Response(const char *Format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list;
    va_start(arg_list, Format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, Format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

/*
    方法：add_status_line
    描述：组装状态行，并写入到写缓冲区中
    参数：
        state       //状态
        title       //标题
    返回值：bool
    by liuyingen 2024.12.19
*/
bool task::  add_status_line(int state, const char *title)
{
    return add_Response("%s %d %s\r\n","HTTP/1.1", state, title);
}

/*
    方法：add_headers
    描述：组装响应头，并写入到写缓冲区中
    参数：
        content_length       //文本长度
    返回值：bool
    by liuyingen 2024.12.19
*/
bool task::add_headers(int content_length)
{
    //文本长度
    bool result1 = add_Response("Content-Length: %d\r\n", content_length);
    //文本类型
    bool result2 = add_Response("Content-Type:%s\r\n", "text/html");
    //连接方式
    bool result3 = add_Response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
    //空白行
    bool result4 = add_Response("%s", "\r\n");
    return result1 && result2 && result3 && result4;
}

/*
    方法：add_content
    描述：组装文本内容，并写入到写缓冲区中
    参数：
        content     //文本内容
    返回值：bool 
    by liuyingen 2024.12.21
*/
bool task::add_content(const char *content)
{
    return add_Response("%s", content);
}

void task::unmap()
{
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}

/*
    方法：process
    描述：处理HTTP请求，包括解析HTTP请求和组装响应报文，由线程池的工作线程调用
    参数：无
    返回值：void
    by liuyingen 2024.12.16
*/
void task::process()
{
    //解析HTTP请求
    HTTP_RESULT result = parseRead();
    if(result == HTTP_RESULT::NO_REQUEST){
        //请求不完整
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        return;
    }

    //根据结果组装请求
    bool write_Result = makeResponse(result);
    if(!write_Result){
        close_connect();
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT); //检测写事件
}

/*
    方法：write
    描述：非阻塞的写,不停的写,直到数据全部写完
    参数：无
    返回值：bool
    by liuyingen 2024.12.12

bool task::write()
{
    int temp = 0;
    int byte_have_send = 0; //已经发送的字节数
    int byte_to_send = m_write_idx;   //将要发送的字节数

    if(byte_to_send == 0){
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;
    }

    while(true){
        //分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp <= -1){
            //EAGAIN 或 EWOULDBLOCK：资源暂时不可用。如果文件描述符是非阻塞的，此错误表示当前没有可以写入的缓冲区。
            //如果TCP写缓冲区没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间服务器无法立即接收到同一客户端的下一个请求，但是可以保证连接完整性
            if(errno == EAGAIN){
                modfd(m_epollfd,m_sockfd,EPOLLOUT);
            }
            unmap();//取消映射
            return false;
        }
        byte_to_send -= temp;
        byte_have_send += temp;

        if(m_write_idx <= byte_have_send){
            //发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger){
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }else{
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}
*/
bool task::write()
{
    int temp = 0;
    int bytes_have_send = 0;  // 已经发送的字节数
    int bytes_to_send = m_write_idx;  // 响应头的大小

    // 如果有文件要发送，加上文件大小
    if(m_iv_count == 2) {
        bytes_to_send += m_file_stat.st_size;
    }

    printf("Total to send: %d (header: %d, file: %ld)\n", 
           bytes_to_send, m_write_idx, 
           m_iv_count == 2 ? m_file_stat.st_size : 0);

    while(bytes_have_send < bytes_to_send) {
        // 更新iovec数组
        if(bytes_have_send < m_write_idx) {
            // 还在发送响应头
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_write_idx - bytes_have_send;
            if(m_iv_count == 2) {
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
            }
        } else {
            // 响应头发送完毕，正在发送文件
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send - bytes_have_send;
        }

        temp = writev(m_sockfd, m_iv, m_iv_count);
        
        printf("writev return: %d, bytes_have_send: %d, bytes_to_send: %d\n", 
               temp, bytes_have_send, bytes_to_send);

        if(temp <= -1) {
            if(errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
    }

    printf("Send completed: %d bytes\n", bytes_have_send);

    // 发送完毕
    unmap();
    if(m_linger) {
        init();
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return true;
    } else {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        //close_connect();
        return false;
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
    if( m_read_idx >= READ_BUFFER_SIZE ) {
        return false;
    }
    int bytes_read = 0;
    while(true) {
        // 从m_read_buf + m_read_idx索引出开始保存数据，大小是READ_BUFFER_SIZE - m_read_idx
        bytes_read = recv(m_sockfd, recvBuf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0 );
        if (bytes_read == -1) {
            if( errno == EAGAIN || errno == EWOULDBLOCK ) {
                // 没有数据
                break;
            }
            return false;   
        } else if (bytes_read == 0) {   // 对方关闭连接
            return false;
        }
        m_read_idx += bytes_read;
        //return true;
    }
    return true;
}
