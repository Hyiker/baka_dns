#ifndef MESSAGE_H
#define MESSAGE_H
#include <stdint.h>

#include "build_conf.h"
#define DOMAIN_LEN_MAX 256
#define DOMAIN_BUF_LEN 5 * 12
#define QR_REQ 0
#define QR_RESP 1
#define OPCODE_QUERY 0
#define OPCODE_IQUERY 1
#define OPCODE_STATUS 2
#define AA_TRUE 1
#define AA_FALSE 0
#define TC_TRUE 1
#define TC_FALSE 0
#define RD_TRUE 1
#define RD_FALSE 0
#define RA_TRUE 1
#define RA_FALSE 0
#define Z 0

#define RCODE_NO_ERROR 0
#define RCODE_FORMAT_ERROR 1
#define RCODE_SERVER_FAILURE 2
#define RCODE_NAME_ERROR 3
#define RCODE_NOT_IMPLEMENTED 4
#define RCODE_REFUSED 5

#define RRTYPE_A 1
#define RRTYPE_AAAA 28
#define RRTYPE_PTR 12
#define RRTYPE_CNAME 5
#define RRTYPE_HINFO 13
#define RRTYPE_MX 15
#define RRTYPE_NS 2
#define RRCLASS_IN 1

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
int message_from_buf(const uint8_t*, uint32_t, struct message*);

int message_to_u8(const struct message*, uint8_t*);
#endif