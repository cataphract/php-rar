#!/usr/bin/env bash
# Refresh docker-image-shas.yml with current OCI index digests.
# The index digest covers all platforms (amd64 + arm64); Docker resolves the
# right platform image from it at pull time.
# Justfile and tests.yml read from docker-image-shas.yml directly — no patching needed.
set -euo pipefail

IMAGE="datadog/dd-appsec-php-ci"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCK_FILE="$SCRIPT_DIR/../docker-image-shas.yml"

TAGS=(
    php-7.0-debug       php-7.0-release-zts
    php-7.1-debug       php-7.1-release-zts
    php-7.2-debug       php-7.2-release-zts
    php-7.3-debug       php-7.3-release-zts
    php-7.4-debug       php-7.4-release-zts
    php-8.0-debug       php-8.0-release-zts
)

get_index_digest() {
    # The top-level "digest" field in the Hub tags API is the manifest-list
    # (OCI index) digest, not a per-platform image digest.
    curl -fsSL "https://hub.docker.com/v2/repositories/${IMAGE}/tags/$1" \
        | python3 -c "import sys,json; print(json.load(sys.stdin)['digest'])"
}

# Collect all digests first so we fail fast before touching any file.
declare -A DIGESTS
for tag in "${TAGS[@]}"; do
    echo "Fetching $tag ..." >&2
    DIGESTS[$tag]=$(get_index_digest "$tag")
done

# ── Update docker-image-shas.yml ─────────────────────────────────────────────

{
    echo "# Docker image SHA lock file."
    echo "# Maps image tags to their multi-arch OCI index digests (amd64 + arm64)."
    echo "# Regenerate with: .github/scripts/update-docker-shas.sh"
    echo ""
    echo "${IMAGE}:"
    for tag in "${TAGS[@]}"; do
        printf "  %-20s \"%s\"\n" "${tag}:" "${DIGESTS[$tag]}"
    done
} > "$LOCK_FILE"
echo "Updated $LOCK_FILE" >&2
