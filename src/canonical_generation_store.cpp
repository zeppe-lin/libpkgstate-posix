// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgstate/canonical_generation_store.h>

#include "generation_codec.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <linux/fs.h>
#include <openssl/evp.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <libpkgstate/error.h>

namespace pkgstate {
namespace {

constexpr const char* binding_file = "binding";
constexpr const char* current_file = "current";
constexpr const char* generations_directory = "generations";
constexpr const char* snapshot_file = "snapshot";
constexpr std::uint64_t maximum_generation_size = 1024ULL * 1024ULL * 1024ULL;

class unique_fd final {
public:
  unique_fd() = default;
  explicit unique_fd(int value) noexcept
      : value_(value)
  {
  }

  unique_fd(const unique_fd&) = delete;
  unique_fd& operator=(const unique_fd&) = delete;

  unique_fd(unique_fd&& other) noexcept
      : value_(std::exchange(other.value_, -1))
  {
  }

  unique_fd& operator=(unique_fd&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      value_ = std::exchange(other.value_, -1);
    }
    return *this;
  }

  ~unique_fd()
  {
    reset();
  }

  [[nodiscard]] int get() const noexcept
  {
    return value_;
  }

  [[nodiscard]] explicit operator bool() const noexcept
  {
    return value_ != -1;
  }

