// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "generation_codec.h"

#include <libpkgstate/canonical_generation_store.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <libpkgstate/error.h>
#include <libpkgstate/installation_receipt.h>
#include <libpkgstate/installed_package.h>
#include <libpkgstate/model.h>
#include <libpkgstate/owned_entry.h>
#include <libpkgstate/package_path.h>
#include <libpkgstate/package_source_record.h>

namespace pkgstate::detail {
namespace {

constexpr std::array<std::uint8_t, 17> binding_magic = {
    'p', 'k', 'g', 's', 't', 'a', 't', 'e', '-',
    'b', 'i', 'n', 'd', 'i', 'n', 'g', 0,
};
constexpr std::array<std::uint8_t, 18> snapshot_magic = {
    'p', 'k', 'g', 's', 't', 'a', 't', 'e', '-',
    's', 'n', 'a', 'p', 's', 'h', 'o', 't', 0,
};

class writer final {
public:
  template<std::size_t Size>
  void raw(const std::array<std::uint8_t, Size>& value)
  {
    bytes_.insert(bytes_.end(), value.begin(), value.end());
  }
  void u8(std::uint8_t value) { bytes_.push_back(value); }
  void u16(std::uint16_t value)
  {
    bytes_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes_.push_back(static_cast<std::uint8_t>(value & 0xffU));
  }
  void u32(std::uint32_t value)
  {
    for (int shift = 24; shift >= 0; shift -= 8)
      bytes_.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
  }
  void u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      bytes_.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
  }
  void i64(std::int64_t value)
  {
    const bool negative = value < 0;
    const std::uint64_t magnitude = negative
        ? static_cast<std::uint64_t>(-(value + 1)) + 1U
        : static_cast<std::uint64_t>(value);
    boolean(negative);
    u64(magnitude);
  }
  void boolean(bool value) { u8(value ? 1 : 0); }
  void bytes(std::string_view value)
  {
    u64(value.size());
    if (!value.empty())
    {
      const auto* begin = reinterpret_cast<const std::uint8_t*>(value.data());
      bytes_.insert(bytes_.end(), begin, begin + value.size());
    }
  }
  template<typename Identity>
  void digest(const Identity& identity) { bytes(identity.string()); }
  [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(bytes_); }
private:
  std::vector<std::uint8_t> bytes_;
};

class reader final {
public:
  explicit reader(std::string_view bytes) : bytes_(bytes) {}
  template<std::size_t Size>
  void expect(const std::array<std::uint8_t, Size>& expected, const char* label)
  {
    require(Size, label);
    const auto* actual = reinterpret_cast<const std::uint8_t*>(bytes_.data() + position_);
    if (!std::equal(expected.begin(), expected.end(), actual))
      fail(std::string("invalid ") + label);
    position_ += Size;
  }
  [[nodiscard]] std::uint8_t u8(const char* label)
  {
    require(1, label);
    return static_cast<std::uint8_t>(bytes_[position_++]);
  }
  [[nodiscard]] std::uint16_t u16(const char* label)
  {
    require(2, label);
    const std::uint16_t result =
        (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes_[position_])) << 8U) |
        static_cast<std::uint8_t>(bytes_[position_ + 1]);
    position_ += 2;
    return result;
  }
  [[nodiscard]] std::uint32_t u32(const char* label)
  {
    require(4, label);
    std::uint32_t result = 0;
    for (std::size_t index = 0; index < 4; ++index)
      result = (result << 8U) | static_cast<std::uint8_t>(bytes_[position_ + index]);
    position_ += 4;
    return result;
  }
  [[nodiscard]] std::uint64_t u64(const char* label)
  {
    require(8, label);
    std::uint64_t result = 0;
    for (std::size_t index = 0; index < 8; ++index)
      result = (result << 8U) | static_cast<std::uint8_t>(bytes_[position_ + index]);
    position_ += 8;
    return result;
  }
  [[nodiscard]] std::int64_t i64(const char* label)
  {
    const bool negative = boolean(label);
    const std::uint64_t magnitude = u64(label);
    const std::uint64_t limit =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) +
        (negative ? 1U : 0U);
    if (magnitude > limit)
      fail(std::string("invalid ") + label);
    if (!negative)
      return static_cast<std::int64_t>(magnitude);
    if (magnitude == limit)
      return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
  }
  [[nodiscard]] bool boolean(const char* label)
  {
    const std::uint8_t value = u8(label);
    if (value > 1)
      fail(std::string("invalid ") + label);
    return value == 1;
  }
  [[nodiscard]] std::string bytes(const char* label)
  {
    const std::uint64_t size64 = u64(label);
    if (size64 > remaining() || size64 > std::numeric_limits<std::size_t>::max())
      fail(std::string(label) + " length exceeds record");
    const std::size_t size = static_cast<std::size_t>(size64);
    std::string result(bytes_.substr(position_, size));
    position_ += size;
    return result;
  }
  [[nodiscard]] std::size_t count(const char* label)
  {
    const std::uint64_t value = u64(label);
    if (value > remaining() || value > std::numeric_limits<std::size_t>::max())
      fail(std::string(label) + " is not plausible");
    return static_cast<std::size_t>(value);
  }
  void finish()
  {
    if (position_ != bytes_.size())
      fail("trailing bytes in canonical generation record");
  }
