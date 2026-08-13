#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "manpage-source-contract: $*" >&2; exit 1; }

pages='libpkgstate-posix.3
pkgstate_canonical_generation_store.3
pkgstate-generation.5
pkgstate-check.1'

printf '%s\n' "$pages" | while IFS= read -r page; do
  source=$root/docs/man/$page.md
  generated=$root/docs/man/generated/$page
  [ -s "$source" ] || fail "missing canonical source: docs/man/$page.md"
  [ -s "$generated" ] || fail "missing generated derivative: docs/man/generated/$page"
  case $page in
    libpkgstate-posix.3) title='LIBPKGSTATE-POSIX(3)' ;;
    pkgstate_canonical_generation_store.3) title='PKGSTATE_CANONICAL_GENERATION_STORE(3)' ;;
    pkgstate-generation.5) title='PKGSTATE-GENERATION(5)' ;;
    pkgstate-check.1) title='PKGSTATE-CHECK(1)' ;;
  esac
  first=$(sed -n '1p' "$source")
  [ "$first" = "% $title libpkgstate-posix | Version 3.0.0" ] ||
    fail "invalid Pandoc title for $page: $first"
  grep -F '# NAME' "$source" >/dev/null || fail "$page omits NAME"
  grep -F '# SEE ALSO' "$source" >/dev/null || fail "$page omits SEE ALSO"
done

if find "$root/docs/man" -maxdepth 1 -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then
  fail 'scdoc source remains'
fi
if grep -RInE '^[-=]{3,}$' "$root/docs/man" --include='*.md' >/dev/null; then
  fail 'Setext heading remains in manual source'
fi
