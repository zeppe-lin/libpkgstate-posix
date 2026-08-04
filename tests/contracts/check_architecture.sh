#!/bin/sh
set -eu
root=$1
fail(){ echo "architecture-contract: $*" >&2; exit 1; }
grep -F "dependency(" "$root/meson.build" >/dev/null || fail 'dependency declarations absent'
grep -F "'libpkgstate'" "$root/meson.build" >/dev/null || fail 'state owner dependency absent'
! grep -R -E 'libpkg(source|build|image|plan|apply)|libpkgstate-(source|build|plan|apply)' "$root/include" "$root/src" "$root/meson.build" >/dev/null || fail 'foreign authority contamination'
grep -R -E '#include <(fcntl|unistd|sys/|linux/)' "$root/src" >/dev/null || fail 'provider contains no host mechanism'
