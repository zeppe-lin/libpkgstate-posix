// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <string>

#include <limits.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

enum class fsync_fault {
  none,
  next_call,
  current_temporary,
  root_after_selection,
  root_after_selection_and_lose_selector,
};

fsync_fault fault = fsync_fault::none;
dev_t root_device = 0;
ino_t root_inode = 0;

void
bind_fault_root(const std::filesystem::path& root)
{
  struct stat status {};
  CHECK(::stat(root.c_str(), &status) == 0);
  root_device = status.st_dev;
  root_inode = status.st_ino;
}

[[nodiscard]] bool
is_bound_root(int descriptor)
{
  struct stat status {};
  if (::fstat(descriptor, &status) == -1)
    return false;
  return status.st_dev == root_device && status.st_ino == root_inode;
}

[[nodiscard]] bool
is_current_temporary(int descriptor)
{
  const std::string proc = "/proc/self/fd/" + std::to_string(descriptor);
  char target[PATH_MAX + 1] = {};
  const ssize_t size = ::readlink(proc.c_str(), target, PATH_MAX);
  if (size <= 0)
    return false;
  target[size] = '\0';
  return std::string(target).find("/current.tmp.") != std::string::npos;
}

} // namespace

extern "C" int
fsync(int descriptor)
{
  if (fault == fsync_fault::next_call)
  {
    fault = fsync_fault::none;
    errno = EIO;
    return -1;
  }

  if (fault == fsync_fault::current_temporary &&
      is_current_temporary(descriptor))
  {
    fault = fsync_fault::none;
    errno = EIO;
    return -1;
  }

  if ((fault == fsync_fault::root_after_selection ||
       fault == fsync_fault::root_after_selection_and_lose_selector) &&
      is_bound_root(descriptor))
  {
    if (fault == fsync_fault::root_after_selection_and_lose_selector)
    {
      static_cast<void>(::syscall(
          SYS_unlinkat, descriptor, "current", 0));
    }
    fault = fsync_fault::none;
    errno = EIO;
    return -1;
  }

  return static_cast<int>(::syscall(SYS_fsync, descriptor));
}

namespace {

pkgstate::state_publication_request
installation_request(const pkgstate::snapshot& current,
                     const pkgstate::installed_package& package)
{
  return pkgstate::state_publication_request::make(
      current,
      {pkgstate::package_state_delta::install(
          package,
          package.receipt().operation_plan(),
          package.receipt().application_evidence())});
}

} // namespace

int main()
{
  using namespace pkgstate;
  using pkgstate::posix::canonical_generation_store;

  {
    temp_directory temporary;
    const auto binding = native_fixture::target();
    const auto root = temporary.path() / "failed-before";
    canonical_generation_store store(root, binding);
    bind_fault_root(root);
    const snapshot empty = store.read();
    const installed_package package =
        native_fixture::package("before", 20, binding);

    fault = fsync_fault::next_call;
    const state_publication_receipt receipt =
        store.compare_and_publish(installation_request(empty, package));
    TEST_EQ(fault, fsync_fault::none);
    TEST_EQ(receipt.outcome(),
            state_publication_outcome::failed_before_publication);
    TEST_EQ(receipt.durability(), state_publication_durability::not_attempted);
    TEST_EQ(receipt.atomicity_boundary(),
            state_storage_atomicity_boundary::none);
    CHECK(!receipt.resulting_snapshot().has_value());
    TEST_EQ(store.read().identity(), empty.identity());
    TEST_EQ(test_support::generation_count(root), std::size_t{1});
    CHECK(!test_support::has_temporary_entry(root));
  }

  {
    temp_directory temporary;
    const auto binding = native_fixture::target();
    const auto root = temporary.path() / "generation-before-selector";
    canonical_generation_store store(root, binding);
    bind_fault_root(root);
    const snapshot empty = store.read();
    const installed_package package =
        native_fixture::package("orphan", 30, binding);
    const auto request = installation_request(empty, package);

    fault = fsync_fault::current_temporary;
    const state_publication_receipt receipt =
        store.compare_and_publish(request);
    TEST_EQ(fault, fsync_fault::none);
    TEST_EQ(receipt.outcome(),
            state_publication_outcome::failed_before_publication);
    TEST_EQ(store.read().identity(), empty.identity());
    TEST_EQ(test_support::generation_count(root), std::size_t{2});
    CHECK(!test_support::has_temporary_entry(root));

    const state_publication_receipt retry = store.compare_and_publish(request);
    TEST_EQ(retry.outcome(), state_publication_outcome::published);
    TEST_EQ(test_support::generation_count(root), std::size_t{2});
    TEST_EQ(store.read().size(), std::size_t{1});
  }

  {
    temp_directory temporary;
    const auto binding = native_fixture::target();
    const auto root = temporary.path() / "unconfirmed";
    canonical_generation_store store(root, binding);
    bind_fault_root(root);
    const snapshot empty = store.read();
    const installed_package package =
        native_fixture::package("unconfirmed", 40, binding);

    fault = fsync_fault::root_after_selection;
    const state_publication_receipt receipt =
        store.compare_and_publish(installation_request(empty, package));
    TEST_EQ(fault, fsync_fault::none);
    TEST_EQ(receipt.outcome(),
            state_publication_outcome::published_durability_unconfirmed);
    TEST_EQ(receipt.durability(), state_publication_durability::unconfirmed);
    TEST_EQ(receipt.atomicity_boundary(),
            state_storage_atomicity_boundary::immutable_generation_selection);
    CHECK(receipt.resulting_snapshot().has_value());
    TEST_EQ(store.read().identity(), *receipt.resulting_snapshot());
    TEST_EQ(store.read().size(), std::size_t{1});
  }

  {
    temp_directory temporary;
    const auto binding = native_fixture::target();
    const auto root = temporary.path() / "indeterminate";
    canonical_generation_store store(root, binding);
    bind_fault_root(root);
    const snapshot empty = store.read();
    const installed_package package =
        native_fixture::package("indeterminate", 60, binding);

    fault = fsync_fault::root_after_selection_and_lose_selector;
    const state_publication_receipt receipt =
        store.compare_and_publish(installation_request(empty, package));
    TEST_EQ(fault, fsync_fault::none);
    TEST_EQ(receipt.outcome(), state_publication_outcome::indeterminate);
    TEST_EQ(receipt.durability(), state_publication_durability::indeterminate);
    TEST_EQ(receipt.atomicity_boundary(),
            state_storage_atomicity_boundary::immutable_generation_selection);
    CHECK(!receipt.resulting_snapshot().has_value());
    CHECK(!receipt.subordinate_evidence().empty());
    TEST_THROWS(store_error, store.read());
  }
}
