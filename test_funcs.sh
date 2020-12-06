#!/bin/bash -e

JOBS=3

function package_name {
  local readonly version=$1 zts=$2
  local zts_suffix=''
  if [[ $zts = 'true' ]]; then
    zts_suffix='-zts'
  fi

  echo "php-${version}${zts_suffix}-bare-dbg"
}

function prefix {
  local readonly version=$1 zts=$2

  echo "/opt/$(package_name $version $zts)"
}

function build_ext {
  local readonly version=$1 zts=$2 coverage=$3
  local readonly prefix=$(prefix $1 $2)
  local cflags= cxxflags= ldflags=
  "$prefix"/bin/phpize
  if [[ $coverage == true ]]; then
    cflags=--coverage
    cxxflags=--coverage
    ldflags=--coverage
  fi
  CFLAGS="$cflags" CXXFLAGS="$cxxflags" LDFLAGS="$ldflags" \
    ./configure --with-php-config="$prefix/bin/php-config"
  make -j $JOBS
}

function do_tests {
  local readonly prefix=$1
  local found_leaks= dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local ret=0
  sed -i s/-@if/@if/ Makefile
  TEST_PHP_EXECUTABLE="$prefix/bin/php" \
    TEST_PHP_JUNIT=report.xml \
    REPORT_EXIT_STATUS=1 \
    NO_INTERACTION=1 \
    TESTS="--set-timeout 300 --show-diff $RUN_TESTS_FLAGS" make test \
    || ret=$?
  found_leaks=$(find tests -name '*.mem' | wc -l)
  if [[ $found_leaks -gt 0 ]]; then
    echo "Found $found_leaks leaks. Failing."
    find tests -name "*.mem" -print -exec cat {} \;
  fi
  return $ret
}

function install_php {
  local readonly version=$1 zts=$2
  local readonly url="$MIRROR/php-$version.tar.gz"

  sudo apt-get install -y gnupg
  sudo apt-key adv --keyserver keyserver.ubuntu.com --recv 5D98E7264E3F3D89463B314B12229434A9F003C9
  echo deb [arch=amd64] http://artefacto-test.s3.amazonaws.com/php-bare-dbg bionic main | sudo tee -a /etc/apt/sources.list
  sudo apt-get update
  sudo apt-get install -y $(package_name $version $zts)
}

function run_tests {
  set -e
  set -o pipefail
  do_tests "$(prefix $1 $2)"
}
