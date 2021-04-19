#include "storage/database.h"

#include <stdlib.h>

#include "utils/logging.h"

struct database db = {0};
int init_database(const char* relay_file) {
    // TODO
    LOG_INFO("initializing database from `%s`\n", relay_file);
    return -1;
}
struct resource_record* select_database(const struct message_question* msgq,
                                        uint32_t dlen) {
    return NULL;
}
