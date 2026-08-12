/*
 * Cross-libc compatibility wrappers, archived into libglibc_compat.a and
 * reached through the GROUP entry in /usr/lib/libc.so.
 *
 * ASan and MSan links must omit this archive. Its strong hidden wrappers
 * override compiler-rt's weak interceptors where names overlap, silently
 * disabling the affected checks.
 *
 * Those builds use the reduced libc entry in /usr/asan/lib or /usr/msan/lib.
 * MUSL_CLANG_SANITIZE makes the musl-clang wrappers and the CMake toolchain do
 * that; a link that bypasses both has to put the directory first itself.
 * See the MUSL_CLANG_SANITIZE section in musl-build-env-tests/README.md for
 * the affected symbols and rationale.
 */

#define _GNU_SOURCE

#ifndef __has_feature
#  define __has_feature(feature) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_LEAK__) || \
    defined(__SANITIZE_MEMORY__) || defined(__SANITIZE_THREAD__) || \
    defined(__SANITIZE_UNDEFINED__) || \
    __has_feature(address_sanitizer) || __has_feature(leak_sanitizer) || \
    __has_feature(memory_sanitizer) || __has_feature(thread_sanitizer) || \
    __has_feature(undefined_behavior_sanitizer)
#  error "glibc_compat.c must not be built with a sanitizer enabled"
#endif

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <limits.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__) && !defined(__GLIBC__)

#define LAZY_RESOLVE(var, init_expr, cleanup_expr)                        \
    __extension__({                                                       \
        typeof(atomic_load(&(var))) _lv =                                 \
            atomic_load_explicit(&(var), memory_order_acquire);           \
        if (!_lv) {                                                       \
            _lv = (init_expr);                                            \
            typeof(_lv) _le = NULL;                                       \
            if (!atomic_compare_exchange_strong_explicit(                 \
                    &(var), &_le, _lv,                                    \
                    memory_order_release, memory_order_relaxed)) {        \
                typeof(_lv) p = _lv;                                     \
                (void)(cleanup_expr);                                     \
                _lv = _le;                                                \
            }                                                             \
        }                                                                 \
        _lv;                                                              \
    })


static void *libc_handle(void)
{
    static _Atomic(void *) h;
    return LAZY_RESOLVE(h,
        ({
            void *_h = dlopen("libc.so.6", RTLD_LAZY);
            if (!_h) {
                (void)fprintf(stderr,
                              "glibc_compat: dlopen(libc.so.6) failed: %s\n",
                              dlerror());
                abort();
            }
            _h;
        }),
        dlclose(p));
}

static int is_glibc(void)
{
    static _Atomic(int) cached = -1;
    int v = atomic_load_explicit(&cached, memory_order_acquire);
    if (v < 0) {
        v = dlsym(RTLD_DEFAULT, "gnu_get_libc_version") != NULL;
        atomic_store_explicit(&cached, v, memory_order_release);
    }
    return v;
}

// Resolve a symbol from a handle, aborting if not found.
static void *xdlsym(void *handle, const char *name)
{
    void *sym = dlsym(handle, name);
    if (!sym) {
        (void)fprintf(stderr, "glibc_compat: dlsym(%s) failed: %s\n",
                      name, dlerror());
        abort();
    }
    return sym;
}

#    ifdef __aarch64__
#        define _STAT_VER 0
#    else
#        define _STAT_VER 1
#    endif

// glibc before 2.33 (2021) doesn't have these
int stat(const char *restrict path, void *restrict buf)
{
    int __xstat(int, const char *restrict, void *restrict);
    return __xstat(_STAT_VER, path, buf);
}

int fstat(int fd, void *buf)
{
    int __fxstat(int, int, void *);
    return __fxstat(_STAT_VER, fd, buf);
}

int lstat(const char *restrict path, void *restrict buf)
{
    int __lxstat(int, const char *restrict, void *restrict);
    return __lxstat(_STAT_VER, path, buf);
}

