#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <vector>
#include <poll.h>
#include <sys/epoll.h>
#define local_port      8888
#define FD_MAXSIZE      4

//  http://192.168.181.129:8888/index.html

//服务器多线程实现
/*
void* Talk_Thread(void* arg){
    //线程执行函数
    int clientfd = (intptr_t)arg;
    unsigned char recvData[1024];
    memset(&recvData,0,1024);
    unsigned char sendData[1024] = "hello,this is server!\n";
    while(1){
        int iSendLength = write(clientfd,sendData,sizeof(sendData));
        if(iSendLength <= 0){
            close(clientfd);
            perror("sendData error!\n");
        }
        printf("wait read\n");
        int iRecvLength = read(clientfd,&recvData,sizeof(recvData));
        if(iRecvLength <= 0){
            perror("recvData error!\n");
        }
        printf("client say: %s\n",recvData);
        printf("read finished!\n");
        usleep(500000);
    }   
}

int main(int argc,char** argv){
    int listenfd;
    //服务端地址(192.168.181.129)
    struct sockaddr_in serverAddr;
    bzero(&serverAddr,sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(local_port);

    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        close(listenfd);
        perror("create socket error!\n");
        return -1;
    }

    //设置地址端口复用
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    int iRet = bind(listenfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(iRet < 0){
        close(listenfd);
        perror("bind error!\n");
        return -1;
    }
    iRet = listen(listenfd,128);
    if(iRet == -1){
        close(listenfd);
        perror("listen faild!\n");
        return -1;
    }

    struct sockaddr_in clientAddr;//客户端地址
    socklen_t clientLen = sizeof(clientAddr);
    unsigned char recvData[1024];
    static int client_Sum = 0;
    //多线程,每次创建一个子线程与一个新的客户端进行通信
    while(1){
        pthread_t curThread;
        int clientfd = accept(listenfd,(struct sockaddr*)&clientAddr,&clientLen);
        if(clientfd == -1){
            close(listenfd);
            perror("accept error!\n");
            return -1;
        }
        client_Sum++;
        if(pthread_create(&curThread,NULL,Talk_Thread,(void*)(intptr_t)clientfd) != 0){
            perror("create thread error!\n");
            client_Sum--;
            close(clientfd);
            continue;
        }
        pthread_detach(curThread);
        printf("client sum: %d\n",client_Sum);
    }
    
    close(listenfd);
    return 0;
}*/

//服务器多进程实现
/*
int main(){

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd <= 0){ perror("create socket error!\n"); return -1;}

    //设置地址端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in serverAddr;
    bzero(&serverAddr,sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(local_port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int iRet = bind(listenfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(iRet == -1){ perror("listenfd bind error!\n"); close(listenfd); return -1;}
    
    iRet = listen(listenfd,128);
    if(iRet == -1){ perror("listenfd listen error!\n"); close(listenfd); return -1;}

    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    unsigned char sendData[1024] = "this is server!\n";
    static int client_sum = 0;
    while(1){
        memset(&clientAddr,0,sizeof(sockaddr));
        int clientfd = accept(listenfd, (struct sockaddr*)&clientAddr,&clientLen);
        if(clientfd == -1) { close(clientfd); exit(-1); }
        client_sum++;
        pid_t pid = fork();
        if(pid == 0){
            //子进程
            printf("client_sum: %d\n",client_sum);
            unsigned char recvData[1024];
            while(1){
                memset(&recvData,0,1024);
                if(0 >= write(clientfd,&sendData,sizeof(sendData))){
                    perror("server write error!\n");
                    close(clientfd);
                    exit(-1);
                }
                printf("wait read\n");
                if(0 >= read(clientfd,&recvData,sizeof(recvData))){
                    perror("server read error!\n");
                    close(clientfd);
                    exit(-1);
                }
                printf("read finished\n");
                printf("client say: %s\n",recvData);
                usleep(500000);
            }
            close(clientfd);
            exit(0);
        }else if (pid > 0){
            close(clientfd);
        }else {
            perror("fork failed");
            close(clientfd);
            exit(-1);
        }
    }
    close(listenfd);
    return 0;
}
*/

//IO多路复用,select方式
/*
int set_nonblocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) {return -1;}
    flags |= O_NONBLOCK;
    if(fcntl(sockfd,F_SETFL,flags) == -1)
        return -1;
    return 0;
}

int main(){

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd == -1) { perror("listenfd create error!\n"); return -1; }
    printf("server cur listenfd: %d\n",listenfd);
    int maxfd = listenfd;

    //设置地址和端口复用
    int reuse = 1;
    int iRet = setsockopt(listenfd,SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &reuse, sizeof(reuse));
    if(iRet == -1) { perror("setsockopt error!\n"); return -1; }

    //设置fd非阻塞
    set_nonblocking(listenfd);

    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(local_port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iRet = bind(listenfd, (struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(iRet == -1) { perror("listenfd bind error!"); return -1; }

    iRet = listen(listenfd,10);
    if(iRet == -1) { perror("listenfd listen error!"); return -1; }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(listenfd,&readfds);

    unsigned char sendData[1024] = "server say: hello,this is server!\n";
    unsigned char recvData[1024];
    while(1){
        fd_set tmpfds;
        FD_ZERO(&tmpfds);
        tmpfds = readfds;

        iRet = select(maxfd + 1, &tmpfds, NULL, NULL, NULL);
        if(iRet == -1){ perror("select error!\n"); continue; }

        if(FD_ISSET(listenfd, &tmpfds)){
            //有新客户端连接
            int clientFd_ = accept(listenfd,NULL,NULL);
            if(clientFd_ > listenfd){
                maxfd = clientFd_;
            }
            set_nonblocking(clientFd_);

            FD_SET(clientFd_,&readfds);
            printf("curClient fd: %d \n",clientFd_);
        }
        for(int i = 0; i < maxfd+1; i++){
            if( i != listenfd && FD_ISSET(i,&readfds)){
                memset(recvData,0,1024);

                int iSendLength = write(i, sendData, sizeof(sendData));
                if(iSendLength <= 0) { perror("server write data error!\n"); continue;}
                printf("server wait read\n");
                int iRecvLength = read(i, &recvData, sizeof(recvData));
                if(iRecvLength <= 0) { perror("server read data error!\n"); continue;}
                printf("client say : %s \n", recvData);
                printf("server read finished\n");
            }
        }
    }
    close(listenfd);
    return 0;
}
*/

