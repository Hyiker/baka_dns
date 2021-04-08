#include "resolv.h"

#include <string.h>

#include "message.h"
int resolv_handle(uint8_t* sendbuf, uint32_t* ans_size,
                  const struct message* query) {
    struct message ans;
    memset(&ans, 0, sizeof(ans));
    ans.header.id = query->header.id;
    uint8_t misc1, misc2;
    ans.header.misc1 =
        QR_RESP | OPCODE_QUERY << 1 | AA_FALSE << 5 | TC_FALSE << 6 | RD_FALSE;
    ans.header.misc2 = RA_FALSE  // currently no support for recursive query
                       | Z << 1 | RCODE_NO_ERROR;
    ans.header.ancount = 1;
    struct resource_record* ar = malloc(sizeof(struct resource_record));
    ans.answer = malloc(sizeof(struct resource_record*));
    memset(ar, 0, sizeof(struct resource_record));
    ans.answer[0] = ar;
}