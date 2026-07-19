#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <sys/resource.h>

/* musl exports signgam from libc; provide it when this binary runs on glibc. */
int signgam;

static int limit_is_valid(const struct rlimit *limit)
{
    return limit->rlim_cur == RLIM_INFINITY ||
        limit->rlim_max == RLIM_INFINITY ||
        limit->rlim_cur <= limit->rlim_max;
}

int main(void)
{
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0 || !limit_is_valid(&limit)) {
        return 10;
    }

    puts("msan-rlimit64-ok");
    return 0;
}
