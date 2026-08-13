#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "repository-contract: $*" >&2; exit 1; }

for f in COPYING COPYRIGHT README.md DESIGN.md TESTING.md HISTORY.md CONTRIBUTING.md MAINTAINING.md meson.build meson.options; do
  test -s "$root/$f" || fail "missing $f"
done
for p in include/libpkgstate-posix src abi docs docs/man docs/man/generated tests tests/cli ci .github/workflows; do
  test -e "$root/$p" || fail "missing $p"
done

for forbidden in \
  include/libpkgstate src/canonical_store.cpp src/publication_codec.cpp \
  src/generation_codec.cpp src/generation_codec.h \
  docs/architecture.md STORAGE.md man tests/tool meson_options.txt \
  tools/render-man-markdown.py tools/check-man-markdown.py; do
  test ! -e "$root/$forbidden" || fail "retired or foreign authority remains: $forbidden"
done
if find "$root" -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then
  fail 'scdoc manual authority remains'
fi
if git -C "$root" ls-files | grep -E \
  '(^|/)([^/]+\.(o|a|pyc)|[^/]+\.so(\..*)?)$' >/dev/null
then
  fail 'generated build product tracked'
fi

for tool in \
  build-html-docs.py check-html-docs.py install-html-docs.py \
  check-html-manifest.py update-man-pages.sh; do
  test -x "$root/tools/$tool" || fail "missing executable tools/$tool"
done
test -s "$root/tools/canonicalize-man-roff.awk" || fail 'roff canonicalizer is absent'

for helper in ci/qualify-html-docs.sh ci/qualify-installed-documentation.py; do
  test -x "$root/$helper" || fail "missing executable $helper"
done

grep -F "input: 'generated/' + page" "$root/docs/man/meson.build" >/dev/null ||
  fail 'ordinary man installation is not sourced from committed generated roff'
grep -F "'update-man-pages'" "$root/docs/man/meson.build" >/dev/null ||
  fail 'manual regeneration target is absent'
grep -F "'check-man-pages'" "$root/docs/man/meson.build" >/dev/null ||
  fail 'manual freshness target is absent'

if grep -RInE 'docs/architecture\.md|architecture\.html|test-doctrine\.html|(^|/)STORAGE\.md|tests/tool' \
    "$root/README.md" "$root/DESIGN.md" "$root/TESTING.md" "$root/docs" "$root/tools" \
    >/dev/null 2>&1; then
  fail 'retired topology path remains in active source'
fi
if grep -RInF '.scdoc' "$root/tools" --exclude='check_repository.sh' >/dev/null 2>&1; then
  fail 'retired scdoc source handling remains in active tooling'
fi
! grep -F 'meson_options.txt' "$root/tools/check-html-manifest.py" >/dev/null ||
  fail 'HTML tooling retains legacy Meson-options fallback'

printf '%s\n' 'repository-contract: ok'
