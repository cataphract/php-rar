#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <unistd.h>

static int test_strerror_r(void)
{
    char buffer[128] = {0};
    int result = strerror_r(EINVAL, buffer, sizeof(buffer));

    if (result != 0 || buffer[0] == '\0') {
        fprintf(stderr, "strerror_r failed: result=%d message=%s\n", result,
                buffer);
        return 1;
    }
    return 0;
}

static int test_getrandom(void)
{
    unsigned char random_bytes[64];
    ssize_t result = getrandom(random_bytes, sizeof(random_bytes), 0);

    if (result != (ssize_t)sizeof(random_bytes)) {
        fprintf(stderr, "getrandom failed: result=%zd errno=%d\n", result,
                errno);
        return 1;
    }
    return 0;
}

static int test_memfd_create(void)
{
    char contents[7] = {0};
    int fd = memfd_create("compat-test", MFD_CLOEXEC);

    if (fd < 0) {
        fprintf(stderr, "memfd_create failed: %s\n", strerror(errno));
        return 1;
    }
    if ((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0 ||
        write(fd, "compat", 6) != 6 ||
        lseek(fd, 0, SEEK_SET) != 0 ||
        read(fd, contents, 6) != 6 ||
        strcmp(contents, "compat") != 0) {
        fprintf(stderr, "memfd did not behave as expected: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int main(void)
{
    if (test_strerror_r() || test_getrandom() || test_memfd_create())
        return 1;

    puts("strerror_r getrandom memfd_create ok");
    return 0;
}
