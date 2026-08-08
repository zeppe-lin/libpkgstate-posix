// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

class temp_directory final {
public:
  temp_directory()
  {
    std::string pattern = "/tmp/libpkgstate-test.XXXXXX";
    std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
    mutable_pattern.push_back('\0');

    char* created = ::mkdtemp(mutable_pattern.data());
    if (created == nullptr)
    {
      std::perror("mkdtemp");
      std::exit(EXIT_FAILURE);
    }
    path_ = created;
  }

  temp_directory(const temp_directory&) = delete;
  temp_directory& operator=(const temp_directory&) = delete;

  ~temp_directory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept
  {
    return path_;
  }

private:
  std::filesystem::path path_;
};

inline void
write_text(const std::filesystem::path& path, const std::string& contents)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  if (!output)
  {
    std::cerr << "could not write test file " << path << '\n';
    std::exit(EXIT_FAILURE);
  }
}

[[nodiscard]] inline std::string
read_text(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
  {
    std::cerr << "could not read test file " << path << '\n';
    std::exit(EXIT_FAILURE);
  }

  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}