  void reset(int value = -1) noexcept
  {
    if (value_ != -1)
      static_cast<void>(::close(value_));
    value_ = value;
  }

private:
  int value_ = -1;
};

class before_selection_failure final : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

[[nodiscard]] std::string
system_failure(std::string_view operation, int error_number)
{
  return std::string(operation) + ": " + std::strerror(error_number);
}

[[noreturn]] void
throw_store_failure(std::string_view operation)
{
  const int error_number = errno;
  throw store_error(system_failure(operation, error_number));
}

[[noreturn]] void
throw_before_selection(std::string_view operation)
{
  const int error_number = errno;
  throw before_selection_failure(system_failure(operation, error_number));
}

[[nodiscard]] unique_fd
open_directory(const std::filesystem::path& path)
{
  const int descriptor = ::open(
      path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (descriptor == -1)
    throw_store_failure("open canonical store directory");
  return unique_fd(descriptor);
}

[[nodiscard]] unique_fd
open_directory_at(int parent, const char* name, std::string_view label)
{
  const int descriptor = ::openat(parent, name,
                                  O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                      O_NOFOLLOW);
  if (descriptor == -1)
    throw_store_failure(label);
  return unique_fd(descriptor);
}

void
lock_directory(int descriptor, int operation, std::string_view label)
{
  for (;;)
  {
    if (::flock(descriptor, operation | LOCK_NB) == 0)
      return;
    if (errno != EINTR)
      throw_store_failure(label);
  }
}

[[nodiscard]] bool
synchronize(int descriptor) noexcept
{
  for (;;)
  {
    if (::fsync(descriptor) == 0)
      return true;
    if (errno != EINTR)
      return false;
  }
}

void
synchronize_directory(int descriptor, std::string_view label)
{
  if (!synchronize(descriptor))
    throw_store_failure(label);
}

void
synchronize_before_selection(int descriptor, std::string_view label)
{
  if (!synchronize(descriptor))
    throw_before_selection(label);
}

void
ensure_directory_at(int parent, const char* name)
{
  if (::mkdirat(parent, name, 0755) == -1 && errno != EEXIST)
    throw_store_failure("create canonical generations directory");

  unique_fd directory = open_directory_at(
      parent, name, "open canonical generations directory");
  struct stat status {};
  if (::fstat(directory.get(), &status) == -1)
    throw_store_failure("inspect canonical generations directory");
  if (!S_ISDIR(status.st_mode))
    throw store_error("canonical generations path is not a directory");
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>>
read_file_at(int directory,
             const char* name,
             std::uint64_t maximum_size,
             bool optional,
             std::string_view label)
{
  const int opened = ::openat(directory,
                              name,
                              O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (opened == -1)
  {
    if (optional && errno == ENOENT)
      return std::nullopt;
    throw_store_failure(label);
  }
  unique_fd file(opened);

  struct stat status {};
  if (::fstat(file.get(), &status) == -1)
    throw_store_failure(label);
  if (!S_ISREG(status.st_mode))
    throw store_error(std::string(label) + " is not a regular file");
  if (status.st_nlink != 1)
    throw store_error(std::string(label) + " has multiple hard links");
  if ((status.st_mode & 0222) != 0)
    throw store_error(std::string(label) + " is not immutable");
  if (status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) > maximum_size)
  {
    throw store_error(std::string(label) + " exceeds the supported size");
  }
  if (static_cast<std::uint64_t>(status.st_size) >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
  {
    throw store_error(std::string(label) + " size is not representable");
  }

  std::vector<std::uint8_t> result(
      static_cast<std::size_t>(status.st_size));
  std::size_t offset = 0;
  while (offset < result.size())
  {
    const ssize_t count = ::read(file.get(),
                                 result.data() + offset,
                                 result.size() - offset);
    if (count == -1)
    {
      if (errno == EINTR)
        continue;
      throw_store_failure(label);
    }
    if (count == 0)
      throw store_error(std::string(label) + " was truncated while reading");
    offset += static_cast<std::size_t>(count);
  }

  std::uint8_t extra = 0;
  for (;;)
  {
    const ssize_t count = ::read(file.get(), &extra, 1);
    if (count == -1 && errno == EINTR)
      continue;
    if (count == -1)
      throw_store_failure(label);
    if (count != 0)
      throw store_error(std::string(label) + " changed while reading");
    break;
  }

  return result;
}

void
write_all(int descriptor,
          const std::vector<std::uint8_t>& bytes,
          bool publication)
{
  std::size_t offset = 0;
  while (offset < bytes.size())
  {
    const ssize_t count = ::write(descriptor,
                                  bytes.data() + offset,
                                  bytes.size() - offset);
    if (count == -1)
    {
      if (errno == EINTR)
        continue;
      if (publication)
        throw_before_selection("write canonical generation data");
      throw_store_failure("write canonical store metadata");
    }
    if (count == 0)
    {
      if (publication)
        throw before_selection_failure(
            "write canonical generation data: zero-length write");
      throw store_error("write canonical store metadata: zero-length write");
    }
    offset += static_cast<std::size_t>(count);
  }
}

[[nodiscard]] std::string
unique_temporary_name(std::string_view prefix)
{
  static std::atomic<std::uint64_t> sequence{0};
  return std::string(prefix) + ".tmp." + std::to_string(::getpid()) + "." +
         std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
}

void
write_atomic_metadata(int directory,
                      const char* final_name,
                      const std::vector<std::uint8_t>& bytes)
{
  const std::string temporary = unique_temporary_name(final_name);
  const int opened = ::openat(directory,
                              temporary.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600);
  if (opened == -1)
    throw_store_failure("create canonical store metadata temporary");
  unique_fd file(opened);

  try
  {
    write_all(file.get(), bytes, false);
    if (::fchmod(file.get(), 0444) == -1)
      throw_store_failure("set canonical store metadata mode");
    if (!synchronize(file.get()))
      throw_store_failure("synchronize canonical store metadata");
    file.reset();

    if (::renameat(directory,
                   temporary.c_str(),
                   directory,
                   final_name) == -1)
    {
      throw_store_failure("select canonical store metadata");
    }
    synchronize_directory(directory, "synchronize canonical store directory");
  }
  catch (...)
  {
    static_cast<void>(::unlinkat(directory, temporary.c_str(), 0));
    throw;
  }
}

[[nodiscard]] std::string_view
as_string_view(const std::vector<std::uint8_t>& bytes)
{
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

[[nodiscard]] std::optional<state_target_binding>
read_binding_locked(int root)
{
  const auto existing = read_file_at(root,
                                     binding_file,
                                     maximum_generation_size,
                                     true,
                                     "canonical store binding");
  if (!existing)
    return std::nullopt;
  return detail::decode_generation_binding(as_string_view(*existing));
}

void
require_binding_locked(int root, const state_target_binding& expected)
{
  const auto existing = read_binding_locked(root);
  if (!existing)
    throw store_error("canonical store binding is missing");
  if (*existing != expected)
    throw store_error("canonical store target binding does not match caller");
}

[[nodiscard]] std::string
generation_name(const installed_state_snapshot_identity& identity)
{
  const std::string text = identity.string();
  constexpr std::string_view prefix = "v1:sha256:";
  if (text.size() != prefix.size() + 64 ||
      text.compare(0, prefix.size(), prefix) != 0)
  {
    throw store_error("unsupported snapshot identity for generation name");
  }
  return "v1-sha256-" + text.substr(prefix.size());
}

[[nodiscard]] installed_state_snapshot_identity
parse_selector(const std::vector<std::uint8_t>& bytes)
{
  if (bytes.empty() || bytes.back() != '\n')
    throw store_error("canonical current selector is not newline terminated");
  if (std::count(bytes.begin(), bytes.end(), static_cast<std::uint8_t>('\n')) !=
      1)
  {
    throw store_error("canonical current selector contains extra lines");
  }

  const std::string value(
      reinterpret_cast<const char*>(bytes.data()), bytes.size() - 1);
  try
  {
    return installed_state_snapshot_identity::parse(value);
  }
  catch (const error& failure)
  {
    throw store_error(std::string("invalid canonical current selector: ") +
                      failure.what());
  }
}

[[nodiscard]] snapshot
read_generation_at(int generations,
                   const installed_state_snapshot_identity& selected,
                   const state_target_binding& expected_binding)
{
  const std::string name = generation_name(selected);
  unique_fd generation = open_directory_at(
      generations, name.c_str(), "open selected canonical generation");
  struct stat generation_status {};
  if (::fstat(generation.get(), &generation_status) == -1)
    throw_store_failure("inspect selected canonical generation");
  if ((generation_status.st_mode & 0222) != 0)
    throw store_error("selected canonical generation is not immutable");
  const auto bytes = read_file_at(generation.get(),
                                  snapshot_file,
                                  maximum_generation_size,
                                  false,
                                  "canonical generation snapshot");
  snapshot result = detail::decode_generation_snapshot(as_string_view(*bytes));
  if (result.target_binding() != expected_binding)
    throw store_error("selected generation target binding is inconsistent");
  if (result.identity() != selected)
    throw store_error("selected generation identity does not match selector");
  return result;
}

[[nodiscard]] snapshot
read_selected_snapshot_locked(int root,
                              const state_target_binding& binding)
{
  unique_fd generations = open_directory_at(
      root, generations_directory, "open canonical generations directory");
  const auto selector = read_file_at(root,
                                     current_file,
                                     256,
                                     false,
                                     "canonical current selector");
  const installed_state_snapshot_identity selected = parse_selector(*selector);
  return read_generation_at(generations.get(), selected, binding);
}

[[nodiscard]] snapshot
read_snapshot_locked(int root, const state_target_binding& binding)
{
  require_binding_locked(root, binding);
  return read_selected_snapshot_locked(root, binding);
}

[[nodiscard]] sha256_digest_bytes
sha256_bytes(const std::vector<std::uint8_t>& bytes)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!context)
    throw state_error("could not allocate generation evidence digest context");

  if (EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context.get(), bytes.data(), bytes.size()) != 1)
  {
    throw state_error("could not compute generation evidence digest");
  }

  sha256_digest_bytes result{};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), result.data(), &size) != 1 ||
      size != result.size())
  {
    throw state_error("could not finalize generation evidence digest");
  }
  return result;
}

