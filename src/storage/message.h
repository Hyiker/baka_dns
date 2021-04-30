#ifndef MESSAGE_H
#define MESSAGE_H
#include <stdint.h>

#include "utils/conf.h"
#define DOMAIN_LEN_MAX 256
#define DOMAIN_BUF_LEN 512
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
#define RRTYPE_NS 2
#define RRTYPE_CNAME 5
#define RRTYPE_PTR 12
#define RRTYPE_HINFO 13
#define RRTYPE_MX 15
#define RRCLASS_IN 1

#define QR_MASK 0x80
#define OPCODE_MASK 0x78
#define AA_MASK 0x04
#define TC_MASK 0x02
#define RD_MASK 0x01
#define RA_MASK 0x80
#define Z_MASK 0x70
#define RCODE_MASK 0x0F

#define _QR(x) ((x & QR_MASK) >> 7)
#define _OPCODE(x) ((x & OPCODE_MASK) >> 3)
#define _AA(x) ((x & AA_MASK) >> 2)
#define _TC(x) ((x & TC_MASK) >> 1)
#define _RD(x) (x & RD_MASK)
#define _RA(x) ((x & RA_MASK) >> 7)
#define _Z(x) ((x & Z_MASK) >> 4)
#define _RCODE(x) (x & RCODE_MASK)

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
int mq_copy(struct message_question*, const struct message_question*);
// copy resource record from _src_ into _dest_
// attention: this function will automatically allocates mem for name & rdata
// return -1 if failed else 1
int rr_copy(struct resource_record*, const struct resource_record*);
int message_to_u8(const struct message*, uint8_t*);

// free heap-allocated data in the message struct
int free_heap_message(struct message*);

void print_msg_header(struct message_header*);

// param by the seq:
// qr, opcode, aa, tc, rd
uint8_t create_misc1(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);

// param by the seq:
// ra, z, rcode
uint8_t create_misc2(uint8_t, uint8_t, uint8_t);

// measure the len of a domain
// normally, 0 <= ret <= DOMAIN_LEN_MAX
// if the length is even greater than DOMAIN_BUF_LEN, returns -1
int domain_len(const uint8_t*);

// create from name, type, _class, ttl, rdlength, rdata
struct resource_record* create_resource_record(uint8_t*, uint16_t, uint16_t,
                                               uint32_t, uint16_t, uint8_t*);

void free_heap_message_question(struct message_question*);
void free_heap_resource_record(struct resource_record*);
// reverse a domain name
// from 3 w w w 4 b u d u 2 c n 0
// into 2 c n 4 b u d u 3 w w w 0
void domain_rev(uint8_t*, uint8_t*);

// compare two resource record
uint8_t rr_cmp(struct resource_record*, struct resource_record*);
#endif