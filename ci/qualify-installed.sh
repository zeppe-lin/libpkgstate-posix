#!/bin/sh
set -eu
[ "$#" -eq 2 ] || exit 2
build=$1; mode=$2; prefix=$(pwd)/$build/install; deps=$(cat "$build/ci-dependency-prefix")
rm -rf "$prefix"; meson install -C "$build/product"
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$deps/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
test "$(pkg-config --modversion libpkgstate-posix)" = 3.0.0
req=$(pkg-config --print-requires libpkgstate-posix); printf '%s\n' "$req" | grep -F 'libpkgstate >= 3.0.0' >/dev/null
flags=''; [ "$mode" = static ] && flags=--static
# shellcheck disable=SC2046
${CXX:-c++} -std=c++17 -Wall -Wextra -Wpedantic -Werror ci/installed-consumer.cpp $(pkg-config $flags --cflags --libs libpkgstate-posix) -o "$build/installed-consumer"
LD_LIBRARY_PATH="$prefix/lib:$deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$build/installed-consumer"
for h in "$prefix"/include/libpkgstate-posix/*.h; do printf '#include <libpkgstate-posix/%s>\n' "$(basename "$h")" > "$build/header.cpp"; ${CXX:-c++} -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only $(pkg-config --cflags libpkgstate-posix) "$build/header.cpp"; done
case $mode in shared) ci/audit-shared-boundary.sh "$prefix/lib/libpkgstate-posix.so.3.0.0";; static) test -f "$prefix/lib/libpkgstate-posix.a";; esac
for f in README.md HISTORY.md DESIGN.md STORAGE.md TESTING.md CONTRIBUTING.md MAINTAINING.md architecture.md abi.md integration.md qualification.md code-style.md 3.0-extraction.md; do test -s "$prefix/share/doc/libpkgstate-posix/$f"; done
