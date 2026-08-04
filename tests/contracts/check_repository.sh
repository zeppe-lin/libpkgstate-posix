#!/bin/sh
set -eu
root=$1
fail(){ echo "repository-contract: $*" >&2; exit 1; }
for f in COPYING COPYRIGHT README.md HISTORY.md CONTRIBUTING.md MAINTAINING.md meson.build meson.options; do test -s "$root/$f" || fail "missing $f"; done
for forbidden in include/libpkgstate src/canonical_store.cpp src/publication_codec.cpp; do test ! -e "$root/$forbidden" || fail "core ownership retained: $forbidden"; done

for duplicated in src/generation_codec.cpp src/generation_codec.h; do test ! -e "$root/$duplicated" || fail "duplicated state protocol: $duplicated"; done

test -x "$root/tools/check-public-documentation.py" || fail 'public documentation checker is absent'
