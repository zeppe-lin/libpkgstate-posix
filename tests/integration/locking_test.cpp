// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

int main()
{
  using namespace pkgstate;
  using pkgstate::posix::canonical_generation_store;

  temp_directory temporary;
  const std::filesystem::path root = temporary.path() / "state";
  const state_target_binding binding = native_fixture::target();
  canonical_generation_store store(root, binding);
  const snapshot empty = store.read();
  const installed_package package = native_fixture::package("locked", 30, binding);
  const state_publication_request request = state_publication_request::make(
      empty,
      {package_state_delta::install(package,
                                    package.receipt().operation_plan(),
                                    package.receipt().application_evidence())});

  const int descriptor =
      ::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  CHECK(descriptor != -1);

  TEST_EQ(::flock(descriptor, LOCK_EX | LOCK_NB), 0);
  TEST_THROWS(store_error, store.read());
  TEST_THROWS(store_error,
              canonical_generation_store::open_existing(root, binding));
  TEST_EQ(::flock(descriptor, LOCK_UN), 0);

  TEST_EQ(::flock(descriptor, LOCK_SH | LOCK_NB), 0);
  TEST_EQ(store.read().identity(), empty.identity());
  {
    canonical_generation_store reader =
        canonical_generation_store::open_existing(root, binding);
    TEST_EQ(reader.read().identity(), empty.identity());
  }
  TEST_THROWS(store_error, store.compare_and_publish(request));
  TEST_THROWS(store_error, canonical_generation_store(root, binding));
  TEST_EQ(::flock(descriptor, LOCK_UN), 0);

  TEST_EQ(store.compare_and_publish(request).outcome(),
          state_publication_outcome::published);
  TEST_EQ(::close(descriptor), 0);
}
