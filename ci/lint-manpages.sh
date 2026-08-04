#!/bin/sh
set -eu
[ "$#" -eq 1 ] || exit 2
for page in pkgstate_canonical_generation_store.3 pkgstate-generation.5 pkgstate-check.1; do test -s "$1/product/man/$page"; out=$(mandoc -Tlint "$1/product/man/$page" 2>&1) || { printf '%s\n' "$out" >&2; exit 1; }; test -z "$out"; done
