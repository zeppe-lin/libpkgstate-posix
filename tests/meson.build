# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
if get_option('tests').allowed()
  sh = find_program('sh')
  store_test = executable(
    'canonical-generation-store-test',
    'canonical_generation_store_test.cpp',
    dependencies: libpkgstate_posix_dep,
  )
  test('publish immutable canonical generations', store_test)

  umbrella = executable('umbrella-header-test', 'umbrella_header_test.cpp', dependencies: libpkgstate_posix_dep)
  test('compile the provider umbrella header', umbrella)
  foreach header : ['export.h', 'canonical_generation_store.h', 'libpkgstate-posix.h']
    target = executable(
      header.replace('.', '-') + '-header-test',
      'public_header_test.cpp',
      cpp_args: ['-DLIBPKGSTATE_POSIX_TEST_HEADER="libpkgstate-posix/' + header + '"'],
      dependencies: libpkgstate_posix_dep,
    )
    test('compile public header ' + header, target)
  endforeach

  if library_kind == 'shared'
    test(
      'verify reviewed provider ABI surface',
      find_program(meson.current_source_dir() / 'check_abi_surface.sh'),
      args: [libpkgstate_posix.full_path(), meson.project_source_root() / 'abi/libpkgstate-posix.exports', find_program('nm').full_path()],
    )
  endif

  foreach contract : ['architecture', 'abi', 'ci', 'documentation', 'extraction', 'release', 'repository']
    test('contract ' + contract, sh, args: [join_paths(meson.current_source_dir(), 'contracts/check_' + contract + '.sh'), meson.project_source_root()])
  endforeach
  test('contract style', sh, args: [join_paths(meson.current_source_dir(), 'contracts/check_style.sh'), meson.project_source_root(), meson.project_build_root()])

  if get_option('tools').allowed()
    fixture = executable('pkgstate-check-fixture', 'pkgstate_check_fixture.cpp', dependencies: libpkgstate_posix_dep)
    test('exercise read-only generation diagnostics', sh, args: [join_paths(meson.current_source_dir(), 'pkgstate_check_cli_test.sh'), pkgstate_check_tool, fixture])
    test('verify pkgstate-check manual contract', sh, args: [join_paths(meson.current_source_dir(), 'pkgstate_check_manual_test.sh'), meson.project_source_root(), pkgstate_check_tool])
  endif
endif
