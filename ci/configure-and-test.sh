#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

usage()
{
  echo "usage: $0 BUILD-DIR {shared|static} LIBPKGSTATE-SOURCE [MESON-ARG ...]" >&2
  exit 2
}

[ "$#" -ge 3 ] || usage
build_dir=$1
link_mode=$2
state_source=$3
shift 3

case $link_mode in
  shared|static) ;;
  *) usage ;;
esac

root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
case $build_dir in
  /*) build=$build_dir ;;
  *) build=$(pwd)/$build_dir ;;
esac
dependency_prefix=$build/dependencies
install_prefix=$build/install

rm -rf "$build"
mkdir -p "$build"

meson setup "$build/libpkgstate" "$state_source" \
  --wrap-mode=nofallback \
  --fatal-meson-warnings \
  --prefix="$dependency_prefix" \
  --libdir=lib \
  -Ddefault_library="$link_mode" \
  -Dlink_mode="$link_mode" \
  -Dtests=disabled \
  -Dman_pages=disabled \
  -Dwerror=true
meson compile -C "$build/libpkgstate"
meson install -C "$build/libpkgstate"

export PKG_CONFIG_PATH="$dependency_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$dependency_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

meson setup "$build/product" "$root" \
  --wrap-mode=nofallback \
  --fatal-meson-warnings \
  --prefix="$install_prefix" \
  --libdir=lib \
  -Ddefault_library="$link_mode" \
  -Dlink_mode="$link_mode" \
  -Dtests=enabled \
  -Dtools=enabled \
  -Dinstall_tools=true \
  -Dwerror=true \
  "$@"
meson compile -C "$build/product"
meson test -C "$build/product" --no-rebuild --print-errorlogs
printf '%s\n' "$dependency_prefix" >"$build/ci-dependency-prefix"
