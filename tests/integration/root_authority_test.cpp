// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>

#include <unistd.h>

int main()
{
  using namespace pkgstate;
  using pkgstate::posix::canonical_generation_store;

  temp_directory temporary;
  const state_target_binding binding = native_fixture::target();
  const std::filesystem::path configured = temporary.path() / "state";
  const std::filesystem::path held = temporary.path() / "state.held";
  const std::filesystem::path moved = temporary.path() / "state.moved";

  canonical_generation_store held_store(configured, binding);
  const snapshot empty = held_store.read();
  const installed_package first = native_fixture::package("held", 40, binding);
  const auto first_request = state_publication_request::make(
      empty,
      {package_state_delta::install(first,
                                    first.receipt().operation_plan(),
                                    first.receipt().application_evidence())});
  TEST_EQ(held_store.compare_and_publish(first_request).outcome(),
          state_publication_outcome::published);
  const snapshot held_published = held_store.read();

  std::filesystem::rename(configured, held);
  canonical_generation_store replacement(configured, binding);
  TEST_EQ(replacement.read().size(), std::size_t{0});
  TEST_EQ(held_store.read().identity(), held_published.identity());
  TEST_EQ(held_store.root_path(), configured);

  const installed_package second =
      native_fixture::package("held-two", 60, binding);
  const auto second_request = state_publication_request::make(
      held_published,
      {package_state_delta::install(second,
                                    second.receipt().operation_plan(),
                                    second.receipt().application_evidence())});
  TEST_EQ(held_store.compare_and_publish(second_request).outcome(),
          state_publication_outcome::published);
  TEST_EQ(held_store.read().size(), std::size_t{2});
  TEST_EQ(replacement.read().size(), std::size_t{0});

  canonical_generation_store reopened =
      canonical_generation_store::open_existing(held, binding);
  std::filesystem::rename(held, moved);
  canonical_generation_store second_replacement(held, binding);
  TEST_EQ(second_replacement.read().size(), std::size_t{0});
  TEST_EQ(reopened.read().size(), std::size_t{2});
  TEST_EQ(held_store.read().size(), std::size_t{2});

  const std::filesystem::path real = temporary.path() / "real";
  const std::filesystem::path alias = temporary.path() / "alias";
  canonical_generation_store real_store(real, binding);
  TEST_EQ(::symlink("real", alias.c_str()), 0);
  TEST_THROWS(store_error,
              canonical_generation_store::open_existing(alias, binding));
  TEST_THROWS(store_error, canonical_generation_store(alias, binding));
}
