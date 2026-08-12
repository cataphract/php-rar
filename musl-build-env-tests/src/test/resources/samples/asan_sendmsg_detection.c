#include <sanitizer/asan_interface.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) != 0)
        return 10;

    char payload = 'x';
    struct iovec iov = {
        .iov_base = &payload,
        .iov_len = sizeof(payload),
    };
    struct msghdr message;
    memset(&message, 0, sizeof(message));
    message.msg_iov = &iov;
    message.msg_iovlen = 1;

    __asan_poison_memory_region(&payload, sizeof(payload));
    ssize_t result = sendmsg(sockets[0], &message, 0);
    __asan_unpoison_memory_region(&payload, sizeof(payload));
    close(sockets[0]);
    close(sockets[1]);
    return result < 0 ? 11 : 0;
}
