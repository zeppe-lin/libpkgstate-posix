// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <filesystem>
#include <functional>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

namespace {

using mutation = std::function<void(const std::filesystem::path&)>;

void
expect_existing_store_refusal(const mutation& mutate)
{
  using pkgstate::store_error;
  using pkgstate::posix::canonical_generation_store;

  temp_directory temporary;
  const std::filesystem::path root = temporary.path() / "state";
  const auto binding = native_fixture::target();
  {
    canonical_generation_store store(root, binding);
    TEST_EQ(store.read().size(), std::size_t{0});
  }

  mutate(root);
  TEST_THROWS(store_error,
              canonical_generation_store::open_existing(root, binding));
}

} // namespace

int main()
{
  expect_existing_store_refusal([](const std::filesystem::path& root) {
    TEST_EQ(::unlink((root / "binding").c_str()), 0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    TEST_EQ(::chmod((root / "binding").c_str(), 0644), 0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    TEST_EQ(::link((root / "binding").c_str(),
                   (root / "binding.alias").c_str()),
            0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    std::filesystem::rename(root / "binding", root / "binding.saved");
    TEST_EQ(::symlink("binding.saved", (root / "binding").c_str()), 0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    test_support::rewrite_immutable(root / "binding", "broken-binding\n");
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    TEST_EQ(::unlink((root / "current").c_str()), 0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    TEST_EQ(::chmod((root / "current").c_str(), 0644), 0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    TEST_EQ(::link((root / "current").c_str(),
                   (root / "current.alias").c_str()),
            0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    std::filesystem::rename(root / "current", root / "current.saved");
    TEST_EQ(::symlink("current.saved", (root / "current").c_str()), 0);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    test_support::rewrite_immutable(root / "current", "not-an-identity\n");
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    std::string selector = test_support::selector_text(root);
    CHECK(!selector.empty() && selector.back() == '\n');
    selector.pop_back();
    test_support::rewrite_immutable(root / "current", selector);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    std::string selector = test_support::selector_text(root);
    selector += "v1:sha256:0000000000000000000000000000000000000000000000000000000000000000\n";
    test_support::rewrite_immutable(root / "current", selector);
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    std::filesystem::rename(root / "generations", root / "generations.saved");
  });

  expect_existing_store_refusal([](const std::filesystem::path& root) {
    std::filesystem::rename(root / "generations", root / "generations.saved");
    TEST_EQ(::symlink("generations.saved", (root / "generations").c_str()), 0);
  });
}
