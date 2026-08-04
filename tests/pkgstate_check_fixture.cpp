// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_fixture.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

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
    pkgstate::canonical_generation_store store(
        std::filesystem::path(argv[1]), target);
    const pkgstate::installed_package package = native_fixture::package(
        "base", 20, target);
    const pkgstate::state_publication_request request =
        pkgstate::state_publication_request::make(
            store.read(),
            {pkgstate::package_state_delta::install(
                package, package.receipt().operation_plan(),
                package.receipt().application_evidence())});
    const pkgstate::state_publication_receipt receipt =
        store.compare_and_publish(request);
    if (receipt.outcome() != pkgstate::state_publication_outcome::published)
    {
      std::cerr << "fixture publication did not complete\n";
      return EXIT_FAILURE;
    }
  }
  catch (const std::exception& failure)
  {
    std::cerr << "pkgstate-check-fixture: " << failure.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
