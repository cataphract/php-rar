# Minimal PHP build using the musl-build-env toolchain.
# PHP is compiled with --disable-all so only the CLI binary is built,
# with minimal dependencies (only musl libc + glibc_compat shim).
#
# The resulting image is used to build and test PHP extensions like php-rar.
# glibc_compat is injected by the build environment's libc linker script so the
# binary and any extensions built here work on both musl and glibc systems.
#
# Build:
#   # First build musl-build-env:
#   docker build --platform=linux/arm64 -t musl-build-env:latest ../musl-build-env
#
#   # Then build this image:
#   docker build \
#     --build-arg BUILD_ENV_IMAGE=musl-build-env:latest \
#     --build-arg PHP_VERSION=8.4.6 \
#     -t php-minimal:8.4-aarch64 .

ARG BUILD_ENV_IMAGE=musl-build-env:latest
FROM ${BUILD_ENV_IMAGE}

ARG PHP_VERSION=8.4.6
# PHP build variants: set these for debug/ZTS builds
ARG PHP_ENABLE_DEBUG=no
ARG PHP_ENABLE_ZTS=no

# PHP build-time dependencies (re2c for lexer, autoconf for configure regeneration)
RUN apk --no-cache add \
    autoconf bison re2c \
    libxml2-dev openssl-dev

# Add the php_pcre2_* stub layer to the compatibility extension archive already
# referenced by the build environment's libc.so linker script. This applies to
# wrappers, raw Clang, CMake, and other compiler frontends without editing them.
COPY pcre2_compat.c pcre2_compat.S /tmp/
RUN clang \
        -fpie -O2 -fno-omit-frame-pointer -ggdb3 \
        -c /tmp/pcre2_compat.c -o /tmp/pcre2_compat_resolve.o && \
    clang \
        -c /tmp/pcre2_compat.S -o /tmp/pcre2_compat_stubs.o && \
    llvm-ar rcs /usr/lib/libadditional_compat.a \
        /tmp/pcre2_compat_resolve.o /tmp/pcre2_compat_stubs.o && \
    rm /tmp/pcre2_compat.c /tmp/pcre2_compat.S \
       /tmp/pcre2_compat_resolve.o /tmp/pcre2_compat_stubs.o

# Download PHP source
RUN wget -q "https://www.php.net/distributions/php-${PHP_VERSION}.tar.gz" && \
    tar -xf "php-${PHP_VERSION}.tar.gz" && \
    rm "php-${PHP_VERSION}.tar.gz"

# Teach PHP's bundled libtool to retain Clang's absolute compiler-rt archives
# when it reconstructs C++ links with -nostdlib. This backports the upstream fix:
# https://git.savannah.gnu.org/cgit/libtool.git/commit/?id=52f78952b9ceb46a8c95a98cd0258fe4b23ce3c1
COPY libtool-clang-rt-static-archives.patch /tmp/

# PHP through 8.2 discards caller-provided LDFLAGS before constructing the
# program link command. Backport the PHP 8.3 fix so sanitizer runtimes and their
# dynamic export list reach the final PHP executable:
# https://github.com/php/php-src/commit/cf587c1a1b6af7b693ef28f6db873d82f4055b0a
COPY php-preserve-ldflags.patch /tmp/

# PHP 7.0 defines XtOffsetOf using a null-pointer member expression before
# offsetof is available. Use Clang's builtin so UBSan does not diagnose those
# offset calculations as null member accesses during startup.
COPY php-7.0-builtin-offsetof.patch /tmp/

# Apply the libtool backport and version-specific source compatibility patches.
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
    patch -d "php-${PHP_VERSION}" -p1 \
        < /tmp/libtool-clang-rt-static-archives.patch && \
    VER_MAJOR=$(echo "${PHP_VERSION}" | cut -d. -f1) && \
    VER_MINOR=$(echo "${PHP_VERSION}" | cut -d. -f2) && \
    if [ "${VER_MAJOR}" -lt 8 ] || \
       { [ "${VER_MAJOR}" -eq 8 ] && [ "${VER_MINOR}" -le 2 ]; }; then \
        patch -d "php-${PHP_VERSION}" -p1 \
            < /tmp/php-preserve-ldflags.patch; \
    fi && \
    if [ "${VER_MAJOR}" = "7" ] && [ "${VER_MINOR}" = "0" ]; then \
        patch -d "php-${PHP_VERSION}" -p1 \
            < /tmp/php-7.0-builtin-offsetof.patch; \
    fi && \
    rm /tmp/libtool-clang-rt-static-archives.patch \
       /tmp/php-preserve-ldflags.patch \
       /tmp/php-7.0-builtin-offsetof.patch && \
    if [ "${VER_MAJOR}" = "7" ]; then \
        sed -i \
            's/readdir_r(dirp, entry);/struct dirent *_rdr; (void)readdir_r(dirp, entry, \&_rdr);/' \
            "php-${PHP_VERSION}/main/reentrancy.c"; \
    fi