void
write_publication_file(int directory,
                       const char* name,
                       const std::vector<std::uint8_t>& bytes)
{
  const int opened = ::openat(directory,
                              name,
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600);
  if (opened == -1)
    throw_before_selection("create canonical generation file");
  unique_fd file(opened);

  try
  {
    write_all(file.get(), bytes, true);
    if (::fchmod(file.get(), 0444) == -1)
      throw_before_selection("set canonical generation file mode");
    synchronize_before_selection(
        file.get(), "synchronize canonical generation file");
  }
  catch (...)
  {
    file.reset();
    static_cast<void>(::unlinkat(directory, name, 0));
    throw;
  }
}

[[nodiscard]] int
rename_noreplace(int old_directory,
                 const char* old_name,
                 int new_directory,
                 const char* new_name)
{
  return static_cast<int>(::syscall(SYS_renameat2,
                                    old_directory,
                                    old_name,
                                    new_directory,
                                    new_name,
                                    RENAME_NOREPLACE));
}

void
remove_temporary_generation(int generations,
                            const std::string& name) noexcept
{
  const int opened = ::openat(generations,
                              name.c_str(),
                              O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (opened != -1)
  {
    unique_fd directory(opened);
    static_cast<void>(::fchmod(directory.get(), 0700));
    static_cast<void>(::unlinkat(directory.get(), snapshot_file, 0));
  }
  static_cast<void>(::unlinkat(generations, name.c_str(), AT_REMOVEDIR));
}

void
validate_existing_generation(int generations,
                             const std::string& name,
                             const snapshot& resulting,
                             const std::vector<std::uint8_t>& encoded)
{
  unique_fd generation = open_directory_at(
      generations, name.c_str(), "open existing canonical generation");
  struct stat generation_status {};
  if (::fstat(generation.get(), &generation_status) == -1)
    throw_store_failure("inspect existing canonical generation");
  if ((generation_status.st_mode & 0222) != 0)
    throw store_error("existing canonical generation is not immutable");
  const auto existing = read_file_at(generation.get(),
                                     snapshot_file,
                                     maximum_generation_size,
                                     false,
                                     "existing canonical generation snapshot");
  if (*existing != encoded)
    throw store_error("canonical generation identity collision or corruption");

  snapshot decoded =
      detail::decode_generation_snapshot(as_string_view(*existing));
  if (decoded.identity() != resulting.identity())
    throw store_error("existing canonical generation is semantically corrupt");
}

void
ensure_generation(int generations,
                  const snapshot& resulting,
                  const std::vector<std::uint8_t>& encoded)
{
  const std::string final_name = generation_name(resulting.identity());
  const int existing = ::openat(generations,
                                final_name.c_str(),
                                O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                    O_NOFOLLOW);
  if (existing != -1)
  {
    unique_fd ignored(existing);
    validate_existing_generation(generations,
                                 final_name,
                                 resulting,
                                 encoded);
    synchronize_before_selection(
        generations, "synchronize existing canonical generation");
    return;
  }
  if (errno != ENOENT)
    throw_store_failure("inspect canonical generation destination");

  const std::string temporary = unique_temporary_name("generation");
  if (::mkdirat(generations, temporary.c_str(), 0700) == -1)
    throw_before_selection("create canonical generation temporary");

  try
  {
    const int opened = ::openat(generations,
                                temporary.c_str(),
                                O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                    O_NOFOLLOW);
    if (opened == -1)
      throw_before_selection("open canonical generation temporary");
    unique_fd temporary_directory(opened);
    write_publication_file(temporary_directory.get(), snapshot_file, encoded);
    synchronize_before_selection(
        temporary_directory.get(),
        "synchronize canonical generation directory");
    if (::fchmod(temporary_directory.get(), 0555) == -1)
      throw_before_selection("set canonical generation directory mode");
    synchronize_before_selection(
        temporary_directory.get(), "synchronize canonical generation mode");
    temporary_directory.reset();

    if (rename_noreplace(generations,
                         temporary.c_str(),
                         generations,
                         final_name.c_str()) == -1)
    {
      if (errno != EEXIST)
        throw_before_selection("install immutable canonical generation");
      remove_temporary_generation(generations, temporary);
      validate_existing_generation(generations,
                                   final_name,
                                   resulting,
                                   encoded);
      synchronize_before_selection(
          generations, "synchronize existing canonical generation");
      return;
    }

    synchronize_before_selection(
        generations, "synchronize canonical generations domain");
  }
  catch (...)
  {
    remove_temporary_generation(generations, temporary);
    throw;
  }
}

void
write_current_temporary(int root,
                        const std::string& temporary,
                        const installed_state_snapshot_identity& selected)
{
  const std::string text = selected.string() + "\n";
  const std::vector<std::uint8_t> bytes(text.begin(), text.end());
  const int opened = ::openat(root,
                              temporary.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600);
  if (opened == -1)
    throw_before_selection("create canonical current selector temporary");
  unique_fd file(opened);

  try
  {
    write_all(file.get(), bytes, true);
    if (::fchmod(file.get(), 0444) == -1)
      throw_before_selection("set canonical current selector mode");
    synchronize_before_selection(
        file.get(), "synchronize canonical current selector");
  }
  catch (...)
  {
    file.reset();
    static_cast<void>(::unlinkat(root, temporary.c_str(), 0));
    throw;
  }
}

class generation_publication_transaction final
    : public canonical_publication_transaction {
public:
  generation_publication_transaction(std::filesystem::path root,
                                     state_target_binding binding)
      : root_path_(std::move(root)),
        binding_(std::move(binding)),
        root_(open_directory(root_path_)),
        current_(lock_and_read())
  {
  }

  [[nodiscard]] const snapshot& current() const noexcept override
  {
    return current_;
  }

  [[nodiscard]] const std::string& storage_format() const noexcept override
  {
    return storage_format_;
  }

  [[nodiscard]] state_publication_backend_result
  publish(const snapshot& resulting_snapshot) override
  {
    if (resulting_snapshot.target_binding() != binding_)
      throw store_error("canonical generation publication target mismatch");

    const std::vector<std::uint8_t> encoded =
        detail::encode_generation_snapshot(resulting_snapshot);
    const auto evidence = state_publication_evidence_identity::from_sha256(
        sha256_bytes(encoded));
    const std::vector<state_publication_evidence_identity> evidence_set = {
        evidence,
    };

    unique_fd generations = open_directory_at(
        root_.get(),
        generations_directory,
        "open canonical generations directory for publication");

    try
    {
      ensure_generation(generations.get(), resulting_snapshot, encoded);
      const std::string temporary = unique_temporary_name(current_file);
      write_current_temporary(
          root_.get(), temporary, resulting_snapshot.identity());

      if (::renameat(root_.get(),
                     temporary.c_str(),
                     root_.get(),
                     current_file) == -1)
      {
        static_cast<void>(::unlinkat(root_.get(), temporary.c_str(), 0));
        throw_before_selection("select canonical current generation");
      }

      const bool durability_confirmed = synchronize(root_.get());

      try
      {
        const snapshot observed = read_snapshot_locked(root_.get(), binding_);
        if (observed.identity() != resulting_snapshot.identity())
        {
          return state_publication_backend_result::indeterminate(
              state_storage_atomicity_boundary::immutable_generation_selection,
              false,
              evidence_set);
        }
      }
      catch (const error&)
      {
        return state_publication_backend_result::indeterminate(
            state_storage_atomicity_boundary::immutable_generation_selection,
            false,
            evidence_set);
      }

      if (durability_confirmed)
      {
        return state_publication_backend_result::published(
            state_storage_atomicity_boundary::immutable_generation_selection,
            evidence_set);
      }

      return state_publication_backend_result::
          published_but_durability_unconfirmed(
              state_storage_atomicity_boundary::immutable_generation_selection,
              evidence_set);
    }
    catch (const before_selection_failure&)
    {
      return state_publication_backend_result::failed_before_publication(
          evidence_set);
    }
  }

private:
  [[nodiscard]] snapshot lock_and_read()
  {
    lock_directory(root_.get(),
                   LOCK_EX,
                   "acquire canonical publication lock");
    return read_snapshot_locked(root_.get(), binding_);
  }

  std::filesystem::path root_path_;
  state_target_binding binding_;
  unique_fd root_;
  snapshot current_;
  const std::string storage_format_ =
      std::string(canonical_generation_storage_format);
};

} // namespace

