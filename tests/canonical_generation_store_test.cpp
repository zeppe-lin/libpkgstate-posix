// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_fixture.h"
#include "temp_directory.h"
#include "test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>
#include <string>
#include <string_view>

#include <sys/stat.h>
#include <unistd.h>

int main()
{
  using namespace pkgstate;
  using pkgstate::posix::canonical_generation_store;
  static_assert(canonical_generation_storage_version == 1);
  TEST_EQ(canonical_generation_storage_format,
          std::string_view("libpkgstate-generation-v1"));

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

  const state_publication_receipt stale = store.compare_and_publish(request);
  TEST_EQ(stale.outcome(), state_publication_outcome::stale_expected_state);

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

  {
    temp_directory redirected_store_temporary;
    const std::filesystem::path configured =
        redirected_store_temporary.path() / "state";
    const std::filesystem::path held =
        redirected_store_temporary.path() / "state.held";
    const std::filesystem::path moved =
        redirected_store_temporary.path() / "state.moved";
    canonical_generation_store held_store(configured, binding);
    const snapshot held_empty = held_store.read();
    installed_package held_package =
        native_fixture::package("held", 40, binding);
    const auto held_request = state_publication_request::make(
        held_empty,
        {package_state_delta::install(
            held_package, held_package.receipt().operation_plan(),
            held_package.receipt().application_evidence())});
    TEST_EQ(held_store.compare_and_publish(held_request).outcome(),
            state_publication_outcome::published);
    const snapshot held_published = held_store.read();

    std::filesystem::rename(configured, held);
    canonical_generation_store replacement(configured, binding);
    TEST_EQ(replacement.read().size(), std::size_t{0});
    TEST_EQ(held_store.read().identity(), held_published.identity());
    TEST_EQ(held_store.root_path(), configured);

    installed_package second_package =
        native_fixture::package("held-two", 60, binding);
    const auto second_request = state_publication_request::make(
        held_published,
        {package_state_delta::install(
            second_package, second_package.receipt().operation_plan(),
            second_package.receipt().application_evidence())});
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
  }

  {
    temp_directory root_symlink_temporary;
    const std::filesystem::path real = root_symlink_temporary.path() / "real";
    const std::filesystem::path alias = root_symlink_temporary.path() / "alias";
    canonical_generation_store real_store(real, binding);
    TEST_EQ(::symlink("real", alias.c_str()), 0);
    TEST_THROWS(store_error,
                canonical_generation_store::open_existing(alias, binding));
  }

  {
    temp_directory mutable_selector_temporary;
    const std::filesystem::path mutable_root =
        mutable_selector_temporary.path() / "state";
    canonical_generation_store mutable_store(mutable_root, binding);
    TEST_EQ(::chmod((mutable_root / "current").c_str(), 0644), 0);
    TEST_THROWS(store_error, mutable_store.read());
  }

  {
    temp_directory linked_selector_temporary;
    const std::filesystem::path linked_root =
        linked_selector_temporary.path() / "state";
    canonical_generation_store linked_store(linked_root, binding);
    const std::filesystem::path selector = linked_root / "current";
    const std::filesystem::path alias = linked_root / "current.alias";
    TEST_EQ(::link(selector.c_str(), alias.c_str()), 0);
    TEST_THROWS(store_error, linked_store.read());
  }

  {
    temp_directory redirected_selector_temporary;
    const std::filesystem::path redirected_root =
        redirected_selector_temporary.path() / "state";
    canonical_generation_store redirected_store(redirected_root, binding);
    const std::filesystem::path selector = redirected_root / "current";
    const std::filesystem::path saved = redirected_root / "current.saved";
    std::filesystem::rename(selector, saved);
    TEST_EQ(::symlink("current.saved", selector.c_str()), 0);
    TEST_THROWS(store_error, redirected_store.read());
  }

  {
    temp_directory mutable_generation_temporary;
    const std::filesystem::path mutable_root =
        mutable_generation_temporary.path() / "state";
    canonical_generation_store mutable_store(mutable_root, binding);
    const snapshot selected = mutable_store.read();
    const std::string identity = selected.identity().string();
    constexpr std::string_view prefix = "v1:sha256:";
    CHECK(identity.rfind(prefix, 0) == 0);
    const std::filesystem::path generation =
        mutable_root / "generations" /
        ("v1-sha256-" + identity.substr(prefix.size()));
    TEST_EQ(::chmod(generation.c_str(), 0755), 0);
    TEST_THROWS(store_error, mutable_store.read());
  }
}
