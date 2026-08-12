#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(USE_ASAN)

__attribute__((noinline)) static void trigger_bug(void)
{
    char *allocation = malloc(32);
    if (!allocation)
        exit(10);
    volatile uintptr_t address = (uintptr_t)allocation;
    free(allocation);
    *(volatile char *)address = 1;
}

#elif defined(USE_LSAN)

const char *__lsan_default_options(void)
{
    return "use_stacks=0:use_registers=0:use_tls=0";
}

__attribute__((noinline)) static void trigger_bug(void)
{
    void *allocation = malloc(1234);
    if (!allocation)
        exit(10);
    memset(allocation, 0xa5, 1234);
    __asm__ __volatile__("" : : "r"(allocation) : "memory");
}

#elif defined(USE_MSAN)

__attribute__((noinline)) static void trigger_bug(void)
{
    int *allocation = malloc(sizeof(*allocation));
    if (!allocation)
        exit(10);
    volatile int uninitialized = *allocation;
    if (uninitialized)
        exit(11);
    free(allocation);
}

#elif defined(USE_UBSAN)

__attribute__((noinline)) static void trigger_bug(void)
{
    volatile int maximum = INT_MAX;
    volatile int one = 1;
    volatile int overflow = maximum + one;
    (void)overflow;
}

#else
#  error "Select one sanitizer detection case"
#endif

int main(void)
{
    trigger_bug();
    return 0;
}
