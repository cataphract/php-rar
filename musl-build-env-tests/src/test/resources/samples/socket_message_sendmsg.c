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

enum {
    LARGE_RIGHTS_FD_COUNT = 253,
};

union large_control {
    struct cmsghdr alignment;
    unsigned char bytes[
        CMSG_SPACE(LARGE_RIGHTS_FD_COUNT * sizeof(int)) +
        CMSG_SPACE(sizeof(struct ucred))];
};

extern const char *gnu_get_libc_version(void) __attribute__((weak));

static void test_sendmsg(void);
static void initialize_rights_control(struct msghdr *message, void *control,
                                      size_t control_size, int fd);
static void initialize_large_control(struct msghdr *message,
                                     union large_control *control, int fd);
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
    test_sendmsg();
    puts("sendmsg padding ok");
    return EXIT_SUCCESS;
}

int main(void)
{
    return socket_message_compat_run();
}

static void test_sendmsg(void)
{
    int sockets[2];
    make_socket_pair(sockets);

    char payload = 's';
    struct iovec iov;
    struct msghdr message;
    union rights_control control;
    /* Struct assignment need not copy padding, so compare byte snapshots. */
    unsigned char original_message[sizeof(message)];
    unsigned char original_control[sizeof(control)];
    initialize_message(&message, &iov, &payload, sizeof(payload),
                       control.bytes, sizeof(control));
    initialize_rights_control(&message, control.bytes, sizeof(control),
                              sockets[0]);
    memcpy(original_message, &message, sizeof(original_message));
    memcpy(original_control, &control, sizeof(original_control));

    if (sendmsg(sockets[0], &message, 0) != sizeof(payload))
        fail("sendmsg");
    check(memcmp(&message, original_message, sizeof(original_message)) == 0,
          "sendmsg modified its const msghdr input");
    check(memcmp(&control, original_control, sizeof(original_control)) == 0,
          "sendmsg modified its const control input");

    char received = 0;
    if (recv(sockets[1], &received, sizeof(received), 0) != sizeof(received))
        fail("recv sendmsg payload");
    check(received == payload, "sendmsg payload was corrupted");

    initialize_message(&message, &iov, &payload, sizeof(payload),
                       control.bytes, sizeof(control));
    initialize_rights_control(&message, control.bytes, sizeof(control),
                              sockets[0]);
    CMSG_FIRSTHDR(&message)->cmsg_len = 1;
    memcpy(original_message, &message, sizeof(original_message));
    memcpy(original_control, &control, sizeof(original_control));
    errno = 0;
    check(sendmsg(sockets[0], &message, 0) == -1 && errno == EINVAL,
          "sendmsg accepted a malformed control message");
    check(memcmp(&message, original_message, sizeof(original_message)) == 0,
          "sendmsg modified a malformed msghdr input");
    check(memcmp(&control, original_control, sizeof(original_control)) == 0,
          "sendmsg modified a malformed control input");

    /* Native musl differs on the boundary cases below; verify that the glibc
     * compatibility path preserves glibc's behavior. */
    if (running_on_glibc()) {
        initialize_message(&message, &iov, &payload, sizeof(payload), NULL, 1);
        errno = 0;
        check(sendmsg(sockets[0], &message, 0) == -1 && errno == EFAULT,
              "sendmsg did not reject a null control buffer");

        int pass_credentials = 1;
        if (setsockopt(sockets[1], SOL_SOCKET, SO_PASSCRED,
                       &pass_credentials, sizeof(pass_credentials)) != 0)
            fail("setsockopt SO_PASSCRED");

        union large_control large_control;
        unsigned char original_large_control[sizeof(large_control)];
        payload = 'l';
        initialize_message(&message, &iov, &payload, sizeof(payload),
                           large_control.bytes, sizeof(large_control));
        initialize_large_control(&message, &large_control, sockets[0]);
        memcpy(original_message, &message, sizeof(original_message));
        memcpy(original_large_control, &large_control,
               sizeof(original_large_control));
        if (sendmsg(sockets[0], &message, 0) != sizeof(payload))
            fail("sendmsg large control data");
        check(memcmp(&message, original_message,
                     sizeof(original_message)) == 0,
              "sendmsg modified a large msghdr input");
        check(memcmp(&large_control, original_large_control,
                     sizeof(original_large_control)) == 0,
              "sendmsg modified large control input");
        received = 0;
        if (recv(sockets[1], &received, sizeof(received), 0) !=
                sizeof(received))
            fail("recv large-control sendmsg payload");
        check(received == payload,
              "large-control sendmsg payload was corrupted");
    }

    errno = 0;
    check(sendmsg(sockets[0], NULL, 0) == -1 && errno == EFAULT,
          "sendmsg did not reject a null message");
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

static void initialize_large_control(struct msghdr *message,
                                     union large_control *control, int fd)
{
    memset(control, 0xa5, sizeof(*control));
    message->msg_control = control->bytes;
    message->msg_controllen = sizeof(*control);

    struct cmsghdr *rights = CMSG_FIRSTHDR(message);
    check(rights != NULL, "CMSG_FIRSTHDR returned NULL for large control");
    rights->cmsg_len = CMSG_LEN(LARGE_RIGHTS_FD_COUNT * sizeof(int));
    rights->cmsg_level = SOL_SOCKET;
    rights->cmsg_type = SCM_RIGHTS;
    int *fds = (int *)CMSG_DATA(rights);
    for (size_t i = 0; i < LARGE_RIGHTS_FD_COUNT; i++)
        fds[i] = fd;
    rights->__pad1 = INT_MIN;

    struct cmsghdr *credentials = (struct cmsghdr *)(
        control->bytes + CMSG_SPACE(LARGE_RIGHTS_FD_COUNT * sizeof(int)));
    credentials->cmsg_len = CMSG_LEN(sizeof(struct ucred));
    credentials->cmsg_level = SOL_SOCKET;
    credentials->cmsg_type = SCM_CREDENTIALS;
    struct ucred *ucred = (struct ucred *)CMSG_DATA(credentials);
    ucred->pid = getpid();
    ucred->uid = getuid();
    ucred->gid = getgid();
    credentials->__pad1 = INT_MIN;
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
