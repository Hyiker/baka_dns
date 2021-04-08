#include <string.h>

#include "message.h"
#include "server.h"

#define UDP_PORT 53
int recv_handle(uint8_t* buf, uint32_t size) {
    printf("Client data of size %u recv\n", size);
    struct message msg;
    message_from_buf(buf, size, &msg);
    return 1;
}
int send_handle(uint8_t* buf, uint32_t* size) {
    const char* echo = "hello!";
    strncpy(buf, echo, sizeof(echo));
    *size = sizeof(echo);
    return 1;
}
int main(int argc, char const* argv[]) {
    int fd = create_socket(INADDR_ANY, UDP_PORT);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }

    listen_socket(fd, recv_handle, send_handle);
    return 0;
}
