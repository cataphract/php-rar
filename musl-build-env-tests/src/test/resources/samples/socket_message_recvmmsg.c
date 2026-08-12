#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#if __SIZEOF_LONG__ <= 4
#  error "This test requires a 64-bit target"
#endif

union rights_control {
    struct cmsghdr alignment;
    unsigned char bytes[CMSG_SPACE(sizeof(int))];
};

extern const char *gnu_get_libc_version(void) __attribute__((weak));

static void test_recvmmsg(void);
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
    test_recvmmsg();
    puts("recvmmsg padding ok");
    return EXIT_SUCCESS;
}

int main(void)
{
    return socket_message_compat_run();
}

static void test_recvmmsg(void)
{
    int sockets[2];
    make_socket_pair(sockets);
    send_rights_message(sockets[0], 'c', sockets[0]);
    send_rights_message(sockets[0], 'd', sockets[0]);

    char payloads[2] = {0};
    struct iovec iov[2];
    struct mmsghdr messages[2];
    union rights_control controls[2];
    memset(messages, 0, sizeof(messages));
    for (size_t i = 0; i < 2; i++) {
        initialize_message(&messages[i].msg_hdr, &iov[i], &payloads[i], 1,
                           controls[i].bytes,
                           i == 0 ? sizeof(struct cmsghdr) : sizeof(controls[i]));
    }

    if (recvmmsg(sockets[1], messages, 2, MSG_WAITFORONE, NULL) != 2)
        fail("recvmmsg");
    for (size_t i = 0; i < 2; i++) {
        check(messages[i].msg_len == 1, "recvmmsg returned a wrong length");
        check(payloads[i] == (char)('c' + i),
              "recvmmsg payload was corrupted");
        check(messages[i].msg_hdr.__pad1 == 0 &&
                  messages[i].msg_hdr.__pad2 == 0,
              "recvmmsg did not clear msghdr padding");
        check_received_control_padding(&messages[i].msg_hdr);
    }
    check((messages[0].msg_hdr.msg_flags & MSG_CTRUNC) != 0,
          "recvmmsg did not report truncated control data");
    check_received_rights(&messages[1].msg_hdr);

    send_rights_message(sockets[0], 'e', sockets[0]);
    memset(messages, 0, sizeof(messages));
    for (size_t i = 0; i < 2; i++) {
        initialize_message(&messages[i].msg_hdr, &iov[i], &payloads[i], 1,
                           controls[i].bytes, sizeof(controls[i]));
    }
    struct timespec timeout = {.tv_sec = 1, .tv_nsec = 0};
    if (recvmmsg(sockets[1], messages, 2, MSG_WAITFORONE, &timeout) != 1)
        fail("recvmmsg timeout");
    check(payloads[0] == 'e', "recvmmsg timeout payload was corrupted");
    check(messages[0].msg_hdr.__pad1 == 0 &&
              messages[0].msg_hdr.__pad2 == 0,
          "recvmmsg timeout padding was not cleared");
    check_received_rights(&messages[0].msg_hdr);
    check(timeout.tv_sec >= 0 && timeout.tv_sec <= 1 &&
              timeout.tv_nsec >= 0 && timeout.tv_nsec < 1000000000L,
          "recvmmsg returned an invalid timeout");

    if (running_on_glibc()) {
        errno = 0;
        check(recvmmsg(-1, NULL, 1, MSG_DONTWAIT, NULL) == -1 &&
                  errno == EBADF,
              "recvmmsg did not reject a null vector");

        size_t invalid_count = IOV_MAX + 1U;
        struct mmsghdr *invalid_messages =
            calloc(invalid_count, sizeof(*invalid_messages));
        if (!invalid_messages)
            fail("calloc oversized recvmmsg vector");
        for (size_t i = 0; i < invalid_count; i++)
            dirty_message_padding(&invalid_messages[i].msg_hdr);
        errno = 0;
        check(recvmmsg(-1, invalid_messages, invalid_count, MSG_DONTWAIT,
                      NULL) == -1 && errno == EBADF,
              "recvmmsg rejected an oversized vector incorrectly");
        for (size_t i = 0; i < IOV_MAX; i++) {
            check(invalid_messages[i].msg_hdr.__pad1 == 0 &&
                      invalid_messages[i].msg_hdr.__pad2 == 0,
                  "recvmmsg did not translate the bounded vector");
        }
        check(invalid_messages[IOV_MAX].msg_hdr.__pad1 == INT_MIN &&
                  invalid_messages[IOV_MAX].msg_hdr.__pad2 == INT_MIN,
              "recvmmsg translated beyond the kernel vector limit");
        free(invalid_messages);
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