private:
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }
  void require(std::size_t size, const char* label)
  {
    if (size > remaining())
      fail(std::string("truncated ") + label);
  }
  [[noreturn]] static void fail(const std::string& message) { throw store_error(message); }
  std::string_view bytes_;
  std::size_t position_ = 0;
};

template<typename Identity>
Identity read_digest(reader& input, const char* label)
{
  return Identity::parse(input.bytes(label));
}

void encode_binding_fields(writer& output, const state_target_binding& binding)
{
  output.digest(binding.managed_target());
  output.digest(binding.state_store());
  output.digest(binding.root_view());
  output.digest(binding.state_backend());
  output.digest(binding.publication_domain());
}

state_target_binding decode_binding_fields(reader& input)
{
  managed_target_identity managed =
      read_digest<managed_target_identity>(input, "managed-target identity");
  state_store_identity store =
      read_digest<state_store_identity>(input, "state-store identity");
  root_view_identity root =
      read_digest<root_view_identity>(input, "root-view identity");
  state_backend_identity backend =
      read_digest<state_backend_identity>(input, "state-backend identity");
  publication_domain_identity publication =
      read_digest<publication_domain_identity>(input, "publication-domain identity");
  return state_target_binding::make(
      std::move(managed), std::move(store), std::move(root),
      std::move(backend), std::move(publication));
}

void encode_provenance(writer& output, const declaration_provenance& value)
{
  output.bytes(value.document());
  output.bytes(value.path());
  output.u32(value.line());
  output.u32(value.column());
}

declaration_provenance decode_provenance(reader& input)
{
  std::string document = input.bytes("provenance document");
  std::string path = input.bytes("provenance path");
  const std::uint32_t line = input.u32("provenance line");
  const std::uint32_t column = input.u32("provenance column");
  return declaration_provenance(
      std::move(document), std::move(path), line, column);
}

void encode_requirement(writer& output, const package_requirement& value)
{
  output.bytes(value.package().name());
  output.u64(value.origins().size());
  for (const requirement_origin& origin : value.origins())
  {
    encode_provenance(output, origin.declaration());
    output.u64(origin.expansion().size());
    for (const profile_expansion_step& step : origin.expansion())
    {
      output.bytes(step.profile().name());
      output.u8(static_cast<std::uint8_t>(step.member_kind()));
      output.bytes(step.member());
      encode_provenance(output, step.provenance());
    }
  }
}

