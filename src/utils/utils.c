#include "utils/utils.h"

#include "arpa/inet.h"
#include "stdlib.h"
#include "utils/logging.h"
int ipv4_convert(uint8_t* src, uint32_t* dest) {
    struct in_addr res;
    if (inet_pton(AF_INET, src, &res) == 1) {
        *dest = res.s_addr;
        return 1;
    } else {
        LOG_ERR("%s is not a valid v4 addr\n", src);
        return -1;
    }
}

int ipv6_convert(uint8_t* src, __uint128_t* dest) {
    struct in6_addr res;
    if (inet_pton(AF_INET6, src, &res) == 1) {
        *dest = *((__uint128_t*)res.s6_addr);
        return 1;
    } else {
        LOG_ERR("%s is not a valid v6 addr\n", src);
        return -1;
    }
}