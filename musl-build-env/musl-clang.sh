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
# The file may assign MUSL_CLANG_SANITIZE and the two Bash arrays below.
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

# For ASan and MSan, the selected library directory goes first on every link,
# C and C++ alike. Its reduced libc.so omits wrappers that override weak
# interceptors and selects only the libc facade for native-musl execution.
#
# Only this policy setting drives the choice; explicit -fsanitize=address or
# -fsanitize=memory arguments are deliberately not inspected.
sanitizer_lib_dir=
sanitized_cxx=false
case "${MUSL_CLANG_SANITIZE:-}" in
    *address*)
        sanitizer_lib_dir=/usr/asan/lib
        $cxx && sanitized_cxx=true
        ;;
    *memory*)
        sanitizer_lib_dir=/usr/msan/lib
        $cxx && sanitized_cxx=true
        ;;
esac

# In sanitized C++ mode, omit explicit runtime libraries supplied by build
# systems. Clang's implicit -lc++ will resolve to the sanitizer directory, and
# that DSO already depends on its matching libc++abi and libunwind DSOs.
link_args=("$@")
if $sanitized_cxx; then
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

# An explicit split-library option is also a runtime-selection request. On
# glibc before 2.34, putting that library before libc preserves the requested
# library selection even when libc exports the same symbols.
pthread_driver_flag=false
pthread_library_flag=false
math_library_flag=false
default_libraries=true
dynamic_output=true
previous_was_l=false
link_options=(
    "${MUSL_CLANG_COMPILE_FLAGS[@]}"
    "${link_args[@]}"
    "${MUSL_CLANG_LINK_FLAGS[@]}"
)
for arg in "${link_options[@]}"; do
    if $previous_was_l; then
        case "$arg" in
            pthread) pthread_library_flag=true ;;
            m) math_library_flag=true ;;
        esac
        previous_was_l=false
        continue
    fi
    case "$arg" in
        -l)
            previous_was_l=true
            ;;
        -pthread)
            pthread_driver_flag=true
            ;;
        -lpthread)
            pthread_library_flag=true
            ;;
        -lm)
            math_library_flag=true
            ;;
        -nostdlib|-nodefaultlibs)
            default_libraries=false
            ;;
        -static|-static-pie|-r|-Wl,-r|-Wl,-r,*|-Wl,--relocatable|\
        -Wl,--relocatable,*)
            dynamic_output=false
            ;;
    esac
done

runtime_selection_flags=()
if $default_libraries; then
    runtime_selection_flags=(-rtlib=compiler-rt -unwindlib=libunwind)
fi

force_pthread=false
if $pthread_library_flag ||
   { $pthread_driver_flag && $default_libraries; }; then
    force_pthread=true
fi

pthread_link_flags=()
if $dynamic_output && $force_pthread; then
    pthread_link_flags=(
        -Wl,--push-state
        -Wl,-Bdynamic
        -Wl,--no-as-needed
        /usr/lib/glibc-compat-libpthread.so.0
        -Wl,--pop-state
    )
fi

math_link_flags=()
if $dynamic_output && $math_library_flag; then
    math_link_flags=(
        -Wl,--push-state
        -Wl,-Bdynamic
        -Wl,--no-as-needed
        /usr/lib/glibc-compat-libm.so.6
        -Wl,--pop-state
    )
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

sanitizer_link_flags=()
if [[ -n $sanitizer_lib_dir ]]; then
    sanitizer_link_flags=(
        -L"$sanitizer_lib_dir" -Wl,-rpath,"$sanitizer_lib_dir")
fi

# In sanitized C++ mode the instrumented shared libc++ comes from the directory
# above instead of the plain static one.
cxx_runtime_flags=()
if $cxx && ! $sanitized_cxx; then
    cxx_runtime_flags=(-static-libstdc++)
fi

# Start user-specified libraries in static mode, while allowing an explicit
# -Bdynamic from the caller to override that preference. Pop the state before
# Clang emits its implicit compiler runtimes and dynamic libc.
exec "$driver" \
    "${common_flags[@]}" \
    "${MUSL_CLANG_COMPILE_FLAGS[@]}" \
    "${runtime_selection_flags[@]}" \
    -Wl,--gc-sections \
    -Wl,-z,nocopyreloc \
    "${sanitizer_link_flags[@]}" \
    "${cxx_runtime_flags[@]}" \
    -Wl,--push-state \
    -Wl,-Bstatic \
    "${link_args[@]}" \
    -Wl,--pop-state \
    "${MUSL_CLANG_LINK_FLAGS[@]}" \
    "${pthread_link_flags[@]}" \
    "${math_link_flags[@]}"
