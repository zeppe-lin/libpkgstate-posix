#!/bin/sh
set -eu
root=$1
fail(){ echo "extraction-contract: $*" >&2; exit 1; }
source_commit=$(sed -n 's/^commit=//p' "$root/EXTRACTED_FROM")
test -n "$source_commit" || fail 'source commit absent'
root_commit=$(git -C "$root" rev-list --max-parents=0 HEAD)
for path in src/canonical_generation_store.cpp src/generation_codec.cpp src/generation_codec.h tests/canonical_generation_store_test.cpp tools/pkgstate_check.cpp; do
  git -C "$root" cat-file -e "$root_commit:$path" || fail "root extraction omits $path"
done
