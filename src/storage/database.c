#include "storage/database.h"

#include <stdlib.h>

#include "utils/logging.h"

struct database db = {0};
int init_database(const char* relay_file) {
    // TODO
    LOG_INFO("initializing database from `%s`\n", relay_file);
    db.tree = tree_init(rr_hash);
    uint8_t domain[] = {3, 'c', 'o', 'm', 2, 'c', 'w', 3, 'w', 'w', 'w', 0};
    uint8_t ipbuf[4] = {0xdc, 0xb5, 0x26, 0x94};

    struct resource_record* rr =
        create_resource_record(domain, RRTYPE_A, RRCLASS_IN, 0, 4, ipbuf);
    tree_insert(db.tree, rr);
    struct resource_record* r = tree_search(db.tree, domain, 12);
    if (!r) {
        LOG_ERR("search err");
        return -1;
    } else {
        LOG_INFO("rr found\n");
        PRINT_U8ARR(rr->rdata, rr->rdlength);
    }

    return 1;
}
struct resource_record* select_database(const struct message_question* msgq,
                                        uint32_t dlen) {
    return NULL;
}
