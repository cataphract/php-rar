#!/usr/bin/env bash
# Build the extension and run the test suite.
# Expected to run inside a datadog/dd-appsec-php-ci container from the repo root.
set -euo pipefail

# Clean up artifacts from any previous build so stale objects don't survive a
# PHP-version switch (safe in CI where the workspace is always fresh).
if [ -f Makefile ]; then
    make -f Makefile distclean
fi

phpize
./configure --with-php-config="$(which php-config)"
make -f Makefile -j"$(nproc)"

# The generated Makefile silences and ignores errors on the `if` commands;
# undo that so test failures surface properly.
sed -i 's/-@if/@if/' Makefile

ret=0
TEST_PHP_EXECUTABLE="$(which php)" \
TEST_PHP_JUNIT=report.xml \
REPORT_EXIT_STATUS=1 \
NO_INTERACTION=1 \
TESTS="--set-timeout 300 --show-diff" \
    make -f Makefile test || ret=$?

found=$(find tests -name '*.mem' | wc -l)
if [ "$found" -gt 0 ]; then
    echo "Found $found memory leak(s):"
    find tests -name '*.mem' -print -exec cat {} \;
    ret=1
fi

exit "$ret"
