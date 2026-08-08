// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>

int main()
{
  using namespace pkgstate;
  using pkgstate::posix::canonical_generation_store;

  temp_directory temporary;
  const std::filesystem::path root = temporary.path() / "state";
  const state_target_binding binding = native_fixture::target();
  canonical_generation_store store(root, binding);

  const snapshot empty = store.read();
  const installed_package package = native_fixture::package("example", 20, binding);
  const auto install = state_publication_request::make(
      empty,
      {package_state_delta::install(package,
                                    package.receipt().operation_plan(),
                                    package.receipt().application_evidence())});
  TEST_EQ(store.compare_and_publish(install).outcome(),
          state_publication_outcome::published);
  const snapshot installed = store.read();

  const auto remove = state_publication_request::make(
      installed,
      {package_state_delta::remove(
          "example",
          package.identity(),
          native_fixture::identity<operation_plan_identity>(90),
          native_fixture::identity<application_evidence_identity>(91))});
  TEST_EQ(store.compare_and_publish(remove).outcome(),
          state_publication_outcome::published);
  TEST_EQ(store.read().identity(), empty.identity());

  const std::filesystem::path old_generation =
      test_support::generation_path(root, installed.identity());
  const std::filesystem::path old_snapshot = old_generation / "snapshot";
  test_support::rewrite_immutable(old_snapshot, "corrupt old generation\n");

  const std::string selector_before = test_support::selector_text(root);
  const auto layout_before = test_support::layout_inventory(root);
  const auto reinstall = state_publication_request::make(
      store.read(),
      {package_state_delta::install(package,
                                    package.receipt().operation_plan(),
                                    package.receipt().application_evidence())});
  TEST_THROWS(store_error, store.compare_and_publish(reinstall));
  TEST_EQ(store.read().identity(), empty.identity());
  TEST_EQ(test_support::selector_text(root), selector_before);
  TEST_EQ(test_support::layout_inventory(root), layout_before);
}
