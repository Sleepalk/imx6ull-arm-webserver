#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define local_port      8888

//  http://192.168.181.129:8888/index.html

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

    //设置端口复用
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
    int clientfd = accept(listenfd,(struct sockaddr*)&clientAddr,sizeof(clientAddr));
    if(clientfd == -1){
        close(listenfd);
        perror("accept error!\n");
        return -1;
    }

    unsigned char recvData[1024];

    while(1){
        unsigned char* sendData = "hello,this is server!\n";
        int iSendLength = write(clientfd,sendData,strlen(sendData));
        if(iSendLength <= 0){
            close(clientfd);
            close(listenfd);
            perror("sendData error!\n");
            return -1;
        }
        sleep(1);
        int iRecvLength = read(clientfd,&recvData,sizeof(recvData) - 1);
        if(iRecvLength <= 0){
            perror("recvData error!\n");
            return -1;
        }
        recvData[iRecvLength] = '\0';
        printf("%s",recvData);
    }
    close(clientfd);
    close(listenfd);
    return 0;
}