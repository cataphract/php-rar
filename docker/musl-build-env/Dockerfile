# Build environment using Alpine 3.23 with LLVM runtimes built from source.
# Provides a musl-based toolchain for building portable Linux binaries
# that work on both musl and glibc systems (via glibc_compat.a).
#
# Build:
#   docker build --build-arg ARCH=aarch64 -t musl-build-env:latest .
#   docker build --build-arg ARCH=x86_64  -t musl-build-env:latest .

FROM alpine:3.23

ARG ARCH

RUN test -n "${ARCH}" || (echo "ARCH build arg is required (aarch64 or x86_64)" && false)

# Install build tools and dependencies
RUN apk --no-cache add \
    alpine-sdk bash binutils clang cmake compiler-rt \
    coreutils git linux-headers lld llvm make \
    musl-dev patchelf python3 wget xz

# Apply musl patches for glibc ABI compatibility
COPY locale.h.diff /locale.h.diff
RUN cd /usr/include && patch -p0 < /locale.h.diff

COPY alltypes.h.diff /alltypes.h.diff
RUN cd /usr/include && patch -p0 < /alltypes.h.diff

# Download and build LLVM runtimes (libunwind, libc++abi, libc++) matching
# the system clang version so headers, ABIs and compiler-rt all align.
# Detect the full version string (e.g. 21.1.2) from the installed clang.
# Only the runtimes are built from source — the system clang is the bootstrap.
RUN set -e && \
    LLVM_VER=$(clang --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1) && \
    echo "Building LLVM ${LLVM_VER} runtimes to match system clang" && \
    wget -q "https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VER}/llvm-project-${LLVM_VER}.src.tar.xz" && \
    tar -xf "llvm-project-${LLVM_VER}.src.tar.xz" && \
    rm "llvm-project-${LLVM_VER}.src.tar.xz" && \
    cd "llvm-project-${LLVM_VER}.src" && mkdir build && cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DLIBUNWIND_ENABLE_SHARED=OFF \
        -DLIBUNWIND_ENABLE_STATIC=ON \
        -DLIBUNWIND_USE_COMPILER_RT=ON \
        -DLIBCXXABI_ENABLE_SHARED=OFF \
        -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
        -DLIBCXXABI_ENABLE_STATIC_UNWINDER=ON \
        -DLIBCXXABI_USE_COMPILER_RT=ON \
        -DLIBCXX_ENABLE_SHARED=OFF \
        -DLIBCXX_HAS_MUSL_LIBC=ON \
        -DLIBCXX_USE_COMPILER_RT=ON \
        -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
        ../runtimes && \
    make -j$(nproc) install-unwind install-cxxabi install-cxx && \
    cd / && rm -rf "/llvm-project-${LLVM_VER}.src"

# Set up symlinks using the system LLVM and GCC versions detected at build time.
# Use grep -oE (not -oP) since busybox grep lacks Perl regex support.
RUN set -e && \
    SYS_LLVM_VER=$(clang --version | grep -oE '[0-9]+' | head -1) && \
    GCC_VER=$(ls /usr/lib/gcc/${ARCH}-alpine-linux-musl/) && \
    echo "System LLVM: ${SYS_LLVM_VER}, GCC: ${GCC_VER}" && \
    GCC_DIR=/usr/lib/gcc/${ARCH}-alpine-linux-musl/${GCC_VER} && \
    CLANG_DIR=/usr/lib/llvm${SYS_LLVM_VER}/lib/clang/${SYS_LLVM_VER} && \
    ln -s ${GCC_DIR} /usr/lib/resource_dir && \
    ln -s ${CLANG_DIR}/lib /usr/lib/resource_dir/lib && \
    ln -s ${CLANG_DIR}/lib/${ARCH}-alpine-linux-musl/libclang_rt.builtins-${ARCH}.a \
        /usr/lib/libclang_rt.builtins.a && \
    GCC_STDATOMIC=${GCC_DIR}/include/stdatomic.h && \
    if [ -f "${GCC_STDATOMIC}" ]; then \
        mv "${GCC_STDATOMIC}" "${GCC_STDATOMIC}_" && \
        cp ${CLANG_DIR}/include/stdatomic.h "${GCC_STDATOMIC}"; \
    fi

# Set up sysroot directory structure.
# Programs compiled with --sysroot=/sysroot/ARCH-none-linux-musl will find
# headers and libraries via these symlinks.
RUN mkdir -p /sysroot/${ARCH}-none-linux-musl/usr && \
    ln -s /usr/lib     /sysroot/${ARCH}-none-linux-musl/usr/lib && \
    ln -s /usr/include /sysroot/${ARCH}-none-linux-musl/usr/include && \
    ln -s /lib         /sysroot/${ARCH}-none-linux-musl/lib

# Build glibc_compat.c into a static archive.
# This library ensures binaries work on both musl and glibc:
#   - On musl: musl exports __xstat and other glibc compat symbols, so the
#     wrappers in glibc_compat resolve correctly.
#   - On glibc: glibc_compat bridges the musl-compiled ABI to glibc internals.
# Always link against -lglibc_compat; the musl-clang wrapper does this by default.
COPY glibc_compat.c /sysroot/glibc_compat.c
RUN clang \
    --target=${ARCH}-none-linux-musl \
    --sysroot=/sysroot/${ARCH}-none-linux-musl \
    -rtlib=compiler-rt \
    -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
    -c /sysroot/glibc_compat.c -o /tmp/glibc_compat.o && \
    ar rcs /sysroot/${ARCH}-none-linux-musl/usr/lib/libglibc_compat.a /tmp/glibc_compat.o && \
    rm /tmp/glibc_compat.o

# Install CMake toolchain file for this architecture
COPY Toolchain.cmake.${ARCH} /sysroot/${ARCH}-none-linux-musl/Toolchain.cmake

# Install musl-clang / musl-clang++ wrapper scripts.
# These encode the target triple, sysroot, required compiler flags, and always
# link -lglibc_compat so every binary works on both musl and glibc systems.
COPY musl-clang.sh   /usr/local/bin/musl-clang
COPY musl-clang++.sh /usr/local/bin/musl-clang++
RUN chmod +x /usr/local/bin/musl-clang /usr/local/bin/musl-clang++
