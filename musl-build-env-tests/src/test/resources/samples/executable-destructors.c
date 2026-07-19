#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int lifecycle_state;

static void write_message(const char *message)
{
    size_t remaining = strlen(message);
    while (remaining != 0) {
        ssize_t written = write(STDOUT_FILENO, message, remaining);
        if (written <= 0) {
            _Exit(90);
        }
        message += written;
        remaining -= (size_t)written;
    }
}

static void exit_callback(void)
{
    if (lifecycle_state != 1) {
        _Exit(91);
    }
    lifecycle_state = 2;
    write_message("atexit\n");
}

static void fini_one(void) __attribute__((destructor(101)));
static void fini_one(void)
{
    if (lifecycle_state != 3) {
        _Exit(92);
    }
    lifecycle_state = 4;
    write_message("fini one\n");
}

static void fini_two(void) __attribute__((destructor(102)));
static void fini_two(void)
{
    if (lifecycle_state != 2) {
        _Exit(93);
    }
    lifecycle_state = 3;
    write_message("fini two\n");
}

static void legacy_fini(void) __attribute__((used));
static void legacy_fini(void)
{
    if (lifecycle_state != 4) {
        _Exit(94);
    }
    lifecycle_state = 5;
    write_message("fini\n");
}

/*
 * crti.o starts _fini and crtn.o ends it. Insert a call between those two
 * fragments so the test also proves that legacy DT_FINI follows .fini_array
 * when the target CRT emits a DT_FINI entry.
 */
#if defined(__aarch64__)
__asm__(
    ".pushsection .fini,\"ax\",%progbits\n"
    "bl legacy_fini\n"
    ".popsection\n");
#elif defined(__x86_64__)
__asm__(
    ".pushsection .fini,\"ax\",@progbits\n"
    "call legacy_fini\n"
    ".popsection\n");
#else
#error Unsupported architecture
#endif

int main(void)
{
    if (atexit(exit_callback) != 0) {
        return 1;
    }
    lifecycle_state = 1;
    write_message("main\n");
    return 0;
}
