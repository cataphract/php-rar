#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int report_pthread_error(const char *operation, int error)
{
    fprintf(stderr, "%s failed: %s\n", operation, strerror(error));
    return 1;
}

int main(void)
{
    pthread_condattr_t attributes;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
    struct timespec deadline;
    int error;

    error = pthread_condattr_init(&attributes);
    if (error != 0)
        return report_pthread_error("pthread_condattr_init", error);
    error = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (error != 0) {
        pthread_condattr_destroy(&attributes);
        return report_pthread_error("pthread_condattr_setclock", error);
    }
    error = pthread_cond_init(&condition, &attributes);
    pthread_condattr_destroy(&attributes);
    if (error != 0)
        return report_pthread_error("pthread_cond_init", error);

    error = pthread_mutex_init(&mutex, NULL);
    if (error != 0) {
        pthread_cond_destroy(&condition);
        return report_pthread_error("pthread_mutex_init", error);
    }
    error = pthread_mutex_lock(&mutex);
    if (error != 0) {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condition);
        return report_pthread_error("pthread_mutex_lock", error);
    }
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
        pthread_mutex_unlock(&mutex);
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condition);
        return 1;
    }
    deadline.tv_nsec += 10 * 1000 * 1000;
    if (deadline.tv_nsec >= 1000 * 1000 * 1000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000 * 1000 * 1000;
    }

    error = pthread_cond_timedwait(&condition, &mutex, &deadline);
    if (error != ETIMEDOUT) {
        if (error == 0)
            fputs("pthread_cond_timedwait unexpectedly succeeded\n", stderr);
        else
            report_pthread_error("pthread_cond_timedwait", error);
        pthread_mutex_unlock(&mutex);
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condition);
        return 1;
    }

    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condition);
    puts("pthread_cond_init ok");
    return 0;
}