package_requirement decode_requirement(reader& input)
{
  package_reference package(input.bytes("requirement package"));
  std::vector<requirement_origin> origins;
  const std::size_t count = input.count("requirement origin count");
  origins.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
  {
    declaration_provenance declaration = decode_provenance(input);
    std::vector<profile_expansion_step> expansion;
    const std::size_t steps = input.count("profile expansion count");
    expansion.reserve(steps);
    for (std::size_t step = 0; step < steps; ++step)
    {
      profile_reference profile(input.bytes("expansion profile"));
      const requirement_member_kind member_kind =
          static_cast<requirement_member_kind>(
              input.u8("expansion member kind"));
      std::string member = input.bytes("expansion member");
      declaration_provenance provenance = decode_provenance(input);
      expansion.emplace_back(
          std::move(profile), member_kind, std::move(member),
          std::move(provenance));
    }
    origins.emplace_back(std::move(declaration), std::move(expansion));
  }
  return package_requirement(std::move(package), std::move(origins));
}

void encode_source(writer& output, const package_source_record& source)
{
  const package_release& release = source.release();
  output.digest(release.identity());
  output.bytes(release.name());
  output.bytes(release.version());
  output.u32(release.release());

  const package_metadata& metadata = source.metadata();
  output.bytes(metadata.summary());
  output.boolean(metadata.description().has_value());
  if (metadata.description()) output.bytes(*metadata.description());
  output.boolean(metadata.homepage().has_value());
  if (metadata.homepage()) output.bytes(*metadata.homepage());
  output.u64(metadata.licenses().size());
  for (const std::string& license : metadata.licenses()) output.bytes(license);

  output.u64(source.runtime_requirements().size());
  for (const package_requirement& requirement : source.runtime_requirements())
    encode_requirement(output, requirement);

  output.u64(source.lifecycle_programs().size());
  for (const lifecycle_program& value : source.lifecycle_programs())
  {
    output.u8(static_cast<std::uint8_t>(value.action()));
    output.u8(static_cast<std::uint8_t>(value.value().language()));
    output.bytes(value.value().material());
  }

  output.u64(source.lifecycle_requirements().size());
  for (const lifecycle_requirement& value : source.lifecycle_requirements())
  {
    output.u8(static_cast<std::uint8_t>(value.action()));
    encode_requirement(output, value.requirement());
  }

  const architecture_binding& architectures = source.architectures();
  output.u64(architectures.declared_build().size());
  for (const architecture_reference& value : architectures.declared_build()) output.bytes(value.name());
  output.u64(architectures.declared_target().size());
  for (const architecture_reference& value : architectures.declared_target()) output.bytes(value.name());
  output.bytes(architectures.selected_build().name());
  output.bytes(architectures.selected_target().name());

  output.u64(source.selected_profiles().size());
  for (const selected_profile& value : source.selected_profiles())
  {
    output.bytes(value.profile().name());
    output.digest(value.identity());
    output.u64(value.declarations().size());
    for (const declaration_provenance& declaration : value.declarations())
      encode_provenance(output, declaration);
  }
  output.digest(source.recipe());
  output.digest(source.snapshot());
}

