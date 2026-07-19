#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static sigjmp_buf jump_environment;

__attribute__((noinline))
static void jump_from_nested_frame(int depth)
{
    volatile unsigned char stack_noise[64];
    memset((void *)stack_noise, depth, sizeof(stack_noise));
    if (depth == 0) {
        siglongjmp(jump_environment, 73);
    }
    jump_from_nested_frame(depth - 1);
}

int main(void)
{
    sigset_t blocked;
    sigset_t current;
    char marker[] = "unchanged";

    if (sigemptyset(&blocked) != 0 || sigaddset(&blocked, SIGUSR1) != 0 ||
        sigprocmask(SIG_BLOCK, &blocked, NULL) != 0) {
        return 1;
    }

    int jump_value = sigsetjmp(jump_environment, 1);
    if (jump_value == 0) {
        if (sigprocmask(SIG_UNBLOCK, &blocked, NULL) != 0) {
            return 2;
        }
        jump_from_nested_frame(6);
        return 3;
    }

    if (jump_value != 73 || strcmp(marker, "unchanged") != 0 ||
        sigprocmask(SIG_SETMASK, NULL, &current) != 0 ||
        sigismember(&current, SIGUSR1) != 1) {
        return 4;
    }

    printf("sigsetjmp restored value=%d marker=%s mask=blocked\n",
           jump_value, marker);
    return 0;
}
