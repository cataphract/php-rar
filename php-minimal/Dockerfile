# Minimal PHP build using the musl-build-env toolchain.
# PHP is compiled with --disable-all so only the CLI binary is built,
# with minimal dependencies (only musl libc + glibc_compat shim).
#
# The resulting image is used to build and test PHP extensions like php-rar.
# glibc_compat is always linked (via the musl-clang wrapper) so the binary
# and any extensions built here work on both musl and glibc systems.
#
# Build:
#   # First build musl-build-env:
#   docker build --build-arg ARCH=aarch64 -t musl-build-env:latest ../musl-build-env
#
#   # Then build this image:
#   docker build \
#     --build-arg BUILD_ENV_IMAGE=musl-build-env:latest \
#     --build-arg PHP_VERSION=8.4.6 \
#     --build-arg ARCH=aarch64 \
#     -t php-minimal:8.4-aarch64 .

ARG BUILD_ENV_IMAGE=musl-build-env:latest
FROM ${BUILD_ENV_IMAGE}

ARG PHP_VERSION=8.4.6
ARG ARCH
# PHP build variants: set these for debug/ZTS builds
ARG PHP_ENABLE_DEBUG=no
ARG PHP_ENABLE_ZTS=no

RUN test -n "${ARCH}" || (echo "ARCH build arg is required (aarch64 or x86_64)" && false)

# PHP build-time dependencies (re2c for lexer, autoconf for configure regeneration)
RUN apk --no-cache add \
    autoconf bison re2c \
    libxml2-dev openssl-dev

# Build libpcre2_compat.a and inject it into the musl-clang wrappers and
# CMake toolchain so that every extension built in this image automatically
# gets the php_pcre2_* stub layer and its dlsym constructor.
COPY pcre2_compat.c pcre2_compat.S /tmp/
RUN ARCH=$(uname -m) && \
    SYSROOT="/sysroot/${ARCH}-none-linux-musl" && \
    clang \
        --target="${ARCH}-none-linux-musl" \
        --sysroot="${SYSROOT}" \
        -rtlib=compiler-rt \
        -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
        -c /tmp/pcre2_compat.c -o /tmp/pcre2_compat_resolve.o && \
    clang \
        --target="${ARCH}-none-linux-musl" \
        --sysroot="${SYSROOT}" \
        -c /tmp/pcre2_compat.S -o /tmp/pcre2_compat_stubs.o && \
    ar rcs "${SYSROOT}/usr/lib/libpcre2_compat.a" \
        /tmp/pcre2_compat_resolve.o /tmp/pcre2_compat_stubs.o && \
    rm /tmp/pcre2_compat.c /tmp/pcre2_compat.S \
       /tmp/pcre2_compat_resolve.o /tmp/pcre2_compat_stubs.o && \
    sed -i 's/-lglibc_compat/-lglibc_compat -lpcre2_compat/' \
        /usr/local/bin/musl-clang /usr/local/bin/musl-clang++ && \
    sed -i 's/-lglibc_compat -Wl,-Bdynamic/-lglibc_compat -lpcre2_compat -Wl,-Bdynamic/' \
        "${SYSROOT}/Toolchain.cmake"

# Download PHP source
RUN wget -q "https://www.php.net/distributions/php-${PHP_VERSION}.tar.gz" && \
    tar -xf "php-${PHP_VERSION}.tar.gz" && \
    rm "php-${PHP_VERSION}.tar.gz"

# Apply version-specific source patches for compatibility with modern clang.
#
# PHP 7.x patches:
#   - reentrancy.c: configure wrongly detects HAVE_OLD_READDIR_R on musl,
#     resulting in a 2-argument readdir_r call. Patch to the standard 3-arg form.
#   - streams/cast.c: COOKIE_SEEKER_USES_OFF64_T is not detected by configure,
#     so the by-value seek variant is compiled instead of the by-pointer variant
#     that fopencookie requires on Linux. Fixed by passing -DCOOKIE_SEEKER_USES_OFF64_T.
#
# PHP 8.0 and 8.1 patches:
#   - streams/cast.c: same COOKIE_SEEKER_USES_OFF64_T issue as PHP 7.x.
RUN \
    VER_MAJOR=$(echo "${PHP_VERSION}" | cut -d. -f1) && \
    if [ "${VER_MAJOR}" = "7" ]; then \
        sed -i \
            's/readdir_r(dirp, entry);/struct dirent *_rdr; (void)readdir_r(dirp, entry, \&_rdr);/' \
            "php-${PHP_VERSION}/main/reentrancy.c"; \
    fi

