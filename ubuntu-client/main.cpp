#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>

#define serverPort      8888

int main(){

    int clientfd = socket(AF_INET,SOCK_STREAM,0);
    if(clientfd <= 0){
        perror("create socket error!\n");
        return -1;
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int iRet = connect(clientfd,(struct sockaddr*)&serverAddr,sizeof(sockaddr));
    if(iRet == -1){
        perror("connect to server error!\n");
        return -1;
    }

    unsigned char recvData[1024];
    unsigned char sendData[1024];
    while(1){
        memset(&recvData,0,1024);
        memset(&sendData,0,1024);
        int iRecvLength = read(clientfd,&recvData,sizeof(recvData));
        if(iRecvLength <= 0){
            perror("client read error!\n");
            break;
        }
        printf("server say: %s",recvData);
        printf("you say:");
        scanf("%s",sendData);
        int iSendLength = write(clientfd,sendData,sizeof(sendData));
        if(iSendLength <= 0){
            perror("client write error!\n");
            break;
        }


        usleep(500000);
    }

    close(clientfd);
    return 0;
}