#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 SOURCE-ROOT" >&2
  exit 2
}

root=$1
manifest=$root/abi/libpkgstate-posix.exports

fail()
{
  echo "abi-contract: $*" >&2
  exit 1
}

test -s "$manifest" || fail 'reviewed export manifest is absent'
test "$(wc -l < "$manifest")" -eq 12 || fail 'unexpected export count'

grep -F '_ZN8pkgstate5posix26canonical_generation_store' "$manifest"   >/dev/null || fail 'namespace-qualified store exports are absent'
for destructor in D0Ev D1Ev D2Ev; do
  grep -F "canonical_generation_store$destructor" "$manifest" >/dev/null ||
    fail "anchored destructor export is absent: $destructor"
done

if grep -E 'initialize|validate_existing|existing_store_tag|begin_publication'   "$manifest" >/dev/null
then
  fail 'private provider mechanism leaked into the public ABI'
fi
if grep -v 'canonical_generation_store' "$manifest" | grep . >/dev/null; then
  fail 'foreign export is present'
fi

grep -F "gnu_symbol_visibility: 'hidden'" "$root/src/meson.build"   >/dev/null || fail 'hidden visibility is absent'
grep -F -- '--version-script=' "$root/src/meson.build" >/dev/null ||
  fail 'version script is absent'
