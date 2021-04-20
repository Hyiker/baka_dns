#include <string.h>

#include "server/message.h"
#include "server/resolv.h"
#include "server/server.h"
#include "storage/database.h"
#include "utils/conf.h"
#include "utils/logging.h"

#define UDP_PORT 53

int main(int argc, char* argv[]) {
    parse_cmd(argc, argv, &conf);
    LOG_INFO("Initializing server...\n");
    if (init_database("/Users/sidhch/Projects/baka_dns/dnsrelay.txt") < 0) {
        exit(EXIT_FAILURE);
    }
    int fd = create_socket(INADDR_ANY, UDP_PORT);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }
    listen_socket(fd, dns_recv_handle, resolv_handle);

    return 0;
}
