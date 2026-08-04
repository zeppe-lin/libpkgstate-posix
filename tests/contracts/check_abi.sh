#!/bin/sh
set -eu
root=$1
fail(){ echo "abi-contract: $*" >&2; exit 1; }
test "$(wc -l < "$root/abi/libpkgstate-posix.exports")" -eq 14 || fail 'unexpected export count'
grep -F 'canonical_generation_store' "$root/abi/libpkgstate-posix.exports" >/dev/null || fail 'store exports absent'
! grep -v 'canonical_generation_store' "$root/abi/libpkgstate-posix.exports" | grep . >/dev/null || fail 'foreign export present'
grep -F "gnu_symbol_visibility: 'hidden'" "$root/src/meson.build" >/dev/null || fail 'hidden visibility absent'
grep -F -- '--version-script=' "$root/src/meson.build" >/dev/null || fail 'version script absent'
