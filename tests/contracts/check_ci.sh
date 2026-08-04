#!/bin/sh
set -eu
root=$1
fail(){ echo "ci-contract: $*" >&2; exit 1; }
for text in 'GCC shared' 'GCC static' 'Clang shared' 'Clang static' 'GCC release' 'address,undefined' 'v3.0.0'; do grep -F "$text" "$root/.github/workflows/ci.yml" >/dev/null || fail "CI omits $text"; done
for script in configure-and-test.sh qualify-installed.sh audit-shared-boundary.sh lint-manpages.sh; do test -x "$root/ci/$script" || fail "missing executable ci/$script"; done
