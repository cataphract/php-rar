# syntax=docker/dockerfile:1

# Native musl build environment with LLVM runtimes built from source. Each image
# targets its own architecture and produces binaries that can run with either
# musl or glibc after their libc dependency/interpreter is patched.
#
# Build:
#   docker build --platform=linux/arm64 -t musl-build-env:latest .
#   docker build --platform=linux/amd64 -t musl-build-env:latest .

ARG ALPINE_VERSION=3.23.4
ARG LLVM_VERSION=21.1.2

FROM alpine:${ALPINE_VERSION} AS toolchain-base

# Pin the build and final stages to the same musl revision so their libc,
# development headers, and generated runtime artifacts stay reproducible.
ARG MUSL_VERSION=1.2.5-r23
RUN apk --no-cache add \
        "musl=${MUSL_VERSION}" "musl-dev=${MUSL_VERSION}" \
        alpine-sdk bash binutils clang cmake compiler-rt \
        coreutils git linux-headers lld llvm make \
        patchelf python3 wget xz && \
    apk info -e "musl=${MUSL_VERSION}" && \
    apk info -e "musl-dev=${MUSL_VERSION}"

# Clang searches its own program directory before /usr/bin. Supplying ld there
# makes LLD the Clang default without changing GCC's /usr/bin/ld.
RUN set -eux; \
    CLANG_BIN_DIR=$(dirname "$(readlink -f "$(command -v clang)")"); \
    ln -s /usr/bin/ld.lld "${CLANG_BIN_DIR}/ld"

FROM toolchain-base AS builder

ARG LLVM_VERSION

# Patch the headers used to build both LLVM runtimes and compatibility objects.
COPY locale.h.diff alltypes.h.diff /build-input/
RUN cd /usr/include && \
    patch -p0 < /build-input/locale.h.diff && \
    patch -p0 < /build-input/alltypes.h.diff

# Download, verify, unpack, and patch the LLVM source. The patched source tree is
# retained in the final image, but its build directories are removed first.
COPY CHECKSUMS /build-input/CHECKSUMS
RUN cd / && \
    wget -q "https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz" && \
    grep -F "llvm-project-${LLVM_VERSION}.src.tar.xz" /build-input/CHECKSUMS | sha512sum -c - && \
    tar -xf "llvm-project-${LLVM_VERSION}.src.tar.xz" && \
    rm "llvm-project-${LLVM_VERSION}.src.tar.xz"

# These compiler-rt patches make sanitizer runtimes built against musl work on
# both libc implementations:
#   - tls:      include musl/glibc pthread metadata in the sanitizer TLS range
#               so LSan sees pointers stored by pthread_setspecific.
#   - msghdr:   account for musl's narrower msg_iovlen/msg_controllen fields.
#   - rlimit64: define sizes for musl's non-distinct *64 resource structures.
#   - stat:     intercept both musl's public APIs and glibc compatibility paths.
COPY compiler-rt-musl-tls.diff compiler-rt-musl-msghdr.diff \
     compiler-rt-musl-rlimit64.diff compiler-rt-musl-stat.diff /build-input/
RUN cd "/llvm-project-${LLVM_VERSION}.src" && \
    patch -p1 < /build-input/compiler-rt-musl-tls.diff && \
    patch -p1 < /build-input/compiler-rt-musl-msghdr.diff && \
    patch -p1 < /build-input/compiler-rt-musl-rlimit64.diff && \
    patch -p1 < /build-input/compiler-rt-musl-stat.diff

# libunwind needs the glibc _dl_find_object ABI declaration when built on musl,
# while retaining its runtime fallback to dl_iterate_phdr.
COPY llvm-libunwind-dl-find-object-runtime.patch /build-input/
RUN cd "/llvm-project-${LLVM_VERSION}.src" && \
    patch -p1 < /build-input/llvm-libunwind-dl-find-object-runtime.patch

