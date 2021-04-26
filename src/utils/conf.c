#include "utils/conf.h"

#include "utils/logging.h"
#include "utils/utils.h"
static struct argp_option options[] = {
    {"verbose", 'v', 0, OPTION_ARG_OPTIONAL, "Produce verbose output"},
    {"relay", 'r', "relay", 0, "Specify a relay file path"},
    {0}};
static char args_doc[] = "ARG1";
static char doc[] = "A stupid dns relay";
struct dns_config conf;
char default_relay_file[] = "relay.txt";
char default_external_dns[] = "1.1.1.1";
static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct dns_config* ptrconf = state->input;
    switch (key) {
        case 'v':
            ptrconf->verbose = 1;
            break;
        case 'r':
            ptrconf->relay_file_path = arg;
            break;
        case ARGP_KEY_ARG:
            ptrconf->external_dns = arg;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
            break;
    }
    return 0;
}
static struct argp argp = {options, parse_opt, args_doc, doc};
int parse_cmd(int argc, char** argv, struct dns_config* confptr) {
    argp_parse(&argp, argc, argv, 0, 0, confptr);
    // check REQUIRED options
    if (!confptr->relay_file_path) {
        // LOG_ERR("relay file not provided\n");
        confptr->relay_file_path = default_relay_file;
    }
    if (!confptr->external_dns) {
        LOG_INFO("no external dns provided, using default `%s`\n",
                 default_external_dns);
        confptr->external_dns = default_external_dns;
    } else {
        LOG_INFO("using external dns `%s`\n", confptr->external_dns);
    }
    uint32_t ip;
    if (ipv4_convert(confptr->external_dns, &ip) < 0) {
        LOG_ERR("invalid external dns addr\n");
        return -1;
    }
    confptr->_external_dns = ip;

    return 1;
}