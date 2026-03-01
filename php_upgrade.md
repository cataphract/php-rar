# PHP 8.1–8.5 Upgrade Procedure for php-rar

This document describes the step-by-step procedure to extend php-rar support
from PHP 8.0 to PHP 8.5. Each minor version is handled independently: CI is
wired up, the extension is compiled and tested inside the matching Docker image,
code changes are applied to fix any failures, and only then is the next version
tackled.

---

## Overview of files touched per version

| File | Change |
|---|---|
| `.github/docker-image-shas.yml` | Add new tag → SHA entries |
| `.github/scripts/update-docker-shas.sh` | Add new tags to the `TAGS` array |
| `Justfile` | Add image variables and `test-X_Y-*` targets |
| `*.c` / `*.h` | C source changes for API compatibility |
| `.github/workflows/tests.yml` | Windows job — update `php-version` (once per bump) |

The Linux CI matrix is generated automatically from `docker-image-shas.yml`, so
no manual edit to `tests.yml` is needed for Linux jobs.

---

## Repeatable procedure for each version

Follow these numbered steps for **each** minor version in order. Example:
8.0 → 8.1 → 8.2 → 8.3 → 8.4 → 8.5.

### Step 1 — Read the upgrade guides in php-src

Clone or browse php-src on the target branch, e.g. `PHP-8.1`:

```
https://github.com/php/php-src/blob/PHP-8.X/UPGRADING
https://github.com/php/php-src/blob/PHP-8.X/UPGRADING.INTERNALS
```

Focus on sections relevant to C extensions:
- Removed or renamed macros / functions
- Changed return types (`int` → `zend_result`)
- Changed struct member types
- New mandatory includes
- Any other backwards-incompatible changes

The per-version notes below summarise the items relevant to php-rar.

### Step 2 — Add the Docker image SHA

Fetch the OCI index digest from Docker Hub for the two new tags:

```bash
# Quick one-liner — prints the index digest for a given tag
curl -fsSL "https://hub.docker.com/v2/repositories/datadog/dd-appsec-php-ci/tags/php-X.Y-debug" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['digest'])"
```

Or regenerate everything at once with the provided script after adding the new
tags to it (see Step 3):

```bash
.github/scripts/update-docker-shas.sh
```

Append the two lines to `.github/docker-image-shas.yml`:

```yaml
  php-X.Y-debug:       "sha256:<INDEX-DIGEST>"
  php-X.Y-release-zts: "sha256:<INDEX-DIGEST>"
```

Also add both tags to the `TAGS` array in `.github/scripts/update-docker-shas.sh`:

```bash
TAGS=(
    ...existing tags...
    php-X.Y-debug       php-X.Y-release-zts
)
```

### Step 3 — Add Justfile targets

Add image variables and `test-X_Y-*` targets following the existing pattern:

```just
image_X_Y_debug       := _base + `grep 'php-X.Y-debug:'       .github/docker-image-shas.yml | cut -d'"' -f2`
image_X_Y_release_zts := _base + `grep 'php-X.Y-release-zts:' .github/docker-image-shas.yml | cut -d'"' -f2`

test-X_Y-debug:
    {{_run}} {{image_X_Y_debug}} .github/scripts/build-and-test.sh
test-X_Y-release-zts:
    {{_run}} {{image_X_Y_release_zts}} .github/scripts/build-and-test.sh

test-X_Y: test-X_Y-debug test-X_Y-release-zts
```

Add `test-X_Y` to the `test-linux` aggregate at the bottom.

### Step 4 — Compile and test

Run both variants locally before pushing:

```bash
just test-X_Y-debug
just test-X_Y-release-zts
```

Or both together:

```bash
just test-X_Y
```

Examine the output for compiler warnings, errors, and test failures.

### Step 5 — Apply C source changes

Based on the compilation output and the per-version notes below, make the
minimum necessary changes to `.c`/`.h` files. Guard every change with `#if
PHP_VERSION_ID >= XXYY00` so that older PHP versions continue to work.

### Step 6 — Re-run tests until green

Repeat Step 4 after each change. When both `debug` and `release-zts` pass,
commit.

### Step 7 — Push and verify CI

Push the branch. The `linux` CI job matrix is auto-built from
`docker-image-shas.yml` — the new versions appear automatically. Verify the
GitHub Actions run is green for all new jobs.

### Step 8 — Update Windows CI (optional, once per bump)

The Windows job in `.github/workflows/tests.yml` pins a specific PHP version.
Update it when the Linux jobs for the matching version are confirmed green:

```yaml
  - name: Build and test
    uses: php/php-windows-builder/extension@v1
    with:
      php-version: 'X.Y'          # ← change here
```

Also update the `name:` and artifact `name:` strings in the same Windows job
block.

---

## Current Docker image SHAs (as of 2026-03-01)

These are the OCI index digests (multi-arch: amd64 + arm64) to use in
`docker-image-shas.yml`.

```yaml
  php-8.1-debug:       "sha256:1a1e5b44cf043e59768c65fd7c94aaefdacde5fa96d83102d35db11ad86f24c6"
  php-8.1-release-zts: "sha256:5b8a269b4228d9191420059daef820b660110be0aca6776557924172fd1ff0c8"
  php-8.2-debug:       "sha256:52ad14560672fc8c5130f5758bbee3fa401bc1d35b412f4a230c6258143291a5"
  php-8.2-release-zts: "sha256:cb143d915b394f16a2d78018765705460f3d1b788fdd2a90ef50fad5f8f5918c"
  php-8.3-debug:       "sha256:bb6df08160126374d3d9247428928aa19a9c2b2429c98356650199b85ae20212"
  php-8.3-release-zts: "sha256:e58e25a017f75df82691d408b8cb70453875ff36718e295ee8c6653a0f117331"
  php-8.4-debug:       "sha256:15045688f6986f4625b1507a7f4be6104e7bbb88caf877f1611463b929f2bca2"
  php-8.4-release-zts: "sha256:8e0ac25a3306b4b9f692c593b8a509cc789c2e001ce52682928065a92c880136"
  php-8.5-debug:       "sha256:bd0170331b34fb469e29d00b19b20fb88b726160f76df274a1bdc3a27ac18d30"
  php-8.5-release-zts: "sha256:e071b2095da55bd24686209422f43a01c65acfc6021f04156d9fb43fd3d4d426"
```

Refresh at any time with `.github/scripts/update-docker-shas.sh` after adding
the new tags.

---

## Summary checklist

For each version X.Y in order (8.1, 8.2, 8.3, 8.4, 8.5):

- [ ] Read `PHP-X.Y/UPGRADING.INTERNALS` on GitHub
- [ ] Add two SHA entries to `.github/docker-image-shas.yml`
- [ ] Add both tags to `TAGS` array in `.github/scripts/update-docker-shas.sh`
- [ ] Add `image_X_Y_*` variables and `test-X_Y-*` targets to `Justfile`
- [ ] Add `test-X_Y` to `test-linux` aggregate in `Justfile`
- [ ] Run `just test-X_Y` and fix all compilation errors
- [ ] Run `just test-X_Y` again; confirm all tests pass
- [ ] Commit infrastructure + code changes together
- [ ] Push; confirm GitHub Actions CI is green for the new matrix entries
- [ ] (Optional) Update Windows `php-version` in `.github/workflows/tests.yml` to X.Y

<!-- vim: set tw=80: -->
