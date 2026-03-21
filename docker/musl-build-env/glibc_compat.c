#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__linux__) && !defined(__GLIBC__)

#    ifdef __x86_64__
float ceilf(float x)
{
    float result;
    // NOLINTNEXTLINE(hicpp-no-assembler)
    __asm__("roundss $0x0A, %[x], %[result]"
            : [result] "=x"(result)
            : [x] "x"(x));
    return result;
}
double ceil(double x)
{
    double result;
    // NOLINTNEXTLINE(hicpp-no-assembler)
    __asm__("roundsd $0x0A, %[x], %[result]"
            : [result] "=x"(result)
            : [x] "x"(x));
    return result;
}
#    endif

#    ifdef __aarch64__
float ceilf(float x)
{
    float result;
    __asm__("frintp %s0, %s1\n" : "=w"(result) : "w"(x));
    return result;
}
double ceil(double x)
{
    double result;
    __asm__("frintp %d0, %d1\n" : "=w"(result) : "w"(x));
    return result;
}
#    endif

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
// an import to pthread_atfork. This will cause a runtime error after the test
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
    // glibc
    if (__dso_handle && __register_atfork) {
        return __register_atfork(prepare, parent, child, __dso_handle);
    }

    static int (*real_atfork)(void (*)(void), void (*)(void), void (*)(void));

    if (!real_atfork) {
        // dlopen musl
#    ifdef __aarch64__
        void *handle = dlopen("ld-musl-aarch64.so.1", RTLD_LAZY);
        if (!handle) {
            (void)fprintf(
                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                stderr, "dlopen of ld-musl-aarch64.so.1 failed: %s\n",
                dlerror());
            abort();
        }
#    else
        void *handle = dlopen("libc.musl-x86_64.so.1", RTLD_LAZY);
        if (!handle) {
            (void)fprintf(
                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                stderr, "dlopen of libc.musl-x86_64.so.1 failed: %s\n",
                dlerror());
            abort();
        }
#    endif
        real_atfork = dlsym(handle, "pthread_atfork");
        if (!real_atfork) {
            (void)fprintf(
                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                stderr, "dlsym of pthread_atfork failed: %s\n", dlerror());
            abort();
        }
    }

    return real_atfork(prepare, parent, child);
}

#    ifdef __x86_64__
struct pthread_cond;
struct pthread_condattr;
typedef struct pthread_cond pthread_cond_t;
typedef struct pthread_condattr pthread_condattr_t;

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
    static int (*real_pthread_cond_init)(pthread_cond_t *cond, const pthread_condattr_t *cond_attr);

    if (!real_pthread_cond_init) {
        void *handle = dlopen("libc.so.6", RTLD_LAZY);
        if (!handle) {
            void *handle = dlopen("libc.musl-x86_64.so.1", RTLD_LAZY);
            if (!handle) {
                (void)fprintf(
                    // NOLINTNEXTLINE(concurrency-mt-unsafe)
                    stderr, "dlopen of libc.so.6 and libc.musl-x86_64.so.1 failed: %s\n",
                    dlerror());
                abort();
            }
        }

        real_pthread_cond_init = dlsym(handle, "pthread_cond_init");
        if (!real_pthread_cond_init) {
            (void)fprintf(
                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                stderr, "dlsym of pthread_cond_init failed: %s\n", dlerror());
            abort();
        }
    }

    return real_pthread_cond_init(cond, cond_attr);
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
// glibc doesn't export it; fegetround() provides the same information.
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

// glibc 2.39 no longer exports res_init as a dynamic symbol. Only __res_init
// is exported. Musl-compiled code references res_init directly.
// __res_init is declared weak so linking succeeds on musl (where it doesn't
// exist). At runtime: on glibc it's non-NULL and called directly; on musl
// it's NULL and we fall through to dlopen musl's own res_init.
extern int __res_init(void) __attribute__((weak));

int res_init(void)
{
    if (__res_init)
        return __res_init();

    // musl path: find res_init in musl's libc via dlopen
    static int (*musl_res_init)(void);
    if (!musl_res_init) {
#    ifdef __aarch64__
        void *handle = dlopen("ld-musl-aarch64.so.1", RTLD_LAZY);
#    else
        void *handle = dlopen("libc.musl-x86_64.so.1", RTLD_LAZY);
#    endif
        if (handle)
            musl_res_init = dlsym(handle, "res_init");
    }
    if (musl_res_init)
        return musl_res_init();
    return 0;
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
