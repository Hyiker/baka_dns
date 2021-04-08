#include "server.h"

#include <string.h>

#ifndef MSG_CONFIRM
#define MSG_CONFIRM MSG_OOB
#endif  // !MSG_CONFIRM

static void print_u8(const uint8_t *u8array, uint32_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%.2x ", u8array[i]);
    }
    printf("\n");
}

int dns_recv_handle(const uint8_t *buf, uint32_t size, struct message *msg) {
    struct message msg;
    memset(&msg, 0, sizeof(msg));
    int msgsig = message_from_buf(buf, size, &msg);
#ifdef VERBOSE
    printf("raw msg(%u): ", size);
    print_u8(buf, size);
#endif
    return msgsig;
}

int dns_send_handle(uint8_t *buf, uint32_t *size,
                    const struct message *msgrcv) {
    const char *echo = "hello!";
    strncpy(buf, echo, sizeof(echo));
    *size = sizeof(echo);
    return 1;
}

int create_socket(in_addr_t addr, uint16_t port) {
    int sockfd;
    struct sockaddr_in servaddr;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("create socket failed");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // ipv4 only for now
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) <
        0) {
        perror("bind failed");
        return -1;
    }
    return sockfd;
}

void listen_socket(int fd,
                   int (*recv_handle)(uint8_t *, uint32_t, struct message *),
                   int (*resolv_handle)(uint8_t *, uint32_t *,
                                        const struct message *)) {
    struct sockaddr_in cliaddr;
    int len, nrecv, nsend;
    char recvbuf[UDP_BUFFER_SIZE], sendbuf[UDP_BUFFER_SIZE];

    len = sizeof(cliaddr);  // len is value/resuslt

    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(&cliaddr, 0, sizeof(cliaddr));
        memset(sendbuf, 0, sizeof(sendbuf));
        nrecv = recvfrom(fd, (char *)recvbuf, UDP_BUFFER_SIZE, MSG_WAITALL,
                         (struct sockaddr *)&cliaddr, &len);
        struct message msgrcv;
        memset(&msgrcv, 0, sizeof(msgrcv));
        recv_handle(recvbuf, nrecv, &msgrcv);

        resolv_handle(sendbuf, &nsend, &msgrcv);
        sendto(fd, sendbuf, nsend, MSG_CONFIRM,
               (const struct sockaddr *)&cliaddr, len);
        printf("finish an echo\n");
    }
}
