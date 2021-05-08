#include "socket/server.h"

#include <string.h>

#include "pthread.h"
#include "socket/resolv.h"
#include "socket/socket.h"
#include "storage/database.h"
#include "unistd.h"
#include "utils/logging.h"
#include "utils/threadpool.h"
#define TCP_QUEUE_CNT 10
int socket_fd = -1;
struct resolv_args {
    struct sockaddr_in cliaddr;
    uint8_t rcvbuffer[UDP_BUFFER_SIZE];
    int nrecv;
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *);
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *);
};
struct responder_args {
    int fd;
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *);
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *);
};
static struct responder_args *create_responder_args(
    int fd, int (*recv_handle)(const uint8_t *, uint32_t, struct message *),
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *)) {
    struct responder_args *la = malloc(sizeof(struct responder_args));
    la->fd = fd;
    la->recv_handle = recv_handle;
    la->resolv_handle = resolv_handle;
    return la;
}
int dns_recv_handle(const uint8_t *buf, uint32_t size, struct message *msg) {
    int msgsig = message_from_buf(buf, size, msg);

    return msgsig;
}

static int create_socket(in_addr_t addr, uint16_t port, int protocol) {
    int sockfd;
    struct sockaddr_in servaddr;
    if ((sockfd = socket(AF_INET, protocol, 0)) < 0) {
        LOG_ERR("create socket failed\n");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // ipv4 only for now
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(addr);
    servaddr.sin_port = htons(port);
    int bind_err;
    if ((bind_err = bind(sockfd, (const struct sockaddr *)&servaddr,
                         sizeof(servaddr))) < 0) {
        LOG_ERR("binding port %u failed: %s\n", port, strerror(bind_err));
        return -1;
    }
    if (protocol == SOCK_STREAM) {
        if (listen(sockfd, TCP_QUEUE_CNT) != 0) {
            LOG_ERR("failed listening to the tcp connection\n");
            return -1;
        } else {
            LOG_INFO("Listening for tcp connection!\n");
        }
    }

    return sockfd;
}

static ssize_t resolv_and_respond(struct resolv_args *args) {
    uint32_t nsend = 0, len = 0;
    struct message msgrcv;
    uint8_t sendbuf[UDP_BUFFER_SIZE];
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(&msgrcv, 0, sizeof(msgrcv));
    len = sizeof(struct sockaddr_in);
    args->recv_handle(args->rcvbuffer, args->nrecv, &msgrcv);
    if (args->resolv_handle(sendbuf, &nsend, &msgrcv) < 0) {
        free(args);
        return -1;
    }
    sendto(socket_fd, sendbuf, nsend, MSG_CONFIRM,
           (const struct sockaddr *)&args->cliaddr, len);
    LOG_INFO("msg size %u sent\n", nsend);
    free_heap_message(&msgrcv);
    free(args);
    return 1;
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
static void udp_responder(struct responder_args *la) {
    struct sockaddr_in cliaddr;
    int len, nrecv, nsend;
    uint8_t recvbuf[UDP_BUFFER_SIZE];
    socket_fd = la->fd;
    len = sizeof(cliaddr);  // len is value/resuslt

    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(&cliaddr, 0, sizeof(cliaddr));
        len = sizeof(cliaddr);
        nrecv = recvfrom(socket_fd, (char *)recvbuf, UDP_BUFFER_SIZE,
                         MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        LOG_INFO("Recv a request through UDP\n");
        threadpool_add_job(
            create_job(resolv_and_respond,
                       create_resolv_args(&cliaddr, recvbuf, nrecv,
                                          la->recv_handle, la->resolv_handle)));
    }
    free(la);
    close(la->fd);
}

static void tls_responder(struct responder_args *la) {
    uint8_t recvbuf[TLS_BUFFER_SIZE], sendbuf[TLS_BUFFER_SIZE];
    struct message msgrcv;
    struct sockaddr_in addr;

    int nsend = 0;
    uint32_t len = sizeof(addr);
    memset(recvbuf, 0, sizeof(recvbuf));
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(&msgrcv, 0, sizeof(msgrcv));
    while (1) {
        int clientfd = accept(la->fd, (struct sockaddr *)&addr, &len);
        if (clientfd < 0) {
            LOG_ERR("bad client file descriptor\n");
            break;
        }

        int nrecv = recv(clientfd, recvbuf, sizeof(recvbuf), 0);
        LOG_INFO("[%d] Recv a request size %d through TLS\n", clientfd, nrecv);
        la->recv_handle(recvbuf + 2, nrecv - 2, &msgrcv);

        if (la->resolv_handle(sendbuf + 2, &nsend, &msgrcv) < 0) {
            LOG_ERR("resolv failed for TLS Server\n");
            free(la);
        }
        // the extra header payload
        *(uint16_t *)sendbuf = htons(nsend);
        int tmp;
        if ((tmp = send(clientfd, sendbuf, nsend + 2, 0)) < 0) {
            LOG_ERR("TLS connection failed sending dns resp\n");
            break;
        }
        close(clientfd);
        LOG_INFO("message size %d sent\n", tmp);
    }

    free(la);
    close(la->fd);
}

ssize_t start_udp_server(pthread_t *pt) {
    int udp_fd = create_socket(INADDR_ANY, UDP_PORT, SOCK_DGRAM);
    if (udp_fd < 0) {
        LOG_ERR("failed creating udp server!\n");
        return -1;
    }
    pthread_create(
        pt, NULL, udp_responder,
        create_responder_args(udp_fd, dns_recv_handle, resolv_handle));
    LOG_INFO("udp server is now running...\n");
    return 1;
}
ssize_t start_tls_server(pthread_t *pt) {
    int tls_fd = create_socket(INADDR_ANY, TCP_PORT, SOCK_STREAM);
    if (tls_fd < 0) {
        LOG_ERR("failed creating tls server!\n");
        return -1;
    }
    pthread_create(
        pt, NULL, tls_responder,
        create_responder_args(tls_fd, dns_recv_handle, resolv_handle));
    LOG_INFO("tls server is now running...\n");
    return 1;
}