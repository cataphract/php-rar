// TL;DR: The executable keeps musl's _start but runs glibc's
// __libc_start_main. Each side expects the other to run the main executable's
// initialization and finalization arrays, so neither does. A .preinit_array
// callback runs .init_array and registers the main executable's exit hooks.
//
// Executable-only glibc compatibility shim.
//
// Problem
// -------
// When a musl-linked executable is changed with patchelf to use glibc's
// interpreter and libc, constructors and destructors registered through
// __attribute__((constructor)), __attribute__((destructor)), .init_array, or
// .fini_array are not called. A statically linked library that relies on one of
// these hooks can therefore malfunction without producing an obvious loader
// error.
//
// Constructor ownership on glibc
// ------------------------------
// glibc's dynamic loader initializes shared libraries, and processes the main
// executable's .preinit_array, but deliberately leaves the main executable's
// DT_INIT and DT_INIT_ARRAY to __libc_start_main. How glibc hands off that work
// changed in 2.34:
//
//   glibc < 2.34:
//
//     glibc CRT _start
//       └─ __libc_start_main(main, init=__libc_csu_init, ...)
//            └─ __libc_csu_init calls _init and walks .init_array
//
//   glibc >= 2.34:
//
//     glibc CRT _start
//       └─ __libc_start_main(main, init=NULL, ...)
//            └─ the NULL branch reads DT_INIT/DT_INIT_ARRAY from the
//               main executable's link map
//
// Constructor ownership on musl
// -----------------------------
// musl's CRT _start passes the executable's _init and _fini addresses to
// __libc_start_main. musl's own __libc_start_main ignores those callback
// arguments and invokes its dynamic-loader constructor path. That path walks
// .init_array and, except on architectures with NO_LEGACY_INITFINI, calls
// DT_INIT first.
//
// The incompatible combination
// ----------------------------
// After patchelf, musl's _start instead resolves __libc_start_main from glibc:
//
//   musl CRT _start
//     └─ glibc __libc_start_main(main, init=_init, ...)
//          └─ init is non-NULL, so glibc treats it as the complete legacy
//             initializer and does not use its DT_INIT_ARRAY fallback
//
// The executable's _init entry point does not walk .init_array, and musl's
// internal constructor path is not present because glibc is running the
// process. This fails before 2.34 because the executable did not bring
// glibc's __libc_csu_init callback, and on 2.34 or later because musl's non-NULL
// _init argument prevents glibc's new link-map fallback.
//
// This shim repairs .init_array processing. It does not make legacy DT_INIT
// behavior identical: on x86_64 glibc calls _init after the preinit callback,
// reversing musl's _init-before-.init_array order, while musl AArch64 suppresses
// DT_INIT and glibc still calls the _init argument. Code using a custom .init
// fragment is outside this shim's compatibility scope.
//
// Finalizer ownership
// -------------------
// musl's dynamic exit path runs registered exit callbacks first and walks
// .fini_array backwards. It then calls the legacy DT_FINI entry unless the
// architecture defines musl's NO_LEGACY_INITFINI; AArch64 does and x86_64 does
// not. For dynamically linked programs, glibc ignores the fini argument passed
// to __libc_start_main because its loader normally owns DSO finalization. musl's
// CRT passes rtld_fini as null, so glibc never registers _dl_fini and
// loader-owned finalization is skipped. This shim repairs the main executable's
// hooks; finalizers in shared libraries remain outside its compatibility scope.
//
// Fix
// ---
// glibc's dynamic loader processes .preinit_array for the main executable on
// both sides of the 2.34 change. Put a callback there that walks .init_array
// explicitly and first registers a cleanup callback through __cxa_atexit.
// Registration happens before .init_array so callbacks installed by C or C++
// constructors, and later by main, run first in LIFO order. The cleanup then
// walks .fini_array backwards and, on x86_64, calls _fini. This matches musl's
// NO_LEGACY_INITFINI behavior and ordering on both supported architectures.
//
// musl has no .preinit_array processing and continues to run the same hooks
// once through its internal startup and exit paths. The linker-provided array
// boundary symbols delimit the constructor and destructor entries.

#if defined(__linux__) && !defined(__GLIBC__)

typedef void (*init_fn)(int, char **, char **);
typedef void (*fini_fn)(void);
extern init_fn __init_array_start[] __attribute__((weak));
extern init_fn __init_array_end[]   __attribute__((weak));
extern fini_fn __fini_array_start[] __attribute__((weak));
extern fini_fn __fini_array_end[]   __attribute__((weak));
#if defined(__x86_64__)
extern void _fini(void);
#endif
extern void *__dso_handle;
extern int __cxa_atexit(void (*function)(void *), void *argument,
                        void *dso_handle);

static void run_fini_hooks(void *unused)
{
    (void)unused;

    if (__fini_array_start && __fini_array_end) {
        for (fini_fn *fn = __fini_array_end; fn > __fini_array_start;) {
            fn--;
            if (*fn)
                (*fn)();
        }
    }

#if defined(__x86_64__)
    _fini();
#endif
}

static void run_init_array(int argc, char **argv, char **envp)
{
    if (__cxa_atexit(run_fini_hooks, 0, __dso_handle) != 0)
        __builtin_trap();

    if (!__init_array_start || !__init_array_end)
        return;
    for (init_fn *fn = __init_array_start; fn < __init_array_end; fn++)
        if (*fn)
            (*fn)(argc, argv, envp);
}

__attribute__((section(".preinit_array"), used))
static init_fn preinit_entry = (init_fn)(void *)run_init_array;

#endif
