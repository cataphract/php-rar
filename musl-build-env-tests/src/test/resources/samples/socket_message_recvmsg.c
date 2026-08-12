#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if __SIZEOF_LONG__ <= 4
#  error "This test requires a 64-bit target"
#endif

union rights_control {
    struct cmsghdr alignment;
    unsigned char bytes[CMSG_SPACE(sizeof(int))];
};

extern const char *gnu_get_libc_version(void) __attribute__((weak));

static void test_recvmsg(void);
static void send_rights_message(int socket, char payload, int fd);
static void check_received_rights(struct msghdr *message);
static void check_received_control_padding(struct msghdr *message);
static void initialize_rights_control(struct msghdr *message, void *control,
                                      size_t control_size, int fd);
static void initialize_message(struct msghdr *message, struct iovec *iov,
                               void *buffer, size_t buffer_size,
                               void *control, socklen_t control_size);
static void dirty_message_padding(struct msghdr *message);
static void make_socket_pair(int sockets[2]);
static void close_socket_pair(int sockets[2]);
static int running_on_glibc(void);
static void check(int condition, const char *message);
static void fail(const char *operation);

int socket_message_compat_run(void)
{
    test_recvmsg();
    puts("recvmsg padding ok");
    return EXIT_SUCCESS;
}

int main(void)
{
    return socket_message_compat_run();
}

static void test_recvmsg(void)
{
    int sockets[2];
    make_socket_pair(sockets);
    send_rights_message(sockets[0], 'r', sockets[0]);

    char payload = 0;
    union rights_control control;
    struct iovec iov;
    struct msghdr message;
    initialize_message(&message, &iov, &payload, sizeof(payload),
                       control.bytes, sizeof(control));

    if (recvmsg(sockets[1], &message, 0) != sizeof(payload))
        fail("recvmsg");
    check(payload == 'r', "recvmsg payload was corrupted");
    check(message.__pad1 == 0 && message.__pad2 == 0,
          "recvmsg did not clear msghdr padding");
    check_received_rights(&message);

    send_rights_message(sockets[0], 't', sockets[0]);
    initialize_message(&message, &iov, &payload, sizeof(payload),
                       control.bytes, sizeof(struct cmsghdr));
    if (recvmsg(sockets[1], &message, 0) != sizeof(payload))
        fail("recvmsg truncated control");
    check(payload == 't', "recvmsg truncated-control payload was corrupted");
    check((message.msg_flags & MSG_CTRUNC) != 0,
          "recvmsg did not report truncated control data");
    check(message.__pad1 == 0 && message.__pad2 == 0,
          "recvmsg truncated-control padding was not cleared");
    check_received_control_padding(&message);

    if (running_on_glibc()) {
        errno = 0;
        check(recvmsg(sockets[1], NULL, 0) == -1 && errno == EFAULT,
              "recvmsg did not reject a null message");
    }
    close_socket_pair(sockets);
}

static void send_rights_message(int socket, char payload, int fd)
{
    struct iovec iov;
    struct msghdr message;
    union rights_control control;

    initialize_message(&message, &iov, &payload, sizeof(payload),
                       control.bytes, sizeof(control));
    initialize_rights_control(&message, control.bytes, sizeof(control), fd);
    message.__pad1 = 0;
    message.__pad2 = 0;
    CMSG_FIRSTHDR(&message)->__pad1 = 0;

    if (sendmsg(socket, &message, 0) != sizeof(payload))
        fail("sendmsg fixture");
}

static void check_received_rights(struct msghdr *message)
{
    struct cmsghdr *control_message = CMSG_FIRSTHDR(message);
    check(control_message != NULL, "no received control message");
    check(control_message->cmsg_level == SOL_SOCKET,
          "received control message has the wrong level");
    check(control_message->cmsg_type == SCM_RIGHTS,
          "received control message has the wrong type");
    check(control_message->cmsg_len == CMSG_LEN(sizeof(int)),
          "received control message has the wrong length");
    check(control_message->__pad1 == 0,
          "received control message padding was not cleared");

    int received_fd;
    memcpy(&received_fd, CMSG_DATA(control_message), sizeof(received_fd));
    if (close(received_fd) != 0)
        fail("close received descriptor");
}

static void check_received_control_padding(struct msghdr *message)
{
    for (struct cmsghdr *control = CMSG_FIRSTHDR(message);
         control; control = CMSG_NXTHDR(message, control)) {
        check(control->__pad1 == 0,
              "received control message padding was not cleared");
    }
}

static void initialize_rights_control(struct msghdr *message, void *control,
                                      size_t control_size, int fd)
{
    memset(control, 0xa5, control_size);
    message->msg_control = control;
    message->msg_controllen = control_size;

    struct cmsghdr *control_message = CMSG_FIRSTHDR(message);
    check(control_message != NULL, "CMSG_FIRSTHDR returned NULL");
    control_message->cmsg_len = CMSG_LEN(sizeof(fd));
    control_message->cmsg_level = SOL_SOCKET;
    control_message->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(control_message), &fd, sizeof(fd));
    control_message->__pad1 = INT_MIN;
}

static void initialize_message(struct msghdr *message, struct iovec *iov,
                               void *buffer, size_t buffer_size,
                               void *control, socklen_t control_size)
{
    memset(message, 0, sizeof(*message));
    iov->iov_base = buffer;
    iov->iov_len = buffer_size;
    message->msg_iov = iov;
    message->msg_iovlen = 1;
    message->msg_control = control;
    message->msg_controllen = control_size;
    dirty_message_padding(message);
}

static void dirty_message_padding(struct msghdr *message)
{
    message->__pad1 = INT_MIN;
    message->__pad2 = INT_MIN;
}

static void make_socket_pair(int sockets[2])
{
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) != 0)
        fail("socketpair");
}

static void close_socket_pair(int sockets[2])
{
    if (close(sockets[0]) != 0)
        fail("close sender");
    if (close(sockets[1]) != 0)
        fail("close receiver");
}

static int running_on_glibc(void)
{
    return gnu_get_libc_version != NULL;
}

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void fail(const char *operation)
{
    perror(operation);
    exit(EXIT_FAILURE);
}
