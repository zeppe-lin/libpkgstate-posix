// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file pkgstate_init.cpp
 *  \brief Explicit bootstrap client for empty canonical native installed state.
 */
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

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
  pkgstate-init --canonical-store path \
    --managed-target identity --state-store identity \
    --root-view identity --state-backend identity \
    --publication-domain identity
  pkgstate-init {-V | -h}

Explicitly initialize or validate one empty canonical native installed-state store.

Options:
  -c, --canonical-store=path  Initialize or validate the generation store
      --managed-target=id     Managed package-target identity
      --state-store=id        Durable installed-state store identity
      --root-view=id          Logical target root-view identity
      --state-backend=id      Installed-state backend identity
      --publication-domain=id Publication and locking-domain identity
  -V, --version               Print version and exit
  -h, --help                  Print this help and exit

The command admits only an empty canonical snapshot bound to the exact supplied
target identities. It never erases packages, repairs foreign state, imports a
historical database, or manufactures target truth from retained evidence.
)";
}

void print_version()
{
  std::cout << "pkgstate-init (libpkgstate-posix) " << PKGSTATE_VERSION << '\n';
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

void initialize(const options& parsed)
{
  const pkgstate::state_target_binding binding = parse_binding(parsed);
  const pkgstate::posix::canonical_generation_store store(parsed.path, binding);
  const pkgstate::snapshot state = store.read();
  if (!state.packages().empty())
  {
    throw pkgstate::store_error(
        "canonical store already contains installed packages");
  }

  std::cout << "storage-format="
            << pkgstate::canonical_generation_storage_format << '\n'
            << "store=" << std::quoted(parsed.path.string()) << '\n'
            << "target-binding=" << binding.identity().string() << '\n'
            << "snapshot=" << state.identity().string() << '\n'
            << "ownership-inventory=" << state.ownership_identity().string()
            << '\n'
            << "packages=0\n";
}

} // namespace

int main(int argc, char** argv)
{
  try
  {
    initialize(parse_options(argc, argv));
    return EXIT_SUCCESS;
  }
  catch (const std::invalid_argument& error)
  {
    std::cerr << "pkgstate-init: " << error.what() << '\n';
    print_help(std::cerr);
    return usage_status;
  }
  catch (const pkgstate::error& error)
  {
    std::cerr << "pkgstate-init: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  catch (const std::exception& error)
  {
    std::cerr << "pkgstate-init: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
