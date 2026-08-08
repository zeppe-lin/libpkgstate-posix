// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"
#include "../support/store_layout.h"
#include "../support/temp_directory.h"
#include "../support/test.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <libpkgstate/generation_codec.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace {

using mutation = std::function<void(const std::filesystem::path&,
                                    const pkgstate::snapshot&)>;

void
expect_selected_generation_refusal(const mutation& mutate)
{
  using pkgstate::store_error;
  using pkgstate::posix::canonical_generation_store;

  temp_directory temporary;
  const std::filesystem::path root = temporary.path() / "state";
  const auto binding = native_fixture::target();
  canonical_generation_store store(root, binding);
  const pkgstate::snapshot selected = store.read();

  mutate(root, selected);
  TEST_THROWS(store_error, store.read());
}

void
replace_snapshot_bytes(const std::filesystem::path& root,
                       const pkgstate::snapshot& selected,
                       const std::vector<std::uint8_t>& bytes)
{
  test_support::rewrite_immutable(
      test_support::generation_path(root, selected.identity()) / "snapshot",
      bytes);
}

} // namespace

int main()
{
  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        TEST_EQ(::chmod(test_support::generation_path(root, selected.identity()).c_str(),
                        0755),
                0);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const auto generation =
            test_support::generation_path(root, selected.identity());
        const auto saved = generation.string() + ".saved";
        std::filesystem::rename(generation, saved);
        TEST_EQ(::symlink(std::filesystem::path(saved).filename().c_str(),
                          generation.c_str()),
                0);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const auto generation =
            test_support::generation_path(root, selected.identity());
        TEST_EQ(::chmod(generation.c_str(), 0755), 0);
        TEST_EQ(::unlink((generation / "snapshot").c_str()), 0);
        TEST_EQ(::chmod(generation.c_str(), 0555), 0);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        TEST_EQ(::chmod((test_support::generation_path(root, selected.identity()) /
                         "snapshot")
                            .c_str(),
                        0644),
                0);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const auto generation =
            test_support::generation_path(root, selected.identity());
        TEST_EQ(::chmod(generation.c_str(), 0755), 0);
        TEST_EQ(::link((generation / "snapshot").c_str(),
                       (generation / "snapshot.alias").c_str()),
                0);
        TEST_EQ(::chmod(generation.c_str(), 0555), 0);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const auto generation =
            test_support::generation_path(root, selected.identity());
        TEST_EQ(::chmod(generation.c_str(), 0755), 0);
        std::filesystem::rename(generation / "snapshot",
                                generation / "snapshot.saved");
        TEST_EQ(::symlink("snapshot.saved", (generation / "snapshot").c_str()),
                0);
        TEST_EQ(::chmod(generation.c_str(), 0555), 0);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        replace_snapshot_bytes(root, selected,
                               std::vector<std::uint8_t>{'b', 'a', 'd'});
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        std::vector<std::uint8_t> bytes =
            pkgstate::encode_generation_snapshot(selected);
        bytes.push_back(0xff);
        replace_snapshot_bytes(root, selected, bytes);
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const pkgstate::snapshot different = native_fixture::state_with_package(
            "different", 50, selected.target_binding());
        replace_snapshot_bytes(root,
                               selected,
                               pkgstate::encode_generation_snapshot(different));
      });

  expect_selected_generation_refusal(
      [](const std::filesystem::path& root, const pkgstate::snapshot& selected) {
        const pkgstate::snapshot wrong_target =
            pkgstate::snapshot::make(native_fixture::target(90));
        replace_snapshot_bytes(root,
                               selected,
                               pkgstate::encode_generation_snapshot(wrong_target));
      });
}
