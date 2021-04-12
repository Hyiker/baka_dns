#ifndef RESOLV_H
#define RESOLV_H
#include <stdint.h>

#include "build_conf.h"
#include "message.h"
// resolve the recv message and construct an answer
// defined by [RFC1034] 4.3.2 Algorithm
int resolv_handle(uint8_t *, uint32_t *, const struct message *);

#endif