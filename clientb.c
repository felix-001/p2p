#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
 
/* 原理见服务器源程序 */
#define ERR_EXIT(m)\
    do{\
        perror(m); \
        exit(1);\
    }while(0)
 
typedef struct{
    struct in_addr ip;
    int port;
}clientInfo;
 
/* 用于udp打洞成功后两个客户端跨服务器通信 */
void echo_ser(int sockfd, struct sockaddr* addr, socklen_t *len)
{   
    char buf[1024];
    while(1)
    {
        bzero(buf, sizeof(buf));
        printf(">> ");
        fflush(stdout);
        fgets(buf, sizeof(buf)-1, stdin);
        //向A发送数据
        sendto(sockfd, buf, strlen(buf), 0, addr, sizeof(struct sockaddr_in));
 
        //接收A发来的数据
        bzero(buf, sizeof(buf));
        printf("start recv A data...\n");
        recvfrom(sockfd, buf, sizeof(buf)-1, 0, addr, len);
        printf("%s \n", buf);
        buf[strlen(buf)] = '\0';
        if(strcmp(buf, "exit") == 0)
            break;
    }
}
 
int main()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1)
        ERR_EXIT("SOCKET");
    //向服务器发送心跳包的一个字节的数据
    char ch = 'a';
    /* char str[] = "abcdefgh"; */
    clientInfo info;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    bzero(&info, sizeof(info));
    struct sockaddr_in clientaddr, serveraddr;
    /* 客户端自身的ip+port */
    /* memset(&clientaddr, 0, sizeof(clientaddr)); */
    /* clientaddr.sin_port = htons(8888); */
    /* clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); */   
    /* clientaddr.sin_family = AF_INET; */
 
    /* 服务器的信息 */
    memset(&clientaddr, 0, sizeof(clientaddr));
    //实际情况下为一个已知的外网的服务器port
    serveraddr.sin_port = htons(60000);
    //实际情况下为一个已知的外网的服务器ip,这里仅用本地ip填充，下面这行的ip自己换成已知的外网服务器的ip
    serveraddr.sin_addr.s_addr = inet_addr("47.105.118.51");   
    /* clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); */   
    serveraddr.sin_family = AF_INET;
 
    /* 向服务器S发送数据包 */
    sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in));
    /* sendto(sockfd, str, sizeof(str), 0, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in)); */
    /* 接收B的ip+port */
    printf("send success\n");
    recvfrom(sockfd, &info, sizeof(clientInfo), 0, (struct sockaddr *)&serveraddr, &addrlen);
    printf("IP: %s\tPort: %d\n", inet_ntoa(info.ip), ntohs(info.port));
 
    serveraddr.sin_addr = info.ip;
    serveraddr.sin_port = info.port;
 
    sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in));
    echo_ser(sockfd, (struct sockaddr *)&serveraddr, &addrlen);
    close(sockfd);
    return 0;
}
