#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* musl exports signgam from libc; provide it when this binary runs on glibc. */
int signgam;

/* Keep the check focused on roots reachable through pthread-specific data. */
const char *__lsan_default_options(void)
{
    return "use_stacks=0:use_registers=0:use_tls=1";
}

int __lsan_do_recoverable_leak_check(void);

typedef int (*touch_tls_fn)(void);

static pthread_key_t allocation_keys[PTHREAD_KEYS_MAX];
static size_t allocation_key_count;
static atomic_int worker_ready;
static atomic_int worker_failed;
static atomic_int module_ready;
static atomic_int module_failed;
static atomic_int worker_finished_exercise;
static atomic_int release_worker;
static touch_tls_fn touch_tls;

static void reset_worker_state(int module_is_ready)
{
    atomic_store_explicit(&worker_ready, 0, memory_order_relaxed);
    atomic_store_explicit(&worker_failed, 0, memory_order_relaxed);
    atomic_store_explicit(&module_ready, module_is_ready, memory_order_relaxed);
    atomic_store_explicit(&module_failed, 0, memory_order_relaxed);
    atomic_store_explicit(
        &worker_finished_exercise, 0, memory_order_relaxed);
    atomic_store_explicit(&release_worker, 0, memory_order_relaxed);
}

static void *touch_tls_in_nested_thread(void *unused)
{
    (void)unused;
    return touch_tls() == 1 ? NULL : (void *)3;
}

static void *hold_allocation_in_pthread_specific_data(void *unused)
{
    pthread_t nested_worker;
    void *nested_result;
    void *result = NULL;
    void *allocation = malloc(1234);

    (void)unused;
    if (allocation == NULL) {
        atomic_store_explicit(&worker_failed, 1, memory_order_relaxed);
        atomic_store_explicit(&worker_ready, 1, memory_order_release);
        atomic_store_explicit(
            &worker_finished_exercise, 1, memory_order_release);
        return (void *)1;
    }
    if (pthread_setspecific(
            allocation_keys[allocation_key_count - 1], allocation) != 0) {
        free(allocation);
        atomic_store_explicit(&worker_failed, 1, memory_order_relaxed);
        atomic_store_explicit(&worker_ready, 1, memory_order_release);
        atomic_store_explicit(
            &worker_finished_exercise, 1, memory_order_release);
        return (void *)2;
    }

    allocation = NULL;
    atomic_store_explicit(&worker_ready, 1, memory_order_release);
    while (!atomic_load_explicit(&module_ready, memory_order_acquire)) {
        sched_yield();
    }

    if (atomic_load_explicit(&module_failed, memory_order_relaxed)) {
        atomic_store_explicit(
            &worker_finished_exercise, 1, memory_order_release);
        return (void *)3;
    }

    if (touch_tls() != 1 ||
        pthread_create(&nested_worker, NULL,
                       touch_tls_in_nested_thread, NULL) != 0 ||
        pthread_join(nested_worker, &nested_result) != 0 ||
        nested_result != NULL) {
        result = (void *)4;
    }

    atomic_store_explicit(
        &worker_finished_exercise, 1, memory_order_release);
    while (!atomic_load_explicit(&release_worker, memory_order_acquire)) {
        sched_yield();
    }
    return result;
}

static int create_pthread_keys(void)
{
    const size_t key_limit = PTHREAD_KEYS_MAX - 8;

    while (allocation_key_count < key_limit &&
           pthread_key_create(
               &allocation_keys[allocation_key_count], free) == 0) {
        allocation_key_count++;
    }
    return allocation_key_count == key_limit ? 0 : -1;
}

static void delete_pthread_keys(void)
{
    for (size_t index = 0; index < allocation_key_count; index++) {
        pthread_key_delete(allocation_keys[index]);
    }
}

static int install_main_thread_root(void)
{
    void *allocation = malloc(2345);

    if (allocation == NULL) {
        return -1;
    }
    if (pthread_setspecific(
            allocation_keys[allocation_key_count - 1], allocation) != 0) {
        free(allocation);
        return -1;
    }
    return 0;
}

static void release_main_thread_root(void)
{
    pthread_key_t key = allocation_keys[allocation_key_count - 1];
    void *allocation = pthread_getspecific(key);

    pthread_setspecific(key, NULL);
    free(allocation);
}

