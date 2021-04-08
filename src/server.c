#include "server.h"

#include <string.h>

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

void listen_socket(int fd, int (*recv_handle)(uint8_t *, uint32_t),
                   int (*send_handle)(uint8_t *, uint32_t *)) {
    struct sockaddr_in cliaddr;
    int len, nrecv, nsend;
    char recvbuf[UDP_BUFFER_SIZE], sendbuf[UDP_BUFFER_SIZE];

    len = sizeof(cliaddr);  // len is value/resuslt

    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(&cliaddr, 0, sizeof(cliaddr));
        nrecv = recvfrom(fd, (char *)recvbuf, UDP_BUFFER_SIZE, MSG_WAITALL,
                         (struct sockaddr *)&cliaddr, &len);
        recv_handle(recvbuf, nrecv);

        send_handle(sendbuf, &nsend);
        sendto(fd, sendbuf, nsend, MSG_CONFIRM,
               (const struct sockaddr *)&cliaddr, len);
        printf("finish an echo\n");
    }
}
