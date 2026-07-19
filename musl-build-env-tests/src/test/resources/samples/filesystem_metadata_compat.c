#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(const char *operation)
{
    fprintf(stderr, "%s failed: %s\n", operation, strerror(errno));
    return 1;
}

int main(void)
{
    char directory_template[] = "/tmp/filesystem-metadata-compat-XXXXXX";
    char file_path[sizeof(directory_template) + 16] = {0};
    char link_path[sizeof(directory_template) + 16] = {0};
    struct stat metadata;
    struct statx extended_metadata;
    int directory_fd = -1;
    int file_fd = -1;
    int result = 1;

    if (mkdtemp(directory_template) == NULL)
        return fail("mkdtemp");
    if (snprintf(file_path, sizeof(file_path), "%s/payload", directory_template) < 0 ||
        snprintf(link_path, sizeof(link_path), "%s/link", directory_template) < 0) {
        fputs("could not construct temporary paths\n", stderr);
        goto cleanup;
    }

    file_fd = open(file_path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (file_fd < 0) {
        fail("open");
        goto cleanup;
    }
    if (write(file_fd, "hello", 5) != 5) {
        fail("write");
        goto cleanup;
    }
    if (symlink(file_path, link_path) != 0) {
        fail("symlink");
        goto cleanup;
    }

    if (stat(file_path, &metadata) != 0) {
        fail("stat");
        goto cleanup;
    }
    if (!S_ISREG(metadata.st_mode) || metadata.st_size != 5) {
        fputs("stat returned incorrect metadata\n", stderr);
        goto cleanup;
    }

    if (fstat(file_fd, &metadata) != 0) {
        fail("fstat");
        goto cleanup;
    }
    if (!S_ISREG(metadata.st_mode) || metadata.st_size != 5) {
        fputs("fstat returned incorrect metadata\n", stderr);
        goto cleanup;
    }

    if (lstat(link_path, &metadata) != 0) {
        fail("lstat");
        goto cleanup;
    }
    if (!S_ISLNK(metadata.st_mode)) {
        fputs("lstat followed the symbolic link\n", stderr);
        goto cleanup;
    }

    directory_fd = open(directory_template, O_RDONLY | O_DIRECTORY);
    if (directory_fd < 0) {
        fail("open directory");
        goto cleanup;
    }
    if (fstatat(directory_fd, "payload", &metadata, 0) != 0) {
        fail("fstatat");
        goto cleanup;
    }
    if (!S_ISREG(metadata.st_mode) || metadata.st_size != 5) {
        fputs("fstatat returned incorrect metadata\n", stderr);
        goto cleanup;
    }

    memset(&extended_metadata, 0, sizeof(extended_metadata));
    if (statx(directory_fd, "payload", 0, STATX_TYPE | STATX_SIZE,
              &extended_metadata) != 0) {
        fail("statx");
        goto cleanup;
    }
    if ((extended_metadata.stx_mask & (STATX_TYPE | STATX_SIZE)) !=
            (STATX_TYPE | STATX_SIZE) ||
        (extended_metadata.stx_mode & S_IFMT) != S_IFREG ||
        extended_metadata.stx_size != 5) {
        fputs("statx returned incorrect metadata\n", stderr);
        goto cleanup;
    }

    puts("stat fstat lstat fstatat statx ok");
    result = 0;

cleanup:
    if (directory_fd >= 0)
        close(directory_fd);
    if (file_fd >= 0)
        close(file_fd);
    if (link_path[0] != '\0')
        unlink(link_path);
    if (file_path[0] != '\0')
        unlink(file_path);
    rmdir(directory_template);
    return result;
}
