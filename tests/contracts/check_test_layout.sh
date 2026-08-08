#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "test-layout-contract: $*" >&2; exit 1; }

for directory in contracts fixtures header integration support tool unit; do
  test -d "$root/tests/$directory" || fail "missing tests/$directory"
done

for suite in unit integration header contract tool; do
  grep -F "suite: '$suite'" "$root/tests/meson.build" >/dev/null ||
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
  publication_outcome_test.cpp; do
  test -s "$root/tests/integration/$test" || fail "missing integration/$test"
done

printf '%s\n' 'test-layout-contract: ok'
