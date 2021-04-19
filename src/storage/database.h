#ifndef DATABASE_H
#define DATABASE_H
#include <stdint.h>

#include "server/message.h"
#include "storage/trees.h"
#define DB_NRR_MAX 100
#define DB_ZONE_MAX 1000
#define DB_NODE_MAX 100
enum db_node_type { A = 1, NS = 2, CNAME = 5, MX = 15 };
struct db_node {
    enum db_node_type type;
    uint32_t n_rr;
    struct resource_record* rr;
};
struct database {
    struct btree* tree;
};

extern struct database db;
// init database from relay file
int init_database(const char*);

// select database by a msg_question
// with a domain_length
// returns the const ptr to the rr
struct resource_record* select_database(const struct message_question*,
                                              uint32_t);

#endif