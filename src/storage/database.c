#include "storage/database.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>

#include "utils/logging.h"
#define RR_BUF_MAX 1000
struct database db = {0};
static uint32_t ipv4_convert(uint8_t* src) {
    const char s[] = ".";
    uint32_t ret = 0;
    char* token;
    token = strtok(src, s);
    while (token != NULL) {
        ret = ret << 8 | atoi(token);
        token = strtok(NULL, s);
    }
    return ret;
}
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
    uint32_t cnt = 0;
    while (fscanf(fp, "%s%s", ipbuf, domainbuf) != -1) {
        str_lower(domainbuf);
        LOG_INFO("Inserting domain [%u]`%s`\n", ++cnt, domainbuf);
        uint32_t ip = ipv4_convert(ipbuf);
        ip = htonl(ip);
        memcpy(formatip, &ip, 4);
        int dlen = format_domain(formatd, domainbuf);
        struct resource_record* rr = create_resource_record(
            formatd, RRTYPE_A, RRCLASS_IN, 0, 4, formatip);
        if (insert_database(rr) < 0) {
            free_heap_resource_record(rr);
        }
    }

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
    return tree_search(db.tree, revbuf, dlen);
}
