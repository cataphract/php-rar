#!/usr/bin/env bash
# Refresh docker-image-shas.yml with current OCI index digests.
# The index digest covers all platforms (amd64 + arm64); Docker resolves the
# right platform image from it at pull time.
# Justfile and tests.yml read from docker-image-shas.yml directly — no patching needed.
set -euo pipefail

IMAGE="ghcr.io/cataphract/php-minimal"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCK_FILE="$SCRIPT_DIR/../docker-image-shas.yml"

TAGS=(
    7.0-debug       7.0-release-zts
    7.1-debug       7.1-release-zts
    7.2-debug       7.2-release-zts
    7.3-debug       7.3-release-zts
    7.4-debug       7.4-release-zts
    8.0-debug       8.0-release-zts
    8.1-debug       8.1-release-zts
    8.2-debug       8.2-release-zts
    8.3-debug       8.3-release-zts
    8.4-debug       8.4-release-zts
    8.5-debug       8.5-release-zts
)

get_index_digest() {
    docker buildx imagetools inspect "${IMAGE}:$1" \
        | awk '/^Digest:/ { print $2; exit }'
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
