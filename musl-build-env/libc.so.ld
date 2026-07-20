/* Development linker script. The runtime libc remains the musl loader named
 * below; these archives add cross-libc and downstream compatibility symbols. */
GROUP (
  /usr/lib/libadditional_compat.a
  /usr/lib/libglibc_compat.a
  AS_NEEDED ( @MUSL_LIBC@ )
)
