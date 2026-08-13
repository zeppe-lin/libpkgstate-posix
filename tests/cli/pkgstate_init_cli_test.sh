#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
pkgstate_init=$1
pkgstate_check=$2
fixture=$3
temporary=$(mktemp -d)
trap 'chmod -R u+w "$temporary" 2>/dev/null || :; rm -rf "$temporary"' EXIT HUP INT TERM
fail() { echo "pkgstate-init-cli-test: $*" >&2; exit 1; }
identity()
{
  seed=$1
  python3 - "$seed" <<'PY'
import sys
seed=int(sys.argv[1])
print('v1:sha256:' + ''.join(f'{(seed+i)&255:02x}' for i in range(32)))
PY
}
binding_options()
{
  printf '%s\n' \
    --managed-target "$(identity 1)" \
    --state-store "$(identity 2)" \
    --root-view "$(identity 3)" \
    --state-backend "$(identity 4)" \
    --publication-domain "$(identity 5)"
}
run_init()
{
  store=$1
  shift
  # shellcheck disable=SC2046
  "$pkgstate_init" --canonical-store "$store" $(binding_options) "$@"
}
run_check()
{
  store=$1
  # shellcheck disable=SC2046
  "$pkgstate_check" --canonical-store "$store" $(binding_options)
}
snapshot_files()
{
  root=$1
  find "$root" -type f -print | LC_ALL=C sort | while IFS= read -r file; do
    relative=${file#"$root"/}
    printf '%s ' "$relative"
    cksum < "$file"
  done
}
output=$temporary/output
error=$temporary/error
canonical=$temporary/nested/canonical
run_init "$canonical" >"$output" 2>"$error" || fail 'empty initialization failed'
test ! -s "$error" || fail 'initialization wrote standard error'
for line in \
  'storage-format=libpkgstate-generation-v1' \
  'packages=0'
do
  grep -F "$line" "$output" >/dev/null || fail "output omits $line"
done
for field in target-binding snapshot ownership-inventory; do
  grep -E "^$field=v1:sha256:[0-9a-f]{64}$" "$output" >/dev/null ||
    fail "output omits $field"
done
grep -F "store=\"$canonical\"" "$output" >/dev/null ||
  fail 'output omits canonical store coordinate'

before=$temporary/before
after=$temporary/after
snapshot_files "$canonical" >"$before"
run_check "$canonical" >"$output" 2>"$error" ||
  fail 'initialized store failed read-only validation'
grep -F 'packages=0' "$output" >/dev/null ||
  fail 'initialized store is not empty'
snapshot_files "$canonical" >"$after"
cmp -s "$before" "$after" || fail 'read-only validation changed state'

first_output=$temporary/first-output
second_output=$temporary/second-output
run_init "$canonical" >"$first_output" 2>"$error" ||
  fail 'existing empty store validation failed'
test ! -s "$error" || fail 'existing empty validation wrote standard error'
snapshot_files "$canonical" >"$after"
cmp -s "$before" "$after" || fail 'existing empty validation changed state'
run_init "$canonical" >"$second_output" 2>"$error" ||
  fail 'repeated empty validation failed'
cmp -s "$first_output" "$second_output" ||
  fail 'repeated empty validation changed the admitted result'

set +e
"$pkgstate_init" \
  --canonical-store "$canonical" \
  --managed-target "$(identity 6)" \
  --state-store "$(identity 2)" \
  --root-view "$(identity 3)" \
  --state-backend "$(identity 4)" \
  --publication-domain "$(identity 5)" \
  >"$output" 2>"$error"
status=$?
set -e
test "$status" -eq 1 || fail "mismatched binding returned status $status"
grep -F 'does not match caller' "$error" >/dev/null ||
  fail 'mismatched binding diagnostic is missing'
snapshot_files "$canonical" >"$after"
cmp -s "$before" "$after" || fail 'mismatched binding changed state'

populated=$temporary/populated
"$fixture" "$populated"
snapshot_files "$populated" >"$before"
set +e
run_init "$populated" >"$output" 2>"$error"
status=$?
set -e
test "$status" -eq 1 || fail "populated store returned status $status"
grep -F 'already contains installed packages' "$error" >/dev/null ||
  fail 'populated-store refusal diagnostic is missing'
snapshot_files "$populated" >"$after"
cmp -s "$before" "$after" || fail 'populated-store refusal changed state'

expect_status()
{
  expected=$1
  shift
  set +e
  "$pkgstate_init" "$@" >"$output" 2>"$error"
  actual=$?
  set -e
  test "$actual" -eq "$expected" ||
    fail "expected status $expected, got $actual for: $*"
}
expect_status 2
grep -F -- '--canonical-store is required' "$error" >/dev/null ||
  fail 'missing-store usage diagnostic is absent'
expect_status 2 unexpected-positional
grep -F 'unexpected positional argument' "$error" >/dev/null ||
  fail 'positional-argument usage diagnostic is absent'
expect_status 2 --canonical-store "$temporary/missing-binding"
grep -F 'all canonical target-binding identities are required' "$error" >/dev/null ||
  fail 'missing-binding usage diagnostic is absent'

"$pkgstate_init" --help >"$output" 2>"$error" || fail 'help failed'
test ! -s "$error" || fail 'help wrote standard error'
grep -F 'admits only an empty canonical snapshot' "$output" >/dev/null ||
  fail 'help omits empty-state boundary'
"$pkgstate_init" --version >"$output" 2>"$error" || fail 'version failed'
grep -E '^pkgstate-init \(libpkgstate-posix\) [^[:space:]]+$' "$output" >/dev/null ||
  fail 'version output is malformed'
