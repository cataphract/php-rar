/*
 * Stand-in for the symbols a derived image adds to libadditional_compat.a,
 * the extension hook that /usr/lib/libc.so and the sanitizer libc.so both
 * reference. php-minimal fills that archive with its php_pcre2_* stubs; this
 * file exists so the tests can observe the archive being linked without
 * depending on any particular downstream image.
 */

int musl_downstream_compat_probe(void);

int
musl_downstream_compat_probe(void)
{
    return 42;
}
