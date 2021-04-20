
#include "server/resolv.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage/database.h"
#include "utils/conf.h"
#include "utils/logging.h"
static void print_question(const struct message_question* mqptr) {
    char questionbuf[512] = {0};
    size_t qi = 0;
    int dlen = domain_len(mqptr->qname);
    if (dlen < 0) {
        return;
    }
    for (size_t i = 0; i < dlen; i++) {
        uint32_t len = mqptr->qname[i];
        if (!len) {
            break;
        }

        for (size_t j = 1; j <= len; j++) {
            questionbuf[qi++] = mqptr->qname[i + j];
        }
        i += len;
        questionbuf[qi++] = '.';
    }
    LOG_INFO("requested domain: %s, qtype = %d\n", questionbuf, mqptr->qtype);
}
static int rr_match(const struct resource_record* rr,
                    const struct message_question* mq, uint32_t dlen) {
    return rr->type == mq->qtype && rr->_class == mq->qclass &&
           strncmp(rr->name, mq->qname, dlen);
}
// copy resource record from _src_ into _dest_
// attention: this function will automatically allocates mem for name & rdata
// return -1 if failed else 1
static int rr_copy(struct resource_record* dest,
                   const struct resource_record* src) {
    memcpy(dest, src, sizeof(struct resource_record));
    int dlen = domain_len(src->name);
    if (dlen < 0) {
        return -1;
    }
    dest->name = malloc(sizeof(uint8_t) * dlen);
    dest->rdata = malloc(sizeof(uint8_t) * src->rdlength);
    memcpy(dest->name, src->name, sizeof(uint8_t) * dlen);
    memcpy(dest->rdata, src->rdata, sizeof(uint8_t) * src->rdlength);
    return 1;
}

int resolv_handle(uint8_t* sendbuf, uint32_t* ans_size,
                  const struct message* query) {
    // setup the response message
    struct message ans;
    memset(&ans, 0, sizeof(ans));
    ans.header.id = query->header.id;
    // clear the ra, currently not supported
    if (_RD(query->header.misc1) && conf.recursion_enabled) {
        // TODO: support recursive service
    }
    // set ancount and qcount in the header field
    //  TODO: copy question: ans.header.qdcount =
    ans.header.ancount = query->header.qdcount;
    ans.header.misc1 =
        create_misc1(QR_RESP, OPCODE_QUERY, AA_TRUE, TC_FALSE, RD_FALSE);
    ans.header.misc2 = create_misc2(RA_FALSE, Z, RCODE_NO_ERROR);

    // allocates rooms for the answers
    ans.answer = malloc(ans.header.ancount * sizeof(struct resource_record*));
    for (size_t i = 0; i < ans.header.ancount; i++) {
        ans.answer[i] = malloc(sizeof(struct resource_record));
        memset(ans.answer[i], 0, sizeof(struct resource_record));
    }

    for (int i = 0; i < query->header.qdcount; i++) {
        if (!query->question[i]) {
            LOG_ERR("bad query->question");
            return -1;
        }
        // for each query, select the relay database

        const struct message_question* question = query->question[i];
        print_question(question);
        int dlen = domain_len(question->qname);
        if (dlen < 0) {
            return -1;
        }

        struct resource_record* rrptr = select_database(question);

        if (rrptr) {
            // if record selected then copy it to answer
            LOG_INFO("rr found\n");
            rr_copy(ans.answer[i], rrptr);
        } else {
            // TODO: find in the cache
            LOG_ERR("look in the cache not implemented\n");
            return -1;
        }
    }
    int msgsize = 0;
    if ((msgsize = message_to_u8(&ans, sendbuf)) < 0) {
        return -1;
    }
    *ans_size = msgsize;

    print_msg_header(&ans.header);
    LOG_INFO("resp msg(%u): ", *ans_size);
    PRINT_U8ARR(sendbuf, *ans_size);
    free_heap_message(&ans);
    return 1;
}