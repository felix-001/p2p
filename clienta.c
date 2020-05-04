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
    printf("start recv B data...\n");
    char buf[1024];
    while(1)
    {
        bzero(buf, sizeof(buf));
        //接收B发来的数据
        recvfrom(sockfd, buf, sizeof(buf)-1, 0, addr, len);
        printf("%s \n", buf);
        //向B发送数据
        printf("send data to B ...\n");
        sendto(sockfd, buf, sizeof(buf)-1, 0, addr, sizeof(struct sockaddr_in));
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
    clientInfo info;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    bzero(&info, sizeof(info));
    struct sockaddr_in clientaddr;
    memset(&clientaddr, 0, sizeof(clientaddr));
    //实际情况下这里用一个已知的外网的服务器的端口号
    clientaddr.sin_port = htons(60000);
    //实际情况下这里用一个已知的外网的服务器的ip地址，这里保护我的云服务器ip所以没有写出来，自己换一下ip地址。
    clientaddr.sin_addr.s_addr = inet_addr("47.105.118.51");
    clientaddr.sin_family = AF_INET;
 
    /* 向服务器S发送数据包 */
    sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&clientaddr, sizeof(struct sockaddr_in));
    /* 接收B的ip+port */
    printf("send success\n");
    recvfrom(sockfd, &info, sizeof(clientInfo), 0, (struct sockaddr *)&clientaddr, &addrlen);
    printf("IP: %s\tPort: %d\n", inet_ntoa(info.ip), ntohs(info.port));
 
    clientaddr.sin_addr = info.ip;
    clientaddr.sin_port = info.port;
    
    sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&clientaddr, sizeof(struct sockaddr_in));
    echo_ser(sockfd, (struct sockaddr *)&clientaddr, &addrlen);
 
    close(sockfd);
    return 0;
}
