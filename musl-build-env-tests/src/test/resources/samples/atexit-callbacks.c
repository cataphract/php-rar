#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int callback_state;

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

static void first_callback(void)
{
    if (callback_state != 2) {
        _Exit(91);
    }
    callback_state = 3;
    write_message("first callback\n");
}

static void second_callback(void)
{
    if (callback_state != 1) {
        _Exit(92);
    }
    callback_state = 2;
    write_message("second callback\n");
}

int main(void)
{
    if (atexit(first_callback) != 0 || atexit(second_callback) != 0) {
        return 1;
    }
    callback_state = 1;
    write_message("main\n");
    return 0;
}
