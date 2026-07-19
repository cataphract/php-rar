#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern const char *gnu_get_libc_version(void) __attribute__((weak));
/* musl exports signgam from libc; provide it when this binary runs on glibc. */
int signgam;

static int running_on_glibc(void)
{
    return gnu_get_libc_version != NULL;
}

static void dirty_musl_padding(struct msghdr *message)
{
    if (running_on_glibc()) {
        return;
    }

    volatile unsigned char *bytes = (volatile unsigned char *)message;
    size_t begin = offsetof(struct msghdr, msg_iovlen) +
        sizeof(message->msg_iovlen);
    size_t end = offsetof(struct msghdr, msg_control);
    while (begin < end) {
        bytes[begin++] = 0x7f;
    }

    begin = offsetof(struct msghdr, msg_controllen) +
        sizeof(message->msg_controllen);
    end = offsetof(struct msghdr, msg_flags);
    while (begin < end) {
        bytes[begin++] = 0x7f;
    }
}

int main(void)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) != 0) {
        return 10;
    }

    char sent[] = "musl-msghdr";
    struct iovec send_iovec = {
        .iov_base = sent,
        .iov_len = sizeof(sent),
    };
    struct msghdr send_message = {
        .msg_iov = &send_iovec,
        .msg_iovlen = 1,
    };
    dirty_musl_padding(&send_message);
    if (sendmsg(sockets[0], &send_message, 0) != (ssize_t)sizeof(sent)) {
        return 11;
    }

    char received[sizeof(sent)] = {0};
    struct iovec receive_iovec = {
        .iov_base = received,
        .iov_len = sizeof(received),
    };
    struct msghdr receive_message = {
        .msg_iov = &receive_iovec,
        .msg_iovlen = 1,
    };
    dirty_musl_padding(&receive_message);
    if (recvmsg(sockets[1], &receive_message, 0) != (ssize_t)sizeof(sent)) {
        return 12;
    }
    if (memcmp(sent, received, sizeof(sent)) != 0) {
        return 13;
    }

    close(sockets[0]);
    close(sockets[1]);
    puts("asan-msghdr-ok");
    return 0;
}
