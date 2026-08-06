#include <pthread.h>
#include <stdio.h>

static void *return_argument(void *argument);

int main(void)
{
    int thread_value = 42;
    pthread_t thread;
    void *thread_result;
    if (pthread_create(&thread, NULL, return_argument, &thread_value) != 0 ||
        pthread_join(thread, &thread_result) != 0 ||
        thread_result != &thread_value) {
        return 1;
    }
    puts("libpthread ok");
    return 0;
}

static void *return_argument(void *argument)
{
    return argument;
}
