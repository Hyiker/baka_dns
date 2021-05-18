#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>

// convert a string based ipv4 addr into an unsigned
// return address is big endian
int ipv4_convert(uint8_t*, uint32_t*);

// convert a string based ipv6 addr into an uint128_t
int ipv6_convert(uint8_t*, __uint128_t*);
#endif