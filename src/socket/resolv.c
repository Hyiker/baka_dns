
#include "socket/resolv.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "socket/client.h"
#include "storage/database.h"
#include "utils/conf.h"
#include "utils/logging.h"
#include "utils/utils.h"
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

int resolv_handle(uint8_t* sendbuf, uint32_t* ans_size, struct message* query) {
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
    memset(ans.answer, 0,
           sizeof(ans.header.ancount * sizeof(struct resource_record*)));
    for (size_t i = 0; i < ans.header.ancount; i++) {
        ans.answer[i] = malloc(sizeof(struct resource_record));
        memset(ans.answer[i], 0, sizeof(struct resource_record));
    }
    int external_dns_flag = 0;

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
            LOG_INFO("Local Resource Record found\n");
            rr_copy(ans.answer[i], rrptr);
        } else {
            // TODO: find in the cache
            // LOG_INFO("record not found, looking in the cache\n");
            // LOG_ERR("look in the cache not implemented\n");

            // if failed in the cache, forward all the request to external dns
            external_dns_flag = 1;
            break;
        }
    }
    if (external_dns_flag) {
        free_heap_message(&ans);
        send_question(conf._external_dns, query, &ans);
    }

    int msgsize = 0;
    if ((msgsize = message_to_u8(&ans, sendbuf)) < 0) {
        return -1;
    }
    *ans_size = msgsize;

    // print_msg_header(&ans.header);
    free_heap_message(&ans);
    return 1;
}