package_source_record decode_source(reader& input)
{
  package_release_identity release_identity =
      read_digest<package_release_identity>(input, "source release identity");
  package_reference package(input.bytes("package name"));
  std::string version = input.bytes("package version");
  const std::uint32_t release_number = input.u32("package release");
  package_release release(
      std::move(release_identity), std::move(package),
      std::move(version), release_number);

  const std::string summary = input.bytes("package summary");
  std::optional<std::string> description;
  if (input.boolean("description presence")) description = input.bytes("package description");
  std::optional<std::string> homepage;
  if (input.boolean("homepage presence")) homepage = input.bytes("package homepage");
  std::vector<std::string> licenses;
  const std::size_t license_count = input.count("license count");
  licenses.reserve(license_count);
  for (std::size_t index = 0; index < license_count; ++index) licenses.push_back(input.bytes("license"));
  package_metadata metadata(summary, std::move(description), std::move(homepage), std::move(licenses));

  std::vector<package_requirement> runtime;
  const std::size_t runtime_count = input.count("runtime requirement count");
  runtime.reserve(runtime_count);
  for (std::size_t index = 0; index < runtime_count; ++index) runtime.push_back(decode_requirement(input));

  std::vector<lifecycle_program> programs;
  const std::size_t program_count = input.count("lifecycle program count");
  programs.reserve(program_count);
  for (std::size_t index = 0; index < program_count; ++index)
  {
    const lifecycle_action action =
        static_cast<lifecycle_action>(input.u8("lifecycle action"));
    const program_language language =
        static_cast<program_language>(input.u8("program language"));
    std::string material = input.bytes("program material");
    programs.emplace_back(
        action, program(language, std::move(material)));
  }

  std::vector<lifecycle_requirement> lifecycle_requirements;
  const std::size_t lifecycle_count = input.count("lifecycle requirement count");
  lifecycle_requirements.reserve(lifecycle_count);
  for (std::size_t index = 0; index < lifecycle_count; ++index)
  {
    const lifecycle_action action = static_cast<lifecycle_action>(
        input.u8("lifecycle requirement action"));
    package_requirement requirement = decode_requirement(input);
    lifecycle_requirements.emplace_back(action, std::move(requirement));
  }

  std::vector<architecture_reference> declared_build;
  const std::size_t build_count = input.count("declared build architecture count");
  declared_build.reserve(build_count);
  for (std::size_t index = 0; index < build_count; ++index) declared_build.emplace_back(input.bytes("declared build architecture"));
  std::vector<architecture_reference> declared_target;
  const std::size_t target_count = input.count("declared target architecture count");
  declared_target.reserve(target_count);
  for (std::size_t index = 0; index < target_count; ++index) declared_target.emplace_back(input.bytes("declared target architecture"));
  architecture_reference selected_build(
      input.bytes("selected build architecture"));
  architecture_reference selected_target(
      input.bytes("selected target architecture"));
  architecture_binding architectures = architecture_binding::make(
      std::move(declared_build), std::move(declared_target),
      std::move(selected_build), std::move(selected_target));

  std::vector<selected_profile> profiles;
  const std::size_t profile_count = input.count("selected profile count");
  profiles.reserve(profile_count);
  for (std::size_t index = 0; index < profile_count; ++index)
  {
    profile_reference profile(input.bytes("selected profile"));
    source_profile_identity identity = read_digest<source_profile_identity>(input, "selected profile identity");
    std::vector<declaration_provenance> declarations;
    const std::size_t declaration_count = input.count("selected profile declaration count");
    declarations.reserve(declaration_count);
    for (std::size_t declaration = 0; declaration < declaration_count; ++declaration)
      declarations.push_back(decode_provenance(input));
    profiles.emplace_back(std::move(profile), std::move(identity), std::move(declarations));
  }

  source_recipe_identity recipe =
      read_digest<source_recipe_identity>(input, "source recipe identity");
  source_snapshot_identity snapshot =
      read_digest<source_snapshot_identity>(input, "source snapshot identity");
  return package_source_record::make(
      std::move(release), std::move(metadata), std::move(runtime),
      std::move(programs), std::move(lifecycle_requirements),
      std::move(architectures), std::move(profiles),
      std::move(recipe), std::move(snapshot));
}

void encode_control(writer& output, const installed_control& control)
{
  encode_source(output, control.source());
  const installation_reason& reason = control.reason();
  output.u8(static_cast<std::uint8_t>(reason.kind()));
  output.boolean(reason.issuer_package().has_value());
  if (reason.issuer_package()) output.bytes(reason.issuer_package()->name());
  output.boolean(reason.issuer_profile().has_value());
  if (reason.issuer_profile())
  {
    output.bytes(reason.issuer_profile()->name());
    output.digest(*reason.issuer_profile_identity());
  }
  output.boolean(reason.policy().has_value());
  if (reason.policy()) output.bytes(*reason.policy());

  const build_provenance& build = control.build();
  output.digest(build.source_record());
  output.digest(build.request());
  output.digest(build.source_materials());
  output.digest(build.build_inputs());
  output.digest(build.environment_policy());
  output.digest(build.build_policy());
  output.digest(build.build_result());
  output.digest(build.payload_manifest());
  output.digest(build.artifact());
  output.digest(build.artifact_content());
  output.digest(build.artifact_binding());
  output.digest(build.execution_evidence());
  output.digest(build.artifact_image());
  output.digest(build.artifact_inspection());
}

