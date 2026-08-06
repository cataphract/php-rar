#define _GNU_SOURCE

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    void *self = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    if (self == NULL || dlclose(self) != 0) {
        return 1;
    }
    puts("libdl ok");
    return 0;
}
