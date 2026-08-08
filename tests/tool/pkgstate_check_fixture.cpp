// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../fixtures/state.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cerr << "usage: pkgstate-check-fixture canonical-store\n";
    return EXIT_FAILURE;
  }

  try
  {
    const pkgstate::state_target_binding target = native_fixture::target();
    pkgstate::posix::canonical_generation_store store(
        std::filesystem::path(argv[1]), target);

    std::vector<pkgstate::installed_package> packages;
    packages.push_back(native_fixture::package(
        "base", 20, target,
        pkgstate::installation_reason::explicit_request()));
    packages.push_back(native_fixture::package(
        "runtime", 40, target,
        pkgstate::installation_reason::runtime_dependency(
            pkgstate::package_reference("base"))));
    packages.push_back(native_fixture::package(
        "profiled", 60, target,
        pkgstate::installation_reason::profile_membership(
            pkgstate::profile_reference("@desktop"),
            native_fixture::identity<pkgstate::source_profile_identity>(61))));
    packages.push_back(native_fixture::package(
        "policy", 80, target,
        pkgstate::installation_reason::system_policy("base-system")));

    for (const pkgstate::installed_package& package : packages)
    {
      const pkgstate::state_publication_request request =
          pkgstate::state_publication_request::make(
              store.read(),
              {pkgstate::package_state_delta::install(
                  package,
                  package.receipt().operation_plan(),
                  package.receipt().application_evidence())});
      const pkgstate::state_publication_receipt receipt =
          store.compare_and_publish(request);
      if (receipt.outcome() != pkgstate::state_publication_outcome::published)
      {
        std::cerr << "fixture publication did not complete\n";
        return EXIT_FAILURE;
      }
    }
  }
  catch (const std::exception& failure)
  {
    std::cerr << "pkgstate-check-fixture: " << failure.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
