# syntax=docker/dockerfile:1

ARG BUILD_ENV_IMAGE=musl-build-env:latest

FROM php:7.0.33-cli-alpine AS php70
FROM php:7.1.33-cli-alpine AS php71
FROM php:7.2.34-cli-alpine AS php72
FROM php:7.3.33-cli-alpine AS php73
FROM php:7.4.33-cli-alpine AS php74
FROM php:8.0.30-cli-alpine AS php80
FROM php:8.1.32-cli-alpine AS php81
FROM php:8.2.28-cli-alpine AS php82
FROM php:8.3.19-cli-alpine AS php83
FROM php:8.4.6-cli-alpine AS php84
FROM php:8.5.0-cli-alpine AS php85

FROM ${BUILD_ENV_IMAGE}

# phpize is a shell script, but it needs Autoconf to generate an extension's
# configure script. The compiler and remaining native build tools come from
# musl-build-env.
RUN apk --no-cache add autoconf

# Assemble all SDK paths in one OCI layer. This lets the variant-independent
# headers be hard-linked without repeating their contents in the image archive.
# Hard links cannot portably refer to files in a previous OCI layer.
RUN --mount=type=bind,source=assemble-php-sdks,target=/tmp/assemble-php-sdks,ro \
    --mount=type=bind,source=libtool-clang-rt-static-archives.patch,target=/tmp/libtool-clang-rt-static-archives.patch,ro \
    --mount=type=bind,source=php-7.0-builtin-offsetof.patch,target=/tmp/php-7.0-builtin-offsetof.patch,ro \
    --mount=from=php70,source=/usr/local,target=/mnt/php70,ro \
    --mount=from=php71,source=/usr/local,target=/mnt/php71,ro \
    --mount=from=php72,source=/usr/local,target=/mnt/php72,ro \
    --mount=from=php73,source=/usr/local,target=/mnt/php73,ro \
    --mount=from=php74,source=/usr/local,target=/mnt/php74,ro \
    --mount=from=php80,source=/usr/local,target=/mnt/php80,ro \
    --mount=from=php81,source=/usr/local,target=/mnt/php81,ro \
    --mount=from=php82,source=/usr/local,target=/mnt/php82,ro \
    --mount=from=php83,source=/usr/local,target=/mnt/php83,ro \
    --mount=from=php84,source=/usr/local,target=/mnt/php84,ro \
    --mount=from=php85,source=/usr/local,target=/mnt/php85,ro \
    sh /tmp/assemble-php-sdks

COPY phpize php-config /usr/local/bin/
COPY musl-clang.conf /etc/musl-clang.conf

ENV CC=musl-clang \
    CXX=musl-clang++ \
    PHP_SDK_ROOT=/opt/php \
    PHP_SDK_VERSION=8.5

# Validate the installed selectors and verify that OCI extraction preserved
# common headers as hard links while keeping generated headers independent.
RUN set -eux; \
    chmod +x /usr/local/bin/phpize /usr/local/bin/php-config; \
    for version in \
        7.0 7.1 7.2 7.3 7.4 \
        8.0 8.1 8.2 8.3 8.4 8.5; \
    do \
        release_prefix="/opt/php/${version}"; \
        release_inode=$(stat -c %i \
            "${release_prefix}/include/php/main/php.h"); \
        release_config_inode=$(stat -c %i \
            "${release_prefix}/include/php/main/php_config.h"); \
        grep -qx '#define ZEND_DEBUG 0' \
            "${release_prefix}/include/php/main/php_config.h"; \
        ! grep -qx '#define ZTS 1' \
            "${release_prefix}/include/php/main/php_config.h"; \
        "${release_prefix}/bin/phpize" --version; \
        test "$("${release_prefix}/bin/php-config" --include-dir)" = \
            "${release_prefix}/include/php"; \
        for variant in debug release-zts; do \
            prefix="/opt/php/${version}-${variant}"; \
            test "$(stat -c %i "${prefix}/include/php/main/php.h")" = \
                "${release_inode}"; \
            test "$(stat -c %i \
                "${prefix}/include/php/main/php_config.h")" != \
                "${release_config_inode}"; \
            case "${variant}" in \
                debug) \
                    grep -qx '#define ZEND_DEBUG 1' \
                        "${prefix}/include/php/main/php_config.h"; \
                    ! grep -qx '#define ZTS 1' \
                        "${prefix}/include/php/main/php_config.h"; \
                    ;; \
                release-zts) \
                    grep -qx '#define ZEND_DEBUG 0' \
                        "${prefix}/include/php/main/php_config.h"; \
                    grep -qx '#define ZTS 1' \
                        "${prefix}/include/php/main/php_config.h"; \
                    ;; \
            esac; \
            "${prefix}/bin/phpize" --version; \
            test "$("${prefix}/bin/php-config" --include-dir)" = \
                "${prefix}/include/php"; \
        done; \
    done
