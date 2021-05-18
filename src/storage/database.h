#ifndef DATABASE_H
#define DATABASE_H
#include <stdint.h>

#include "storage/message.h"
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
    struct bucket_tree* tree;
};

extern struct database db;
// init database from relay file
int init_database(const char*);

// insert a rr into db
int insert_database(struct resource_record* rr);
// select database by a msg_question
// returns the ptr to the rr
ssize_t select_database(const struct message_question*,
                                        struct resource_record*);

#endif