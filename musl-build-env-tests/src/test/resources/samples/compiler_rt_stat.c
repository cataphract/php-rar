#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int metadata_is_valid(const struct stat *metadata)
{
    volatile uint64_t initialized_values =
        (uint64_t)metadata->st_dev + metadata->st_ino + metadata->st_mode +
        metadata->st_nlink + metadata->st_uid + metadata->st_gid +
        metadata->st_rdev + metadata->st_size + metadata->st_blksize +
        metadata->st_blocks + metadata->st_atim.tv_sec +
        metadata->st_mtim.tv_sec + metadata->st_ctim.tv_sec;

    return initialized_values != UINT64_MAX && S_ISCHR(metadata->st_mode);
}

int main(int argc, char **argv)
{
    struct stat metadata;
    int descriptor = -1;
    int result;

    if (argc != 2) {
        return 10;
    }

    if (strcmp(argv[1], "stat") == 0) {
        result = stat("/dev/null", &metadata);
    } else if (strcmp(argv[1], "fstat") == 0) {
        descriptor = open("/dev/null", O_RDONLY);
        if (descriptor < 0) {
            return 11;
        }
        result = fstat(descriptor, &metadata);
    } else if (strcmp(argv[1], "lstat") == 0) {
        result = lstat("/dev/null", &metadata);
    } else if (strcmp(argv[1], "fstatat") == 0) {
        result = fstatat(AT_FDCWD, "/dev/null", &metadata, 0);
    } else {
        return 12;
    }

    if (descriptor >= 0) {
        close(descriptor);
    }
    if (result != 0 || !metadata_is_valid(&metadata)) {
        return 13;
    }

    printf("msan-%s-ok\n", argv[1]);
    return 0;
}