int fstatat(int dirfd, const char *restrict pathname, void *restrict statbuf, int flags)
{
    int __fxstatat(int, int, const char *restrict, void *restrict, int);
    return __fxstatat(_STAT_VER, dirfd, pathname, statbuf, flags);
}

// statx(2) was introduced in Linux 4.11 (April 2017, syscall 332 on x86-64,
// 291 on aarch64). Libc wrappers followed later: glibc 2.28 (August 2018)
// and musl 1.2.0 (October 2020).
struct statx;
int statx(int dirfd, const char *restrict pathname, int flags,
          unsigned int mask, struct statx *restrict statxbuf)
{
    return syscall(SYS_statx, dirfd, pathname, flags, mask, statxbuf);
}

// glibc doesn't define pthread_atfork on aarch64. We need to delegate to
// glibc's __register_atfork() instead. __register_atfork() takes an extra
// argument, __dso_handle, which is a pointer to the DSO that is registering the
// fork handlers. This is used to ensure that the handlers are not called after
// the DSO is unloaded. glibc on amd64 also implements pthread_atfork() in terms
// of __register_atfork().  (musl never unloads modules so that potential
// problem doesn't exist)

// On amd64, even though pthread_atfork is exported by glibc, it should not be
// used. Code that uses pthread_atfork will compile to an import to
// __register_atfork(), but here we're compiling against musl, resulting in an
// import to pthread_atfork. This will cause a runtime error after the test
// that unloads our module. The reason is that when we call pthread_atfork in
// glibc, __register_atfork() is called with the __dso_handle of libc6.so, not
// the __dso_handle of our module. So the fork handler is not unregistered when
// our module is unloaded.

extern void *__dso_handle __attribute__((weak));
int __register_atfork(void (*prepare)(void), void (*parent)(void),
    void (*child)(void), void *__dso_handle) __attribute__((weak));

int pthread_atfork(
    void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    if (__dso_handle && __register_atfork) {
        return __register_atfork(prepare, parent, child, __dso_handle);
    }

    static _Atomic(typeof(pthread_atfork) *) real_atfork;
    typeof(pthread_atfork) *fn = LAZY_RESOLVE(
        real_atfork,
        (typeof(pthread_atfork) *)xdlsym(libc_handle(), "pthread_atfork"),
        (void)p);
    return fn(prepare, parent, child);
}

// On 64-bit platforms, musl represents msg_iovlen and msg_controllen as
// 32-bit values followed by explicit padding. glibc and the kernel represent
// each pair as a size_t. The same difference exists between musl's
// cmsghdr.cmsg_len and glibc's size_t field.
//
// Musl's sendmsg and recvmsg wrappers translate these layouts before entering
// the kernel, and its sendmmsg implementation calls sendmsg once per message.
// Delegate directly to those wrappers when running on musl. A musl-compiled
// binary running on glibc instead resolves these functions to glibc, which
// expects its own field widths, so translate only on that path.
ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
    static _Atomic(typeof(sendmsg) *) real_sendmsg;
    typeof(sendmsg) *fn = LAZY_RESOLVE(
        real_sendmsg,
        (typeof(sendmsg) *)xdlsym(libc_handle(), "sendmsg"), (void)p);

#if __SIZEOF_LONG__ > 4
    if (is_glibc()) {
        struct msghdr host_message;
        struct cmsghdr control_buffer[
            CMSG_SPACE(255 * sizeof(int)) / sizeof(struct cmsghdr) + 1];
        void *allocated_control = NULL;

        if (message) {
            host_message = *message;
            host_message.__pad1 = 0;
            host_message.__pad2 = 0;
            message = &host_message;

            if (host_message.msg_controllen && host_message.msg_control) {
                if (host_message.msg_controllen > sizeof(control_buffer)) {
                    allocated_control = malloc(host_message.msg_controllen);
                    if (!allocated_control) {
                        errno = ENOMEM;
                        return -1;
                    }
                }
                void *translated_control = allocated_control ?
                    allocated_control : control_buffer;
                memcpy(translated_control, host_message.msg_control,
                       host_message.msg_controllen);
                host_message.msg_control = translated_control;
                for (struct cmsghdr *control = CMSG_FIRSTHDR(&host_message);
                     control;
                     control = CMSG_NXTHDR(&host_message, control)) {
                    control->__pad1 = 0;
                }
            }
        }

        ssize_t result = fn(fd, message, flags);
        if (allocated_control) {
            int saved_errno = errno;
            free(allocated_control);
            errno = saved_errno;
        }
        return result;
    }
