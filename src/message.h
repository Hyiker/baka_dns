#ifndef MESSAGE_H
#define MESSAGE_H
#include <stdint.h>

#include "build_conf.h"
#define DOMAIN_LEN_MAX 256
#define DOMAIN_BUF_LEN 512
struct message_header {
    uint16_t id;
    uint8_t misc1,  // qr, opcode, aa, tc, rd
        misc2;      // ra, z, rcode
    uint16_t qdcount, ancount, nscount, arcount;
};
struct message_question {
    uint8_t* qname;  // heap unsized ptr, reserve for unicode
    uint16_t qtype;
    uint16_t qclass;
};
struct resource_record {
    uint8_t* name;
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t* rdata;  // specific format depends on type & class
};
struct message {
    struct message_header header;
    struct message_question** question;  // len by qdcount
    struct resource_record** answer;     // len by ancount
    struct resource_record** authority;  // len by nscount
    struct resource_record** addition;   // len by arcount
};

// convert buf into message
// ret: non-neg is size of message
// else means errors occur when converting
int message_from_buf(uint8_t*, uint32_t, struct message*);

#endif