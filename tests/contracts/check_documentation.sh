#!/bin/sh
set -eu
root=$1
state_include=${2:-}
fail(){ echo "documentation-contract: $*" >&2; exit 1; }
if [ -z "$state_include" ]; then
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists 'libpkgstate >= 3.0.0'; then
    state_include=$(pkg-config --variable=includedir libpkgstate)
  else
    fail 'libpkgstate include root is unavailable'
  fi
fi
for f in man/libpkgstate-posix.3.scdoc README.md HISTORY.md DESIGN.md STORAGE.md TESTING.md CONTRIBUTING.md MAINTAINING.md docs/architecture.md docs/abi.md docs/integration.md docs/qualification.md docs/code-style.md; do test -s "$root/$f" || fail "missing $f"; done
grep -F 'mechanism provider' "$root/docs/architecture.md" >/dev/null || fail 'provider placement absent'
grep -F 'does not depend on this provider' "$root/docs/architecture.md" >/dev/null || fail 'dependency direction absent'

grep -F 'state-owned generation codec' "$root/docs/architecture.md" >/dev/null || fail 'protocol ownership absent'
grep -F 'Authoritative regular-file opens are non-blocking' "$root/STORAGE.md" >/dev/null || fail 'non-blocking special-file refusal absent'
python3 "$root/tools/check-public-documentation.py" \
  "$root" libpkgstate-posix libpkgstate-posix.h
if command -v clang++ >/dev/null 2>&1; then
  python3 "$root/tools/check-doxygen-contract.py" \
    --root "$root" --include-subdir libpkgstate-posix \
    --include-root "$state_include" \
    --namespace pkgstate --clang "$(command -v clang++)"
fi

python3 "$root/tools/check-man-markdown.py" \
  --root "$root" --project libpkgstate-posix --version 3.0.0
python3 "$root/tools/check-html-manifest.py" \
  --root "$root" --project libpkgstate-posix
