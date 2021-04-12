#include <string.h>

#include "database.h"
#include "message.h"
#include "resolv.h"
#include "server.h"

#define UDP_PORT 53

int main(int argc, char const* argv[]) {
    int fd = create_socket(INADDR_ANY, UDP_PORT);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }
    init_database("");
    listen_socket(fd, dns_recv_handle, resolv_handle);
    return 0;
}
