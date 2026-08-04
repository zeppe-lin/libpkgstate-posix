#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

usage()
{
  echo "usage: $0 BUILD-DIR {shared|static}" >&2
  exit 2
}

[ "$#" -eq 2 ] || usage
build_dir=$1
link_mode=$2
case $link_mode in
  shared|static) ;;
  *) usage ;;
esac

install_prefix=$build_dir/install
dependency_prefix=$(cat "$build_dir/ci-dependency-prefix")
rm -rf "$install_prefix"
meson install -C "$build_dir/product"

export PKG_CONFIG_PATH="$install_prefix/lib/pkgconfig:$dependency_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$install_prefix/lib:$dependency_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset PKG_CONFIG_SYSROOT_DIR

[ "$(pkg-config --modversion libpkgstate-posix)" = 3.0.0 ] || {
  echo 'installed libpkgstate-posix version is not 3.0.0' >&2
  exit 1
}

public=$(pkg-config --print-requires libpkgstate-posix)
printf '%s\n' "$public" | grep -F 'libpkgstate >= 3.0.0' >/dev/null || {
  echo 'public state-owner requirement is absent' >&2
  exit 1
}
if printf '%s\n' "$public" | grep -E 'libcrypto|libpkgstate-' >/dev/null; then
  echo 'private or adapter dependency leaked into public metadata' >&2
  exit 1
fi
private=$(pkg-config --print-requires-private libpkgstate-posix)
printf '%s\n' "$private" | grep -F libcrypto >/dev/null || {
  echo 'private crypto requirement is absent' >&2
  exit 1
}

case $link_mode in
  shared) flags=$(pkg-config --cflags --libs libpkgstate-posix) ;;
  static) flags=$(pkg-config --static --cflags --libs libpkgstate-posix) ;;
esac
case $link_mode in
  shared)
    if printf '%s\n' "$flags" | grep -F -- '-lcrypto' >/dev/null; then
      echo 'private crypto edge leaked into shared consumer flags' >&2
      exit 1
    fi
    ;;
  static)
    printf '%s\n' "$flags" | grep -F -- '-lcrypto' >/dev/null || {
      echo 'static link closure omits libcrypto' >&2
      exit 1
    }
    ;;
esac

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
cxx=${CXX:-c++}
# shellcheck disable=SC2086
$cxx -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$(dirname "$0")/installed-consumer.cpp" $flags \
  -o "$temporary/consumer"
"$temporary/consumer"

for header in "$install_prefix"/include/libpkgstate-posix/*.h; do
  unit=$temporary/$(basename "$header").cpp
  printf '#include <libpkgstate-posix/%s>\n' \
    "$(basename "$header")" >"$unit"
  # shellcheck disable=SC2046
  $cxx -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
    $(pkg-config --cflags libpkgstate-posix) "$unit"
done

case $link_mode in
  shared)
    "$(dirname "$0")/audit-shared-boundary.sh" \
      "$install_prefix/lib/libpkgstate-posix.so.3.0.0"
    ;;
  static)
    [ -f "$install_prefix/lib/libpkgstate-posix.a" ] || {
      echo 'installed static archive is absent' >&2
      exit 1
    }
    ;;
esac

[ -x "$install_prefix/bin/pkgstate-check" ] || {
  echo 'installed pkgstate-check is absent' >&2
  exit 1
}
"$install_prefix/bin/pkgstate-check" --version | grep -F \
  'pkgstate-check (libpkgstate-posix) 3.0.0' >/dev/null || {
  echo 'installed pkgstate-check reports the wrong version' >&2
  exit 1
}

python3 ci/qualify-installed-documentation.py "$install_prefix" libpkgstate-posix

for page in "$build_dir"/product/man/*.[1357]; do
  [ -e "$page" ] || continue
  section=${page##*.}
  installed=$install_prefix/share/man/man$section/$(basename "$page")
  [ -s "$installed" ] || {
    echo "installed manual is absent: $installed" >&2
    exit 1
  }
done
