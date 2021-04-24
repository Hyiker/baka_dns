#include <string.h>

#include "socket/resolv.h"
#include "socket/server.h"
#include "socket/socket.h"
#include "storage/database.h"
#include "storage/message.h"
#include "utils/conf.h"
#include "utils/logging.h"
#include "time.h"

int main(int argc, char* argv[]) {
    srand(time(NULL));
    if (parse_cmd(argc, argv, &conf) < 0) {
        LOG_ERR("Bad cmd arguments\n");
        exit(EXIT_FAILURE);
    }
    LOG_INFO("Initializing server, using relay file `%s`...\n",
             conf.relay_file_path);
    if (init_database(conf.relay_file_path) < 0) {
        exit(EXIT_FAILURE);
    }
    int fd = create_socket(INADDR_ANY, DNS_PORT);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }
    listen_socket(fd, dns_recv_handle, resolv_handle);
    close(fd);

    return 0;
}
