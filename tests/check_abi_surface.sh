#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

LC_ALL=C
export LC_ALL

if [ "$#" -ne 3 ]; then
  echo "usage: $0 LIBRARY MANIFEST NM" >&2
  exit 2
fi

library=$1
manifest=$2
nm=$3

tmp=${TMPDIR:-/tmp}/libpkgstate-abi.$$
trap 'rm -f "$tmp"' EXIT HUP INT TERM

"$nm" -D --defined-only "$library" |
  awk '{print $3}' |
  sed '/^$/d' |
  sort -u >"$tmp"

if ! cmp -s "$manifest" "$tmp"; then
  echo "exported ABI differs from reviewed manifest: $manifest" >&2
  diff -u "$manifest" "$tmp" >&2 || true
  exit 1
fi
