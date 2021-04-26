#ifndef CLIENT_H
#define CLIENT_H
#define CLIENT_BUFFER_SIZE 1024
#include "storage/message.h"
// send a dns question to dest _ip_
// use a preset message question
// stores the response in messageptr
int send_question(uint32_t, struct message*, struct message*);
#endif