#include "resolv.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

int resolv_handle(uint8_t* sendbuf, uint32_t* ans_size,
                  const struct message* query) {
    struct message ans;
    memset(&ans, 0, sizeof(ans));
    // header setup
    ans.header.id = query->header.id;
    uint8_t misc1, misc2;
    ans.header.misc1 =
        QR_RESP | OPCODE_QUERY << 1 | AA_FALSE << 5 | TC_FALSE << 6 | RD_FALSE;
    ans.header.misc2 = RA_FALSE  // currently no support for recursive query
                       | Z << 1 | RCODE_NO_ERROR;
    ans.header.ancount = 1;

    // answer setup
    struct resource_record* ar = malloc(sizeof(struct resource_record));
    ans.answer = malloc(sizeof(struct resource_record*));
    memset(ar, 0, sizeof(struct resource_record));

    uint32_t domain_len =
        strnlen(query->question[0]->qname, DOMAIN_LEN_MAX) + 1;
    ar->name = malloc(domain_len * sizeof(uint8_t));
    memcpy(ar->name, query->question[0]->qname, domain_len);
    ar->type = RRTYPE_A;
    ar->_class = RRCLASS_IN;
    ar->ttl = 0;
    ar->rdlength = 4;
    ar->rdata = malloc(4 * sizeof(uint8_t));
    uint32_t ip = 0xdcb52694;
    ip = htonl(ip);
    memcpy(ar->rdata, &ip, sizeof(ip));

    ans.answer[0] = ar;
    int msgsize = 0;
    if ((msgsize = message_to_u8(&ans, sendbuf)) < 0) {
        return -1;
    }
    *ans_size = msgsize;
#ifdef VERBOSE
    printf("resp msg(%u): ", *ans_size);
    print_u8(sendbuf, *ans_size);
#endif
    return 1;
}