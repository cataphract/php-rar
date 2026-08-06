#include <dlfcn.h>
#include <stdio.h>

typedef int (*check_shared_math_fn)(void);

int main(int argc, char **argv)
{
    if (argc != 2) {
        fputs("expected a shared library path\n", stderr);
        return 1;
    }

    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }

    check_shared_math_fn check_shared_math =
        (check_shared_math_fn)dlsym(handle, "check_shared_math");
    if (!check_shared_math || !check_shared_math()) {
        fputs("shared math check failed\n", stderr);
        return 1;
    }

    puts("shared ceil ceilf ok");
    return 0;
}
