#include "socket/client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "socket/socket.h"
#include "utils/logging.h"
#define EXTERNAL_SECS_MAX 4
#define EXTERNAL_DNS_PORT 53
int send_question(uint32_t ipaddr, struct message *reqptr,
                  struct message *respptr) {
    int sockfd;
    uint8_t buffer[CLIENT_BUFFER_SIZE];
    struct sockaddr_in servaddr;
    uint16_t old_id = reqptr->header.id, new_id = (uint16_t)rand();
    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERR("client socket creation failed\n");
        pthread_exit(NULL);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(EXTERNAL_DNS_PORT);
    servaddr.sin_addr.s_addr = ipaddr;

    // timeout options
    struct timeval timeout;
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                   sizeof(timeout)) < 0) {
        LOG_ERR("bad timeout option\n");
        close(sockfd);
        return -1;
    }

    int n;
    uint32_t len = sizeof(servaddr);
    reqptr->header.id = new_id;
    n = message_to_u8(reqptr, buffer);
    if (n < 0) {
        LOG_ERR("invalid request message\n");
        close(sockfd);
        return -1;
    }
    sendto(sockfd, buffer, n, MSG_CONFIRM, (const struct sockaddr *)&servaddr,
           sizeof(servaddr));
    LOG_INFO("dns request sent\n");
    memset(buffer, 0, sizeof(buffer));
    reqptr->header.id = old_id;

    n = recvfrom(sockfd, (char *)buffer, CLIENT_BUFFER_SIZE, MSG_WAITALL,
                 (struct sockaddr *)&servaddr, &len);
    LOG_INFO("dns response received\n");
    if (n < 0) {
        LOG_ERR("failed receiving resp\n");
        close(sockfd);
        return -1;
    }

    if (message_from_buf(buffer, n, respptr) < 0) {
        LOG_ERR("bad response data\n");
        close(sockfd);
        return -1;
    }
    if (respptr->header.id != new_id) {
        LOG_ERR("bad response data\n");
        close(sockfd);
        free_heap_message(respptr);
        return -1;
    }

    respptr->header.id = old_id;

    close(sockfd);
    return 1;
}