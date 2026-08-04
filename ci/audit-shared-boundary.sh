#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 INSTALLED-LIBRARY" >&2
  exit 2
}

library=$1
[ -s "$library" ] || {
  echo "shared-boundary-audit: missing library: $library" >&2
  exit 1
}

output=$(readelf -d "$library")
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F \
  'Library soname: [libpkgstate-posix.so.3]' >/dev/null || {
  echo 'shared-boundary-audit: wrong SONAME' >&2
  exit 1
}

needed=$(printf '%s\n' "$output" | grep 'Shared library:' || true)
printf '%s\n' "$needed" | grep -F \
  'Shared library: [libpkgstate.so.4]' >/dev/null || {
  echo 'shared-boundary-audit: state owner dependency is absent' >&2
  exit 1
}
printf '%s\n' "$needed" | grep -E \
  'Shared library: \[libcrypto\.so[^]]*\]' >/dev/null || {
  echo 'shared-boundary-audit: private crypto mechanism is absent' >&2
  exit 1
}
if printf '%s\n' "$needed" | grep -E \
  'libpkg(source|build|image|plan|apply)|libpkgstate-(source|build|plan|apply)|libarchive|libyaml' \
  >/dev/null
then
  echo 'shared-boundary-audit: foreign authority dependency is present' >&2
  exit 1
fi
