# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
if get_option('tools').allowed()
  pkgstate_check_tool = executable(
    'pkgstate-check',
    'pkgstate_check.cpp',
    cpp_args: ['-DPKGSTATE_VERSION="' + meson.project_version() + '"'],
    dependencies: libpkgstate_posix_dep,
    install: get_option('install_tools'),
  )
endif