# First build: compiler-rt plus static libc++/libc++abi/libunwind. Install into
# an explicit staging root, then expose those files inside the builder so the
# ASan-instrumented runtime build below can link against them.
RUN set -eux; \
    SOURCE_DIR="/llvm-project-${LLVM_VERSION}.src"; \
    CLANG_RESOURCE_DIR=$(clang -print-resource-dir); \
    COMPILER_TARGET=$(clang -dumpmachine); \
    cmake -S "${SOURCE_DIR}/runtimes" -B "${SOURCE_DIR}/build" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_COMPILER_TARGET="${COMPILER_TARGET}" \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_COMPILER_TARGET="${COMPILER_TARGET}" \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1" \
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
        -DCOMPILER_RT_CXX_LIBRARY=libcxx \
        -DCOMPILER_RT_STATIC_CXX_LIBRARY=ON \
        -DCOMPILER_RT_USE_LLVM_UNWINDER=ON \
        -DSANITIZER_CXX_ABI=libc++ \
        -DSANITIZER_CXX_ABI_INTREE=ON \
        -DSANITIZER_USE_STATIC_CXX_ABI=ON \
        -DSANITIZER_USE_STATIC_LLVM_UNWINDER=ON \
        -DCOMPILER_RT_USE_BUILTINS_LIBRARY=ON \
        -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
        -DCOMPILER_RT_INSTALL_PATH="${CLANG_RESOURCE_DIR}" \
        -DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
        -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
        -DCOMPILER_RT_BUILD_MEMPROF=OFF \
        -DCOMPILER_RT_BUILD_ORC=OFF \
        -DCOMPILER_RT_BUILD_XRAY=OFF \
        -DCOMPILER_RT_INCLUDE_TESTS=OFF \
        -DLLVM_ENABLE_RUNTIMES="compiler-rt;libcxx;libcxxabi;libunwind"; \
    DESTDIR=/opt/runtime-root cmake --build "${SOURCE_DIR}/build" \
        --parallel "$(nproc)" \
        --target install-unwind install-cxxabi install-cxx install-compiler-rt; \
    rm -f /opt/runtime-root/usr/lib/libc++experimental.a; \
    STAGED_RESOURCE_LIB="/opt/runtime-root${CLANG_RESOURCE_DIR}/lib"; \
    mv "${STAGED_RESOURCE_LIB}/linux" \
       "${STAGED_RESOURCE_LIB}/${COMPILER_TARGET}"; \
    ln -s "${COMPILER_TARGET}" "${STAGED_RESOURCE_LIB}/linux"; \
    cp -a /opt/runtime-root/. /

# Second build: shared libc++/libc++abi/libunwind with AddressSanitizer (+vptr)
# instrumentation and extensive libc++ hardening. The libraries intentionally
# retain undefined __asan_* references for the host executable's static ASan
# runtime to satisfy.
RUN set -eux; \
    SOURCE_DIR="/llvm-project-${LLVM_VERSION}.src"; \
    cmake -S "${SOURCE_DIR}/runtimes" -B "${SOURCE_DIR}/build-asan" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr/asan \
        -DLLVM_USE_SANITIZER=Address \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1 -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE" \
        -DLIBCXX_ADDITIONAL_COMPILE_FLAGS="-fsanitize=vptr" \
        -DCMAKE_SHARED_LINKER_FLAGS="-rtlib=compiler-rt -unwindlib=libunwind" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DLIBUNWIND_ENABLE_SHARED=ON \
        -DLIBUNWIND_ENABLE_STATIC=OFF \
        -DLIBUNWIND_USE_COMPILER_RT=ON \
        -DLIBCXXABI_ENABLE_SHARED=ON \
        -DLIBCXXABI_ENABLE_STATIC=OFF \
        -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
        -DLIBCXXABI_ENABLE_STATIC_UNWINDER=OFF \
        -DLIBCXXABI_USE_COMPILER_RT=ON \
        -DLIBCXX_ENABLE_SHARED=ON \
        -DLIBCXX_ENABLE_STATIC=OFF \
        -DLIBCXX_HAS_MUSL_LIBC=ON \
        -DLIBCXX_USE_COMPILER_RT=ON \
        -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=OFF \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind"; \
    DESTDIR=/opt/asan-root cmake --build "${SOURCE_DIR}/build-asan" \
        --parallel "$(nproc)" \
        --target install-unwind install-cxxabi install-cxx; \
    rm -f /opt/asan-root/usr/asan/lib/libc++experimental.a

