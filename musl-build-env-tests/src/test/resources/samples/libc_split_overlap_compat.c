#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

static ssize_t (*const call_read)(int, void *, size_t) = read;
static void (*const call_flockfile)(FILE *) = flockfile;
static void (*const call_funlockfile)(FILE *) = funlockfile;
static double (*const call_frexp)(double, int *) = frexp;
static double (*const call_copysign)(double, double) = copysign;

int main(void)
{
    char byte;
    int exponent;

    errno = 0;
    if (call_read(-1, &byte, sizeof(byte)) != -1 || errno != EBADF) {
        return 1;
    }

    call_flockfile(stdout);
    call_funlockfile(stdout);

    double fraction = call_frexp(8.0, &exponent);
    if (call_copysign(fraction, -1.0) != -0.5 || exponent != 4) {
        return 2;
    }

    puts("libc overlap ok");
    return 0;
}
