// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>

#include <sys/stat.h>

int main()
{
  using pkgstate::snapshot;
  using pkgstate::store_error;
  using pkgstate::posix::canonical_generation_store;

  const auto binding = native_fixture::target();

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    CHECK(!std::filesystem::exists(root));

    canonical_generation_store store(root, binding);
    const snapshot empty = store.read();
    const snapshot expected_empty = snapshot::make(binding);
    TEST_EQ(empty.identity(), expected_empty.identity());
    TEST_EQ(empty.size(), expected_empty.size());
    TEST_EQ(store.root_path(), root);
    TEST_EQ(store.target_binding(), binding);
    CHECK(std::filesystem::is_regular_file(root / "binding"));
    CHECK(std::filesystem::is_regular_file(root / "current"));
    CHECK(std::filesystem::is_directory(root / "generations"));
    TEST_EQ(test_support::generation_count(root), std::size_t{1});
    const std::filesystem::path generation =
        test_support::generation_path(root, empty.identity());
    const std::filesystem::path snapshot_file = generation / "snapshot";
    CHECK(std::filesystem::is_regular_file(snapshot_file));
    TEST_EQ(test_support::read_bytes(root / "binding"),
            pkgstate::encode_generation_binding(binding));
    TEST_EQ(test_support::selector_text(root), empty.identity().string() + "\n");
    TEST_EQ(test_support::read_bytes(snapshot_file),
            pkgstate::encode_generation_snapshot(empty));

    struct stat binding_status {};
    struct stat current_status {};
    struct stat generation_status {};
    struct stat snapshot_status {};
    TEST_EQ(::stat((root / "binding").c_str(), &binding_status), 0);
    TEST_EQ(::stat((root / "current").c_str(), &current_status), 0);
    TEST_EQ(::stat(generation.c_str(), &generation_status), 0);
    TEST_EQ(::stat(snapshot_file.c_str(), &snapshot_status), 0);
    TEST_EQ(binding_status.st_nlink, static_cast<nlink_t>(1));
    TEST_EQ(current_status.st_nlink, static_cast<nlink_t>(1));
    TEST_EQ(snapshot_status.st_nlink, static_cast<nlink_t>(1));
    TEST_EQ(binding_status.st_mode & 0222, static_cast<mode_t>(0));
    TEST_EQ(current_status.st_mode & 0222, static_cast<mode_t>(0));
    TEST_EQ(generation_status.st_mode & 0222, static_cast<mode_t>(0));
    TEST_EQ(snapshot_status.st_mode & 0222, static_cast<mode_t>(0));
    CHECK(!test_support::has_temporary_entry(root));

    canonical_generation_store existing =
        canonical_generation_store::open_existing(root, binding);
    TEST_EQ(existing.read().identity(), empty.identity());
    TEST_THROWS(store_error,
                canonical_generation_store::open_existing(
                    root, native_fixture::target(80)));
    TEST_THROWS(store_error,
                canonical_generation_store(root,
                                           native_fixture::target(80)));
    TEST_EQ(store.read().identity(), empty.identity());
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "missing";
    TEST_THROWS(store_error,
                canonical_generation_store::open_existing(root, binding));
    CHECK(!std::filesystem::exists(root));
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    std::filesystem::create_directories(root);
    canonical_generation_store store(root, binding);
    TEST_EQ(store.read().size(), std::size_t{0});
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "not-a-directory";
    test_support::write_text(root, "not a store\n");
    TEST_THROWS(store_error, canonical_generation_store(root, binding));
  }

  TEST_THROWS(store_error,
              canonical_generation_store(std::filesystem::path{}, binding));
}
