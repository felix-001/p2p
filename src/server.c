#include "public.h"

int main(int argc, char *argv[])
{
    if (!argv[1]) {
        printf("usage: %s <port>\n", argv[0]);
        return 0;
    }
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!sockfd) {
        perror("create socket error");
        return 0;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_port = htons(atoi(argv[1]));
    addr.sin_family = AF_INET;
    int ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret) {
        LOGE("bind error");
        return 0;
    }
    for(;;) {
        static int i = 0;
        client_info_t clients[2];
        socklen_t addrlen = sizeof(struct sockaddr_in);
        char data[10] = {0};

        LOGI("waiting for connection");
        bzero(clients, sizeof(clients));
        recvfrom(sockfd, data, sizeof(data), 0, (struct sockaddr *)&addr, &addrlen);
        memcpy(&clients[i].ip, &addr.sin_addr, sizeof(struct in_addr));
        clients[i].port = addr.sin_port;
        LOGI("got connection from %s:%d", inet_ntoa(clients[i].ip), ntohs(clients[i].port));
        i++;
        if (i == 2) {
            LOGI("tell client 0 that client 1 info %s:%d", inet_ntoa(clients[1].ip), ntohs(clients[1].port));
            addr.sin_addr = clients[0].ip;
            addr.sin_port = clients[0].port;
            sendto(sockfd, &clients[1], sizeof(client_info_t), 0, (struct sockaddr *)&addr, addrlen);

            LOGI("tell client 1 that client 0 info %s:%d", inet_ntoa(clients[0].ip), ntohs(clients[0].port));
            addr.sin_addr = clients[1].ip;
            addr.sin_port = clients[1].port;
            sendto(sockfd, &clients[0], sizeof(client_info_t), 0, (struct sockaddr *)&addr, addrlen);
            i = 0;
        }
    }

    return 0;
}
