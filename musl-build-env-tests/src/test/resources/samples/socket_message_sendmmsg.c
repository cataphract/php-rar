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

static void test_sendmmsg(void);
static void initialize_rights_control(struct msghdr *message, void *control,
                                      size_t control_size, int fd);
static void initialize_message(struct msghdr *message, struct iovec *iov,
                               void *buffer, size_t buffer_size,
                               void *control, socklen_t control_size);
static void dirty_message_padding(struct msghdr *message);
static void make_socket_pair(int sockets[2]);
static void close_socket_pair(int sockets[2]);
static void check(int condition, const char *message);
static void fail(const char *operation);

int socket_message_compat_run(void)
{
    test_sendmmsg();
    puts("sendmmsg padding ok");
    return EXIT_SUCCESS;
}

int main(void)
{
    return socket_message_compat_run();
}

static void test_sendmmsg(void)
{
    int sockets[2];
    make_socket_pair(sockets);

    char payloads[] = {'a', 'b'};
    struct iovec iov[2];
    struct mmsghdr messages[2];
    union rights_control controls[2];
    memset(messages, 0, sizeof(messages));
    for (size_t i = 0; i < 2; i++) {
        initialize_message(&messages[i].msg_hdr, &iov[i], &payloads[i], 1,
                           controls[i].bytes, sizeof(controls[i]));
        initialize_rights_control(&messages[i].msg_hdr, controls[i].bytes,
                                  sizeof(controls[i]), sockets[0]);
    }

    if (sendmmsg(sockets[0], messages, 2, 0) != 2)
        fail("sendmmsg");
    for (size_t i = 0; i < 2; i++) {
        check(messages[i].msg_len == 1, "sendmmsg returned a wrong length");
        check(messages[i].msg_hdr.__pad1 == INT_MIN &&
                  messages[i].msg_hdr.__pad2 == INT_MIN,
              "sendmmsg modified msghdr padding");
        check(CMSG_FIRSTHDR(&messages[i].msg_hdr)->__pad1 == INT_MIN,
              "sendmmsg modified cmsghdr padding");

        char received = 0;
        if (recv(sockets[1], &received, sizeof(received), 0) != 1)
            fail("recv sendmmsg payload");
        check(received == payloads[i], "sendmmsg payload was corrupted");
    }

    memset(messages, 0, sizeof(messages));
    initialize_message(&messages[0].msg_hdr, &iov[0], &payloads[0], 1,
                       NULL, 0);
    messages[1].msg_hdr.msg_iov = NULL;
    messages[1].msg_hdr.msg_iovlen = 1;
    dirty_message_padding(&messages[1].msg_hdr);
    messages[1].msg_len = UINT_MAX;
    check(sendmmsg(sockets[0], messages, 2, 0) == 1,
          "sendmmsg did not report a partial send");
    check(messages[0].msg_len == 1 && messages[1].msg_len == UINT_MAX,
          "sendmmsg changed lengths after a partial send");
    char received = 0;
    if (recv(sockets[1], &received, sizeof(received), 0) != 1)
        fail("recv partial sendmmsg payload");
    check(received == payloads[0], "partial sendmmsg payload was corrupted");

    check(sendmmsg(sockets[0], NULL, 0, 0) == 0,
          "sendmmsg rejected an empty null vector");
    errno = 0;
    check(sendmmsg(sockets[0], NULL, 1, 0) == -1 && errno == EFAULT,
          "sendmmsg did not reject a null vector");

    struct mmsghdr invalid_message = {0};
    errno = 0;
    check(sendmmsg(-1, &invalid_message, IOV_MAX + 1U, 0) == -1 &&
              errno == EBADF,
          "sendmmsg did not clamp an oversized vector");
    close_socket_pair(sockets);
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