# Build PHP with clang targeting musl.
# --disable-all: disable all optional extensions (minimal build).
# musl-clang applies compiler-rt/libunwind and static-library policy; the image
# layout supplies the native target, LLD, and cross-libc compatibility layer.
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
# Autoconf probes deliberately leak small allocations; suppress LSan only while
# configure runs so those probes cannot stall or turn successful checks red.
RUN \
    VER_MAJOR=$(echo "${PHP_VERSION}" | cut -d. -f1) && \
    VER_MINOR=$(echo "${PHP_VERSION}" | cut -d. -f2) && \
    DEBUG_FLAG=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "--enable-debug" || echo "") && \
    ZTS_FLAG=$([ "${PHP_ENABLE_ZTS}" != "yes" ] && echo "" || \
        { [ "${VER_MAJOR}" = "7" ] && \
            echo "--enable-maintainer-zts --disable-zend-signals" || \
            echo "--enable-zts --disable-zend-signals"; }) && \
    COMPAT_FLAGS=$([ "${VER_MAJOR}" = "7" ] || { [ "${VER_MAJOR}" = "8" ] && [ "${VER_MINOR}" -le 1 ]; } && echo "-DCOOKIE_SEEKER_USES_OFF64_T -D__off64_t=long -Doff64_t=long" || echo "") && \
    SAN_FLAGS="-fsanitize=address,undefined -fno-sanitize=function" && \
    ASAN_CFLAGS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "${SAN_FLAGS}" || echo "") && \
    UBSAN_CXX=$(ls /usr/lib/llvm*/lib/clang/*/lib/linux/libclang_rt.ubsan_standalone_cxx-*.a | head -1) && \
    SAN_LDEXTRA="-Wl,--whole-archive,${UBSAN_CXX},--no-whole-archive -Wl,--dynamic-list=${UBSAN_CXX}.syms -L/usr/asan/lib -Wl,-rpath,/usr/asan/lib -Wl,-Bdynamic -lc++abi -Wl,-Bstatic" && \
    ASAN_LDFLAGS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "${SAN_FLAGS} ${SAN_LDEXTRA}" || echo "") && \
    CONFIGURE_ASAN_OPTIONS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "detect_leaks=0" || echo "") && \
    OPT_FLAG=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "-O0" || echo "-O2") && \
    cd "php-${PHP_VERSION}" && \
    ASAN_OPTIONS="${CONFIGURE_ASAN_OPTIONS}" \
    CC=musl-clang \
    CXX=musl-clang++ \
    CFLAGS="-g ${OPT_FLAG} ${COMPAT_FLAGS} ${ASAN_CFLAGS}" \
    LDFLAGS="${ASAN_LDFLAGS}" \
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

# For debug builds, install declarative musl-clang policy so extensions compiled
# inside this image (e.g. by build-and-test.sh) are ASan/UBSan-instrumented.
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
# 2. Set MUSL_CLANG_SANITIZE_CXX=address so musl-clang++ selects the
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
COPY musl-clang-debug.conf /tmp/musl-clang-debug.conf
RUN if [ "${PHP_ENABLE_DEBUG}" = "yes" ]; then \
        mv /tmp/musl-clang-debug.conf /etc/musl-clang.conf; \
        patchelf --add-needed libc++.so.1 /usr/local/bin/php; \
    else \
        rm /tmp/musl-clang-debug.conf; \
    fi

# Verify: PHP binary should depend only on musl libc
RUN php --version
# PHP 8.4+ exposes these constants as booleans. Cast them so false compares as
# 0, consistently with the integer constants used by older PHP versions.
RUN test "$(php -r 'echo (int) PHP_DEBUG;')" = \
        "$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo 1 || echo 0)" && \
    test "$(php -r 'echo (int) PHP_ZTS;')" = \
        "$([ "${PHP_ENABLE_ZTS}" = "yes" ] && echo 1 || echo 0)"
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
