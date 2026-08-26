#define _GNU_SOURCE

#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <pty.h>
#include <stdio.h>
#include <sys/mman.h>

static void *return_argument(void *argument);

int main(void)
{
    volatile double value = 1.25;
    int thread_value = 42;
    pthread_t thread;
    void *thread_result;
    void *handle = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    int terminal_master;
    int terminal_slave;

    if (pthread_create(&thread, NULL, return_argument, &thread_value) != 0 ||
        pthread_join(thread, &thread_result) != 0 ||
        thread_result != &thread_value || ceil(value) != 2.0 || handle == NULL ||
        shm_unlink("/musl-build-env-unused") == 0 ||
        openpty(&terminal_master, &terminal_slave, NULL, NULL, NULL) != 0) {
        return 1;
    }
    fclose(fdopen(terminal_master, "r"));
    fclose(fdopen(terminal_slave, "r"));
    dlclose(handle);
    puts("multi-library ok");
    return 0;
}

static void *return_argument(void *argument)
{
    return argument;
}
