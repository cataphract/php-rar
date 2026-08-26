# syntax=docker/dockerfile:1

# Native musl build environment with LLVM runtimes built from source. Each image
# targets its own architecture and produces binaries that can run with either
# musl or glibc after an executable's interpreter is patched.
#
# Build:
#   docker build --platform=linux/arm64 -t musl-build-env:latest .
#   docker build --platform=linux/amd64 -t musl-build-env:latest .

ARG ALPINE_VERSION=3.23.4
ARG LLVM_VERSION=21.1.2
ARG OMIT_COMPILER_RT_PATCH=

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
ARG OMIT_COMPILER_RT_PATCH

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
# native musl. Running these sanitizer binaries on glibc is not supported.
#   - tls:      include musl pthread metadata in the sanitizer TLS range so
#               LSan sees pointers stored by pthread_setspecific.
#   - msghdr:   account for musl's narrower msg_iovlen/msg_controllen fields.
#   - rlimit64: define sizes for musl's non-distinct *64 resource structures.
#   - stat:     intercept musl's public stat-family APIs.
COPY compiler-rt-musl-tls.diff compiler-rt-musl-msghdr.diff \
     compiler-rt-musl-rlimit64.diff compiler-rt-musl-stat.diff /build-input/
RUN set -eux; \
    case "${OMIT_COMPILER_RT_PATCH}" in \
        ""|tls|msghdr|rlimit64|stat) ;; \
        *) echo "Unknown compiler-rt patch: ${OMIT_COMPILER_RT_PATCH}" >&2; \
           exit 1 ;; \
    esac; \
    cd "/llvm-project-${LLVM_VERSION}.src"; \
    for patch_name in tls msghdr rlimit64 stat; do \
        if [ "${OMIT_COMPILER_RT_PATCH}" != "${patch_name}" ]; then \
            patch -p1 < "/build-input/compiler-rt-musl-${patch_name}.diff"; \
        fi; \
    done

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

# Third build: the same shared runtimes with MemorySanitizer instrumentation,
# which C++ links need because MSan reports uninstrumented standard library code
# as uninitialized. No -fsanitize=vptr here: it would make the DSOs import
# __ubsan_* symbols that a plain -fsanitize=memory executable link does not
# provide. Plain Memory, not MemoryWithOrigins, to match those executables.
RUN set -eux; \
    SOURCE_DIR="/llvm-project-${LLVM_VERSION}.src"; \
    cmake -S "${SOURCE_DIR}/runtimes" -B "${SOURCE_DIR}/build-msan" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr/msan \
        -DLLVM_USE_SANITIZER=Memory \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1 -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE" \
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
    DESTDIR=/opt/msan-root cmake --build "${SOURCE_DIR}/build-msan" \
        --parallel "$(nproc)" \
        --target install-cxxabi install-cxx; \
    rm -f /opt/msan-root/usr/msan/lib/libc++experimental.a

# The unwinder above is built only because LIBCXXABI_USE_LLVM_UNWINDER requires
# it. The installed one must not be instrumented: the MSan runtime unwinds
# through it to report, and instrumented unwinding recurses back into the
# runtime until the stack is exhausted.
RUN set -eux; \
    SOURCE_DIR="/llvm-project-${LLVM_VERSION}.src"; \
    cmake -S "${SOURCE_DIR}/runtimes" -B "${SOURCE_DIR}/build-msan-unwind" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr/msan \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" \
        -DCMAKE_SHARED_LINKER_FLAGS="-rtlib=compiler-rt" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DLIBUNWIND_ENABLE_SHARED=ON \
        -DLIBUNWIND_ENABLE_STATIC=OFF \
        -DLIBUNWIND_USE_COMPILER_RT=ON \
        -DLLVM_ENABLE_RUNTIMES="libunwind"; \
    DESTDIR=/opt/msan-root cmake --build "${SOURCE_DIR}/build-msan-unwind" \
        --parallel "$(nproc)" \
        --target install-unwind

# Build the compatibility archive with hidden symbols (equivalent to the old
# --exclude-libs policy) and the executable-only preinit/fini object.
#
# The compiler may lower operations in glibc_compat.o to compiler-rt builtins.
# Embed the required members privately so the compatibility object's references
# are resolved without letting that runtime satisfy references from callers.
COPY glibc_compat.c glibc_compat_exe.c embed-private-archive-members \
     /build-input/
RUN set -eux; \
    mkdir -p /opt/compat; \
    clang -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
      -fvisibility=hidden -ffunction-sections -fdata-sections \
      -c /build-input/glibc_compat.c -o /tmp/glibc_compat.o; \
    BUILTINS=$(clang -rtlib=compiler-rt --print-libgcc-file-name); \
    sh /build-input/embed-private-archive-members \
      /tmp/glibc_compat.o "${BUILTINS}"; \
    llvm-ar rcs /opt/compat/libglibc_compat.a /tmp/glibc_compat.o; \
    clang -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
      -c /build-input/glibc_compat_exe.c \
      -o /opt/compat/glibc_compat_exe.o; \
    rm /tmp/glibc_compat.o

