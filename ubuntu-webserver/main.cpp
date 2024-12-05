#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#define local_port      8888

//  http://192.168.181.129:8888/index.html

//服务器多线程实现

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
}

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

//多路复用,select