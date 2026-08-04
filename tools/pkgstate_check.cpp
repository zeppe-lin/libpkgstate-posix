// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file pkgstate_check.cpp
 *  \brief Read-only diagnostics for canonical native installed state.
 */
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <getopt.h>

#include <libpkgstate-posix/canonical_generation_store.h>
#include <libpkgstate/error.h>

#ifndef PKGSTATE_VERSION
#define PKGSTATE_VERSION "unknown"
#endif

namespace {

constexpr int usage_status = 2;

enum long_option_value {
  managed_target_option = 1000,
  state_store_option,
  root_view_option,
  state_backend_option,
  publication_domain_option,
};

struct options final {
  std::filesystem::path path;
  std::optional<std::string> managed_target;
  std::optional<std::string> state_store;
  std::optional<std::string> root_view;
  std::optional<std::string> state_backend;
  std::optional<std::string> publication_domain;
};

void print_help(std::ostream& out)
{
  out << R"(Usage:
  pkgstate-check --canonical-store path \
    --managed-target identity --state-store identity \
    --root-view identity --state-backend identity \
    --publication-domain identity
  pkgstate-check {-V | -h}

Validate and summarize an existing canonical native installed-state store.

Options:
  -c, --canonical-store=path  Inspect an existing generation store
      --managed-target=id     Managed package-target identity
      --state-store=id        Durable installed-state store identity
      --root-view=id          Logical target root-view identity
      --state-backend=id      Installed-state backend identity
      --publication-domain=id Publication and locking-domain identity
  -V, --version               Print version and exit
  -h, --help                  Print this help and exit

The command is read-only. It never initializes a store, publishes a generation,
repairs state, imports historical databases, or reconstructs missing control.
)";
}

void print_version()
{
  std::cout << "pkgstate-check (libpkgstate-posix) " << PKGSTATE_VERSION << '\n';
}

options parse_options(int argc, char** argv)
{
  options parsed;
  static const option long_options[] = {
      {"canonical-store", required_argument, nullptr, 'c'},
      {"managed-target", required_argument, nullptr, managed_target_option},
      {"state-store", required_argument, nullptr, state_store_option},
      {"root-view", required_argument, nullptr, root_view_option},
      {"state-backend", required_argument, nullptr, state_backend_option},
      {"publication-domain", required_argument, nullptr,
       publication_domain_option},
      {"version", no_argument, nullptr, 'V'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0},
  };

  opterr = 0;
  for (;;)
  {
    const int value = getopt_long(argc, argv, "c:Vh", long_options, nullptr);
    if (value == -1)
      break;
    switch (value)
    {
      case 'c':
        if (!parsed.path.empty())
          throw std::invalid_argument("--canonical-store specified twice");
        parsed.path = optarg;
        break;
      case managed_target_option:
        parsed.managed_target = optarg;
        break;
      case state_store_option:
        parsed.state_store = optarg;
        break;
      case root_view_option:
        parsed.root_view = optarg;
        break;
      case state_backend_option:
        parsed.state_backend = optarg;
        break;
      case publication_domain_option:
        parsed.publication_domain = optarg;
        break;
      case 'V':
        print_version();
        std::exit(EXIT_SUCCESS);
      case 'h':
        print_help(std::cout);
        std::exit(EXIT_SUCCESS);
      default:
        throw std::invalid_argument("invalid command-line option");
    }
  }

  if (optind != argc)
    throw std::invalid_argument("unexpected positional argument: " +
                                std::string(argv[optind]));
  if (parsed.path.empty())
    throw std::invalid_argument("--canonical-store is required");
  if (!parsed.managed_target || !parsed.state_store || !parsed.root_view ||
      !parsed.state_backend || !parsed.publication_domain)
  {
    throw std::invalid_argument(
        "all canonical target-binding identities are required");
  }
  return parsed;
}

pkgstate::state_target_binding parse_binding(const options& parsed)
{
  return pkgstate::state_target_binding::make(
      pkgstate::managed_target_identity::parse(*parsed.managed_target),
      pkgstate::state_store_identity::parse(*parsed.state_store),
      pkgstate::root_view_identity::parse(*parsed.root_view),
      pkgstate::state_backend_identity::parse(*parsed.state_backend),
      pkgstate::publication_domain_identity::parse(
          *parsed.publication_domain));
}

void inspect(const options& parsed)
{
  const pkgstate::state_target_binding binding = parse_binding(parsed);
  const pkgstate::posix::canonical_generation_store store =
      pkgstate::posix::canonical_generation_store::open_existing(parsed.path,
                                                           binding);
  const pkgstate::snapshot state = store.read();

  std::map<std::string, std::size_t> owners_by_path;
  std::size_t claims = 0;
  std::size_t rejected = 0;
  std::size_t explicit_packages = 0;
  std::size_t dependency_packages = 0;
  std::size_t profile_packages = 0;
  std::size_t policy_packages = 0;

  for (const pkgstate::installed_package& package : state.packages())
  {
    switch (package.control().reason().kind())
    {
      case pkgstate::installation_reason_kind::explicit_request:
        ++explicit_packages;
        break;
      case pkgstate::installation_reason_kind::runtime_dependency:
        ++dependency_packages;
        break;
      case pkgstate::installation_reason_kind::profile_membership:
        ++profile_packages;
        break;
      case pkgstate::installation_reason_kind::system_policy:
        ++policy_packages;
        break;
    }
    for (const pkgstate::owned_entry& entry : package.manifest())
    {
      ++claims;
      ++owners_by_path[entry.path().string()];
      if (entry.rejected())
        ++rejected;
    }
  }

  std::size_t shared = 0;
  for (const auto& entry : owners_by_path)
    if (entry.second > 1)
      ++shared;

  std::cout << "storage-format="
            << pkgstate::canonical_generation_storage_format << '\n'
            << "store=" << std::quoted(parsed.path.string()) << '\n'
            << "target-binding=" << binding.identity().string() << '\n'
            << "snapshot=" << state.identity().string() << '\n'
            << "ownership-inventory=" << state.ownership_identity().string()
            << '\n'
            << "packages=" << state.packages().size() << '\n'
            << "ownership-claims=" << claims << '\n'
            << "owned-paths=" << owners_by_path.size() << '\n'
            << "shared-paths=" << shared << '\n'
            << "rejected-object-references=" << rejected << '\n'
            << "reason-explicit=" << explicit_packages << '\n'
            << "reason-runtime-dependency=" << dependency_packages << '\n'
            << "reason-profile=" << profile_packages << '\n'
            << "reason-system-policy=" << policy_packages << '\n';
}

} // namespace

int main(int argc, char** argv)
{
  try
  {
    inspect(parse_options(argc, argv));
    return EXIT_SUCCESS;
  }
  catch (const std::invalid_argument& error)
  {
    std::cerr << "pkgstate-check: " << error.what() << '\n';
    print_help(std::cerr);
    return usage_status;
  }
  catch (const pkgstate::error& error)
  {
    std::cerr << "pkgstate-check: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  catch (const std::exception& error)
  {
    std::cerr << "pkgstate-check: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
