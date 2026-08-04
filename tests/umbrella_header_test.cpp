// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgstate-posix/libpkgstate-posix.h>
int main()
{
  static_assert(pkgstate::canonical_generation_storage_version == 4);
  return 0;
}
