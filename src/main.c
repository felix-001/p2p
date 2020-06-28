/**
* @file main.c
* @author rigensen
* @brief 
* @date 日  6/28 15:18:54 2020
*/
#include "public.h"
#include "ministun.h"
#include "mqtt.h"
#include "sockets.h"

#define BROKER "mqtt.eclipse.org"
#define PORT "1883"

static struct mqtt_client client;
static uint8_t sendbuf[2048];
static uint8_t recvbuf[1024];

static void* client_refresher(void* client)
{
    LOGI("client refresher enter...");
    for(;;) {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}


int get_mac_addr(char *addr)
{
    int sock, i;
    struct ifreq ifreq;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("create socket error\n");
        goto err;
    }
    strcpy(ifreq.ifr_name, "eth0");
    if(ioctl(sock, SIOCGIFHWADDR, &ifreq)<0) {
        perror("SIOCGIFHWADDR");
        goto err;
    }
    for (i=0; i<6; i++) {
        sprintf(addr+i*2, "%02x", (unsigned char)ifreq.ifr_hwaddr.sa_data[i]);
    }
    return 0;
err:
    return -1;
}

void publish_callback(void** unused, struct mqtt_response_publish *published)
{
    char* topic_name = (char*) malloc(published->topic_name_size + 1);

    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    LOGI("Received publish('%s'): %s", topic_name, (const char*) published->application_message);

    free(topic_name);
}

int mqtt_create(char *mqtt_host, char * port,  char *user, char *passwd)
{
    int sockfd;
    char mac[16] = {0};
    pthread_t client_daemon;

    if (get_mac_addr(mac) < 0)
        goto err;
    sockfd = open_nb_socket(mqtt_host, port);
    if (sockfd == -1) {
        perror("Failed to open socket: ");
        goto err;
    }
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    mqtt_connect(&client, mac, NULL, NULL, 0, user, passwd, MQTT_CONNECT_CLEAN_SESSION, 400);
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        goto err;
    } else {
        LOGI("mqtt connect %s:%s success", mqtt_host, port);
    }
    if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
        fprintf(stderr, "Failed to start client daemon.\n");
        goto err;
    }

    return 0;
err:
    return -1;
}

int main(int argc, char *argv[])
{
    int err = 0;
    struct sockaddr_in addr;
    char mac[16] = {0};
    enum MQTTErrors mqtt_ret;

    if (!argv[1]) {
        printf("usage: %s <mac_address>", argv[0]);
        return 0;
    }

    err = stun_get_mapped_addr(&addr);
    if (err < 0) {
        LOGE("stun_get_mapped_addr() error:%d", err);
        return 0;
    }
    LOGI("mapped addr: %s:%d", inet_ntoa(addr.sin_addr), addr.sin_port);
    if (get_mac_addr(mac) < 0) {
        LOGE("get mac addr error");
        return 0;
    }
    LOGI("mac: %s", mac);
    if (mqtt_create(BROKER, PORT, NULL, NULL) < 0) {
        return 0;
    }
    mqtt_ret = mqtt_subscribe(&client, mac, 1);
    if (mqtt_ret != MQTT_OK) {
        LOGE("mqtt_ret:%d", mqtt_ret);
        return 0;
    }
    while(fgetc(stdin) != EOF);
    return 0;
}
