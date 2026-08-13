#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "test-layout-contract: $*" >&2; exit 1; }
meson_file=$root/tests/meson.build

for directory in contracts fixtures header integration support cli unit; do
  test -d "$root/tests/$directory" || fail "missing tests/$directory"
done
[ ! -e "$root/tests/tool" ] || fail 'legacy tests/tool role remains'
for suite in unit integration header contract cli; do
  grep -F "suite: '$suite'" "$meson_file" >/dev/null ||
    fail "Meson suite is absent: $suite"
done

for legacy in \
  tests/canonical_generation_store_test.cpp \
  tests/native_fixture.h tests/temp_directory.h tests/test.h \
  tests/public_header_test.cpp tests/umbrella_header_test.cpp \
  tests/pkgstate_check_fixture.cpp tests/pkgstate_check_cli_test.sh \
  tests/pkgstate_check_manual_test.sh; do
  test ! -e "$root/$legacy" || fail "legacy flat test remains: $legacy"
done

for test in \
  initialization_test.cpp publication_test.cpp root_authority_test.cpp \
  locking_test.cpp layout_validation_test.cpp generation_validation_test.cpp \
  recovery_refusal_test.cpp generation_collision_test.cpp \
  publication_outcome_test.cpp special_file_refusal_test.cpp; do
  test -s "$root/tests/integration/$test" || fail "missing integration/$test"
done

for test in pkgstate_check_fixture.cpp pkgstate_init_cli_test.sh pkgstate_init_manual_test.sh pkgstate_check_cli_test.sh pkgstate_check_manual_test.sh; do
  test -s "$root/tests/cli/$test" || fail "missing cli/$test"
done

# Every normal executable shell contract must be represented in Meson. The
# generated-man freshness check is conditional on Pandoc; ABI/pkg-config/style
# use specialized registration paths.
for contract in "$root"/tests/contracts/check_*.sh; do
  name=$(basename "$contract")
  case $name in
    check_abi_surface.sh|check_documentation.sh|check_manpage_generated.sh|check_pkgconfig_metadata.sh|check_style.sh)
      continue
      ;;
  esac
  stem=${name#check_}
  stem=${stem%.sh}
  grep -F "'$stem'" "$meson_file" >/dev/null ||
    fail "unregistered shell contract: $name"
done

printf '%s\n' 'test-layout-contract: ok'
