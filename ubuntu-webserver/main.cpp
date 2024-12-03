#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#define local_port      8888

int main(int argc,char** argv){
    int listenfd;
    //服务端地址(192.168.181.129)
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(local_port);

    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        printf("create socket error!\n");
        return -1;
    }

    int iRet = bind();
    return 0;
}