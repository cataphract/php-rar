#!/bin/bash -e

BUILDS_DIR=$HOME/php_builds
MIRROR=${MIRROR:-http://us1.php.net/distributions}
JOBS=3

function prefix {
  local readonly version=$1 zts=$2
  local zts_suffix=''
  if [[ $zts = 'yes' ]]; then
    zts_suffix='-zts'
  fi

  echo "$BUILDS_DIR/${version}$zts_suffix"
}

function install_php {
  local readonly version=$1 zts=$2
  local readonly url="$MIRROR/php-$version.tar.gz" \
    extract_dir="/tmp/php-$version" prefix=$(prefix $version $zts)
  local extra_flags=''

  mkdir -p "$extract_dir"
  wget -O - "$url" | tar -C "$extract_dir" --strip-components=1 -xzf -

  pushd "$extract_dir"
  if [[ $zts = 'yes' ]]; then
    extra_flags="$extra_flags --enable-maintainer-zts"
  fi
  ./configure --prefix="$prefix" --disable-all --enable-cli \
    $extra_flags
  make -j $JOBS install
  popd
}

function build_ext {
  local readonly prefix=$1 coverage=$2
  "$prefix"/bin/phpize
  if [[ $coverage = 'yes' ]]; then
    export CPPFLAGS="$CPPFLAGS --coverage"
  fi
  ./configure --with-php-config="$prefix/bin/php-config"
  make -j $JOBS
}

function do_tests {
  local readonly prefix=$1
  local found_leaks= dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  echo "--suppressions=$dir/valgrind.supp" | tee ~/.valgrindrc
  TEST_PHP_EXECUTABLE="$prefix/bin/php" REPORT_EXIT_STATUS=1 \
    "$prefix/bin/php" "$prefix"/lib/php/build/run-tests.php \
    -q -d extension=modules/rar.so --set-timeout 300 --show-diff \
    $RUN_TESTS_FLAGS tests
  found_leaks=$(find tests -name '*.mem' | wc -l)
  if [[ $found_leaks -gt 0 ]]; then
    echo "Found $found_leaks leaks. Failing."
    find tests -name "*.mem" -print -exec cat {} \;
    return 1
  fi
}

# public functions below

function maybe_install_php {
  set -e
  set -o pipefail
  local readonly version=$1 zts=$2
  local readonly prefix=$(prefix $version $zts)
  if [[ ! -d $prefix ]]; then
    install_php $version $zts
  fi
}

function build {
  set -e
  set -o pipefail
  if [[ ! -f modules/rar.so ]]; then
    build_ext "$(prefix $1 $2)" "$3"
  fi
}

function run_tests {
  set -e
  set -o pipefail
  do_tests "$(prefix $1 $2)"
}
