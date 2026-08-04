// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_fixture.h"
#include "temp_directory.h"
#include "test.h"

#include <filesystem>

int main()
{
  using namespace pkgstate;
  static_assert(canonical_generation_storage_version == 3);
  TEST_EQ(canonical_generation_storage_format,
          std::string_view("libpkgstate-generation-v3"));

  temp_directory temporary;
  const std::filesystem::path root = temporary.path() / "state";
  const state_target_binding binding = native_fixture::target();
  canonical_generation_store store(root, binding);

  const snapshot empty = snapshot::make(binding);
  TEST_EQ(store.read().identity(), empty.identity());

  installed_package proposed = native_fixture::package("example", 20, binding);
  state_publication_request request = state_publication_request::make(
      empty,
      {package_state_delta::install(
          proposed, proposed.receipt().operation_plan(),
          proposed.receipt().application_evidence())});
  const state_publication_receipt publication =
      store.compare_and_publish(request);
  TEST_EQ(publication.outcome(), state_publication_outcome::published);

  const snapshot reread = store.read();
  TEST_EQ(reread.size(), std::size_t{1});
  TEST_EQ(reread.find_package("example")->receipt(), proposed.receipt());
  TEST_EQ(reread.identity(), publication.resulting_snapshot().value());

  canonical_generation_store existing =
      canonical_generation_store::open_existing(root, binding);
  TEST_EQ(existing.read().identity(), reread.identity());
  TEST_THROWS(store_error,
              canonical_generation_store::open_existing(
                  root, native_fixture::target(80)));
}
