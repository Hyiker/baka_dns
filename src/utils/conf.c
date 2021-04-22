#include "utils/conf.h"

#include "utils/logging.h"
static struct argp_option options[] = {
    {"verbose", 'v', 0, OPTION_ARG_OPTIONAL, "Verbose Output"},
    {"relay", 'r', "relay", 0, "Relay file path"},
    {0}};
static char args_doc[] = "ARG1";
static char doc[] = "A stupid dns relay";
struct dns_config conf = {0, 0};
char* default_relay_file = "relay.txt";
static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct dns_config* ptrconf = state->input;
    switch (key) {
        case 'v':
            ptrconf->verbose = 1;
            break;
        case 'r':
            ptrconf->relay_file_path = arg;
            break;

            // TODO: parse external dns here
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
        LOG_ERR("external dns server must be provided\n");
        return -1;
    }

    return 1;
}