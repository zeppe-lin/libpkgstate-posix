#!/bin/sh
set -eu
root=$1
fail(){ echo "release-contract: $*" >&2; exit 1; }
grep -F "version: '3.0.0'" "$root/meson.build" >/dev/null || fail 'project version mismatch'
grep -F "soversion: '3'" "$root/src/meson.build" >/dev/null || fail 'SONAME generation mismatch'
grep -F 'generation-v3' "$root/README.md" >/dev/null || fail 'storage lineage absent'