canonical_generation_store::canonical_generation_store(
    std::filesystem::path root,
    state_target_binding target_binding)
    : root_(std::move(root)), target_binding_(std::move(target_binding))
{
  initialize();
}

canonical_generation_store
canonical_generation_store::open_existing(
    std::filesystem::path root,
    state_target_binding target_binding)
{
  return canonical_generation_store(
      std::move(root),
      std::move(target_binding),
      existing_store_tag{});
}

canonical_generation_store::canonical_generation_store(
    std::filesystem::path root,
    state_target_binding target_binding,
    existing_store_tag)
    : root_(std::move(root)), target_binding_(std::move(target_binding))
{
  validate_existing();
}

const std::filesystem::path&
canonical_generation_store::root_path() const noexcept
{
  return root_;
}

const state_target_binding&
canonical_generation_store::target_binding() const noexcept
{
  return target_binding_;
}

snapshot
canonical_generation_store::read() const
{
  unique_fd root = open_directory(root_);
  lock_directory(root.get(), LOCK_SH, "acquire canonical store read lock");
  return read_snapshot_locked(root.get(), target_binding_);
}

std::unique_ptr<canonical_publication_transaction>
canonical_generation_store::begin_publication() const
{
  return std::make_unique<generation_publication_transaction>(
      root_, target_binding_);
}

