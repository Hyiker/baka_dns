#include "utils/conf.h"

static struct argp_option options[] = {{"verbose", 'v', 0, 0, "Verbose Output"},
                                       {0}};
static char args_doc[] = "ARG1";
static char doc[] = "Use verbose to get more detailed output";
struct dns_config conf = {0, 0};
static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct dns_config* ptrconf = state->input;
    switch (key) {
        case 'v':
            ptrconf->verbose = 1;
            break;

        default:
            return ARGP_ERR_UNKNOWN;
            break;
    }
    return 0;
}
static struct argp argp = {options, parse_opt, args_doc, doc};
void parse_cmd(int argc, char** argv, struct dns_config* confptr) {
    argp_parse(&argp, argc, argv, 0, 0, confptr);
}