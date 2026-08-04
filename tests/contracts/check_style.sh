#!/bin/sh
set -eu
root=$1
build_root=$2
fail(){ echo "style-contract: $*" >&2; exit 1; }
[ -s "$root/.clang-format" ] || fail 'missing .clang-format'
[ -s "$root/.editorconfig" ] || fail 'missing .editorconfig'
tab=$(printf '\t')
source_files()
{
  find "$root" \
    -path "$root/.git" -prune -o \
    -path "$build_root" -prune -o \
    -type f \( \
      -name '*.build' -o -name '*.cpp' -o -name '*.exports' -o \
      -name '*.h' -o -name '*.md' -o -name '*.options' -o \
      -name '*.sh' -o -name '*.yml' -o \
      -name '.clang-format' -o -name '.editorconfig' -o \
      -name '.gitignore' -o -name 'COPYING' -o -name 'COPYRIGHT' -o \
      -name 'Doxyfile' \
    \) -print0
}
if source_files | xargs -0 -r grep -n "$tab" >/dev/null; then fail 'tab character present'; fi
if source_files | xargs -0 -r grep -n -E '[[:blank:]]+$' >/dev/null; then fail 'trailing whitespace present'; fi
