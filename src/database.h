#ifndef DATABASE_H
#define DATABASE_H
#include <stdint.h>

#include "message.h"
#define DB_NRR_MAX 100
enum db_node_type { A = 1, NS = 2, CNAME = 5, MX = 15 };
struct db_zone {};
struct db_node {
    enum db_node_type type;
    uint32_t n_rr;
    struct resource_record* rr;
};
struct database {};

static struct database* database_ptr;
// init database from master file
int init_database(const char*);

// search zone from db
// and store result in buffer
// return 1 if matched
// else -1
int db_find_zone(struct db_zone*, const char*, uint32_t);

// match node in specific zone
// and store result in buffer
// return 1 if matched
// else -1
int db_find_node(const struct db_zone*, struct db_node*, const char*, uint32_t);
#endif