installed_control decode_control(reader& input)
{
  package_source_record source = decode_source(input);
  const installation_reason_kind kind =
      static_cast<installation_reason_kind>(input.u8("installation reason"));
  std::optional<package_reference> package;
  if (input.boolean("reason package presence")) package.emplace(input.bytes("reason package"));
  std::optional<profile_reference> profile;
  std::optional<source_profile_identity> profile_identity;
  if (input.boolean("reason profile presence"))
  {
    profile.emplace(input.bytes("reason profile"));
    profile_identity = read_digest<source_profile_identity>(input, "reason profile identity");
  }
  std::optional<std::string> policy;
  if (input.boolean("reason policy presence")) policy = input.bytes("reason policy");

  installation_reason reason = installation_reason::explicit_request();
  switch (kind)
  {
    case installation_reason_kind::explicit_request:
      if (package || profile || policy) throw store_error("invalid explicit installation reason");
      break;
    case installation_reason_kind::runtime_dependency:
      if (!package || profile || policy) throw store_error("invalid dependency installation reason");
      reason = installation_reason::runtime_dependency(std::move(*package));
      break;
    case installation_reason_kind::profile_membership:
      if (package || !profile || !profile_identity || policy) throw store_error("invalid profile installation reason");
      reason = installation_reason::profile_membership(std::move(*profile), std::move(*profile_identity));
      break;
    case installation_reason_kind::system_policy:
      if (package || profile || !policy) throw store_error("invalid policy installation reason");
      reason = installation_reason::system_policy(std::move(*policy));
      break;
    default:
      throw store_error("invalid installation reason value");
  }

  package_source_record_identity source_record =
      read_digest<package_source_record_identity>(input, "build source record identity");
  build_request_identity request =
      read_digest<build_request_identity>(input, "build request identity");
  source_material_set_identity source_materials =
      read_digest<source_material_set_identity>(input, "source material set identity");
  build_input_set_identity build_inputs =
      read_digest<build_input_set_identity>(input, "build input set identity");
  environment_policy_identity environment_policy =
      read_digest<environment_policy_identity>(input, "environment policy identity");
  build_policy_identity build_policy =
      read_digest<build_policy_identity>(input, "build policy identity");
  build_result_identity build_result =
      read_digest<build_result_identity>(input, "build result identity");
  payload_manifest_identity payload_manifest =
      read_digest<payload_manifest_identity>(input, "payload manifest identity");
  build_artifact_identity artifact =
      read_digest<build_artifact_identity>(input, "build artifact identity");
  artifact_content_identity artifact_content =
      read_digest<artifact_content_identity>(input, "artifact content identity");
  artifact_binding_identity artifact_binding =
      read_digest<artifact_binding_identity>(input, "artifact binding identity");
  execution_evidence_identity execution_evidence =
      read_digest<execution_evidence_identity>(input, "execution evidence identity");
  artifact_image_identity artifact_image =
      read_digest<artifact_image_identity>(input, "artifact image identity");
  artifact_inspection_identity artifact_inspection =
      read_digest<artifact_inspection_identity>(input, "artifact inspection identity");
  build_provenance build(
      std::move(source_record), std::move(request), std::move(source_materials),
      std::move(build_inputs), std::move(environment_policy),
      std::move(build_policy), std::move(build_result),
      std::move(payload_manifest), std::move(artifact),
      std::move(artifact_content), std::move(artifact_binding), std::move(execution_evidence),
      std::move(artifact_image), std::move(artifact_inspection));
  return installed_control::make(std::move(source), std::move(reason), std::move(build));
}