# Build the compatibility archive with hidden symbols (equivalent to the old
# --exclude-libs policy) and the executable-only preinit/fini object.
COPY glibc_compat.c glibc_compat_exe.c /build-input/
RUN mkdir -p /opt/compat && \
    clang -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
      -fvisibility=hidden -ffunction-sections -fdata-sections \
      -c /build-input/glibc_compat.c -o /tmp/glibc_compat.o && \
    llvm-ar rcs /opt/compat/libglibc_compat.a /tmp/glibc_compat.o && \
    clang -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
      -c /build-input/glibc_compat_exe.c \
      -o /opt/compat/glibc_compat_exe.o && \
    rm /tmp/glibc_compat.o

# Keep patched LLVM sources, but never copy build trees or their intermediates
# into the final image.
RUN rm -rf "/llvm-project-${LLVM_VERSION}.src/build" \
           "/llvm-project-${LLVM_VERSION}.src/build-asan"

FROM toolchain-base AS final

ARG LLVM_VERSION
ARG MUSL_VERSION=1.2.5-r23

COPY --from=builder /opt/runtime-root/ /
COPY --from=builder /opt/asan-root/ /
COPY --from=builder /usr/include/bits/alltypes.h /usr/include/bits/alltypes.h
COPY --from=builder /usr/include/locale.h /usr/include/locale.h
COPY --from=builder /opt/compat/libglibc_compat.a /usr/lib/libglibc_compat.a
COPY --from=builder /opt/compat/glibc_compat_exe.o /usr/lib/glibc_compat_exe.o
COPY --from=builder /llvm-project-${LLVM_VERSION}.src /llvm-project-${LLVM_VERSION}.src

COPY Toolchain.cmake /usr/local/share/musl/Toolchain.cmake
COPY glibc-*-symbols*.txt /usr/local/share/musl/
COPY musl-clang.sh /usr/local/bin/musl-clang
COPY musl-ld.sh /usr/local/bin/musl-ld
COPY libc.so.ld /usr/lib/libc.so.ld

# Integrate compatibility behavior into normal linker inputs:
#   - libc.so pulls hidden glibc_compat plus an extensible downstream archive;
#   - executable CRT objects contain the preinit/fini shim;
#   - Clang finds compiler-rt in its real resource directory;
#   - the linker wrapper adds glibc compatibility dependencies when required.
#     See SYMBOL_LISTS.md for the source versions and architecture differences.
RUN set -eux; \
    MUSL_LIBC=$(readlink -f /usr/lib/libc.so); \
    sed -i "s|@MUSL_LIBC@|${MUSL_LIBC}|" /usr/lib/libc.so.ld; \
    rm /usr/lib/libc.so; \
    mv /usr/lib/libc.so.ld /usr/lib/libc.so; \
    llvm-ar rcs /usr/lib/libadditional_compat.a; \
    ld.lld -r /usr/lib/Scrt1.o /usr/lib/glibc_compat_exe.o -o /tmp/Scrt1.o; \
    mv /tmp/Scrt1.o /usr/lib/Scrt1.o; \
    ld.lld -r /usr/lib/crt1.o /usr/lib/glibc_compat_exe.o -o /tmp/crt1.o; \
    mv /tmp/crt1.o /usr/lib/crt1.o; \
    rm /usr/lib/glibc_compat_exe.o; \
    BUILTINS=$(clang -rtlib=compiler-rt --print-libgcc-file-name); \
    ln -s "${BUILTINS}" /usr/lib/libclang_rt.builtins.a; \
    chmod +x /usr/local/bin/musl-clang /usr/local/bin/musl-ld; \
    ln -s musl-clang /usr/local/bin/musl-clang++; \
    CLANG_BIN_DIR=$(dirname "$(readlink -f "$(command -v clang)")"); \
    ln -sf /usr/local/bin/musl-ld "${CLANG_BIN_DIR}/ld"; \
    ln -sf /usr/local/bin/musl-ld "${CLANG_BIN_DIR}/ld.lld"; \
    LEGACY_TOOLCHAIN_DIR="/sysroot/$(uname -m)-none-linux-musl"; \
    mkdir -p "${LEGACY_TOOLCHAIN_DIR}"; \
    ln -s /usr/local/share/musl/Toolchain.cmake \
        "${LEGACY_TOOLCHAIN_DIR}/Toolchain.cmake"; \
    apk info -e "musl=${MUSL_VERSION}"; \
    apk info -e "musl-dev=${MUSL_VERSION}"; \
    test ! -e "/llvm-project-${LLVM_VERSION}.src/build"; \
    test ! -e "/llvm-project-${LLVM_VERSION}.src/build-asan"
