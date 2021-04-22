#ifndef RESOLV_H
#define RESOLV_H
#include <stdint.h>

#include "utils/conf.h"
#include "storage/message.h"

// resolve the recv message and construct an answer
int resolv_handle(uint8_t *, uint32_t *, const struct message *);

#endif