
#include "socket/resolv.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "socket/client.h"
#include "storage/cache.h"
#include "storage/database.h"
#include "utils/conf.h"
#include "utils/logging.h"
#include "utils/utils.h"
#define ANCOUNT_MAX 100
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
    uint8_t failure_flag = RCODE_NO_ERROR;
    memset(&ans, 0, sizeof(ans));
    ans.header.id = query->header.id;
    ans.header.misc1 =
        create_misc1(QR_RESP, OPCODE_QUERY, AA_TRUE, TC_FALSE, RD_FALSE);
    ans.header.misc2 = create_misc2(RA_FALSE, Z, RCODE_NO_ERROR);

    struct resource_record ans_buffer[ANCOUNT_MAX] = {0};
    int external_dns_flag = 0;
    int ans_cnt = 0;
    for (int i = 0; i < query->header.qdcount; i++) {
        if (!query->question[i]) {
            LOG_ERR("bad query->question");
            failure_flag = RCODE_FORMAT_ERROR;
            break;
        }
        // for each query, select the relay database

        const struct message_question* question = query->question[i];
        print_question(question);
        int dlen = domain_len(question->qname);
        if (dlen < 0) {
            failure_flag = RCODE_FORMAT_ERROR;
            break;
        }

        int n_rec = cache_find_rr(ans_buffer, question->qname, question->qtype,
                                  question->qclass);
        if (n_rec < 0) {
            LOG_ERR("failed when looking in cache\n");
            failure_flag = RCODE_SERVER_FAILURE;
            break;
        } else if (n_rec > 0) {
            ans_cnt += n_rec;
            LOG_INFO("RR found in the cache\n");
            for (size_t j = 0; j < n_rec; j++) {
                cache_put_rr(&ans_buffer[j]);
            }

            continue;
        } else {
            LOG_INFO("RR not found in local cache\n");
        }

        struct resource_record* rrptr = select_database(question);

        if (rrptr) {
            if (!check_blocked(rrptr)) {
                // if record selected then copy it to answer
                LOG_INFO("Local Resource Record found\n");
                rr_copy(&ans_buffer[ans_cnt++], rrptr);
            } else {
                rrptr = NULL;
            }

        } else {
            LOG_INFO("RR not found in local db\n");
            // LOG_INFO("record not found, looking in the cache\n");

            // if failed in the cache, forward all the request to external dns
            external_dns_flag = 1;
            break;
        }
    }
    if (external_dns_flag && !failure_flag) {
        free_heap_message(&ans);
        if (send_question(conf._external_dns, query, &ans) < 0) {
            // FIXME: construct an empty resp
            LOG_ERR("fail reaching external dns, using an empty response\n");
            external_dns_flag = 0;
            ans.header.ancount = ans.header.arcount = ans.header.nscount = 0;
        } else {
            for (size_t j = 0; j < ans.header.ancount; j++) {
                cache_put_rr(ans.answer[j]);
            }
        }
    }
    if (!external_dns_flag && !failure_flag) {
        ans.header.ancount = ans_cnt;
        // no external dns sent, and no failure
        ans.answer =
            malloc(ans.header.ancount * sizeof(struct resource_record*));
        memset(ans.answer, 0,
               sizeof(ans.header.ancount * sizeof(struct resource_record*)));
        for (size_t i = 0; i < ans.header.ancount; i++) {
            ans.answer[i] = malloc(sizeof(struct resource_record));
            memcpy(ans.answer[i], &ans_buffer[i],
                   sizeof(struct resource_record));
        }
    }
    if (failure_flag) {
        free_heap_message(&ans);
        ans.header.misc2 = create_misc2(RA_FALSE, Z, failure_flag);
        ans.header.ancount = ans.header.arcount = ans.header.nscount = 0;
    }
    if (!external_dns_flag || failure_flag) {
        // paste question in the response
        ans.header.qdcount = query->header.qdcount;
        ans.question =
            malloc(sizeof(struct message_question*) * ans.header.qdcount);
        for (size_t i = 0; i < ans.header.qdcount; i++) {
            ans.question[i] = malloc(sizeof(struct message_question));
            mq_copy(ans.question[i], query->question[i]);
        }
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