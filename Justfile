# Build and test inside the matching CI Docker image.
# Image SHAs are read from .github/docker-image-shas.yml (multi-arch OCI index digests).
# To refresh SHAs: .github/scripts/update-docker-shas.sh

# ── Images ────────────────────────────────────────────────────────────────────

_base := "datadog/dd-appsec-php-ci@"
_shas := ".github/docker-image-shas.yml"

image_7_0_debug       := _base + `grep 'php-7.0-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_0_release_zts := _base + `grep 'php-7.0-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_1_debug       := _base + `grep 'php-7.1-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_1_release_zts := _base + `grep 'php-7.1-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_2_debug       := _base + `grep 'php-7.2-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_2_release_zts := _base + `grep 'php-7.2-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_3_debug       := _base + `grep 'php-7.3-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_3_release_zts := _base + `grep 'php-7.3-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_4_debug       := _base + `grep 'php-7.4-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_7_4_release_zts := _base + `grep 'php-7.4-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_8_0_debug       := _base + `grep 'php-8.0-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_8_0_release_zts := _base + `grep 'php-8.0-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_8_1_debug       := _base + `grep 'php-8.1-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_8_1_release_zts := _base + `grep 'php-8.1-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`
image_8_2_debug       := _base + `grep 'php-8.2-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_8_2_release_zts := _base + `grep 'php-8.2-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`

_run := "docker run --rm --entrypoint bash -v \"$PWD:/workspace\" -w /workspace --user root"

# ── Default ───────────────────────────────────────────────────────────────────

default:
    @just --list

# ── Individual targets ────────────────────────────────────────────────────────

test-7_0-debug:
    {{_run}} {{image_7_0_debug}} .github/scripts/build-and-test.sh
test-7_0-release-zts:
    {{_run}} {{image_7_0_release_zts}} .github/scripts/build-and-test.sh

test-7_1-debug:
    {{_run}} {{image_7_1_debug}} .github/scripts/build-and-test.sh
test-7_1-release-zts:
    {{_run}} {{image_7_1_release_zts}} .github/scripts/build-and-test.sh

test-7_2-debug:
    {{_run}} {{image_7_2_debug}} .github/scripts/build-and-test.sh
test-7_2-release-zts:
    {{_run}} {{image_7_2_release_zts}} .github/scripts/build-and-test.sh

test-7_3-debug:
    {{_run}} {{image_7_3_debug}} .github/scripts/build-and-test.sh
test-7_3-release-zts:
    {{_run}} {{image_7_3_release_zts}} .github/scripts/build-and-test.sh

test-7_4-debug:
    {{_run}} {{image_7_4_debug}} .github/scripts/build-and-test.sh
test-7_4-release-zts:
    {{_run}} {{image_7_4_release_zts}} .github/scripts/build-and-test.sh

test-8_0-debug:
    {{_run}} {{image_8_0_debug}} .github/scripts/build-and-test.sh
test-8_0-release-zts:
    {{_run}} {{image_8_0_release_zts}} .github/scripts/build-and-test.sh

test-8_1-debug:
    {{_run}} {{image_8_1_debug}} .github/scripts/build-and-test.sh
test-8_1-release-zts:
    {{_run}} {{image_8_1_release_zts}} .github/scripts/build-and-test.sh

test-8_2-debug:
    {{_run}} {{image_8_2_debug}} .github/scripts/build-and-test.sh
test-8_2-release-zts:
    {{_run}} {{image_8_2_release_zts}} .github/scripts/build-and-test.sh

# ── Per-version aggregates (sequential to avoid workspace conflicts) ───────────

test-7_0: test-7_0-debug test-7_0-release-zts
test-7_1: test-7_1-debug test-7_1-release-zts
test-7_2: test-7_2-debug test-7_2-release-zts
test-7_3: test-7_3-debug test-7_3-release-zts
test-7_4: test-7_4-debug test-7_4-release-zts
test-8_0: test-8_0-debug test-8_0-release-zts
test-8_1: test-8_1-debug test-8_1-release-zts
test-8_2: test-8_2-debug test-8_2-release-zts

# ── All Linux targets ─────────────────────────────────────────────────────────

test-linux: test-7_0 test-7_1 test-7_2 test-7_3 test-7_4 test-8_0 test-8_1 test-8_2
