#!/bin/sh
set -eu
root=$1
build=${2:-}
fail(){ echo "style-contract: $*" >&2; exit 1; }
find "$root" \( -path "$root/.git" -o ${build:+-path "$build" -o} -name 'build*' \) -prune -o -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.sh' -o -name '*.md' -o -name 'meson.build' -o -name 'meson.options' \) -print | while IFS= read -r f; do if LC_ALL=C grep -n '[	]' "$f" >/dev/null; then fail "tab in ${f#$root/}"; fi; done
git -C "$root" diff --check
