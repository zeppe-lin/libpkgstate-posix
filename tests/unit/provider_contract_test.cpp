// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <string_view>
#include <type_traits>

int main()
{
  using pkgstate::canonical_store;
  using pkgstate::posix::canonical_generation_store;

  static_assert(pkgstate::canonical_generation_storage_version == 1);
  TEST_EQ(pkgstate::canonical_generation_storage_format,
          std::string_view("libpkgstate-generation-v1"));

  static_assert(std::is_base_of_v<canonical_store,
                                  canonical_generation_store>);
  static_assert(!std::is_copy_constructible_v<canonical_generation_store>);
  static_assert(!std::is_copy_assignable_v<canonical_generation_store>);
  static_assert(!std::is_move_constructible_v<canonical_generation_store>);
  static_assert(!std::is_move_assignable_v<canonical_generation_store>);
}
