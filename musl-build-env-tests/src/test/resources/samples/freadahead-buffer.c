#include <stdio.h>
#include <stdio_ext.h>

int main(void)
{
    static const unsigned char payload[] =
        "0123456789abcdef"
        "0123456789abcdef"
        "0123456789abcdef"
        "0123456789abcdef";
    const size_t payload_size = sizeof(payload) - 1;

    FILE *stream = tmpfile();
    if (stream == NULL) {
        perror("tmpfile");
        return 1;
    }
    if (fwrite(payload, 1, payload_size, stream) != payload_size ||
        fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return 2;
    }

    if (__freadahead(stream) != 0 || fgetc(stream) != payload[0]) {
        fclose(stream);
        return 3;
    }
    size_t after_first_byte = __freadahead(stream);

    unsigned char next_bytes[7];
    if (fread(next_bytes, 1, sizeof(next_bytes), stream) != sizeof(next_bytes)) {
        fclose(stream);
        return 4;
    }
    size_t after_eight_bytes = __freadahead(stream);

    unsigned char remainder[56];
    if (fread(remainder, 1, sizeof(remainder), stream) != sizeof(remainder)) {
        fclose(stream);
        return 5;
    }
    size_t after_all_bytes = __freadahead(stream);

    if (after_first_byte != 63 || after_eight_bytes != 56 ||
        after_all_bytes != 0) {
        fprintf(stderr, "unexpected counts: %zu,%zu,%zu\n",
                after_first_byte, after_eight_bytes, after_all_bytes);
        fclose(stream);
        return 6;
    }

    printf("freadahead counts=%zu,%zu,%zu\n",
           after_first_byte, after_eight_bytes, after_all_bytes);
    if (fclose(stream) != 0) {
        return 7;
    }
    return 0;
}