void encode_entry(writer& output, const owned_entry& entry)
{
  output.bytes(entry.path().string());
  const installed_object_metadata& object = entry.object();
  output.u8(static_cast<std::uint8_t>(object.kind()));
  output.u32(object.mode());
  output.u64(object.uid());
  output.u64(object.gid());
  output.i64(object.mtime().seconds());
  output.u32(object.mtime().nanoseconds());
  output.boolean(object.size().has_value());
  if (object.size()) output.u64(*object.size());
  output.boolean(object.regular_content().has_value());
  if (object.regular_content()) output.digest(*object.regular_content());
  output.boolean(object.symlink_target().has_value());
  if (object.symlink_target()) output.bytes(*object.symlink_target());
  output.boolean(object.device().has_value());
  if (object.device())
  {
    output.u64(object.device()->major());
    output.u64(object.device()->minor());
  }
  output.boolean(object.hardlink_anchor().has_value());
  if (object.hardlink_anchor()) output.bytes(object.hardlink_anchor()->string());
  output.u8(static_cast<std::uint8_t>(entry.origin()));
  output.boolean(entry.rejected().has_value());
  if (entry.rejected())
  {
    output.u8(static_cast<std::uint8_t>(entry.rejected()->side()));
    output.digest(entry.rejected()->identity());
  }
}

owned_entry decode_entry(reader& input)
{
  package_path path = package_path::parse(input.bytes("owned path"));
  const owned_object_kind kind =
      static_cast<owned_object_kind>(input.u8("owned kind"));
  const std::uint32_t mode = input.u32("owned mode");
  const std::uint64_t uid = input.u64("owned uid");
  const std::uint64_t gid = input.u64("owned gid");
  const std::int64_t mtime_seconds =
      input.i64("owned mtime seconds");
  const std::uint32_t mtime_nanoseconds =
      input.u32("owned mtime nanoseconds");
  installed_object_timestamp mtime(mtime_seconds, mtime_nanoseconds);
  std::optional<std::uint64_t> size;
  if (input.boolean("owned size presence"))
    size = input.u64("owned size");
  std::optional<installed_regular_content_identity> regular_content;
  if (input.boolean("owned regular content presence"))
    regular_content = read_digest<installed_regular_content_identity>(
        input, "owned regular content identity");
  std::optional<std::string> symlink_target;
  if (input.boolean("owned symlink target presence"))
    symlink_target = input.bytes("owned symlink target");
  std::optional<installed_device_number> device;
  if (input.boolean("owned device presence"))
  {
    const std::uint64_t major = input.u64("owned device major");
    const std::uint64_t minor = input.u64("owned device minor");
    device.emplace(major, minor);
  }
  std::optional<package_path> hardlink_anchor;
  if (input.boolean("owned hardlink presence"))
    hardlink_anchor = package_path::parse(input.bytes("owned hardlink anchor"));
  installed_object_metadata object(
      kind, mode, uid, gid, std::move(mtime), std::move(size),
      std::move(regular_content), std::move(symlink_target),
      std::move(device), std::move(hardlink_anchor));
  const active_object_origin origin =
      static_cast<active_object_origin>(input.u8("owned origin"));
  std::optional<rejected_object_reference> rejected;
  if (input.boolean("rejected object presence"))
  {
    const rejected_object_side side = static_cast<rejected_object_side>(
        input.u8("rejected object side"));
    rejected_object_identity identity =
        read_digest<rejected_object_identity>(
            input, "rejected object identity");
    rejected.emplace(side, std::move(identity));
  }
  return owned_entry::make(
      std::move(path), std::move(object), origin, std::move(rejected));
}

void encode_receipt(writer& output, const installation_receipt& receipt)
{
  output.u16(receipt.schema_version());
  encode_control(output, receipt.control());
  output.u64(receipt.manifest().size());
  for (const owned_entry& entry : receipt.manifest()) encode_entry(output, entry);
  output.digest(receipt.operation_plan());
  output.digest(receipt.application_evidence());
  output.boolean(receipt.transaction_evidence().has_value());
  if (receipt.transaction_evidence()) output.digest(*receipt.transaction_evidence());
}

