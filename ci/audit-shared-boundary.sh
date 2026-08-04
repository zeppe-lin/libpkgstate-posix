#!/bin/sh
set -eu
[ "$#" -eq 1 ] || exit 2
out=$(readelf -d "$1")
printf '%s\n' "$out" | grep -F 'Library soname: [libpkgstate-posix.so.3]' >/dev/null
needed=$(printf '%s\n' "$out" | grep 'Shared library:' || true)
printf '%s\n' "$needed" | grep -F 'Shared library: [libpkgstate.so.4]' >/dev/null
printf '%s\n' "$needed" | grep -E 'Shared library: \[libcrypto\.so' >/dev/null
! printf '%s\n' "$needed" | grep -E 'libpkg(source|build|image|plan|apply)|libpkgstate-(source|build|plan|apply)|libarchive|libyaml' >/dev/null