#endif
    return fn(fd, message, flags);
}

ssize_t recvmsg(int fd, struct msghdr *message, int flags)
{
    static _Atomic(typeof(recvmsg) *) real_recvmsg;
    typeof(recvmsg) *fn = LAZY_RESOLVE(
        real_recvmsg,
        (typeof(recvmsg) *)xdlsym(libc_handle(), "recvmsg"), (void)p);

#if __SIZEOF_LONG__ > 4
    if (is_glibc() && message) {
        struct msghdr host_message = *message;
        host_message.__pad1 = 0;
        host_message.__pad2 = 0;
        ssize_t result = fn(fd, &host_message, flags);
        if (result >= 0) {
            for (struct cmsghdr *control = CMSG_FIRSTHDR(&host_message);
                 control;
                 control = CMSG_NXTHDR(&host_message, control)) {
                control->__pad1 = 0;
            }
        }
        host_message.__pad1 = 0;
        host_message.__pad2 = 0;
        *message = host_message;
        return result;
    }
#endif
    return fn(fd, message, flags);
}

int sendmmsg(int fd, struct mmsghdr *messages, unsigned int count,
             unsigned int flags)
{
#if __SIZEOF_LONG__ > 4
    if (is_glibc()) {
        if (count > IOV_MAX)
            count = IOV_MAX;
        if (!count)
            return 0;

        unsigned int i;
        for (i = 0; i < count; i++) {
            ssize_t result = sendmsg(fd, &messages[i].msg_hdr, flags);
            if (result < 0)
                break;
            messages[i].msg_len = (unsigned int)result;
        }
        return i ? (int)i : -1;
    }
#endif
    static _Atomic(typeof(sendmmsg) *) real_sendmmsg;
    typeof(sendmmsg) *fn = LAZY_RESOLVE(
        real_sendmmsg,
        (typeof(sendmmsg) *)xdlsym(libc_handle(), "sendmmsg"), (void)p);
    return fn(fd, messages, count, flags);
}

int recvmmsg(int fd, struct mmsghdr *messages, unsigned int count,
             unsigned int flags, struct timespec *timeout)
{
    static _Atomic(typeof(recvmmsg) *) real_recvmmsg;
    typeof(recvmmsg) *fn = LAZY_RESOLVE(
        real_recvmmsg,
        (typeof(recvmmsg) *)xdlsym(libc_handle(), "recvmmsg"), (void)p);

#if __SIZEOF_LONG__ > 4
    if (is_glibc()) {
        if (!messages)
            return fn(fd, messages, count, flags, timeout);
        if (count > IOV_MAX)
            count = IOV_MAX;

        for (unsigned int i = 0; i < count; i++) {
            messages[i].msg_hdr.__pad1 = 0;
            messages[i].msg_hdr.__pad2 = 0;
        }

        int result = fn(fd, messages, count, flags, timeout);
        for (int i = 0; i < result; i++) {
            struct msghdr *message = &messages[i].msg_hdr;
            message->__pad1 = 0;
            message->__pad2 = 0;
            for (struct cmsghdr *control = CMSG_FIRSTHDR(message);
                 control;
                 control = CMSG_NXTHDR(message, control)) {
                control->__pad1 = 0;
            }
        }
        return result;
    }
#endif
    return fn(fd, messages, count, flags, timeout);
}

