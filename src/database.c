#include "database.h"

#include <stdlib.h>
int init_database(const char* master_file_path) {
    // TODO
    database_ptr = NULL;
    return -1;
}

int db_find_zone(struct db_zone* zoneptr, const char* domain, uint32_t dlen) {
    // TODO
    return -1;
}

int db_find_node(const struct db_zone* zoneptr, struct db_node* nodeptr,
                 const char* domain, uint32_t dlen) {
    // TODO
}