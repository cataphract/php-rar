#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GUARD_SIZE 16
#define GUARD_BYTE UINT8_C(0xa5)

#if defined(__aarch64__) && __SIZEOF_POINTER__ == 8
_Static_assert(sizeof(pthread_mutexattr_t) == 8,
               "pthread_mutexattr_t must match the glibc aarch64 ABI");
_Static_assert(sizeof(pthread_condattr_t) == 8,
               "pthread_condattr_t must match the glibc aarch64 ABI");
_Static_assert(sizeof(pthread_barrierattr_t) == 8,
               "pthread_barrierattr_t must match the glibc aarch64 ABI");
_Static_assert(sizeof(pthread_attr_t) == 64,
               "pthread_attr_t must match the glibc aarch64 ABI");
_Static_assert(sizeof(pthread_mutex_t) == 48,
               "pthread_mutex_t must match the glibc aarch64 ABI");
#endif

struct guarded_attr {
    pthread_attr_t value;
    unsigned char guard[GUARD_SIZE];
};

struct guarded_mutex {
    pthread_mutex_t value;
    unsigned char guard[GUARD_SIZE];
};

struct guarded_mutexattr {
    pthread_mutexattr_t value;
    unsigned char guard[GUARD_SIZE];
};

struct guarded_condattr {
    pthread_condattr_t value;
    unsigned char guard[GUARD_SIZE];
};

struct guarded_barrierattr {
    pthread_barrierattr_t value;
    unsigned char guard[GUARD_SIZE];
};

static int guard_is_intact(const unsigned char guard[GUARD_SIZE])
{
    for (size_t i = 0; i < GUARD_SIZE; ++i) {
        if (guard[i] != GUARD_BYTE) {
            return 0;
        }
    }
    return 1;
}

static int test_thread_attr(void)
{
    struct guarded_attr object;
    memset(&object, GUARD_BYTE, sizeof(object));

    if (pthread_attr_init(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 1;
    }

    int detach_state = -1;
    if (pthread_attr_getdetachstate(&object.value, &detach_state) != 0 ||
        detach_state != PTHREAD_CREATE_JOINABLE ||
        pthread_attr_destroy(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 2;
    }
    return 0;
}

static int test_mutex(void)
{
    struct guarded_mutex object;
    memset(&object, GUARD_BYTE, sizeof(object));

    if (pthread_mutex_init(&object.value, NULL) != 0 ||
        !guard_is_intact(object.guard)) {
        return 1;
    }
    if (pthread_mutex_lock(&object.value) != 0 ||
        pthread_mutex_unlock(&object.value) != 0 ||
        pthread_mutex_destroy(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 2;
    }
    return 0;
}

static int test_mutex_attr(void)
{
    struct guarded_mutexattr object;
    memset(&object, GUARD_BYTE, sizeof(object));

    if (pthread_mutexattr_init(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 1;
    }
    if (pthread_mutexattr_settype(&object.value, PTHREAD_MUTEX_ERRORCHECK) != 0 ||
        pthread_mutexattr_destroy(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 2;
    }
    return 0;
}

static int test_cond_attr(void)
{
    struct guarded_condattr object;
    memset(&object, GUARD_BYTE, sizeof(object));

    if (pthread_condattr_init(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 1;
    }
    if (pthread_condattr_setpshared(&object.value, PTHREAD_PROCESS_PRIVATE) != 0 ||
        pthread_condattr_destroy(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 2;
    }
    return 0;
}

static int test_barrier_attr(void)
{
    struct guarded_barrierattr object;
    memset(&object, GUARD_BYTE, sizeof(object));

    if (pthread_barrierattr_init(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 1;
    }
    if (pthread_barrierattr_setpshared(&object.value, PTHREAD_PROCESS_PRIVATE) != 0 ||
        pthread_barrierattr_destroy(&object.value) != 0 ||
        !guard_is_intact(object.guard)) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int attr_result = test_thread_attr();
    int mutex_result = test_mutex();
    int mutexattr_result = test_mutex_attr();
    int condattr_result = test_cond_attr();
    int barrierattr_result = test_barrier_attr();

    if (attr_result != 0 || mutex_result != 0 || mutexattr_result != 0 ||
        condattr_result != 0 || barrierattr_result != 0) {
        fprintf(stderr,
                "pthread ABI failure: attr=%d mutex=%d mutexattr=%d "
                "condattr=%d barrierattr=%d\n",
                attr_result, mutex_result, mutexattr_result, condattr_result,
                barrierattr_result);
        return 1;
    }

    puts("pthread ABI accepted");
    return 0;
}
