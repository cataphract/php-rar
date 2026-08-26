/* Reduced libc entry for ASan and MSan links. It omits libglibc_compat.a so
 * compiler-rt's weak interceptors remain active. */
GROUP (
  /usr/lib/libadditional_compat.a
  AS_NEEDED ( /usr/lib/glibc-compat-libc.so.6 )
)
