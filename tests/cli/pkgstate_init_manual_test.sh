#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
source_root=$1
pkgstate_init=$2
manual=$source_root/docs/man/pkgstate-init.1.md
fail() { echo "pkgstate-init-manual-test: $*" >&2; exit 1; }
test -s "$manual" || fail 'manual source is missing'
test "$(sed -n '1p' "$manual")" = '% PKGSTATE-INIT(1) libpkgstate-posix | Version 3.1.0' ||
  fail 'manual heading is wrong'
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
"$pkgstate_init" --help >"$temporary/help" 2>"$temporary/error" || fail 'help failed'
test ! -s "$temporary/error" || fail 'help wrote standard error'
for option in --canonical-store --managed-target --state-store --root-view --state-backend --publication-domain --version --help; do
  grep -F -- "$option" "$temporary/help" >/dev/null || fail "help omits $option"
  grep -F -- "$option" "$manual" >/dev/null || fail "manual omits $option"
done
normalized=$(tr '\n\t' '  ' <"$manual" | tr -s ' ')
printf '%s\n' "$normalized" | grep -F 'admits only an empty canonical snapshot' >/dev/null ||
  fail 'manual omits empty-state contract'
grep -F 'does not erase packages' "$manual" >/dev/null ||
  fail 'manual omits populated-state refusal'
grep -F 'Command-line usage is invalid.' "$manual" >/dev/null ||
  fail 'manual omits status 2'
