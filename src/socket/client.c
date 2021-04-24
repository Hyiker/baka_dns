#include "socket/client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "socket/socket.h"
#include "utils/logging.h"

int send_question(uint32_t ipaddr, const struct message_question *question,
                  struct resource_record *rr) {
    int sockfd;
    char buffer[CLIENT_BUFFER_SIZE];
    struct sockaddr_in servaddr;
    uint16_t req_id = (uint16_t)rand();
    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERR("client socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DNS_PORT);
    servaddr.sin_addr.s_addr = htonl(ipaddr);

    int n, len;
    struct message msg = {
        {(uint16_t)req_id,
         create_misc1(QR_REQ, OPCODE_QUERY, AA_TRUE, TC_FALSE, RD_FALSE),
         create_misc2(RA_FALSE, Z, RCODE_NO_ERROR)},
        0};
    msg.header.qdcount = 1;
    msg.question = malloc(sizeof(struct message_question *));
    msg.question[0] = question;
    n = message_to_u8(&msg, buffer);
    if (n < 0) {
        LOG_ERR("invalid request message\n");
        free(msg.question);
        return -1;
    }

    sendto(sockfd, buffer, n, MSG_CONFIRM, (const struct sockaddr *)&servaddr,
           sizeof(servaddr));
    LOG_INFO("dns request sent\n");
    memset(buffer, 0, sizeof(buffer));
    memset(&msg, 0, sizeof(msg));

    n = recvfrom(sockfd, (char *)buffer, CLIENT_BUFFER_SIZE, MSG_WAITALL,
                 (struct sockaddr *)&servaddr, &len);
    LOG_INFO("dns response received\n");
    message_from_buf(buffer, n, &msg);
    rr_copy(rr, msg.answer[0]);
    free_heap_message(&msg);
    close(sockfd);
}