// realpath on x86_64 glibc has two symbol versions:
//   realpath@@GLIBC_2.3   — the current version (supports resolved=NULL, allocates buffer)
//   realpath@GLIBC_2.2.5  — the legacy version (requires non-NULL resolved, returns EINVAL
//                           immediately if resolved==NULL, no heap allocation)
//
// fuse-overlayfs calls realpath(path, NULL) to let glibc allocate the result buffer.
// A musl-compiled binary carries an unversioned PLT reference to realpath; glibc's
// dynamic linker resolves it to @GLIBC_2.2.5 (the oldest match), which returns
// EINVAL without making any syscalls.
//
// dlsym(RTLD_DEFAULT, "realpath") returns the default @@GLIBC_2.3 version.
// This is only needed on x86_64 because glibc's versioning differs by arch.
#    ifdef __x86_64__
char *realpath(const char *restrict path, char *restrict resolved)
{
    static _Atomic(typeof(realpath) *) real_realpath;
    typeof(realpath) *fn = LAZY_RESOLVE(
        real_realpath,
        (typeof(realpath) *)xdlsym(libc_handle(), "realpath"), (void)p);
    return fn(path, resolved);
}
#    endif

// pthread_cond_init on x86_64 glibc has two symbol versions:
//   pthread_cond_init@@GLIBC_2.3.2 — the current/default version (supports CLOCK_MONOTONIC)
//   pthread_cond_init@GLIBC_2.2.5  — the legacy version (rejects CLOCK_MONOTONIC with EINVAL)
//
// A musl-compiled binary carries an *unversioned* PLT reference to
// pthread_cond_init.  When loaded on glibc, the dynamic linker resolves an
// unversioned reference to the *oldest* matching symbol, i.e. @GLIBC_2.2.5.
// That legacy version returns EINVAL when the condattr clock is
// CLOCK_MONOTONIC, silently breaking condition variables.
//
// dlsym(RTLD_DEFAULT, ...) always returns the default (@@GLIBC_2.3.2) version,
// so resolving through dlsym here bypasses the version-pinning issue.
//
// This is not a problem on aarch64.
#    ifdef __x86_64__
struct pthread_cond;
struct pthread_condattr;
typedef struct pthread_cond pthread_cond_t;
typedef struct pthread_condattr pthread_condattr_t;

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
    static _Atomic(typeof(pthread_cond_init) *) real_pthread_cond_init;
    typeof(pthread_cond_init) *fn = LAZY_RESOLVE(
        real_pthread_cond_init,
        (typeof(pthread_cond_init) *)xdlsym(
            libc_handle(), "pthread_cond_init"),
        (void)p);
    return fn(cond, cond_attr);
}
#    endif

// the symbol strerror_r in glibc is not the POSIX version; it returns char *
// __xpg_sterror_r is exported by both glibc and musl
int strerror_r(int errnum, char *buf, size_t buflen)
{
    int __xpg_strerror_r(int, char *, size_t);
    return __xpg_strerror_r(errnum, buf, buflen);
}

// when compiling with --coverage, some references to atexit show up.
// glibc doesn't provide atexit for similar reasons as pthread_atfork presumably
int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle);
int atexit(void (*function)(void))
{
    if (!__dso_handle) {
        (void)fprintf(stderr, "Aborting because __dso_handle is NULL\n");
        abort();
    }

    // the cast is harmless on amd64 and aarch64. Passing an extra argument to a
    // function that expects none causes no problems
    return __cxa_atexit((void (*)(void *))function, 0, __dso_handle);
}

// introduced in glibc 2.25
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    int fd;
    size_t bytes_read = 0;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    while (bytes_read < buflen) {
        ssize_t result = read(fd, (char*)buf + bytes_read, buflen - bytes_read);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        bytes_read += result;
    }

    close(fd);
    return (ssize_t)bytes_read;
}

#ifdef __x86_64__
#define MEMFD_CREATE_SYSCALL 319
#elif __aarch64__
#define MEMFD_CREATE_SYSCALL 279
#endif

