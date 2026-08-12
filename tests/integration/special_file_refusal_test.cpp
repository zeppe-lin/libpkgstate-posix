// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <cstdlib>
#include <filesystem>
#include <functional>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using mutation = std::function<void(const std::filesystem::path&,
                                    const pkgstate::snapshot&)>;

void
replace_with_fifo(const std::filesystem::path& path)
{
  TEST_EQ(::unlink(path.c_str()), 0);
  TEST_EQ(::mkfifo(path.c_str(), 0444), 0);
}

void
expect_existing_store_refusal_without_block(const mutation& mutate)
{
  using pkgstate::store_error;
  using pkgstate::posix::canonical_generation_store;

  temp_directory temporary;
  const std::filesystem::path root = temporary.path() / "state";
  const auto binding = native_fixture::target();
  pkgstate::snapshot selected = pkgstate::snapshot::make(binding, {});
  {
    canonical_generation_store store(root, binding);
    selected = store.read();
  }

  mutate(root, selected);

  const pid_t child = ::fork();
  TEST_NE(child, -1);
  if (child == 0)
  {
    ::alarm(2);
    try
    {
      static_cast<void>(
          canonical_generation_store::open_existing(root, binding));
    }
    catch (const store_error&)
    {
      std::_Exit(0);
    }
    catch (...)
    {
      std::_Exit(2);
    }
    std::_Exit(1);
  }

  int status = 0;
  TEST_EQ(::waitpid(child, &status, 0), child);
  TEST(WIFEXITED(status));
  TEST_EQ(WEXITSTATUS(status), 0);
}

} // namespace

int
main()
{
  expect_existing_store_refusal_without_block(
      [](const std::filesystem::path& root, const pkgstate::snapshot&) {
        replace_with_fifo(root / "binding");
      });

  expect_existing_store_refusal_without_block(
      [](const std::filesystem::path& root, const pkgstate::snapshot&) {
        replace_with_fifo(root / "current");
      });

  expect_existing_store_refusal_without_block(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const auto generation =
            test_support::generation_path(root, selected.identity());
        TEST_EQ(::chmod(generation.c_str(), 0755), 0);
        replace_with_fifo(generation / "snapshot");
        TEST_EQ(::chmod(generation.c_str(), 0555), 0);
      });
}
