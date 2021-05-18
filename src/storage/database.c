#include "storage/database.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>

#include "message.h"
#include "utils/logging.h"
#include "utils/utils.h"
#define RR_BUF_MAX 1000
struct database db;

void str_lower(char* str) {
    while (*str) {
        *str = tolower(*str);
        str++;
    }
}
static int format_domain(uint8_t* dest, char* src) {
    const char s[] = ".";
    int n = 0, dlen = 0;
    char* token;
    token = strtok(src, s);
    if (token == NULL) {
        return -1;
    }

    while (token != NULL) {
        n = strlen(token);
        dest[0] = n;
        memcpy(++dest, token, n);
        dest += n;
        dlen += n + 1;
        token = strtok(NULL, s);
    }
    dest[0] = 0;
    return dlen + 1;
}
int init_database(const char* relay_file) {
    // TODO
    LOG_INFO("initializing database from `%s`\n", relay_file);
    db.tree = tree_init(rr_hash);
    struct resource_record* rrs[RR_BUF_MAX];
    FILE* fp = NULL;
    fp = fopen(relay_file, "r");
    if (!fp) {
        LOG_ERR("no such relay file `%s`\n", relay_file);
        return -1;
    }
    char domainbuf[1024], ipbuf[1024], formatd[DOMAIN_BUF_LEN], formatip[1024];
    char* linebuf = NULL;
    uint32_t len, nread;
    while ((nread = getline(&linebuf, &len, fp)) != -1) {
        if (nread > 2048) {
            LOG_ERR("line len too long\n");
            continue;
        }

        sscanf(linebuf, "%1024s%1024s", ipbuf, domainbuf);
        str_lower(domainbuf);
        uint32_t ipv4 = 0;
        __uint128_t ipv6 = 0;
        uint32_t rrtype;
        if (ipv4_convert(ipbuf, &ipv4) < 0) {
            LOG_ERR("bad ipv4 format, recognizing as ipv6 addr\n");
            // fallback to ipv6
            if (ipv6_convert(ipbuf, &ipv6) < 0) {
                LOG_ERR("bad ipv6 format\n");
                continue;
            } else {
                rrtype = RRTYPE_AAAA;
                *(__uint128_t*)formatip = ipv6;
            }
        } else {
            rrtype = RRTYPE_A;
            *(uint32_t*)formatip = ipv4;
        }

        int dlen = format_domain(formatd, domainbuf);
        if (dlen < 0 || dlen >= DOMAIN_LEN_MAX) {
            LOG_ERR("bad domain\n");
            continue;
        }

        struct resource_record* rr = create_resource_record(
            formatd, rrtype, RRCLASS_IN, 0,
            rrtype == RRTYPE_A ? sizeof(ipv4) : sizeof(ipv6), formatip);
        if (insert_database(rr) < 0) {
            free_heap_resource_record(rr);
            free(rr);
        }
    }
    free(linebuf);

    return 1;
}
int insert_database(struct resource_record* rr) {
    int dlen = domain_len(rr->name);
    if (dlen < 0) {
        LOG_ERR("bad domain length\n");
        return -1;
    }
    uint8_t revbuf[DOMAIN_BUF_LEN] = {0};
    domain_rev(revbuf, rr->name);
    return tree_insert(db.tree, revbuf, rr);
}
struct resource_record* select_database(const struct message_question* msgq) {
    int dlen = domain_len(msgq->qname);
    if (dlen < 0) {
        LOG_ERR("bad domain length\n");
        return NULL;
    }
    uint8_t revbuf[DOMAIN_BUF_LEN] = {0};
    domain_rev(revbuf, msgq->qname);
    return tree_search(db.tree, revbuf, dlen, msgq->qtype);
}