void
canonical_generation_store::validate_existing() const
{
  if (root_.empty())
    throw store_error("canonical generation store path is empty");

  unique_fd root = open_directory(root_);
  lock_directory(root.get(),
                 LOCK_SH,
                 "acquire canonical existing-store validation lock");
  static_cast<void>(read_snapshot_locked(root.get(), target_binding_));
}

void
canonical_generation_store::initialize()
{
  if (root_.empty())
    throw store_error("canonical generation store path is empty");

  std::error_code failure;
  std::filesystem::create_directories(root_, failure);
  if (failure)
  {
    throw store_error("create canonical generation store directory: " +
                      failure.message());
  }

  unique_fd root = open_directory(root_);
  lock_directory(root.get(),
                 LOCK_EX,
                 "acquire canonical store initialization lock");
  ensure_directory_at(root.get(), generations_directory);

  const auto stored_binding = read_binding_locked(root.get());
  if (stored_binding)
  {
    if (*stored_binding != target_binding_)
    {
      throw store_error(
          "canonical store target binding does not match caller");
    }
    static_cast<void>(
        read_selected_snapshot_locked(root.get(), target_binding_));
    synchronize_directory(root.get(), "synchronize canonical store layout");
    return;
  }

  const snapshot empty = snapshot::make(target_binding_, {});
  const auto selector = read_file_at(root.get(),
                                     current_file,
                                     256,
                                     true,
                                     "canonical current selector");
  if (!selector)
  {
    const std::vector<std::uint8_t> encoded =
        detail::encode_generation_snapshot(empty);
    unique_fd generations = open_directory_at(
        root.get(),
        generations_directory,
        "open canonical generations directory for initialization");

    try
    {
      ensure_generation(generations.get(), empty, encoded);
      const std::string temporary = unique_temporary_name(current_file);
      write_current_temporary(root.get(), temporary, empty.identity());
      if (::renameat(root.get(),
                     temporary.c_str(),
                     root.get(),
                     current_file) == -1)
      {
        static_cast<void>(::unlinkat(root.get(), temporary.c_str(), 0));
        throw_store_failure("select initial canonical generation");
      }
      if (!synchronize(root.get()))
      {
        throw store_error(
            "initial canonical generation selected but durability "
            "confirmation failed");
      }
    }
    catch (const before_selection_failure& initialization_failure)
    {
      throw store_error(
          std::string("initialize canonical generation store: ") +
          initialization_failure.what());
    }
  }
  else
  {
    const snapshot selected =
        read_selected_snapshot_locked(root.get(), target_binding_);
    if (selected.identity() != empty.identity() || selected.size() != 0)
    {
      throw store_error(
          "unbound canonical store does not select the empty generation");
    }
  }

  write_atomic_metadata(
      root.get(),
      binding_file,
      detail::encode_generation_binding(target_binding_));
  static_cast<void>(read_snapshot_locked(root.get(), target_binding_));
  synchronize_directory(root.get(), "synchronize canonical store layout");
}


} // namespace pkgstate
