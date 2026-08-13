#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 SOURCE-ROOT" >&2
  exit 2
}
root=$1
fail(){ echo "ci-contract: $*" >&2; exit 1; }
workflow=$root/.github/workflows/ci.yml

for text in \
  'GCC shared' 'GCC static' 'Clang shared' 'Clang static' \
  'GCC release' 'address,undefined' 'meson==1.10.2'
do
  grep -F "$text" "$workflow" >/dev/null || fail "CI omits $text"
done
[ "$(grep -c 'Check out libpkgstate 3.1.0' "$workflow")" -eq 2 ] ||
  fail 'CI does not qualify both matrices against libpkgstate 3.1.0'
[ "$(grep -c 'ref: v3.1.0' "$workflow")" -eq 2 ] ||
  fail 'CI does not pin both state-owner checkouts to v3.1.0'
if grep -F 'Check out libpkgstate 3.0.0' "$workflow" >/dev/null ||
   grep -F 'ref: v3.0.0' "$workflow" >/dev/null; then
  fail 'CI retains the retired state-owner 3.0 checkout'
fi
if grep -F 'scdoc' "$workflow" >/dev/null; then
  fail 'CI retains retired scdoc dependency'
fi
for script in configure-and-test.sh qualify-installed.sh audit-shared-boundary.sh lint-manpages.sh; do
  test -x "$root/ci/$script" || fail "missing executable ci/$script"
  sh -n "$root/ci/$script" || fail "invalid shell: ci/$script"
done

grep -F 'libpkgstate.so.4' "$root/ci/audit-shared-boundary.sh" >/dev/null ||
  fail 'shared audit does not bind the state-owner SONAME'
for tool in pkgstate-init pkgstate-check; do
  grep -F "$tool" "$root/ci/qualify-installed.sh" >/dev/null ||
    fail "installed reference tool is not qualified: $tool"
done
grep -F 'html_docs: enabled' "$workflow" >/dev/null || fail 'GCC shared HTML build is absent'
grep -F 'pandoc' "$workflow" >/dev/null || fail 'Pandoc qualification dependency is absent'
grep -F -- '-Dhtml_docs=' "$workflow" >/dev/null || fail 'HTML Meson feature is not configured'
grep -F 'qualify-html-docs.sh' "$workflow" >/dev/null || fail 'installed HTML qualification is absent'
