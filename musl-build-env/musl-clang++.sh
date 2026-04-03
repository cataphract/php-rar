#!/bin/bash

set -eo pipefail
ARCH=$(uname -m)
SYSROOT="/sysroot/${ARCH}-none-linux-musl"

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
    for _dir in "${lib_paths[@]}"; do
        [[ -f "$_dir/lib${1}.a" ]] && return 0
    done
    return 1
}

# Rewrite arguments: for each -l flag, if a .a exists prefer it via
# --push-state -Bstatic ... --pop-state.  Everything else passes through.
new_args=()
prev_was_l=false
for arg in "$@"; do
    if $prev_was_l; then
        if has_static "$arg"; then
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
            if has_static "$name"; then
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
    -l:libglibc_compat.a \
    -Wl,--exclude-libs,libglibc_compat.a \
    "${exe_flags[@]}"
