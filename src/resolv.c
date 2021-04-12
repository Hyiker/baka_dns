
#include "resolv.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "utils.h"

static int rr_match(const struct resource_record* rr,
                    const struct message_question* mq, uint32_t dlen) {
    return rr->type == mq->qtype && rr->_class == mq->qclass &&
           strncmp(rr->name, mq->qname, dlen);
}
static struct resource_record* rr_copy(const struct resource_record* rr,
                                       uint32_t dlen) {
    struct resource_record* new_rr = malloc(sizeof(struct resource_record));

    memcpy(new_rr, rr, sizeof(struct resource_record));
    new_rr->name = malloc(sizeof(uint8_t) * dlen);
    new_rr->rdata = malloc(sizeof(uint8_t) * rr->rdlength);
    memcpy(new_rr->name, rr->name, sizeof(uint8_t) * dlen);
    memcpy(new_rr->rdata, rr->rdata, sizeof(uint8_t) * rr->rdlength);
    return new_rr;
}

int resolv_handle(uint8_t* sendbuf, uint32_t* ans_size,
                  const struct message* query) {
    // step0: setup the response message
    struct message ans;
    memset(&ans, 0, sizeof(ans));
    ans.header.id = query->header.id;
    // step1: clear the ra, currently not supported
    if (_RD(query->header.misc1) && RECURSION_ENABLED) {
        // TODO: support recursive service
    }

    // step2: for each query, search nearest ancestor to QNAME
    for (int i = 0; i < query->header.qdcount; i++) {
        if (!query->question[i]) {
            perror("bad query->question");
            return -1;
        }

        const struct message_question* question = query->question[i];
        int dlen = domain_len(question->qname);

        struct db_zone zonebuf;
        int find_res = db_find_zone(&zonebuf, question->qname, dlen);
        if (find_res < 0) {
            return -1;
        }

        if (find_res > 0) {
            // step 3. matching down label in the zone
            struct db_node nodebuf;
            int match_res =
                db_find_node(&zonebuf, &nodebuf, question->qname, dlen);
            if (match_res < 0) {
                return -1;
            }
            // a. the whole of qname is matched
            if (nodebuf.type == CNAME && question->qtype != CNAME) {
                // if the data at the node is a cname
                // and qtype doesn't match it
                // TODO: handle it
            } else {
                struct resource_record* rrbuf[DB_NRR_MAX];
                for (size_t i = 0; i < nodebuf.n_rr; i++) {
                    if (rr_match(&nodebuf.rr[i], question, dlen)) {
                        rrbuf[ans.header.ancount++] =
                            rr_copy(&nodebuf.rr[i], dlen);
                    }
                }
                if (ans.header.ancount) {
                    ans.answer = malloc(ans.header.ancount *
                                        sizeof(struct resource_record*));
                    memcpy(
                        ans.answer, rrbuf,
                        ans.header.ancount * sizeof(struct resource_record*));
                }
                // ends here
            }
            // b. authoritative data
            // TODO
        }
    }

    // header setup
    uint8_t misc1, misc2;
    ans.header.misc1 =
        create_misc1(QR_RESP, OPCODE_QUERY, AA_FALSE, TC_FALSE, RD_FALSE);
    ans.header.misc2 = create_misc2(RA_FALSE, Z, RCODE_NO_ERROR);

    ans.header.qdcount = 1;
    ans.header.ancount = 1;

    // answer setup
    struct resource_record* ar = malloc(sizeof(struct resource_record));
    struct message_question* mq = malloc(sizeof(struct message_question));
    ans.answer = malloc(sizeof(struct resource_record*));
    ans.question = malloc(sizeof(struct message_question*));
    memset(ar, 0, sizeof(struct resource_record));
    memset(mq, 0, sizeof(struct message_question));

    uint32_t dlen = domain_len(query->question[0]->qname);
    mq->qname = malloc(dlen);
    mq->qtype = query->question[0]->qtype;
    mq->qclass = query->question[0]->qclass;
    memcpy(mq->qname, query->question[0]->qname, dlen);
    ar->name = malloc(dlen * sizeof(uint8_t));
    memcpy(ar->name, query->question[0]->qname, dlen);
    ar->type = RRTYPE_A;
    ar->_class = RRCLASS_IN;
    ar->ttl = 0;
    ar->rdlength = 4;
    ar->rdata = malloc(4 * sizeof(uint8_t));
    uint32_t ip = 0xdcb52694;
    ip = htonl(ip);
    memcpy(ar->rdata, &ip, sizeof(ip));

    ans.answer[0] = ar;
    ans.question[0] = mq;
    int msgsize = 0;
    if ((msgsize = message_to_u8(&ans, sendbuf)) < 0) {
        return -1;
    }
    *ans_size = msgsize;
#ifdef VERBOSE
    print_msg_header(&ans.header);
    printf("resp msg(%u): ", *ans_size);
    print_u8(sendbuf, *ans_size);
#endif
    free_heap_message(&ans);
    return 1;
}