/* Development linker script. Archives take precedence over linker-only DSOs
 * that select portable glibc dependency names for unresolved musl symbols. */
GROUP (
  /usr/lib/libadditional_compat.a
  /usr/lib/libglibc_compat.a
  AS_NEEDED (
    /usr/lib/glibc-compat-libpthread.so.0
    /usr/lib/glibc-compat-librt.so.1
    /usr/lib/glibc-compat-libm.so.6
    /usr/lib/glibc-compat-libdl.so.2
    /usr/lib/glibc-compat-libutil.so.1
    /usr/lib/glibc-compat-libc.so.6
  )
)
