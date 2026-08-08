// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>

#include <libpkgstate/digest.h>

namespace test_support {

[[nodiscard]] inline std::vector<std::uint8_t>
read_bytes(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("could not read test file " + path.string());
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),
                                   std::istreambuf_iterator<char>());
}

inline void
write_bytes(const std::filesystem::path& path,
            const std::vector<std::uint8_t>& bytes)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("could not write test file " + path.string());
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output)
    throw std::runtime_error("could not write test file " + path.string());
}

inline void
write_text(const std::filesystem::path& path, std::string_view text)
{
  write_bytes(path,
              std::vector<std::uint8_t>(text.begin(), text.end()));
}

inline void
rewrite_immutable(const std::filesystem::path& path,
                  const std::vector<std::uint8_t>& bytes)
{
  if (::chmod(path.c_str(), 0644) == -1)
    throw std::runtime_error("could not make test file writable " +
                             path.string());
  write_bytes(path, bytes);
  if (::chmod(path.c_str(), 0444) == -1)
    throw std::runtime_error("could not restore test file mode " +
                             path.string());
}

inline void
rewrite_immutable(const std::filesystem::path& path, std::string_view text)
{
  rewrite_immutable(path,
                    std::vector<std::uint8_t>(text.begin(), text.end()));
}

[[nodiscard]] inline std::string
generation_component(const pkgstate::installed_state_snapshot_identity& identity)
{
  const std::string text = identity.string();
  constexpr std::string_view prefix = "v1:sha256:";
  if (text.rfind(prefix, 0) != 0)
    throw std::runtime_error("unexpected snapshot identity in test fixture");
  return "v1-sha256-" + text.substr(prefix.size());
}

[[nodiscard]] inline std::filesystem::path
generation_path(const std::filesystem::path& root,
                const pkgstate::installed_state_snapshot_identity& identity)
{
  return root / "generations" / generation_component(identity);
}

[[nodiscard]] inline std::string
selector_text(const std::filesystem::path& root)
{
  const std::vector<std::uint8_t> bytes = read_bytes(root / "current");
  return std::string(bytes.begin(), bytes.end());
}

[[nodiscard]] inline std::size_t
generation_count(const std::filesystem::path& root)
{
  std::size_t count = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator(root / "generations"))
  {
    if (entry.is_directory())
      ++count;
  }
  return count;
}

[[nodiscard]] inline bool
has_temporary_entry(const std::filesystem::path& root)
{
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(root))
  {
    if (entry.path().filename().string().find(".tmp.") != std::string::npos)
      return true;
  }
  return false;
}

[[nodiscard]] inline std::vector<std::string>
layout_inventory(const std::filesystem::path& root)
{
  std::vector<std::string> result;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(root))
  {
    const auto relative = std::filesystem::relative(entry.path(), root).string();
    struct stat status {};
    if (::lstat(entry.path().c_str(), &status) == -1)
      throw std::runtime_error("could not inspect test layout " + relative);
    std::string line = relative + ":" + std::to_string(status.st_mode & 07777);
    if (S_ISREG(status.st_mode))
    {
      const auto bytes = read_bytes(entry.path());
      line.append(":");
      line.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    else if (S_ISLNK(status.st_mode))
    {
      line.append(":");
      line.append(std::filesystem::read_symlink(entry.path()).string());
    }
    result.push_back(std::move(line));
  }
  std::sort(result.begin(), result.end());
  return result;
}

} // namespace test_support
