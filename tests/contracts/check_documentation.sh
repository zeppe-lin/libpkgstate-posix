#!/bin/sh
set -eu
root=$1
fail(){ echo "documentation-contract: $*" >&2; exit 1; }
for f in man/libpkgstate-posix.3.scdoc README.md HISTORY.md DESIGN.md STORAGE.md TESTING.md CONTRIBUTING.md MAINTAINING.md docs/architecture.md docs/abi.md docs/integration.md docs/qualification.md docs/code-style.md; do test -s "$root/$f" || fail "missing $f"; done
grep -F 'mechanism provider' "$root/docs/architecture.md" >/dev/null || fail 'provider placement absent'
grep -F 'does not depend on this provider' "$root/docs/architecture.md" >/dev/null || fail 'dependency direction absent'
