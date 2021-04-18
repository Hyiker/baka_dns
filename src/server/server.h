#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils/conf.h"
#include "message.h"

#define UDP_BUFFER_SIZE 1024

// create a socket with local _addr_ and _port_
// the socket will be bind to local once it's created
// returns the fd if success, else print error and return -1
int create_socket(in_addr_t, uint16_t);

// listen to socket by fd
// when a recv event is triggered, use _recv_handle_ to deal with the _buffer_
// then use send_handle to set content and size for the sendbuf
void listen_socket(
    int, int (*recv_handle)(const uint8_t *, uint32_t, struct message *),
    int (*resolv_handle)(uint8_t *, uint32_t *, const struct message *));

int dns_recv_handle(const uint8_t *, uint32_t, struct message *);

int dns_send_handle(uint8_t *, uint32_t *, const struct message *);
#endif