// introduced in glibc 2.27
int memfd_create(const char *name, unsigned flags) {
  return syscall(MEMFD_CREATE_SYSCALL, name, flags);
}

// __flt_rounds is a musl internal that returns the FLT_ROUNDS value.
// glibc doesn't export it; fegetround() provides the same information. The
// post-link step adds glibc's libm dependency for this direct reference.
int __flt_rounds(void)
{
    switch (fegetround()) {
        case FE_TONEAREST:  return 1;
        case FE_UPWARD:     return 2;
        case FE_DOWNWARD:   return 3;
        case FE_TOWARDZERO: return 0;
        default:            return -1;
    }
}

// glibc has never exported sigsetjmp as a dynamic symbol -- it is defined in
// <setjmp.h> as a macro expanding to __sigsetjmp, so only __sigsetjmp appears
// in the DSO. Musl-compiled code references sigsetjmp directly, so we provide
// this bridge.
//
// IMPORTANT: this MUST be a bare tail-call with NO C function frame.
// A C function wrapper adds push rbp / mov rbp,rsp before calling __sigsetjmp.
// glibc's __sigsetjmp saves whatever rbp it sees into the jmp_buf; with an
// extra frame it saves the wrapper's frame pointer, not the caller's.
// After siglongjmp restores that wrong rbp, every rbp-relative stack access
// in the caller reads from the wrong address.
// Use a module-level asm definition so no prologue is emitted.
#ifdef __x86_64__
__asm__(
    ".globl sigsetjmp\n"
    ".type  sigsetjmp, @function\n"
    "sigsetjmp:\n"
    "    jmp __sigsetjmp@plt\n"
    ".size  sigsetjmp, . - sigsetjmp\n"
);
#elif defined(__aarch64__)
__asm__(
    ".globl sigsetjmp\n"
    ".type  sigsetjmp, @function\n"
    "sigsetjmp:\n"
    "    b __sigsetjmp\n"
    ".size  sigsetjmp, . - sigsetjmp\n"
);
#endif

// glibc doesn't export res_init as a dynamic symbol. Only __res_init
// is exported. Musl-compiled code references res_init directly.
// __res_init is declared weak so linking succeeds on musl (where it doesn't
// exist). At runtime: on glibc it's non-NULL and called directly; on musl
// it's NULL and we fall through to dlopen the runtime libc's res_init.
extern int __res_init(void) __attribute__((weak));

int res_init(void)
{
    if (__res_init)
        return __res_init();

    // musl path: find res_init in the runtime libc via dlopen
    static _Atomic(typeof(res_init) *) musl_res_init;
    typeof(res_init) *fn = LAZY_RESOLVE(
        musl_res_init,
        (typeof(res_init) *)xdlsym(libc_handle(), "res_init"), (void)p);
    return fn();
}

// __freadahead is a stdio_ext function exported by musl but not glibc.
// It returns the number of bytes remaining in the FILE's read buffer.
// Both musl and glibc store the read pointer at offset 8 and the read end
// at offset 16 in their FILE structs (musl: rpos/rend, glibc:
// _IO_read_ptr/_IO_read_end).  This layout has been stable in both
// implementations since their inception on 64-bit platforms.
size_t __freadahead(void *f)
{
    char **rpos = (char **)((char *)f + 8);
    char **rend = (char **)((char *)f + 16);
    if (!*rend)
        return 0;
    return (size_t)(*rend - *rpos);
}

// copied from musl
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
size_t strlcpy(char *d, const char *s, size_t n)
{
    char *d0 = d;
    size_t *wd;

    if (!n--) goto finish;
    for (; n && (*d=*s); n--, s++, d++);
    *d = 0;
finish:
    return d-d0 + strlen(s);
}
size_t strlcat(char *d, const char *s, size_t n)
{
    size_t l = strnlen(d, n);
    if (l == n) return l + strlen(s);
    return l + strlcpy(d+l, s, n-l);
}

#endif
