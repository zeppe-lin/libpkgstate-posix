#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
pkgstate_check=$1
fixture=$2
temporary=$(mktemp -d)
trap 'chmod -R u+w "$temporary" 2>/dev/null || :; rm -rf "$temporary"' EXIT HUP INT TERM
fail() { echo "pkgstate-check-cli-test: $*" >&2; exit 1; }
identity()
{
  seed=$1
  python3 - "$seed" <<'PY'
import sys
seed=int(sys.argv[1])
print('v1:sha256:' + ''.join(f'{(seed+i)&255:02x}' for i in range(32)))
PY
}
canonical=$temporary/canonical
output=$temporary/output
error=$temporary/error
before=$temporary/before
after=$temporary/after
"$fixture" "$canonical"
snapshot_files()
{
  root=$1
  find "$root" -type f -print | LC_ALL=C sort | while IFS= read -r file; do
    relative=${file#"$root"/}
    printf '%s ' "$relative"
    cksum < "$file"
  done
}
snapshot_files "$canonical" > "$before"
"$pkgstate_check" \
  --canonical-store "$canonical" \
  --managed-target "$(identity 1)" \
  --state-store "$(identity 2)" \
  --root-view "$(identity 3)" \
  --state-backend "$(identity 4)" \
  --publication-domain "$(identity 5)" \
  > "$output" 2> "$error" || fail 'native diagnostics failed'
test ! -s "$error" || fail 'diagnostics wrote standard error'
for line in \
  'storage-format=libpkgstate-generation-v4' \
  'packages=1' \
  'ownership-claims=2' \
  'owned-paths=2' \
  'shared-paths=0' \
  'rejected-object-references=1' \
  'reason-explicit=1' \
  'reason-runtime-dependency=0' \
  'reason-profile=0' \
  'reason-system-policy=0'
do
  grep -F "$line" "$output" >/dev/null || fail "output omits $line"
done
for field in target-binding snapshot ownership-inventory; do
  grep -E "^$field=v1:sha256:[0-9a-f]{64}$" "$output" >/dev/null ||
    fail "output omits $field"
done
snapshot_files "$canonical" > "$after"
cmp -s "$before" "$after" || fail 'diagnostics changed durable state'
if "$pkgstate_check" \
  --canonical-store "$canonical" \
  --managed-target "$(identity 6)" \
  --state-store "$(identity 2)" \
  --root-view "$(identity 3)" \
  --state-backend "$(identity 4)" \
  --publication-domain "$(identity 5)" \
  > "$output" 2> "$error"
then
  fail 'mismatched binding succeeded'
fi
grep -F 'does not match caller' "$error" >/dev/null ||
  fail 'mismatched binding diagnostic is missing'
missing=$temporary/missing
if "$pkgstate_check" \
  --canonical-store "$missing" \
  --managed-target "$(identity 1)" \
  --state-store "$(identity 2)" \
  --root-view "$(identity 3)" \
  --state-backend "$(identity 4)" \
  --publication-domain "$(identity 5)" \
  > "$output" 2> "$error"
then
  fail 'absent store succeeded'
fi
test ! -e "$missing" || fail 'diagnostics initialized an absent store'
"$pkgstate_check" --help > "$output" 2> "$error" || fail 'help failed'
grep -F 'never initializes a store' "$output" >/dev/null ||
  fail 'help omits read-only boundary'
"$pkgstate_check" --version > "$output" 2> "$error" || fail 'version failed'
grep -E '^pkgstate-check \(libpkgstate-posix\) [^[:space:]]+$' "$output" >/dev/null ||
  fail 'version output is malformed'
