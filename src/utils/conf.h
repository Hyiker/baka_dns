#ifndef BUILD_CONF_H
#define BUILD_CONF_H

#include <argp.h>
#include <stdint.h>
struct dns_config {
    uint8_t verbose;
    uint8_t recursion_enabled;
};

extern struct dns_config conf;
// parse args from command line
void parse_cmd(int, char **, struct dns_config *);
#endif