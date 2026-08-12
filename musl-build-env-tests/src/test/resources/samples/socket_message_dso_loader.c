#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s library\n", argv[0]);
        return EXIT_FAILURE;
    }

    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    dlerror();
    int (*run)(void) =
        (int (*)(void))dlsym(module, "socket_message_compat_run");
    const char *error = dlerror();
    if (error) {
        fprintf(stderr, "dlsym: %s\n", error);
        return EXIT_FAILURE;
    }

    int result = run();
    if (dlclose(module) != 0) {
        fprintf(stderr, "dlclose: %s\n", dlerror());
        return EXIT_FAILURE;
    }
    return result;
}
