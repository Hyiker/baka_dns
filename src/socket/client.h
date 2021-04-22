#ifndef CLIENT_H
#define CLIENT_H
#define CLIENT_BUFFER_SIZE 1024
#include "storage/message.h"
// send a dns question to dest _ip_
// use a preset message question
// when get a call back, or timeout triggered,
// use a callback handle to deal with
int send_question(uint32_t, struct message_question *,
                  struct resource_record *);
#endif