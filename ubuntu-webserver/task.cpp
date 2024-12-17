#include "task.h"
#include <iostream>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <string.h>

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
    m_linger = false;   //默认不保持连接
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;   //请求行
    m_check_index = 0;  //当前解析字符的下标
    m_start_line = 0;   //当前正在解析的行的起始位置
    m_url = 0;          //解析到的url
    m_method = METHOD::GET;     //请求的方式，默认为GET
    m_version = 0;      //解析到的HTTP版本
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
    while(((m_check_state == CHECK_STATE_REQUESTBODY) && (line_state == LINE_OK)) || (line_state = parse_line()) == LINE_OK){
        //解析到了一行完整数据 或者 解析到了请求体, 都说明数据完整

        //拿到刚刚解析到的一行数据
        text = get_line();
        m_start_line = m_check_index;

        std::cout << "get a line: %s" << text << std::endl; 

        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE :{
            //请求行

            break;
        }
        case CHECK_STATE_REQUESTHEAD :{
            //请求头
            break;
        }
        case CHECK_STATE_REQUESTBODY :{
            //请求体
            break;
        }
        default:
            break;
        }
    }
}

/* 解析具体的一行 */
LINE_STATE task::parse_line()
{
    char curTemp;
    for(;m_check_index < m_read_idx; m_check_index++){
        temp = recvBuf[m_check_index];
        if(temp == '\r'){
            if((m_check_index + 1) == m_read_idx){
                return LINE_OPEN;
            }
            if(recvBuf[m_check_index + 1] == '\n'){
                recvBuf[m_check_index++] = '\0';
                recvBuf[m_check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp == '\n'){
            if(m_check_index == 0)
                return LINE_BAD;
            if(recvBuf[m_check_index - 1] == '\r'){
                recvBuf[m_check_index - 1] = '\0';
                recvBuf[m_check_index++] = '\0';
                return LINE_OK;
            }   
            else
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
    参数：text          //解析的文本
    返回值：HTTP_RESULT     //解析出来的HTTP处理结果，供组装请求使用
    by liuyingen 2024.12.17
*/
HTTP_RESULT task::parse_request_line(char* text)
{
    // GET /index.html HTTP/1.1
    m_url = strpbrk(text,"\t");
    *m_url++ = '\0';        //  GET\0/index.html\tHTTP/1.1 , m_url执行完++后指向/index.html\tHTTP/1.1
    char* curMethod = text;
    if(strcasecmp(curMethod,"GET") == 0){
        m_method = curMethod;
    }else{
        return HTTP_RESULT::BAD_REQUEST;
    }
    m_version = strpbrk(m_url,"\t");
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
    if(strncasecmp(m_url,"HTTP/1.1",7) == 0){
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
    方法：makeResponse
    描述：组装响应请求
    参数：
        result      //parseRead返回的结果，根据返回的结果组装不同的请求
    返回值：bool
    by liuyingen 2024.12.16
*/
bool task::makeResponse(HTTP_RESULT result)
{
    return false;
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
*/
bool task::write()
{
    int temp;
    int byte_have_send; //已经发送的字节数
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

        if(byte_to_send <= 0){
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
