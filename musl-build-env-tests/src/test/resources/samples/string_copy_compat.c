#define _BSD_SOURCE

#include <stdio.h>
#include <string.h>

int main(void)
{
    char copy_truncated[5];
    char copy_complete[16];
    char concatenated[6] = "ab";
    char unterminated[4] = {'a', 'b', 'c', 'd'};

    if (strlcpy(copy_truncated, "abcdef", sizeof(copy_truncated)) != 6 ||
        strcmp(copy_truncated, "abcd") != 0) {
        fputs("strlcpy truncation behavior was incorrect\n", stderr);
        return 1;
    }
    if (strlcpy(copy_complete, "cat", sizeof(copy_complete)) != 3 ||
        strcmp(copy_complete, "cat") != 0) {
        fputs("strlcpy copy behavior was incorrect\n", stderr);
        return 1;
    }
    if (strlcat(concatenated, "cdefgh", sizeof(concatenated)) != 8 ||
        strcmp(concatenated, "abcde") != 0) {
        fputs("strlcat truncation behavior was incorrect\n", stderr);
        return 1;
    }
    if (strlcat(unterminated, "xy", sizeof(unterminated)) != 6 ||
        memcmp(unterminated, "abcd", sizeof(unterminated)) != 0) {
        fputs("strlcat unterminated-destination behavior was incorrect\n",
              stderr);
        return 1;
    }

    puts("strlcpy strlcat ok");
    return 0;
}
