#include <string.h>

#include "socket/resolv.h"
#include "socket/server.h"
#include "socket/socket.h"
#include "storage/cache.h"
#include "storage/database.h"
#include "storage/message.h"
#include "time.h"
#include "utils/conf.h"
#include "utils/logging.h"
#include "utils/threadpool.h"

void init_storage(int argc, char* argv[]) {
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
    init_cache();
    if (init_threadpool() < 0) {
        exit(EXIT_FAILURE);
    }
}
int main(int argc, char* argv[]) {
    init_storage(argc, argv);
    pthread_t thread_udp;
    if (start_udp_server(&thread_udp) < 0) {
        LOG_ERR("failed firing up udp server!\n");
        exit(EXIT_FAILURE);
    }
    if (conf.dot_enable) {
        pthread_t thread_tls;
        if (start_tls_server(&thread_tls) < 0) {
            LOG_ERR("failed firing up tls server!\n");
            exit(EXIT_FAILURE);
        }
        pthread_join(thread_tls, NULL);
    }
    pthread_join(thread_udp, NULL);

    return 0;
}
