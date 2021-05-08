#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "storage/message.h"
#include "utils/conf.h"

#define UDP_BUFFER_SIZE 4096
#define TLS_BUFFER_SIZE 5120


// runs an udp server
ssize_t start_udp_server(pthread_t *);

ssize_t start_tls_server(pthread_t *);

int dns_recv_handle(const uint8_t *, uint32_t, struct message *);

int dns_send_handle(uint8_t *, uint32_t *, const struct message *);
#endif