//IO多路复用,poll方式
/*
int set_nonblocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) { return -1;}
    flags |= O_NONBLOCK;
    if(fcntl(sockfd, F_SETFL, flags) == -1) { return -1;}
    return 0;
}

int main(){

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd == -1) { perror("create socket error!\n"); return -1; }

    //设置地址和端口复用
    int reuse = 1;
    int iRet = setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT,&reuse,sizeof(reuse));
    if(iRet == -1) { perror("setsockopt error!\n"); return -1;}

    if(set_nonblocking(listenfd) == -1) { perror("listenfd set_nonblock error!\n"); return -1;}

    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(local_port);

    iRet = bind(listenfd, (struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(iRet == -1) { perror("listenfd bind error!"); return -1; }

    iRet = listen(listenfd,10);
    if(iRet == -1) { perror("listenfd listen error!"); return -1; }

    struct pollfd readfds[1024];
    for(int i = 0; i < 1024; i++){
        readfds[i].fd = -1;
        readfds[i].events = POLLIN;//读事件
    }

    readfds[0].fd = listenfd;
    int maxfd = 0;
    unsigned char iSendData[1024] = "server say: hello,this is server!\n";
    unsigned char iRecvData[1024];
    memset(iRecvData,0,1024);
    while(1){
        int ret = poll(readfds,maxfd + 1, -1);
        if(ret == -1){
            perror("poll error!\n");
            return -1;
        }

        if(readfds[0].revents & POLLIN){
            //新客户端到达
            int curClientFd_ = accept(listenfd,NULL,NULL);
            if(set_nonblocking(curClientFd_) == -1) { perror("curClientFd_ set_nonblock error!\n"); return -1;}
            int i;
            for(i = 1; i < 1024; i++){
                if(readfds[i].fd == -1){
                    readfds[i].fd = curClientFd_;
                    readfds[i].events = POLLIN;
                    printf("current client sum: %d \n",i);
                    break;
                }
            }
            maxfd = maxfd > i ? maxfd : i;
        }

        //处理客户端请求
        for(int i = 1; i < maxfd + 1; i++){
            if(readfds[i].revents & POLLIN){
                memset(iRecvData,0,1024);
                int iSendLength = write(readfds[i].fd, iSendData, sizeof(iSendData));
                if(iSendLength <= 0) { perror("server write data error!\n"); close(readfds[i].fd);}

                printf("server wait read!\n");
                int iRecvLength = read(readfds[i].fd, iRecvData, sizeof(iRecvData));
                if(iRecvLength <= 0) { perror("server read data error!\n"); close(readfds[i].fd);}

                printf("client say: %s\n",iRecvData);
                printf("server read finished!\n");
            }
        }
    }

    close(listenfd);
    return 0;
}*/

//IO多路复用,epoll方式
int set_nonblocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) { perror("set_nonblocking error!\n"); return -1;}
    flags |= O_NONBLOCK;
    if(fcntl(sockfd, F_SETFL, flags) == -1 ) { perror("set_nonblocking error!\n"); return -1;}
    return 0;
}

int main(){

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1) { perror("create socket error!\n"); return -1;}

    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &reuse, sizeof(reuse));

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(local_port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int iRet = bind(listenfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if(iRet == -1) { perror("listenfd bind error!\n"); return -1; }

    iRet = listen(listenfd,128);
    if(iRet == -1) { perror("listenfd listen error!\n"); return -1;}

    int epollfd = epoll_create(100);
    if(epollfd == -1) { perror("epollfd create error!\n"); return -1; }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    iRet = epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);
    if(iRet == -1) { perror("add listenfd to epoll error!\n"); return -1;}

    struct epoll_event events[100];
    unsigned char sendData[1024] = "server say: hello,this is server!\n";
    unsigned char recvData[1024];
    while(1){
        int nfds = epoll_wait(epollfd, events, 100, -1);
        if(nfds == -1) { perror("epoll wait error!\n"); return -1; }

        for(int n = 0; n < nfds; n++){
            if(events[n].data.fd == listenfd && events[n].events == EPOLLIN){
                //有新客户端加入
                int curClientFd_ = accept(listenfd, NULL, NULL);
                set_nonblocking(curClientFd_);

                ev.data.fd = curClientFd_;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, curClientFd_ , &ev);

                printf("curClient fd: %d\n",curClientFd_);
            }else{
                //处理客户端请求
                memset(recvData,0,1024);

                int iSendLength = write(events[n].data.fd, sendData, sizeof(sendData));
                if(iSendLength <= 0) { perror("server write data error!\n"); continue;}
                printf("server wait read\n");
                int iRecvLength = read(events[n].data.fd, &recvData, sizeof(recvData));
                if(iRecvLength <= 0) { perror("server read data error!\n"); continue;}
                printf("client say : %s \n", recvData);
                printf("server read finished\n");
            }
        }
    }
    close(listenfd);
    close(epollfd);
    return 0;
}

