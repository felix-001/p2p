/**
* @file main.c
* @author rigensen
* @brief 
* @date 日  6/28 15:18:54 2020
*/
#include "public.h"
#include "ministun.h"
#include "MQTTAsync.h"

#define ADDRESS     "tcp://mqtt.eclipse.org:1883"

static MQTTAsync client;
static int server_mode = 0;
static int disc_finished = 0;
static char tuple[64];
static char g_mac[16];
static int connected;

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
        LOGI("SIOCGIFHWADDR: %s", strerror(errno));
        goto err;
    }
    for (i=0; i<6; i++) {
        sprintf(addr+i*2, "%02x", (unsigned char)ifreq.ifr_hwaddr.sa_data[i]);
    }
    return 0;
err:
    return -1;
}

void onDisconnectFailure(void* context, MQTTAsync_failureData* response)
{
	LOGE("Disconnect failed, rc %d", response->code);
    disc_finished = 1;
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	LOGI("Successful disconnection");
    disc_finished = 1;
}

void onSendFailure(void* context, MQTTAsync_failureData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
	int rc;

	LOGE("Message send failed token %d error code %d\n", response->token, response->code);
	opts.onSuccess = onDisconnect;
	opts.onFailure = onDisconnectFailure;
	opts.context = client;
	if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS) {
		LOGE("Failed to start disconnect, return code %d\n", rc);
	}
}

void onSend(void* context, MQTTAsync_successData* response)
{
	LOGI("Message with token value %d delivery confirmed", response->token);
}

void onSubscribe(void* context, MQTTAsync_successData* response)
{
	LOGI("Subscribe succeeded");
}

void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	LOGE("Subscribe failed, rc %d", response->code);
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	LOGE("Connect failed, rc %d", response->code);
}

void udp_session(char *ip, int port)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    char msg[64] = {0};
    char rcv[64] = {0};
    
    LOGI("ip:%s port:%d", ip, port);
    if (!sockfd) {
        perror("create socket error");
        return;
    }
    if (server_mode) {
        sprintf(msg, "server: %s", tuple);
    } else {
        sprintf(msg, "client: %s", tuple);
    }
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_family = AF_INET;
    sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (server_mode) {
        sleep(1);
    }
    recvfrom(sockfd, rcv, sizeof(rcv), 0, (struct sockaddr *)&addr, &addrlen);
    if (server_mode) {
        sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    }
    LOGI("%s", rcv);
}

void nat_hole(char *tuple)
{
    char *sub;

    sub = strstr(tuple, ":");
    if (!sub) {
        LOGE("get `:` error");
        return;
    }
    *sub = '\0';
    udp_session(tuple, atoi(sub+1));
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	LOGI("Connection lost");
	if (cause)
		printf("     cause: %s\n", cause);

	LOGI("Reconnecting");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
		LOGE("Failed to start connect, return code %d\n", rc);
	} else {
        LOGI("Reconnec success");
    }
}


void *session_thread(void *arg)
{
    char *tuple = strdup((char *)arg);

    nat_hole(tuple);

    free(tuple);

    return NULL;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    pthread_t tid;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);

    if (server_mode) {
        MQTTAsync client = (MQTTAsync)context;
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
        int rc;

        opts.onSuccess = onSend;
        opts.onFailure = onSendFailure;
        opts.context = client;
        pubmsg.payload = tuple;
        pubmsg.payloadlen = (int)strlen(tuple);
        pubmsg.qos = 0;
        pubmsg.retained = 0;
        if ((rc = MQTTAsync_sendMessage(client, g_mac, &pubmsg, &opts)) != MQTTASYNC_SUCCESS) {
            LOGE("Failed to start sendMessage, return code %d", rc);
        } 
    }
    LOGI("message: %.*s", message->payloadlen, (char*)message->payload);
    pthread_create(&tid, NULL, session_thread, message->payload);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;
    char mac[16] = {0};

    if (get_mac_addr(mac) < 0 ) {
        if (server_mode) {
            strcpy(mac, "6c92bf328740");
        } else {
            LOGE("get mac addr error");
            return;
        }
    }
    LOGI("mac: %s", mac);
	LOGI("Subscribing to topic %s\nfor client %s using QoS%d", mac, mac, 0);
	opts.onSuccess = onSubscribe;
	opts.onFailure = onSubscribeFailure;
	opts.context = client;
    LOGI("client:%p", client);
	if ((rc = MQTTAsync_subscribe(client, mac, 0, &opts)) != MQTTASYNC_SUCCESS){
		LOGE("Failed to start subscribe, return code %d\n", rc);
	}
    connected = 1;
    
}

int mqtt_create()
{
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    char mac[16] = {0};
    int rc;

    if (get_mac_addr(mac) < 0) {
        if (server_mode) {
            strcpy(mac, "6c92bf328740");
        } else {
            LOGE("get mac addr error");
            goto exit;
        }
    }

	if ((rc = MQTTAsync_create(&client, ADDRESS, mac, MQTTCLIENT_PERSISTENCE_NONE, NULL))
			!= MQTTASYNC_SUCCESS) {
		LOGE("Failed to create client, return code %d\n", rc);
		goto exit;
	}

	if ((rc = MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL)) != MQTTASYNC_SUCCESS) {
		printf("Failed to set callbacks, return code %d\n", rc);
		goto exit;
	}

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
		LOGE("Failed to start connect, return code %d\n", rc);
		goto exit;
	}

    return 0;
 exit:
    return -1;
} 

void mqtt_publish()
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

    if (!server_mode) {
        opts.onSuccess = onSend;
        opts.onFailure = onSendFailure;
        opts.context = client;
        pubmsg.payload = tuple;
        pubmsg.payloadlen = (int)strlen(tuple);
        pubmsg.qos = 0;
        pubmsg.retained = 0;
        LOGI("client:%p", client);
        if ((rc = MQTTAsync_sendMessage(client, g_mac, &pubmsg, &opts)) != MQTTASYNC_SUCCESS) {
            LOGE("Failed to start sendMessage, return code %d\n", rc);
        } else {
            LOGI("mqtt publish topic: %s tuple : %s success", g_mac, tuple);
        }
    } 
}

int main(int argc, char *argv[])
{
    int err = 0;
    struct sockaddr_in addr;
    MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;

    if (!argv[1]) {
        printf("usage: %s <mac_address>\n", argv[0]);
        return 0;
    }

    if (argv[2] && !strcmp(argv[2], "server")) {
        server_mode = 1;
    }

    if (mqtt_create() < 0) {
        return 0;
    }
    strcpy(g_mac, argv[1]);

    err = stun_get_mapped_addr(&addr);
    if (err < 0) {
        LOGE("stun_get_mapped_addr() error:%d", err);
        return 0;
    }
    LOGI("mapped addr: %s:%d", inet_ntoa(addr.sin_addr), addr.sin_port);
    sprintf(tuple, "%s:%d", inet_ntoa(addr.sin_addr), addr.sin_port);
    while(!connected) {
        usleep(100);
    }
    mqtt_publish();
    
    while(getchar() != 'q') {
        usleep(100);
    }
    LOGI("disconnect mqtt");
    disc_opts.onSuccess = onDisconnect;
	disc_opts.onFailure = onDisconnectFailure;
    disc_opts.context = client;
	if ((err = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS) {
		LOGE("Failed to start disconnect, return code %d\n", err);
	} else {
        LOGI("done");
    }
    while(!disc_finished) {
        usleep(100);
    }
    LOGI("destory mqtt client");
    MQTTAsync_destroy(&client);
    return 0;
}
