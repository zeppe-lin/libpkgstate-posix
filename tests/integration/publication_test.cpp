// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include <openssl/evp.h>

namespace {

pkgstate::state_publication_evidence_identity
evidence_for(const std::vector<std::uint8_t>& bytes)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  CHECK(context != nullptr);
  CHECK(EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) == 1);
  CHECK(EVP_DigestUpdate(context.get(), bytes.data(), bytes.size()) == 1);

  pkgstate::sha256_digest_bytes digest{};
  unsigned int size = 0;
  CHECK(EVP_DigestFinal_ex(context.get(), digest.data(), &size) == 1);
  TEST_EQ(size, static_cast<unsigned int>(digest.size()));
  return pkgstate::state_publication_evidence_identity::from_sha256(digest);
}

} // namespace

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
  const state_publication_request install = state_publication_request::make(
      empty,
      {package_state_delta::install(package,
                                    package.receipt().operation_plan(),
                                    package.receipt().application_evidence())});

  const state_publication_receipt published =
      store.compare_and_publish(install);
  TEST_EQ(published.outcome(), state_publication_outcome::published);
  TEST_EQ(published.durability(), state_publication_durability::confirmed);
  TEST_EQ(published.atomicity_boundary(),
          state_storage_atomicity_boundary::immutable_generation_selection);
  TEST_EQ(published.storage_format(),
          std::string(canonical_generation_storage_format));
  CHECK(published.resulting_snapshot().has_value());
  TEST_EQ(published.subordinate_evidence().size(), std::size_t{1});

  const snapshot installed = store.read();
  TEST_EQ(installed.size(), std::size_t{1});
  TEST_EQ(installed.identity(), *published.resulting_snapshot());
  const std::filesystem::path installed_generation =
      test_support::generation_path(root, installed.identity());
  const auto stored_bytes =
      test_support::read_bytes(installed_generation / "snapshot");
  TEST_EQ(published.subordinate_evidence().front(),
          evidence_for(stored_bytes));
  TEST_EQ(test_support::generation_count(root), std::size_t{2});

  const std::string selector_before_stale = test_support::selector_text(root);
  const auto layout_before_stale = test_support::layout_inventory(root);
  const state_publication_receipt stale = store.compare_and_publish(install);
  TEST_EQ(stale.outcome(), state_publication_outcome::stale_expected_state);
  TEST_EQ(stale.durability(), state_publication_durability::not_attempted);
  TEST_EQ(stale.atomicity_boundary(), state_storage_atomicity_boundary::none);
  CHECK(stale.subordinate_evidence().empty());
  TEST_EQ(test_support::selector_text(root), selector_before_stale);
  TEST_EQ(test_support::layout_inventory(root), layout_before_stale);

  const state_publication_request removal = state_publication_request::make(
      installed,
      {package_state_delta::remove(
          "example",
          package.identity(),
          native_fixture::identity<operation_plan_identity>(90),
          native_fixture::identity<application_evidence_identity>(91))});
  const state_publication_receipt removed =
      store.compare_and_publish(removal);
  TEST_EQ(removed.outcome(), state_publication_outcome::published);
  TEST_EQ(store.read().identity(), empty.identity());
  TEST_EQ(test_support::generation_count(root), std::size_t{2});

  const state_publication_request reinstall = state_publication_request::make(
      store.read(),
      {package_state_delta::install(package,
                                    package.receipt().operation_plan(),
                                    package.receipt().application_evidence())});
  const state_publication_receipt republished =
      store.compare_and_publish(reinstall);
  TEST_EQ(republished.outcome(), state_publication_outcome::published);
  TEST_EQ(store.read().identity(), installed.identity());
  TEST_EQ(test_support::generation_count(root), std::size_t{2});
  TEST_EQ(republished.subordinate_evidence(),
          published.subordinate_evidence());
  CHECK(!test_support::has_temporary_entry(root));
}
