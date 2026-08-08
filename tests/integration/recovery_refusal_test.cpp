// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>

#include <sys/stat.h>
#include <unistd.h>

int main()
{
  using namespace pkgstate;
  using pkgstate::posix::canonical_generation_store;

  const state_target_binding binding = native_fixture::target();

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    {
      canonical_generation_store store(root, binding);
      TEST_EQ(store.read().size(), std::size_t{0});
    }
    TEST_EQ(::unlink((root / "binding").c_str()), 0);
    TEST_THROWS(store_error,
                canonical_generation_store::open_existing(root, binding));
    canonical_generation_store resumed_initialization(root, binding);
    TEST_EQ(resumed_initialization.read().size(), std::size_t{0});
    CHECK(std::filesystem::is_regular_file(root / "binding"));
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    {
      canonical_generation_store store(root, binding);
    }
    std::filesystem::rename(root / "current", root / "current.lost");
    TEST_THROWS(store_error, canonical_generation_store(root, binding));
    CHECK(!std::filesystem::exists(root / "current"));
    CHECK(std::filesystem::exists(root / "current.lost"));
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    pkgstate::installed_state_snapshot_identity selected =
        pkgstate::snapshot::make(binding).identity();
    {
      canonical_generation_store store(root, binding);
      selected = store.read().identity();
    }
    const auto generation = test_support::generation_path(root, selected);
    std::filesystem::rename(generation, generation.string() + ".lost");
    TEST_THROWS(store_error, canonical_generation_store(root, binding));
    CHECK(!std::filesystem::exists(generation));
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    canonical_generation_store store(root, binding);
    const snapshot selected = store.read();

    test_support::write_text(root / "current.tmp.crash", "garbage\n");
    std::filesystem::create_directory(root / "generations" /
                                      "generation.tmp.crash");
    test_support::write_text(root / "generations" /
                                 "generation.tmp.crash" / "snapshot",
                             "incomplete");

    TEST_EQ(store.read().identity(), selected.identity());
    canonical_generation_store existing =
        canonical_generation_store::open_existing(root, binding);
    TEST_EQ(existing.read().identity(), selected.identity());
    CHECK(std::filesystem::exists(root / "current.tmp.crash"));
    CHECK(std::filesystem::exists(root / "generations" /
                                  "generation.tmp.crash"));
  }

  {
    temp_directory temporary;
    const std::filesystem::path root = temporary.path() / "state";
    {
      canonical_generation_store store(root, binding);
      const snapshot empty = store.read();
      const installed_package package = native_fixture::package("claimed", 30, binding);
      const auto request = state_publication_request::make(
          empty,
          {package_state_delta::install(package,
                                        package.receipt().operation_plan(),
                                        package.receipt().application_evidence())});
      TEST_EQ(store.compare_and_publish(request).outcome(),
              state_publication_outcome::published);
    }
    TEST_EQ(::unlink((root / "binding").c_str()), 0);
    TEST_THROWS(store_error, canonical_generation_store(root, binding));
    CHECK(!std::filesystem::exists(root / "binding"));
  }
}
