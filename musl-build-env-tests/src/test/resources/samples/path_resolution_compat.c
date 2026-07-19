#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char *resolved = realpath("/tmp/../tmp", NULL);

    if (resolved == NULL) {
        fprintf(stderr, "realpath failed: %s\n", strerror(errno));
        return 1;
    }
    if (strcmp(resolved, "/tmp") != 0) {
        fprintf(stderr, "realpath returned an unexpected path: %s\n", resolved);
        free(resolved);
        return 1;
    }

    free(resolved);
    puts("realpath ok");
    return 0;
}
