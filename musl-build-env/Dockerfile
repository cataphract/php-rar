# Build environment using Alpine 3.23 with LLVM runtimes built from source.
# Provides a musl-based toolchain for building portable Linux binaries
# that work on both musl and glibc systems (via glibc_compat.a).
#
# Build:
#   docker build --build-arg ARCH=aarch64 -t musl-build-env:latest .
#   docker build --build-arg ARCH=x86_64  -t musl-build-env:latest .

FROM alpine:3.23.4

# LLVM is pinned (and the tarball checksum verified) so the runtimes we build
# from source match the system clang Alpine 3.23.4 ships. Bump both together.
ARG LLVM_VERSION=21.1.2
ARG ARCH

RUN test -n "${ARCH}" || (echo "ARCH build arg is required (aarch64 or x86_64)" && false)

# Install build tools and dependencies. The system clang and llvm/lld are the
# actual compiler and linker used throughout -- they are NOT rebuilt. Only the
# runtime libraries are rebuilt from source below: compiler-rt (builtins +
# sanitizer runtimes) and libc++/libc++abi/libunwind, so they are musl-ABI
# correct and (for the ASan variant) instrumented. The apk compiler-rt package
# is the one bootstrap-only piece: it provides builtins so clang can link during
# the from-source compiler-rt build, then our build overwrites it in the clang
# resource dir.
RUN apk --no-cache add \
    alpine-sdk bash binutils clang cmake compiler-rt \
    coreutils git linux-headers lld llvm make \
    musl-dev patchelf python3 wget xz

# Apply musl patches for glibc ABI compatibility
COPY locale.h.diff /locale.h.diff
RUN cd /usr/include && patch -p0 < /locale.h.diff

COPY alltypes.h.diff /alltypes.h.diff
RUN cd /usr/include && patch -p0 < /alltypes.h.diff

# Download, verify and unpack the LLVM source.
COPY CHECKSUMS /CHECKSUMS
RUN wget -q "https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz" && \
    grep -F "llvm-project-${LLVM_VERSION}.src.tar.xz" /CHECKSUMS | sha512sum -c - && \
    tar -xf "llvm-project-${LLVM_VERSION}.src.tar.xz" && \
    rm "llvm-project-${LLVM_VERSION}.src.tar.xz"

# Patch compiler-rt for musl. These fix the sanitizer runtimes when built
# against musl rather than glibc:
#   - tls:      include musl/glibc pthread metadata in the sanitizer TLS range
#               so LSan sees pointers stored by pthread_setspecific.
#   - msghdr:   musl's 4-byte msg_iovlen/msg_controllen vs glibc's 8-byte
#               (otherwise ASAN's recvmsg/sendmsg interceptor reads padding as a
#               high word and aborts with a ~81 GB length).
#   - rlimit64: musl has no separate *64 resource/statvfs structs; define the
#               sizes so the runtime links against musl.
#   - stat:     musl exports plain stat/fstat (not glibc's __xstat wrappers), so
#               enable the stat interceptors there too.
COPY compiler-rt-musl-tls.diff compiler-rt-musl-msghdr.diff \
     compiler-rt-musl-rlimit64.diff compiler-rt-musl-stat.diff /
RUN cd "llvm-project-${LLVM_VERSION}.src" && \
    patch -p1 < /compiler-rt-musl-tls.diff && \
    patch -p1 < /compiler-rt-musl-msghdr.diff && \
    patch -p1 < /compiler-rt-musl-rlimit64.diff && \
    patch -p1 < /compiler-rt-musl-stat.diff

# Patch libunwind for musl. glibc 2.35 added _dl_find_object and LLVM libunwind
# probes for it with dlsym at runtime, but musl's <link.h> doesn't declare the
# glibc-only ABI, so the source fails to compile. The patch provides the
# compatible public struct/prototype and keeps the runtime fallback to
# dl_iterate_phdr when _dl_find_object is absent.
COPY llvm-libunwind-dl-find-object-runtime.patch /
RUN cd "llvm-project-${LLVM_VERSION}.src" && \
    patch -p1 < /llvm-libunwind-dl-find-object-runtime.patch

# First build: compiler-rt (builtins + sanitizer runtimes) and the static C++
# runtime (libunwind, libc++abi, libc++) from source against musl, matching the
# system clang. libc++ merges libc++abi (ENABLE_STATIC_ABI_LIBRARY=ON) so a bare
# -lc++ is self-contained; the sanitizer runtimes use the static C++ ABI and
# unwinder to match.
RUN set -e && \
    LLVM_MAJOR=${LLVM_VERSION%%.*} && \
    cd "llvm-project-${LLVM_VERSION}.src" && mkdir build && cd build && \
    cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_COMPILER_TARGET=${ARCH}-alpine-linux-musl \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_COMPILER_TARGET=${ARCH}-alpine-linux-musl \
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
        -DCOMPILER_RT_INSTALL_PATH=/usr/lib/llvm${LLVM_MAJOR}/lib/clang/${LLVM_MAJOR} \
        -DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
        -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
        -DCOMPILER_RT_BUILD_MEMPROF=OFF \
        -DCOMPILER_RT_BUILD_ORC=OFF \
        -DCOMPILER_RT_BUILD_XRAY=OFF \
        -DCOMPILER_RT_INCLUDE_TESTS=OFF \
        -DLLVM_ENABLE_RUNTIMES="compiler-rt;libcxx;libcxxabi;libunwind" \
        ../runtimes && \
    make -j$(nproc) install-unwind install-cxxabi install-cxx && \
    make -j$(nproc) install-compiler-rt

