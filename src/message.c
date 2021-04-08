#include "message.h"

#include <netinet/in.h>
#include <string.h>

// ## code expansion macro starts ##
#define EVAL(...) EVAL1024(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...) EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...) EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...) EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...) EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...) EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...) EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...) EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...) EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...) EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...) __VA_ARGS__
#define DEFER2(m) m EMPTY EMPTY()()
#define FIRST(a, ...) a
#define HAS_ARGS(...) BOOL(FIRST(_END_OF_ARGUMENTS_ __VA_ARGS__)())
#define _END_OF_ARGUMENTS_() 0
#define MAP(m, first, ...)                                               \
    m(first) IF_ELSE(HAS_ARGS(__VA_ARGS__))(                             \
        DEFER2(_MAP)()(m, __VA_ARGS__))(/* Do nothing, just terminate */ \
    )
#define _MAP() MAP
// ## code expansion macro ends ##

// copy the network byte ordered u8 array into dest with _size_
static uint16_t u8n_to_u16h(uint8_t* ptr) {
    uint16_t dest;
    memcpy(&dest, ptr, sizeof(uint16_t));
    dest = ntohs(dest);
    return dest;
}
static uint16_t u8n_to_u8h(uint8_t* ptr) {
    uint16_t dest;
    memcpy(&dest, ptr, sizeof(uint8_t));
    return dest;
}
static void msg_header_from_buf(uint8_t buf,
                                struct message_header* msg_header) {
    uint16_t id, qdcount, ancount, nscount, arcount;
    uint8_t misc1, misc2;
    uint8_t* ptr = buf;
    id = u8n_to_u16h(ptr);
    ptr += sizeof(id);
    misc1 = u8n_to_u8h(ptr);
    ptr += sizeof(misc1);
    misc2 = u8n_to_u8h(ptr);
    ptr += sizeof(misc2);
    qdcount = u8n_to_u16h(ptr);
    ptr += sizeof(qdcount);
    ancount = u8n_to_u16h(ptr);
    ptr += sizeof(ancount);
    nscount = u8n_to_u16h(ptr);
    ptr += sizeof(nscount);
    arcount = u8n_to_u16h(ptr);
    ptr += sizeof(arcount);
}
int message_from_buf(uint8_t* buf, uint32_t size, struct message* msg) {
    msg_header_from_buf(buf, &msg->header);
    return 1;
}