installation_receipt decode_receipt(reader& input, const state_target_binding& binding)
{
  if (input.u16("installation receipt schema") != installation_receipt_schema_version)
    throw store_error("unsupported installation receipt schema");
  installed_control control = decode_control(input);
  std::vector<owned_entry> manifest;
  const std::size_t count = input.count("ownership manifest count");
  manifest.reserve(count);
  for (std::size_t index = 0; index < count; ++index) manifest.push_back(decode_entry(input));
  operation_plan_identity plan = read_digest<operation_plan_identity>(input, "operation plan identity");
  application_evidence_identity evidence = read_digest<application_evidence_identity>(input, "application evidence identity");
  std::optional<transaction_evidence_identity> transaction;
  if (input.boolean("transaction evidence presence"))
    transaction = read_digest<transaction_evidence_identity>(input, "transaction evidence identity");
  return installation_receipt::make(
      std::move(control), binding, std::move(manifest), std::move(plan),
      std::move(evidence), std::move(transaction));
}

std::string_view as_string_view(const std::vector<std::uint8_t>& bytes)
{
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

void require_canonical_encoding(const std::vector<std::uint8_t>& canonical,
                                std::string_view original,
                                const char* label)
{
  if (as_string_view(canonical) != original)
    throw store_error(std::string(label) + " is not canonically encoded");
}

} // namespace

std::vector<std::uint8_t> encode_generation_binding(const state_target_binding& binding)
{
  writer output;
  output.raw(binding_magic);
  output.u16(canonical_generation_storage_version);
  encode_binding_fields(output, binding);
  return output.take();
}

state_target_binding decode_generation_binding(std::string_view bytes)
{
  try
  {
    reader input(bytes);
    input.expect(binding_magic, "canonical binding magic");
    if (input.u16("canonical binding version") != canonical_generation_storage_version)
      throw store_error("unsupported canonical binding encoding version");
    state_target_binding result = decode_binding_fields(input);
    input.finish();
    require_canonical_encoding(encode_generation_binding(result), bytes,
                               "canonical binding record");
    return result;
  }
  catch (const store_error&) { throw; }
  catch (const error& failure)
  {
    throw store_error(std::string("invalid canonical binding record: ") + failure.what());
  }
}

std::vector<std::uint8_t> encode_generation_snapshot(const snapshot& value)
{
  writer output;
  output.raw(snapshot_magic);
  output.u16(canonical_generation_storage_version);
  output.u16(value.schema_version());
  encode_binding_fields(output, value.target_binding());
  output.u64(value.packages().size());
  for (const installed_package& package : value.packages())
    encode_receipt(output, package.receipt());
  return output.take();
}

snapshot decode_generation_snapshot(std::string_view bytes)
{
  try
  {
    reader input(bytes);
    input.expect(snapshot_magic, "canonical generation magic");
    if (input.u16("canonical generation version") != canonical_generation_storage_version)
      throw store_error("unsupported canonical generation encoding version");
    if (input.u16("installed-state schema version") != installed_state_schema_version)
      throw store_error("unsupported installed-state snapshot schema version");
    state_target_binding binding = decode_binding_fields(input);
    std::vector<installed_package> packages;
    const std::size_t count = input.count("installed package count");
    packages.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
      packages.push_back(installed_package::make(decode_receipt(input, binding)));
    input.finish();
    snapshot result = snapshot::make(std::move(binding), std::move(packages));
    require_canonical_encoding(encode_generation_snapshot(result), bytes,
                               "canonical generation snapshot");
    return result;
  }
  catch (const store_error&) { throw; }
  catch (const error& failure)
  {
    throw store_error(std::string("invalid canonical generation snapshot: ") + failure.what());
  }
}

} // namespace pkgstate::detail