# Keep patched LLVM production sources for debugging, but never copy build
# trees or the 1.6 GiB of test inputs and unit-test sources into the final image.
RUN set -eux; \
    SOURCE_DIR="/llvm-project-${LLVM_VERSION}.src"; \
    rm -rf "${SOURCE_DIR}/build" \
           "${SOURCE_DIR}/build-asan" \
           "${SOURCE_DIR}/build-msan" \
           "${SOURCE_DIR}/build-msan-unwind"; \
    find "${SOURCE_DIR}" -type d \
        \( -name test -o -name tests -o -name unittests \) \
        -prune -exec rm -rf {} +

FROM toolchain-base AS final

ARG LLVM_VERSION
ARG MUSL_VERSION=1.2.5-r23

COPY --from=builder /opt/runtime-root/ /
COPY --from=builder /opt/asan-root/ /
COPY --from=builder /opt/msan-root/ /
COPY --from=builder /usr/include/bits/alltypes.h /usr/include/bits/alltypes.h
COPY --from=builder /usr/include/locale.h /usr/include/locale.h
COPY --from=builder /opt/compat/libglibc_compat.a /usr/lib/libglibc_compat.a
COPY --from=builder /opt/compat/glibc_compat_exe.o /usr/lib/glibc_compat_exe.o
COPY --from=builder /llvm-project-${LLVM_VERSION}.src /llvm-project-${LLVM_VERSION}.src

COPY Toolchain.cmake /usr/local/share/musl/Toolchain.cmake
COPY glibc-*-symbols*.txt /usr/local/share/musl/
COPY create-glibc-link-facades /usr/local/bin/create-glibc-link-facades
COPY musl-clang.sh /usr/local/bin/musl-clang
COPY libc.so.ld /usr/lib/libc.so.ld
COPY sanitizer-libc.so.ld /usr/lib/sanitizer-libc.so.ld

# Integrate compatibility behavior into normal linker inputs:
#   - libc.so pulls hidden glibc_compat plus an extensible downstream archive;
#   - executable CRT objects contain the preinit/fini shim;
#   - Clang finds compiler-rt in its real resource directory;
#   - linker-only facades select portable glibc dependency names in one pass.
#     See SYMBOL_LISTS.md for their source versions and architecture differences.
# ASan and MSan builds put /usr/{asan,msan}/lib first on the link line. The libc
# entry there omits libglibc_compat.a while retaining the downstream archive;
# its sole DSO input is the libc facade because sanitizer binaries are native
# musl only.
RUN set -eux; \
    MUSL_LOADER="/lib/ld-musl-$(uname -m).so.1"; \
    MUSL_ALIAS="/lib/libc.musl-$(uname -m).so.1"; \
    LOADER_CHECKSUM=$(sha256sum "${MUSL_LOADER}" | awk '{print $1}'); \
    LOADER_SONAME=$(readelf -d "${MUSL_LOADER}" | sed -n \
        's/.*Library soname: \[\(.*\)\]/\1/p'); \
    ALIAS_TARGET=$(readlink "${MUSL_ALIAS}"); \
    test -n "${LOADER_SONAME}"; \
    chmod +x /usr/local/bin/create-glibc-link-facades; \
    /usr/local/bin/create-glibc-link-facades; \
    for SANITIZER_LIB_DIR in /usr/asan/lib /usr/msan/lib; do \
        mkdir -p "${SANITIZER_LIB_DIR}"; \
        cp /usr/lib/sanitizer-libc.so.ld "${SANITIZER_LIB_DIR}/libc.so"; \
    done; \
    rm /usr/lib/sanitizer-libc.so.ld; \
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
    chmod +x /usr/local/bin/musl-clang; \
    ln -s musl-clang /usr/local/bin/musl-clang++; \
    LEGACY_TOOLCHAIN_DIR="/sysroot/$(uname -m)-none-linux-musl"; \
    mkdir -p "${LEGACY_TOOLCHAIN_DIR}"; \
    ln -s /usr/local/share/musl/Toolchain.cmake \
        "${LEGACY_TOOLCHAIN_DIR}/Toolchain.cmake"; \
    apk info -e "musl=${MUSL_VERSION}"; \
    apk info -e "musl-dev=${MUSL_VERSION}"; \
    test "$(sha256sum "${MUSL_LOADER}" | awk '{print $1}')" = \
        "${LOADER_CHECKSUM}"; \
    test "$(readelf -d "${MUSL_LOADER}" | sed -n \
        's/.*Library soname: \[\(.*\)\]/\1/p')" = "${LOADER_SONAME}"; \
    test "$(readlink "${MUSL_ALIAS}")" = "${ALIAS_TARGET}"; \
    test ! -e "/llvm-project-${LLVM_VERSION}.src/build"; \
    test ! -e "/llvm-project-${LLVM_VERSION}.src/build-asan"; \
    test ! -e "/llvm-project-${LLVM_VERSION}.src/build-msan"; \
    test ! -e "/llvm-project-${LLVM_VERSION}.src/build-msan-unwind"
