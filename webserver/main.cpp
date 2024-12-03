#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "threadpool.h"
#include "locker.h"
#include "http_conn.h"
#include <signal.h>

#define MAX_FD 65535//最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000//一次监听的最大事件数量

//添加信号捕捉
void addsignal(int sig,void(handler)(int)){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);//设置所有信号集合,临时阻塞其他信号
    sigaction(sig,&sa,NULL);
}

//添加文件描述符到epoll中
extern void addfd(int epollfd,int fd,bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd,int fd);
//从epoll中修改文件描述符
extern void modfd(int epollfd,int fd,int ev);
//服务器部分
int main(int argc,char* argv[]){

    if(argc <= 1){
        printf("按照如下格式运行:%s port_number\n",basename(argv[0]));
        exit(-1);
    }
    //获取端口号
    //int port = atoi(argv[1]);//atoi()把字符串转换成整数
    int port = 666666;//atoi()把字符串转换成整数
    
    //对SIGPIPE信号进行处理,忽略SIGPIPE信号,SIGPIPE在向已关闭的pipe或socket写数据时触发,如果不处理,程序会终止
    addsignal(SIGPIPE,SIG_IGN);
    
    //创建线程池，初始化线程池
    threadpool<http_conn> *pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    }
    catch(...){
        //捕捉所有异常
        exit(-1);
    }

    //创建一个数组用于保存所有的客户端信息,可以保存65535个客户端的信息
    http_conn* users = new http_conn[MAX_FD];

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd == -1){
        perror("socket faild\n");
        exit(-1);
    }

    //设置端口复用,端口复用要在绑定之前进行设置
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    //服务端地址
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    if(ret == -1){
        perror("bind faild\n");
        exit(-1);
    }

    ret = listen(listenfd,128);
    if(ret == -1){
        perror("listen faild\n");
        exit(-1);
    }

    //创建epoll对象,事件数组，可以监听10000个事件
    epoll_event event[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    //将监听的epoll描述符添加到epoll对象中
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd = epollfd;
    
    while(1){
        int num = epoll_wait(epollfd,event,MAX_EVENT_NUMBER,-1);
        if((num < 0) && (errno != EINTR)){
            printf("epoll faild");
            break;
        }

        //循环遍历事件数组
        for(int i = 0; i < num; ++i){
            int sockfd = event[i].data.fd;
            if(sockfd == listenfd){
                //有客户端连接进来
                sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlen);
                if(http_conn::m_user_count >= MAX_FD){
                    //目前连接数满了
                    //给客户端写一个信息，提示服务器正忙
                    close(connfd);
                    continue;
                }
                //将新的客户的数据初始化。放到数组中
                users[connfd].init(connfd,client_address);
            }
            else if(event[i].events & (EPOLLHUP | EPOLLERR)){
                //对方异常断开或者错误等事件
                users[sockfd].close_conn();
            }
            else if(event[i].events & EPOLLIN){
                //检测读事件
                if(users[sockfd].read()){
                    //一次性把所有数据都读完
                    pool->append(users + sockfd);//读完之后把客户信息添加到任务队列中，等待子线程处理
                }
                else{
                    users[sockfd].close_conn();//没读完就直接关闭
                }
            }else if(event[i].events & EPOLLOUT){
                //检测写事件
                if(!users[sockfd].write()){
                    //读事件不成功
                    users[sockfd].close_conn();
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}