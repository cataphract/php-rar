#!/bin/sh
# Wrapper for clang++ targeting musl libc with the correct sysroot and flags.
# Use this as CXX= for autoconf-based builds.
# -lglibc_compat is always appended so every binary works on both musl and glibc.
ARCH=$(uname -m)
SYSROOT="/sysroot/${ARCH}-none-linux-musl"
LLVM_MAJOR=$(clang --version | grep -oE '[0-9]+' | head -1)
BUILTINS_DIR="/usr/lib/llvm${LLVM_MAJOR}/lib/clang/${LLVM_MAJOR}/lib/linux"

exec clang++ \
    --target="${ARCH}-none-linux-musl" \
    --sysroot="${SYSROOT}" \
    -stdlib=libc++ \
    -rtlib=compiler-rt \
    -unwindlib=libunwind \
    -fno-omit-frame-pointer \
    -Qunused-arguments \
    -fuse-ld=lld \
    "$@" \
    -L"${SYSROOT}/usr/lib" \
    -L"${BUILTINS_DIR}" \
    -lglibc_compat \
    -lclang_rt.builtins-${ARCH}
