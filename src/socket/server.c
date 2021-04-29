#include "socket/server.h"

#include <string.h>

#include "pthread.h"
#include "socket/socket.h"
#include "storage/database.h"
#include "utils/logging.h"
int socket_fd = -1;
struct resolv_args {
    struct sockaddr_in cliaddr;
    uint8_t rcvbuffer[UDP_BUFFER_SIZE];
    int nrecv;
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *);
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *);
};
int dns_recv_handle(const uint8_t *buf, uint32_t size, struct message *msg) {
    int msgsig = message_from_buf(buf, size, msg);

    return msgsig;
}

int create_socket(in_addr_t addr, uint16_t port) {
    int sockfd;
    struct sockaddr_in servaddr;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERR("create socket failed");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // ipv4 only for now
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) <
        0) {
        LOG_ERR("bind failed");
        return -1;
    }
    return sockfd;
}

static void resolv_and_respond(struct resolv_args *args) {
    uint32_t nsend = 0, len = 0;
    struct message msgrcv;
    uint8_t sendbuf[UDP_BUFFER_SIZE];
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(&msgrcv, 0, sizeof(msgrcv));
    len = sizeof(struct sockaddr_in);
    args->recv_handle(args->rcvbuffer, args->nrecv, &msgrcv);
    if (args->resolv_handle(sendbuf, &nsend, &msgrcv) < 0) {
        exit(EXIT_FAILURE);
    }
    sendto(socket_fd, sendbuf, nsend, MSG_CONFIRM,
           (const struct sockaddr *)&args->cliaddr, len);
    LOG_INFO("msg size %u sent\n", nsend);
    free_heap_message(&msgrcv);
    free(args);
    pthread_exit(NULL);
}
static struct resolv_args *create_resolv_args(
    struct sockaddr_in *cliaddr, uint8_t rcvbuffer[UDP_BUFFER_SIZE], int nrecv,
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *),
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *)) {
    struct resolv_args *args = malloc(sizeof(struct resolv_args));
    memcpy(&args->cliaddr, cliaddr, sizeof(struct sockaddr_in));
    memcpy(args->rcvbuffer, rcvbuffer, sizeof(args->rcvbuffer));
    args->nrecv = nrecv;
    args->recv_handle = recv_handle;
    args->resolv_handle = resolv_handle;
    return args;
}
void listen_socket(
    int fd, int (*recv_handle)(const uint8_t *, uint32_t, struct message *),
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *)) {
    struct sockaddr_in cliaddr;
    int len, nrecv, nsend;
    uint8_t recvbuf[UDP_BUFFER_SIZE];
    socket_fd = fd;
    len = sizeof(cliaddr);  // len is value/resuslt

    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(&cliaddr, 0, sizeof(cliaddr));
        len = sizeof(cliaddr);
        nrecv = recvfrom(socket_fd, (char *)recvbuf, UDP_BUFFER_SIZE,
                         MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        pthread_t t1;
        pthread_create(&t1, NULL, resolv_and_respond,
                       create_resolv_args(&cliaddr, recvbuf, nrecv, recv_handle,
                                          resolv_handle));
        pthread_join(t1, NULL);
    }
}