# Build PHP with clang targeting musl.
# --disable-all: disable all optional extensions (minimal build).
# musl-clang wrapper encodes the target triple, sysroot, clang flags, and
# always links -lglibc_compat so the binary works on both musl and glibc.
#
# Debug builds host the sanitizer runtime for all the instrumented code in the
# process: PHP links and exports the ASan + UBSan runtimes, and the dlopen'd
# extension and the instrumented libc++ resolve __asan_*/__ubsan_* from it.
#
# This "main executable statically holds the runtime and exports it; instrumented
# .so's resolve against it at load" arrangement is the DEFAULT LLVM/Clang model,
# not a workaround: sanitizer runtimes ship as static archives, get linked into
# the -fsanitize executable, which is marked --export-dynamic. (ASan in
# particular MUST live in the main executable -- it installs interceptors and
# shadow memory before main(), so it cannot be brought in lazily via dlopen.)
#
# The one non-default wrinkle is WHICH runtime PHP has to carry. Normally the C++
# driver (clang++) is what pulls in libclang_rt.ubsan_standalone_cxx, so a
# C++-linked host (like nginx) gets it for free. PHP is a C program linked with
# the C driver, which pulls libclang_rt.ubsan_standalone (C) but NOT the _cxx
# half -- and that half holds the vptr handler
# __ubsan_handle_dynamic_type_cache_miss that the vptr-instrumented libc++.so
# (from /usr/asan) needs. So SAN_LDEXTRA links the cxx runtime explicitly:
# --whole-archive forces it in
# (comma-joined so libtool keeps the bracket), its own .syms dynamic-list
# exports exactly the __ubsan_* interface (the same list clang would use for a
# C++ program), and the shared libc++abi supplies the handler's typeinfo /
# __dynamic_cast.
RUN \
    DEBUG_FLAG=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "--enable-debug" || echo "") && \
    ZTS_FLAG=$([ "${PHP_ENABLE_ZTS}" = "yes" ] && echo "--enable-zts --disable-zend-signals" || echo "") && \
    VER_MAJOR=$(echo "${PHP_VERSION}" | cut -d. -f1) && \
    VER_MINOR=$(echo "${PHP_VERSION}" | cut -d. -f2) && \
    COMPAT_FLAGS=$([ "${VER_MAJOR}" = "7" ] || { [ "${VER_MAJOR}" = "8" ] && [ "${VER_MINOR}" -le 1 ]; } && echo "-DCOOKIE_SEEKER_USES_OFF64_T -D__off64_t=long -Doff64_t=long" || echo "") && \
    SAN_FLAGS="-fsanitize=address,undefined -fno-sanitize=function" && \
    ASAN_CFLAGS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "${SAN_FLAGS}" || echo "") && \
    UBSAN_CXX=$(ls /usr/lib/llvm*/lib/clang/*/lib/linux/libclang_rt.ubsan_standalone_cxx-*.a | head -1) && \
    SAN_LDEXTRA="-Wl,--whole-archive,${UBSAN_CXX},--no-whole-archive -Wl,--dynamic-list=${UBSAN_CXX}.syms -L/usr/asan/lib -Wl,-rpath,/usr/asan/lib /usr/asan/lib/libc++abi.so" && \
    ASAN_LDFLAGS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "${SAN_FLAGS} ${SAN_LDEXTRA}" || echo "") && \
    OPT_FLAG=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "-O0" || echo "-O2") && \
    cd "php-${PHP_VERSION}" && \
    CC=musl-clang \
    CXX=musl-clang++ \
    CFLAGS="-g ${OPT_FLAG} ${COMPAT_FLAGS} ${ASAN_CFLAGS}" \
    LDFLAGS="-fuse-ld=lld ${ASAN_LDFLAGS}" \
    ./configure \
        --disable-all \
        --disable-cgi \
        --disable-fpm \
        --without-pear \
        --enable-cli \
        --enable-session \
        --enable-pdo \
        ${DEBUG_FLAG} \
        ${ZTS_FLAG} && \
    make -j$(nproc) && \
    make install

RUN rm -rf "/php-${PHP_VERSION}"

# Ensure extension builds (phpize + configure) use our musl toolchain.
ENV CC=musl-clang CXX=musl-clang++

# For debug builds: patch the musl-clang wrappers so that extensions compiled
# inside this image (e.g. by build-and-test.sh) are ASAN/UBSan-instrumented.
#
# 1. Inject -fsanitize=address,undefined into every compile/link (matching the
#    PHP build above). -fno-sanitize=function: the function-pointer-type check
#    fires constantly on intentional type-punned calls (PHP core's zend_sort,
#    module ctors, hashtable dtors, ...) -- pure noise, disabled like upstream
#    PHP does. This also prevents the ASAN __cxa_throw interceptor crash that
#    occurs when a non-instrumented extension (with statically-linked libc++) is
#    loaded into an ASAN-instrumented PHP process. The sanitizer runtime symbols
#    (__asan_* / __ubsan_handle_*) resolve at dlopen against the export-dynamic
#    PHP binary, which is itself built with the same flags.
#
# 2. Append the compiler-rt builtins archive to the link. libtool links the
#    extension with -nostdlib, which drops the builtins clang would normally add
#    via -rtlib=compiler-rt. Sanitizer-instrumented code emits helpers that live
#    there: the aarch64 outline-atomics (__aarch64_*, used by the asan shadow
#    updates) and UBSan's overflow helpers (e.g. __muloti4). Without it the .so
#    fails to load with "symbol not found". (libc stays dynamic -- see the
#    has_static note in the musl-clang wrappers -- so the outline-atomics
#    getauxval ifunc resolver runs against the host's initialized libc.)
#
# 3. Set MUSL_CLANG_SANITIZE_CXX=address so musl-clang++ links the
#    ASAN-instrumented shared libc++/libc++abi/libunwind from /usr/asan (built in
#    musl-build-env) instead of the plain static libc++, keeping libc++'s
#    container-overflow annotations consistent with the instrumented extension.
#
# Then force the PHP binary to load that shared C++ runtime at startup
# (DT_NEEDED libc++.so.1, which pulls libc++abi.so.1 / libunwind.so.1). PHP is
# pure C and would otherwise only load libc++ late, when the extension is
# dlopen'd -- after ASAN has initialized. ASAN's __cxa_throw interceptor
# resolves the real __cxa_throw via dlsym at init; if libc++abi isn't loaded yet
# it caches NULL, and the first C++ exception thrown from the extension (e.g.
# unrar's error handler on a missing password) aborts with
# "CHECK failed: real___cxa_throw != 0". The dynamic loader loads DT_NEEDED libs
# before ASAN's preinit, so adding the dependency makes the real symbol
# available in time.
RUN if [ "${PHP_ENABLE_DEBUG}" = "yes" ]; then \
        for f in /usr/local/bin/musl-clang /usr/local/bin/musl-clang++; do \
            sed -i 's|"$@"|"-fsanitize=address,undefined" "-fno-sanitize=function" "$@"|' "$f"; \
            sed -i 's#-l:libglibc_compat.a#-l:libglibc_compat.a /usr/lib/libclang_rt.builtins.a#' "$f"; \
            sed -i '1a export MUSL_CLANG_SANITIZE_CXX=address' "$f"; \
        done; \
        patchelf --add-rpath /usr/asan/lib /usr/local/bin/php; \
        patchelf --add-needed libc++.so.1 /usr/local/bin/php; \
    fi

# Verify: PHP binary should depend only on musl libc
RUN php --version
RUN echo "Dynamic dependencies of PHP binary:" && \
    ldd /usr/local/bin/php && \
    echo "" && \
    NON_MUSL=$(ldd /usr/local/bin/php | grep -v 'musl\|linux-vdso\|ld-musl\|statically' || true) && \
    if [ -n "$NON_MUSL" ]; then \
        echo "WARNING: unexpected non-musl dependencies: $NON_MUSL"; \
    else \
        echo "OK: PHP depends only on musl libc"; \
    fi

# vim: set et:
