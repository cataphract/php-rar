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
RUN \
    DEBUG_FLAG=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "--enable-debug" || echo "") && \
    ZTS_FLAG=$([ "${PHP_ENABLE_ZTS}" = "yes" ] && echo "--enable-zts" || echo "") && \
    VER_MAJOR=$(echo "${PHP_VERSION}" | cut -d. -f1) && \
    VER_MINOR=$(echo "${PHP_VERSION}" | cut -d. -f2) && \
    COMPAT_FLAGS=$([ "${VER_MAJOR}" = "7" ] || { [ "${VER_MAJOR}" = "8" ] && [ "${VER_MINOR}" -le 1 ]; } && echo "-DCOOKIE_SEEKER_USES_OFF64_T -D__off64_t=long -Doff64_t=long" || echo "") && \
    ASAN_CFLAGS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "-fsanitize=address" || echo "") && \
    ASAN_LDFLAGS=$([ "${PHP_ENABLE_DEBUG}" = "yes" ] && echo "-fsanitize=address" || echo "") && \
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
        ${DEBUG_FLAG} \
        ${ZTS_FLAG} && \
    make -j$(nproc) && \
    make install

RUN rm -rf "/php-${PHP_VERSION}"

# Ensure extension builds (phpize + configure) use our musl toolchain.
ENV CC=musl-clang CXX=musl-clang++

# For debug builds: patch the musl-clang wrappers to add -fsanitize=address
# so that extensions compiled inside this image (e.g. by build-and-test.sh)
# are also ASAN-instrumented. This prevents the ASAN __cxa_throw interceptor
# crash that occurs when a non-instrumented extension (with statically-linked
# libc++) is loaded into an ASAN-instrumented PHP process.
RUN if [ "${PHP_ENABLE_DEBUG}" = "yes" ]; then \
        for f in /usr/local/bin/musl-clang /usr/local/bin/musl-clang++; do \
            sed -i 's|"$@"|"-fsanitize=address" "$@"|' "$f"; \
        done; \
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
