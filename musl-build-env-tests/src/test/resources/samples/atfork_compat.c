#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t prepare_called;
static volatile sig_atomic_t parent_called;
static volatile sig_atomic_t child_called;

static void prepare_handler(void)
{
    prepare_called = 1;
}

static void parent_handler(void)
{
    parent_called = 1;
}

static void child_handler(void)
{
    child_called = 1;
}

int main(void)
{
    int status;
    pid_t child;
    int error = pthread_atfork(prepare_handler, parent_handler, child_handler);

    if (error != 0) {
        fprintf(stderr, "pthread_atfork failed: %d\n", error);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        if (prepare_called != 1 || parent_called != 0 || child_called != 1)
            _exit(2);
        _exit(0);
    }

    if (prepare_called != 1 || parent_called != 1 || child_called != 0) {
        fputs("atfork handlers ran in the wrong process\n", stderr);
        return 1;
    }
    if (waitpid(child, &status, 0) != child) {
        perror("waitpid");
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "child handler check failed with status %d\n", status);
        return 1;
    }

    puts("pthread_atfork ok");
    return 0;
}
