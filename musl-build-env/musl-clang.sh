#!/bin/bash

set -euo pipefail

driver=clang
cxx=false
case "${0##*/}" in
    *++)
        driver=clang++
        cxx=true
        ;;
esac

# Derived images can install a declarative policy without editing this script.
# The file may assign MUSL_CLANG_SANITIZE_CXX and the two Bash arrays below.
MUSL_CLANG_COMPILE_FLAGS=()
MUSL_CLANG_LINK_FLAGS=()
if [[ -r /etc/musl-clang.conf ]]; then
    # shellcheck source=/dev/null
    source /etc/musl-clang.conf
fi

common_flags=(-fno-omit-frame-pointer)
if $cxx; then
    common_flags+=(-stdlib=libc++)
fi

# Do not pass link-only policy to dependency generation, preprocessing,
# assembly, syntax checks, or object compilation.
compile_only=false
for arg in "$@"; do
    case "$arg" in
        -c|-S|-E|-M|-MM|-fsyntax-only)
            compile_only=true
            break
            ;;
    esac
done

if $compile_only; then
    exec "$driver" \
        "${common_flags[@]}" \
        "${MUSL_CLANG_COMPILE_FLAGS[@]}" \
        "$@"
fi

asan_cxx=false
if $cxx; then
    case "${MUSL_CLANG_SANITIZE_CXX:-}" in
        *address*) asan_cxx=true ;;
    esac
fi

# In ASan C++ mode, omit explicit runtime libraries supplied by build systems.
# Clang's implicit -lc++ will resolve to /usr/asan/lib below and that DSO already
# depends on its matching libc++abi and libunwind DSOs.
link_args=("$@")
if $asan_cxx; then
    filtered_args=()
    previous_was_l=false
    for arg in "$@"; do
        if $previous_was_l; then
            case "$arg" in
                c++|c++abi|unwind) ;;
                *) filtered_args+=(-l "$arg") ;;
            esac
            previous_was_l=false
            continue
        fi
        case "$arg" in
            -l)
                previous_was_l=true
                ;;
            -lc++|-lc++abi|-lunwind)
                ;;
            *)
                filtered_args+=("$arg")
                ;;
        esac
    done
    $previous_was_l && filtered_args+=(-l)
    link_args=("${filtered_args[@]}")
fi

# An explicit -lc from the caller (e.g. libtool's C++ tag links with -nostdlib
# and spells out the runtime libraries itself) must not be resolved inside the
# -Bstatic region
rewritten_args=()
previous_was_l=false
for arg in "${link_args[@]}"; do
    if $previous_was_l; then
        previous_was_l=false
        if [[ $arg == c ]]; then
            rewritten_args+=(-Wl,--push-state -Wl,-Bdynamic -lc -Wl,--pop-state)
        else
            rewritten_args+=(-l "$arg")
        fi
        continue
    fi
    case "$arg" in
        -l)
            previous_was_l=true
            ;;
        -lc)
            rewritten_args+=(-Wl,--push-state -Wl,-Bdynamic -lc -Wl,--pop-state)
            ;;
        *)
            rewritten_args+=("$arg")
            ;;
    esac
done
$previous_was_l && rewritten_args+=(-l)
link_args=("${rewritten_args[@]}")

cxx_runtime_flags=()
if $cxx; then
    if $asan_cxx; then
        cxx_runtime_flags=(-L/usr/asan/lib -Wl,-rpath,/usr/asan/lib)
    else
        cxx_runtime_flags=(-static-libstdc++)
    fi
fi

# Start user-specified libraries in static mode, while allowing an explicit
# -Bdynamic from the caller to override that preference. Pop the state before
# Clang emits its implicit compiler runtimes and dynamic libc.
exec "$driver" \
    "${common_flags[@]}" \
    "${MUSL_CLANG_COMPILE_FLAGS[@]}" \
    -rtlib=compiler-rt \
    -unwindlib=libunwind \
    -Wl,--gc-sections \
    "${cxx_runtime_flags[@]}" \
    -Wl,--push-state \
    -Wl,-Bstatic \
    "${link_args[@]}" \
    -Wl,--pop-state \
    "${MUSL_CLANG_LINK_FLAGS[@]}"
