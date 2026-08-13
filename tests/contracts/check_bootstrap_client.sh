#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "bootstrap-client-contract: $*" >&2; exit 1; }
source=$root/tools/pkgstate_init.cpp
meson=$root/tools/meson.build
tests=$root/tests/meson.build
manual=$root/docs/man/pkgstate-init.1.md

for path in "$source" "$manual" "$root/tests/cli/pkgstate_init_cli_test.sh"; do
  test -s "$path" || fail "missing ${path#$root/}"
done
grep -F 'canonical_generation_store store(parsed.path, binding);' "$source" >/dev/null ||
  fail 'bootstrap client does not invoke provider initialization authority'
grep -F 'if (!state.packages().empty())' "$source" >/dev/null ||
  fail 'bootstrap client does not refuse populated canonical state'
if grep -F 'open_existing' "$source" >/dev/null; then
  fail 'bootstrap client incorrectly uses read-only existing-store authority'
fi
if grep -F 'compare_and_publish' "$source" >/dev/null; then
  fail 'bootstrap client contains package-state publication authority'
fi
grep -F "'pkgstate-init'" "$meson" >/dev/null ||
  fail 'bootstrap client is not built as pkgstate-init'
grep -F 'pkgstate_init_cli_test.sh' "$tests" >/dev/null ||
  fail 'bootstrap CLI behavior is not registered'
grep -F 'pkgstate_init_manual_test.sh' "$tests" >/dev/null ||
  fail 'bootstrap manual behavior is not registered'
grep -F 'admits only an empty canonical snapshot' "$manual" >/dev/null ||
  fail 'manual omits empty-state authority boundary'
grep -F 'does not erase packages' "$manual" >/dev/null ||
  fail 'manual omits populated-state refusal'
