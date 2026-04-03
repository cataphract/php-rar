variable "PHP_VERSION" {
  # patch version, e.g. "8.4.6"
  default = "8.4.6"
}

variable "PHP_MINOR" {
  # minor series for the tag, e.g. "8.4"
  default = "8.4"
}

variable "PHP_VARIANT" {
  # one of: debug, release, release-zts
  default = "debug"
}

target "_musl-build-env" {
  context    = "./musl-build-env"
  dockerfile = "Dockerfile"
}

target "musl-build-env-amd64" {
  inherits  = ["_musl-build-env"]
  platforms = ["linux/amd64"]
  args      = { ARCH = "x86_64" }
  tags      = ["ghcr.io/cataphract/musl-build-env:latest-x86_64"]
}

target "musl-build-env-arm64" {
  inherits  = ["_musl-build-env"]
  platforms = ["linux/arm64"]
  args      = { ARCH = "aarch64" }
  tags      = ["ghcr.io/cataphract/musl-build-env:latest-aarch64"]
}

target "_php-minimal" {
  context    = "./php-minimal"
  dockerfile = "Dockerfile"
  args = {
    PHP_VERSION      = PHP_VERSION
    PHP_ENABLE_DEBUG = PHP_VARIANT == "debug" ? "yes" : "no"
    PHP_ENABLE_ZTS   = PHP_VARIANT == "release-zts" ? "yes" : "no"
  }
}

target "php-minimal-amd64" {
  inherits  = ["_php-minimal"]
  platforms = ["linux/amd64"]
  args = {
    BUILD_ENV_IMAGE = "ghcr.io/cataphract/musl-build-env:latest-x86_64"
    ARCH            = "x86_64"
  }
  tags = ["ghcr.io/cataphract/php-minimal:${PHP_MINOR}-${PHP_VARIANT}-x86_64"]
}

target "php-minimal-arm64" {
  inherits  = ["_php-minimal"]
  platforms = ["linux/arm64"]
  args = {
    BUILD_ENV_IMAGE = "ghcr.io/cataphract/musl-build-env:latest-aarch64"
    ARCH            = "aarch64"
  }
  tags = ["ghcr.io/cataphract/php-minimal:${PHP_MINOR}-${PHP_VARIANT}-aarch64"]
}
