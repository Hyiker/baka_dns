#include "socket/server.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <string.h>

#include "socket/resolv.h"
#include "socket/socket.h"
#include "storage/database.h"
#include "unistd.h"
#include "utils/logging.h"
#include "utils/spec_defs.h"
#include "utils/threadpool.h"
#define TCP_QUEUE_CNT 10
#define SSL_PASSPHRASE "114514"
int socket_fd = -1;
struct resolv_args {
    struct sockaddr_in cliaddr;
    uint8_t rcvbuffer[UDP_BUFFER_SIZE];
    int nrecv;
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *);
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *);
    // dot only
    SSL_CTX *ctx;
    // dot only
    int clientfd;
};
struct responder_args {
    int fd;
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *);
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *);
    // dot only
    SSL_CTX *ctx;
};

static void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

static void cleanup_openssl() {
    EVP_cleanup();
}

static SSL_CTX *create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        LOG_ERR("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}
int my_pem_passwd_cb(char *buf, int size, int rwflag, void *userdata) {
    // using an explict passphrase
    strcpy(buf, SSL_PASSPHRASE);
    return strlen(buf);
}

void configure_context(SSL_CTX *ctx) {
    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, SSL_CERT_PATH, SSL_FILETYPE_PEM) <=
        0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    SSL_CTX_set_default_passwd_cb(ctx, my_pem_passwd_cb);
    if (SSL_CTX_use_PrivateKey_file(ctx, SSL_KEY_PATH, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

static struct responder_args *create_responder_args(
    int fd, int (*recv_handle)(const uint8_t *, uint32_t, struct message *),
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *),
    SSL_CTX *ctx) {
    struct responder_args *la = malloc(sizeof(struct responder_args));
    la->fd = fd;
    la->recv_handle = recv_handle;
    la->resolv_handle = resolv_handle;
    la->ctx = ctx;
    return la;
}
int dns_recv_handle(const uint8_t *buf, uint32_t size, struct message *msg) {
    return message_from_buf(buf, size, msg);
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

static ssize_t udp_resolv_and_respond(struct resolv_args *args) {
    uint32_t nsend = 0, len = 0;
    struct message msgrcv;
    uint8_t sendbuf[UDP_BUFFER_SIZE];
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(&msgrcv, 0, sizeof(msgrcv));
    len = sizeof(struct sockaddr_in);
    if (args->recv_handle(args->rcvbuffer, args->nrecv, &msgrcv) < -1) {
        LOG_ERR("Malformed packet\n");
        // construct an error response and return
        struct message errmsg;
        construct_error_response(RCODE_FORMAT_ERROR, &errmsg);
        nsend = message_to_u8(&errmsg, sendbuf);
        sendto(socket_fd, sendbuf, nsend, MSG_CONFIRM,
               (const struct sockaddr *)&args->cliaddr, len);
        free_heap_message(&msgrcv);
        free(args);
        return -1;
    }

    if (args->resolv_handle(sendbuf, &nsend, &msgrcv) < 0) {
        free(args);
        free_heap_message(&msgrcv);
        return -1;
    }
    sendto(socket_fd, sendbuf, nsend, MSG_CONFIRM,
           (const struct sockaddr *)&args->cliaddr, len);
    LOG_INFO("msg size %u sent\n", nsend);
    free_heap_message(&msgrcv);
    free(args);
    return 1;
}
static void cleanup_session(SSL *ssl, struct resolv_args *args) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(args->clientfd);
    free(args);
}

static ssize_t ssl_resolv_and_respond(struct resolv_args *args) {
    struct message msgrcv;
    int nsend = 0;
    uint8_t recvbuf[TLS_BUFFER_SIZE], sendbuf[TLS_BUFFER_SIZE];
    memset(recvbuf, 0, sizeof(recvbuf));
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(&msgrcv, 0, sizeof(msgrcv));
    SSL *ssl;
    ssl = SSL_new(args->ctx);
    SSL_set_fd(ssl, args->clientfd);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        free(args);
        return -1;
    }
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(args->clientfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
               sizeof tv);
    uint16_t expected_size;
    int resp = SSL_read(ssl, &expected_size, sizeof(expected_size));
    if (resp != sizeof(expected_size)) {
        LOG_ERR("read size timeout\n");
        return -1;
    }

    // read the expected size
    expected_size = ntohs(expected_size);
    int nrecv = 0, _rcv = 0;
    while (nrecv != expected_size &&
           (_rcv = SSL_read(ssl, recvbuf + nrecv, sizeof(recvbuf))) != -1) {
        nrecv += _rcv;
    }

    if (_rcv < 0 || nrecv != expected_size) {
        LOG_ERR("broken packet from client\n");
        cleanup_session(ssl, args);
        return -1;
    }

    LOG_INFO("Recv a request size %d through TLS\n", nrecv);
    if (args->recv_handle(recvbuf, nrecv, &msgrcv) < 0) {
        LOG_ERR("Malformed packet\n");
        // construct an error response and return
        struct message errmsg;
        construct_error_response(RCODE_FORMAT_ERROR, &errmsg);
        nsend = message_to_u8(&errmsg, sendbuf);
        *(uint16_t *)sendbuf = htons(nsend);
        SSL_write(ssl, sendbuf, nsend + 2);
        free_heap_message(&msgrcv);
        cleanup_session(ssl, args);
        return -1;
    }

    if (args->resolv_handle(sendbuf + 2, &nsend, &msgrcv) < 0) {
        LOG_ERR("resolv failed for TLS Server\n");
        cleanup_session(ssl, args);
        return -1;
    }
    // the extra header payload
    *(uint16_t *)sendbuf = htons(nsend);
    if (SSL_write(ssl, sendbuf, nsend + 2) < 0) {
        LOG_ERR("TLS connection failed sending dns resp\n");
        cleanup_session(ssl, args);
        return -1;
    }
    free_heap_message(&msgrcv);
    cleanup_session(ssl, args);
    LOG_INFO("message size %d sent\n", nsend);
    return 1;
}

static struct resolv_args *create_resolv_args(
    struct sockaddr_in *cliaddr, uint8_t rcvbuffer[UDP_BUFFER_SIZE], int nrecv,
    int (*recv_handle)(const uint8_t *, uint32_t, struct message *),
    int (*resolv_handle)(uint8_t *, uint32_t *, struct message *), SSL_CTX *ctx,
    int clientfd) {
    struct resolv_args *args = malloc(sizeof(struct resolv_args));
    if (cliaddr) {
        memcpy(&args->cliaddr, cliaddr, sizeof(struct sockaddr_in));
    }
    if (rcvbuffer) {
        memcpy(args->rcvbuffer, rcvbuffer, sizeof(args->rcvbuffer));
    }

    args->nrecv = nrecv;
    args->recv_handle = recv_handle;
    args->resolv_handle = resolv_handle;
    args->ctx = ctx;
    args->clientfd = clientfd;
    return args;
}
static void udp_responder(struct responder_args *la) {
    struct sockaddr_in cliaddr;
    int len, nrecv, nsend;
    uint8_t recvbuf[UDP_BUFFER_SIZE];
    socket_fd = la->fd;
    len = sizeof(cliaddr);  // len is value/result
    fd_set fds;
    FD_ZERO(&fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;
    while (1) {
        FD_SET(socket_fd, &fds);
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(&cliaddr, 0, sizeof(cliaddr));
        len = sizeof(cliaddr);
        select(socket_fd + 1, &fds, NULL, NULL, &tv);
        if (FD_ISSET(socket_fd, &fds)) {
            nrecv = recvfrom(socket_fd, (char *)recvbuf, UDP_BUFFER_SIZE,
                             MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
            LOG_INFO("Recv a request through UDP\n");
            threadpool_add_job(create_job(
                udp_resolv_and_respond,
                create_resolv_args(&cliaddr, recvbuf, nrecv, la->recv_handle,
                                   la->resolv_handle, NULL, 0)));
        }
    }
    free(la);
    close(la->fd);
}

static void tls_responder(struct responder_args *la) {
    struct sockaddr_in addr;

    uint32_t len = sizeof(addr);
    while (1) {
        int clientfd = accept(la->fd, (struct sockaddr *)&addr, &len);
        if (clientfd < 0) {
            LOG_ERR("bad client file descriptor\n");
            continue;
        }
        threadpool_add_job(create_job(
            ssl_resolv_and_respond,
            create_resolv_args(&addr, NULL, NULL, la->recv_handle,
                               la->resolv_handle, la->ctx, clientfd)));
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
        create_responder_args(udp_fd, dns_recv_handle, resolv_handle, NULL));
    LOG_INFO("udp server is now running...\n");
    return 1;
}
ssize_t start_tls_server(pthread_t *pt) {
    SSL_CTX *ctx;
    init_openssl();
    ctx = create_context();
    configure_context(ctx);

    int tls_fd = create_socket(INADDR_ANY, TCP_PORT, SOCK_STREAM);
    if (tls_fd < 0) {
        LOG_ERR("failed creating tls server!\n");
        return -1;
    }
    pthread_create(
        pt, NULL, tls_responder,
        create_responder_args(tls_fd, dns_recv_handle, resolv_handle, ctx));
    LOG_INFO("tls server is now running...\n");
    return 1;
}