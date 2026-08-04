#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 SOURCE-ROOT" >&2
  exit 2
}
root=$1

fail()
{
  echo "ci-contract: $*" >&2
  exit 1
}

for text in \
  'GCC shared' 'GCC static' 'Clang shared' 'Clang static' \
  'GCC release' 'address,undefined' 'v3.0.0' 'meson==1.10.2'
do
  grep -F "$text" "$root/.github/workflows/ci.yml" >/dev/null ||
    fail "CI omits $text"
done
for script in \
  configure-and-test.sh qualify-installed.sh \
  audit-shared-boundary.sh lint-manpages.sh
do
  test -x "$root/ci/$script" || fail "missing executable ci/$script"
  sh -n "$root/ci/$script" || fail "invalid shell: ci/$script"
done

grep -F 'libpkgstate.so.4' "$root/ci/audit-shared-boundary.sh" >/dev/null ||
  fail 'shared audit does not bind the state-owner SONAME'
grep -F 'pkgstate-check' "$root/ci/qualify-installed.sh" >/dev/null ||
  fail 'installed reference tool is not qualified'
