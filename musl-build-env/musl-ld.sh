#!/bin/bash

set -euo pipefail

readonly real_linker=/usr/bin/ld.lld
readonly symbol_directory=/usr/local/share/musl
architecture=$(uname -m)
readonly architecture
readonly dependency_symbol_lists=(
    "libpthread.so.0:${symbol_directory}/glibc-2.33-libpthread-symbols-${architecture}.txt"
    "librt.so.1:${symbol_directory}/glibc-2.33-librt-symbols.txt"
    "libm.so.6:${symbol_directory}/glibc-libm-symbols.txt"
    "libdl.so.2:${symbol_directory}/glibc-2.33-libdl-symbols.txt"
    "libutil.so.1:${symbol_directory}/glibc-2.33-libutil-symbols.txt"
)

references_symbol_from()
{
    local symbol_list=$1
    local symbol

    while IFS= read -r symbol; do
        if grep -Fqx -- "$symbol" "$symbol_list"; then
            return 0
        fi
    done <<< "$undefined"
    return 1
}

output=
output_follows=false
for arg in "$@"; do
    if $output_follows; then
        output=$arg
        output_follows=false
        continue
    fi

    case "$arg" in
        -o|--output)
            output_follows=true
            ;;
        --output=*)
            output=${arg#--output=}
            ;;
        -o?*)
            output=${arg#-o}
            ;;
    esac
done

"$real_linker" "$@"

# Compiler drivers always pass an explicit output path to the linker. Ignore
# linker probes and unusual direct invocations that do not.
[[ -n "$output" && -f "$output" ]] || exit 0
if [[ "$output" != /* ]]; then
    output="$PWD/$output"
fi

# patchelf rejects relocatable objects and static executables, neither of which
# has a dynamic dependency table to update.
needed=$(patchelf --print-needed "$output" 2>/dev/null) || exit 0
undefined=$(llvm-nm -Duj "$output" 2>/dev/null | sed 's/@.*//') || exit 0

# musl records its architecture-specific libc SONAME in every dynamically
# linked output. libc.so.6 is a reserved self-alias in musl's loader and the
# native name on glibc, so use it in the output without changing the link-time
# libc selected by /usr/lib/libc.so.
while IFS= read -r dependency; do
    case "$dependency" in
        libc.musl-*)
            patchelf --replace-needed "$dependency" libc.so.6 "$output"
            ;;
    esac
done <<< "$needed"
needed=$(patchelf --print-needed "$output")

# These glibc compatibility names are self-aliases in musl's dynamic loader.
# Adding them makes old glibc releases load the DSO that owns an imported
# symbol, while musl resolves the dependency back to its loader/libc DSO.
for entry in "${dependency_symbol_lists[@]}"; do
    dependency=${entry%%:*}
    symbol_list=${entry#*:}
    if ! grep -Fqx -- "$dependency" <<< "$needed" &&
        references_symbol_from "$symbol_list"; then
        patchelf --add-needed "$dependency" "$output"
        needed+=$'\n'"$dependency"
    fi
done
