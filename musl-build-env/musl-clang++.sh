#!/bin/bash

set -eo pipefail
ARCH=$(uname -m)
SYSROOT="/sysroot/${ARCH}-none-linux-musl"

# When MUSL_CLANG_SANITIZE_CXX requests address sanitizing (set in the debug
# build env), link the C++ runtime against the ASAN-instrumented shared
# libc++/libc++abi/libunwind in /usr/asan instead of the plain static ones, so
# libc++'s container-overflow annotations match instrumented consumers and
# don't produce false positives. Those .so's carry undefined __asan_* symbols
# (built without -shared-libasan) that resolve at load from the static ASAN
# runtime in the host process (e.g. PHP) -- no second runtime is introduced.
asan_cxx=false
case "${MUSL_CLANG_SANITIZE_CXX:-}" in
    *address*) asan_cxx=true ;;
esac

# Compile-only: no linker logic needed.
for arg in "$@"; do
    case "$arg" in
        -c|-S|-E)
            exec clang++ \
                --target="${ARCH}-none-linux-musl" \
                --sysroot="${SYSROOT}" \
                -stdlib=libc++ \
                -rtlib=compiler-rt \
                -unwindlib=libunwind \
                -fno-omit-frame-pointer \
                -Qunused-arguments \
                "$@"
            ;;
    esac
done

# --- Link mode ---

# Detect executable vs shared library.
linking_exe=true
for arg in "$@"; do
    case "$arg" in
        -shared) linking_exe=false; break;;
    esac
done

exe_flags=()
if $linking_exe; then
    exe_flags=("${SYSROOT}/usr/lib/glibc_compat_exe.o")
fi

# Collect library search paths: user -L flags then internal clang paths.
lib_paths=()
prev=""
for arg in "$@"; do
    if [[ "$prev" == "-L" ]]; then
        lib_paths+=("$arg")
    else
        case "$arg" in -L?*) lib_paths+=("${arg#-L}");; esac
    fi
    prev="$arg"
done
IFS=: read -r -a _clang_dirs <<< "$(clang++ --target="${ARCH}-none-linux-musl" \
    --sysroot="${SYSROOT}" -stdlib=libc++ -rtlib=compiler-rt \
    -print-search-dirs 2>/dev/null | sed -n 's/^libraries: =//p')"
for _d in "${_clang_dirs[@]}"; do
    [[ -n "$_d" ]] && lib_paths+=("$_d")
done

# Check if libNAME.a exists in any search path.
has_static() {
    # Never statically link libc. The output (shared object or executable) must
    # resolve libc dynamically against the host's libc at runtime, exactly like
    # the PHP binary does. Statically linking libc would (a) duplicate symbols
    # that glibc_compat deliberately overrides (strerror_r, sigsetjmp, atexit,
    # ...) and (b) embed a second, uninitialized libc state -- e.g. getauxval()
    # dereferencing a NULL auxv pointer and crashing when the object is dlopen'd
    # into an already-running process. It also lets the host's sanitizer
    # interceptors see the object's libc calls instead of bypassing them.
    [[ "$1" == "c" ]] && return 1
    for _dir in "${lib_paths[@]}"; do
        [[ -f "$_dir/lib${1}.a" ]] && return 0
    done
    return 1
}

# Rewrite arguments: for each -l flag, if a .a exists prefer it via
# --push-state -Bstatic ... --pop-state.  Everything else passes through.
# Under ASAN the C++ runtime libs are dropped here and re-added below as the
# shared /usr/asan copies; skip them so they aren't also linked statically.
is_asan_cxx_lib() {
    $asan_cxx || return 1
    case "$1" in c++|c++abi|unwind) return 0;; *) return 1;; esac
}

new_args=()
prev_was_l=false
for arg in "$@"; do
    if $prev_was_l; then
        if is_asan_cxx_lib "$arg"; then
            :
        elif has_static "$arg"; then
            new_args+=(-Wl,--push-state -Wl,-Bstatic "-l$arg" -Wl,--pop-state)
        else
            new_args+=("-l$arg")
        fi
        prev_was_l=false
        continue
    fi
    case "$arg" in
        -l)
            prev_was_l=true
            ;;
        -l:*)
            new_args+=("$arg")
            ;;
        -l*)
            name="${arg#-l}"
            if is_asan_cxx_lib "$name"; then
                :
            elif has_static "$name"; then
                new_args+=(-Wl,--push-state -Wl,-Bstatic "$arg" -Wl,--pop-state)
            else
                new_args+=("$arg")
            fi
            ;;
        *)
            new_args+=("$arg")
            ;;
    esac
done

# Replacement C++ runtime block for ASAN: link the instrumented shared libraries
# from /usr/asan dynamically, with a matching rpath so the loader finds them.
asan_cxx_runtime=()
if $asan_cxx; then
    asan_cxx_runtime=(
        -Wl,-Bdynamic
        -L/usr/asan/lib -Wl,-rpath,/usr/asan/lib
        -lc++ -lc++abi -lunwind
        -Wl,-Bstatic
    )
fi

exec clang++ \
    --target="${ARCH}-none-linux-musl" \
    --sysroot="${SYSROOT}" \
    -stdlib=libc++ \
    -rtlib=compiler-rt \
    -unwindlib=libunwind \
    -fno-omit-frame-pointer \
    -Qunused-arguments \
    -fuse-ld=lld \
    -Wl,--gc-sections \
    "${new_args[@]}" \
    "${asan_cxx_runtime[@]}" \
    -l:libglibc_compat.a \
    -Wl,--exclude-libs,libglibc_compat.a \
    "${exe_flags[@]}"
