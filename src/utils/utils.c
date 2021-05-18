#include "utils/utils.h"

#include "stdlib.h"
#include "utils/logging.h"
int ipv4_convert(uint8_t* src, uint32_t* dest) {
    const char s[] = ".";
    int ret = 0;
    char* token;
    token = strtok(src, s);
    int index = 0;
    while (token != NULL) {
        int segment = -1;
        char* endpoint;
        segment = strtol(token, &endpoint, 10);
        if (endpoint == NULL || *endpoint != '\0')
        {
            return -1;
        }
        
        ret = ret << 8 | segment;
        index++;
        if (segment < 0 || segment >= 256) {
            LOG_ERR("ipv4 addr segment must be within [0,255]\n");
            return -1;
        }
        if (index >= 5) {
            LOG_ERR("too many segments for an ipv4 addr\n");
            return -1;
        }
        token = strtok(NULL, s);
    }
    *dest = ret;
    return 1;
}