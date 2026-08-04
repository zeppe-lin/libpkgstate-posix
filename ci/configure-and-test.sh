#!/bin/sh
set -eu
[ "$#" -ge 3 ] || { echo "usage: $0 BUILD-DIR {shared|static} LIBPKGSTATE-SOURCE [MESON-ARG...]" >&2; exit 2; }
build=$1; mode=$2; state_source=$3; shift 3
case $mode in shared|static) ;; *) exit 2;; esac
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
prefix=$(pwd)/$build/dependencies
rm -rf "$build"; mkdir -p "$build"
meson setup "$build/libpkgstate" "$state_source" --wrap-mode=nofallback --fatal-meson-warnings --prefix="$prefix" --libdir=lib -Ddefault_library="$mode" -Dlink_mode="$mode" -Dtests=disabled -Dman_pages=disabled -Dwerror=true
meson compile -C "$build/libpkgstate"; meson install -C "$build/libpkgstate"
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" LD_LIBRARY_PATH="$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
meson setup "$build/product" "$root" --wrap-mode=nofallback --fatal-meson-warnings --prefix="$(pwd)/$build/install" --libdir=lib -Ddefault_library="$mode" -Dlink_mode="$mode" -Dtests=enabled -Dtools=enabled -Dinstall_tools=true -Dwerror=true "$@"
meson compile -C "$build/product"; meson test -C "$build/product" --print-errorlogs
printf '%s\n' "$prefix" > "$build/ci-dependency-prefix"