# Second build: build ONLY the shared libc++/libc++abi/libunwind (.so) libraries
# with AddressSanitizer (+ vptr) instrumentation and libc++ assertions enabled,
# installed under /usr/asan. Having the C++ runtime ASAN-instrumented keeps the
# container-overflow annotations it emits consistent with ASAN-instrumented
# consumers (e.g. the rar extension), eliminating false-positive
# container-overflow reports. We use LLVM_USE_SANITIZER=Address (rather than
# -shared-libasan) so the .so's are instrumented but link against no ASAN
# runtime: their __asan_* references stay undefined and resolve at load from the
# static ASAN runtime already present in the host process (PHP) -- there is no
# second runtime. LLVM_USE_SANITIZER also drops the -Wl,-z,defs that
# HandleLLVMOptions otherwise forces on every shared-lib link (which would
# reject those undefined __asan_* symbols). The musl-clang++ wrapper links these
# instead of the static libc++ when MUSL_CLANG_SANITIZE_CXX requests it.
RUN set -e && \
    LLVM_MAJOR=${LLVM_VERSION%%.*} && \
    cd "llvm-project-${LLVM_VERSION}.src" && mkdir build-asan && cd build-asan && \
    cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr/asan \
        -DLLVM_USE_SANITIZER=Address \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_C_COMPILER_TARGET=${ARCH}-alpine-linux-musl \
        -DCMAKE_C_FLAGS="-fno-omit-frame-pointer -D_LIBCPP_HAS_NO_C11_ALIGNED_ALLOC=1" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_COMPILER_TARGET=${ARCH}-alpine-linux-musl \
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
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
        ../runtimes && \
    make -j$(nproc) install-unwind install-cxxabi install-cxx

# libc++experimental.a is never linked and pulls in symbols we don't ship.
RUN rm -f /usr/lib/libc++experimental.a /usr/asan/lib/libc++experimental.a

RUN rm -rf "/llvm-project-${LLVM_VERSION}.src"

# Set up symlinks for the resource dir and builtins. compiler-rt installs into
# clang/<major>/lib/linux (COMPILER_RT_DEFAULT_TARGET_ONLY=ON).
# Use grep -oE (not -oP) since busybox grep lacks Perl regex support.
RUN set -e && \
    LLVM_MAJOR=${LLVM_VERSION%%.*} && \
    GCC_VER=$(ls /usr/lib/gcc/${ARCH}-alpine-linux-musl/) && \
    echo "LLVM: ${LLVM_MAJOR}, GCC: ${GCC_VER}" && \
    GCC_DIR=/usr/lib/gcc/${ARCH}-alpine-linux-musl/${GCC_VER} && \
    CLANG_DIR=/usr/lib/llvm${LLVM_MAJOR}/lib/clang/${LLVM_MAJOR} && \
    ln -s ${GCC_DIR} /usr/lib/resource_dir && \
    ln -s ${CLANG_DIR}/lib /usr/lib/resource_dir/lib && \
    ln -s ${CLANG_DIR}/lib/linux/libclang_rt.builtins-${ARCH}.a \
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
COPY glibc_compat.c glibc_compat_exe.c /sysroot/
RUN CFLAGS="--target=${ARCH}-none-linux-musl \
      --sysroot=/sysroot/${ARCH}-none-linux-musl \
      -rtlib=compiler-rt \
      -fpie -O2 -fno-omit-frame-pointer -ggdb3" && \
    LIBDIR=/sysroot/${ARCH}-none-linux-musl/usr/lib && \
    # Base archive: safe for both shared libraries and executables.
    # -ffunction-sections -fdata-sections puts each function/global in its own
    # ELF section so the linker (with --gc-sections) can drop unreferenced ones.
    clang ${CFLAGS} -ffunction-sections -fdata-sections \
      -c /sysroot/glibc_compat.c -o /tmp/glibc_compat.o && \
    ar rcs ${LIBDIR}/libglibc_compat.a /tmp/glibc_compat.o && \
    # Executable-only object: adds a .preinit_array entry that calls
    # .init_array constructors.
    # Shipped as a .o (not .a) so the linker always includes it — the
    # .preinit_array section has no referenced symbols.
    clang ${CFLAGS} \
      -c /sysroot/glibc_compat_exe.c -o ${LIBDIR}/glibc_compat_exe.o && \
    rm /tmp/glibc_compat.o

# Install CMake toolchain file for this architecture
COPY Toolchain.cmake.${ARCH} /sysroot/${ARCH}-none-linux-musl/Toolchain.cmake

# Install musl-clang / musl-clang++ wrapper scripts.
# These encode the target triple, sysroot, required compiler flags, and always
# link -lglibc_compat so every binary works on both musl and glibc systems.
COPY musl-clang.sh   /usr/local/bin/musl-clang
COPY musl-clang++.sh /usr/local/bin/musl-clang++
RUN chmod +x /usr/local/bin/musl-clang /usr/local/bin/musl-clang++

# compiler-rt's musl TLS patch reads map_base/map_size from musl's private
# pthread layout. Fail the build if the package revision with that layout moves.
ARG MUSL_VERSION=1.2.5-r23
RUN apk info -e "musl=${MUSL_VERSION}" && \
    apk info -e "musl-dev=${MUSL_VERSION}"