static int exercise_worker(pthread_attr_t *attributes, int *leaks)
{
    pthread_t worker;
    void *worker_result;

    reset_worker_state(1);
    if (pthread_create(&worker, attributes,
                       hold_allocation_in_pthread_specific_data, NULL) != 0) {
        return 1;
    }
    while (!atomic_load_explicit(&worker_ready, memory_order_acquire)) {
        sched_yield();
    }
    if (atomic_load_explicit(&worker_failed, memory_order_relaxed)) {
        pthread_join(worker, NULL);
        return 2;
    }
    while (!atomic_load_explicit(
               &worker_finished_exercise, memory_order_acquire)) {
        sched_yield();
    }

    *leaks |= __lsan_do_recoverable_leak_check();

    atomic_store_explicit(&release_worker, 1, memory_order_release);
    if (pthread_join(worker, &worker_result) != 0 || worker_result != NULL) {
        return 3;
    }
    return 0;
}

int main(int argument_count, char **arguments)
{
    pthread_t worker;
    void *worker_result;
    void *module;
    void *symbol;
    void *custom_stack_mapping;
    size_t custom_stack_size = 1024 * 1024;
    size_t custom_mapping_size;
    long page_size;
    pthread_attr_t zero_guard_attributes;
    pthread_attr_t custom_stack_attributes;
    int exercise_result;
    int leaks;

    if (argument_count != 2 || create_pthread_keys() != 0 ||
        install_main_thread_root() != 0) {
        return 10;
    }
    reset_worker_state(0);
    if (pthread_create(&worker, NULL,
                       hold_allocation_in_pthread_specific_data, NULL) != 0) {
        delete_pthread_keys();
        return 11;
    }
    while (!atomic_load_explicit(&worker_ready, memory_order_acquire)) {
        sched_yield();
    }
    if (atomic_load_explicit(&worker_failed, memory_order_relaxed)) {
        pthread_join(worker, NULL);
        delete_pthread_keys();
        return 12;
    }

    module = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
    symbol = module != NULL ? dlsym(module, "touch_dynamically_loaded_tls") : NULL;
    if (symbol == NULL) {
        atomic_store_explicit(&module_failed, 1, memory_order_relaxed);
        atomic_store_explicit(&module_ready, 1, memory_order_release);
        pthread_join(worker, NULL);
        delete_pthread_keys();
        return 13;
    }
    memcpy(&touch_tls, &symbol, sizeof(touch_tls));
    atomic_store_explicit(&module_ready, 1, memory_order_release);
    while (!atomic_load_explicit(
               &worker_finished_exercise, memory_order_acquire)) {
        sched_yield();
    }

    leaks = __lsan_do_recoverable_leak_check();

    atomic_store_explicit(&release_worker, 1, memory_order_release);
    if (pthread_join(worker, &worker_result) != 0 || worker_result != NULL) {
        return 14;
    }

    if (pthread_attr_init(&zero_guard_attributes) != 0) {
        return 15;
    }
    if (pthread_attr_setguardsize(&zero_guard_attributes, 0) != 0) {
        pthread_attr_destroy(&zero_guard_attributes);
        return 15;
    }
    exercise_result = exercise_worker(&zero_guard_attributes, &leaks);
    pthread_attr_destroy(&zero_guard_attributes);
    if (exercise_result != 0) {
        return 16 + exercise_result;
    }

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 ||
        custom_stack_size > SIZE_MAX - (size_t)page_size) {
        return 20;
    }
    custom_mapping_size = custom_stack_size + (size_t)page_size;
    custom_stack_mapping = mmap(NULL, custom_mapping_size,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (custom_stack_mapping == MAP_FAILED ||
        mprotect((char *)custom_stack_mapping + custom_stack_size,
                 (size_t)page_size, PROT_NONE) != 0) {
        if (custom_stack_mapping != MAP_FAILED) {
            munmap(custom_stack_mapping, custom_mapping_size);
        }
        return 21;
    }
    if (pthread_attr_init(&custom_stack_attributes) != 0) {
        munmap(custom_stack_mapping, custom_mapping_size);
        return 22;
    }
    if (pthread_attr_setstack(&custom_stack_attributes,
                              custom_stack_mapping,
                              custom_stack_size) != 0) {
        pthread_attr_destroy(&custom_stack_attributes);
        munmap(custom_stack_mapping, custom_mapping_size);
        return 23;
    }

    exercise_result = exercise_worker(&custom_stack_attributes, &leaks);
    pthread_attr_destroy(&custom_stack_attributes);
    munmap(custom_stack_mapping, custom_mapping_size);
    if (exercise_result != 0) {
        return 23 + exercise_result;
    }
    release_main_thread_root();
    delete_pthread_keys();
    dlclose(module);

    if (leaks != 0) {
        fprintf(stderr,
                "LSan reported an allocation reachable from pthread-specific data\n");
        return 27;
    }

    puts("lsan-pthread-specific-ok");
    return 0;
}
