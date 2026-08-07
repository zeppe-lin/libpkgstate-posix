#!/bin/sh
set -eu
root=$1
fail(){ echo "architecture-contract: $*" >&2; exit 1; }
grep -F "dependency(" "$root/meson.build" >/dev/null || fail 'dependency declarations absent'
grep -F "'libpkgstate'" "$root/meson.build" >/dev/null || fail 'state owner dependency absent'
! grep -R -E 'libpkg(source|build|image|plan|apply)|libpkgstate-(source|build|plan|apply)' "$root/include" "$root/src" "$root/meson.build" >/dev/null || fail 'foreign authority contamination'
grep -R -E '#include <(fcntl|unistd|sys/|linux/)' "$root/src" >/dev/null || fail 'provider contains no host mechanism'

test ! -e "$root/src/generation_codec.cpp" || fail 'provider duplicates state-owned generation codec'
test ! -e "$root/src/generation_codec.h" || fail 'provider duplicates state-owned generation codec header'
grep -F '<libpkgstate/generation_codec.h>' "$root/include/libpkgstate-posix/canonical_generation_store.h" >/dev/null || fail 'provider does not consume state codec'

grep -F 'namespace pkgstate::posix' \
  "$root/include/libpkgstate-posix/canonical_generation_store.h" >/dev/null ||
  fail 'provider API is not namespace-isolated'

grep -F 'int root_descriptor_' \
  "$root/include/libpkgstate-posix/canonical_generation_store.h" >/dev/null ||
  fail 'store handle does not retain root descriptor authority'
grep -F 'root_descriptor_, "reopen canonical store read authority"' \
  "$root/src/canonical_generation_store.cpp" >/dev/null ||
  fail 'store reads do not reopen from retained descriptor authority'
grep -F 'root_descriptor_, target_binding_' \
  "$root/src/canonical_generation_store.cpp" >/dev/null ||
  fail 'publication transactions do not consume retained store authority'
