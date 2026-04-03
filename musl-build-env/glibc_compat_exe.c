// Executable-only glibc compatibility shim.
//
// Problem
// -------
// When a musl-compiled, patchelf'd executable runs on glibc, constructors
// registered via __attribute__((constructor)) / .init_array are never called.
// Any statically-linked library that relies on a constructor for initialization
// will silently malfunction.
//
// Root cause
// ----------
// On every glibc version, _dl_init (in ld.so) only calls .init_array for
// shared libraries, never for the main executable.  The main executable's
// .init_array is always the responsibility of __libc_start_main, via its
// `init` function-pointer argument.  The mechanism differs by version:
//
//   glibc < 2.34:
//
//     ld.so _dl_init
//       └─ call_init() for each shared lib (NOT main exe)
//     app _start (glibc Scrt1.o)
//       └─ __libc_start_main(main, init=__libc_csu_init, ...)
//            ├─ init != NULL → call __libc_csu_init()
//            │   └─ iterates __init_array_start..__end  ◄── called here
//            └─ main()
//
//   glibc >= 2.34 (__libc_csu_init removed):
//
//     ld.so _dl_init
//       └─ call_init() for each shared lib (NOT main exe)
//     app _start (glibc Scrt1.o)
//       └─ __libc_start_main(main, init=NULL, ...)
//            ├─ init == NULL → inlined call_init reads DT_INIT_ARRAY
//            │   └─ iterates entries from link map  ◄── called here
//            └─ main()
//
//   patchelf'd musl binary on ANY glibc:
//
//     ld.so _dl_init
//       └─ call_init() for each shared lib (NOT main exe)
//     musl _start_c
//       └─ __libc_start_main(main, init=<musl's init>, ...)
//            ├─ init != NULL → call musl's init
//            │   (registers EH frames only, does NOT iterate .init_array)
//            └─ main()                           ◄── .init_array SKIPPED
//
// The key: musl's _start always passes a non-NULL init function that does
// not iterate .init_array.  On glibc < 2.34 this shadows __libc_csu_init;
// on glibc >= 2.34 it prevents the init==NULL fallback that reads the link
// map.  Either way, .init_array constructors are never called.
//
// Fix
// ---
// Use .preinit_array, which _dl_init DOES process for the main executable
// (on all glibc versions), to call all .init_array entries ourselves.
// The linker provides __init_array_start/__init_array_end symbols spanning
// the .init_array section.

#if defined(__linux__) && !defined(__GLIBC__)

typedef void (*init_fn)(int, char **, char **);
extern init_fn __init_array_start[] __attribute__((weak));
extern init_fn __init_array_end[]   __attribute__((weak));

static void run_init_array(int argc, char **argv, char **envp)
{
    if (!__init_array_start || !__init_array_end)
        return;
    for (init_fn *fn = __init_array_start; fn < __init_array_end; fn++)
        if (*fn)
            (*fn)(argc, argv, envp);
}

__attribute__((section(".preinit_array"), used))
static init_fn preinit_entry = (init_fn)(void *)run_init_array;

#endif
