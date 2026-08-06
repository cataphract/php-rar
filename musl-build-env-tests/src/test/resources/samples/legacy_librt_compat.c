#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    char shared_memory_name[64];
    if (snprintf(shared_memory_name, sizeof(shared_memory_name),
                 "/musl-build-env-%ld", (long)getpid()) < 0) {
        return 1;
    }
    int shared_memory = shm_open(
        shared_memory_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shared_memory < 0) {
        return 2;
    }
    close(shared_memory);
    if (shm_unlink(shared_memory_name) != 0) {
        return 3;
    }
    puts("librt ok");
    return 0;
}
