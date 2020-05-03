/**
* @file client.c
* @author rigensen
* @brief 
* @date 日  5/ 3 14:12:39 2020
*/

#include "public.h"

int main(int argc, char *argv[])
{
    if (!argv[1] || !argv[2] || !argv[3]) {
        printf("usage: %s <ip> <port>\n", argv[0]);
        return 0;
    }
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (!sockfd) {
        perror("create socket error");
        return 0;
    }
    client_info_t client;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    char ch = 'c';

    bzero(&client, sizeof(client));
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_family = AF_INET;

    sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    recvfrom(sockfd, &client, sizeof(client_info_t), 0, (struct sockaddr *)&addr, &addrlen);
    LOGI("got connection from %s:%d",inet_ntoa(client.ip), ntohs(client.port));

    addr.sin_addr = client.ip;
    addr.sin_port = client.port;
    sendto(sockfd, &ch, sizeof(ch), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    int mode = atoi(argv[3]);
    if (mode) {
        char buf[100] = "this is a p2p message";
        LOGI("send data to %s:%d",inet_ntoa(client.ip), ntohs(client.port));
        sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    } else {
        char buf[100] = {0};
        recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);
        LOGI("recv data from %s:%d %s",inet_ntoa(client.ip), ntohs(client.port), buf);
    }
    close(sockfd);
    return 0;
}

