# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
if get_option('tests').allowed()
  sh = find_program('sh')

  foreach spec : [
    ['provider-contract', 'unit/provider_contract_test.cpp'],
  ]
    target = executable(
      spec[0],
      spec[1],
      dependencies: libpkgstate_posix_dep,
    )
    test(spec[0], target, suite: 'unit')
  endforeach

  foreach spec : [
    ['initialization-test', 'initialize canonical generation store', 'integration/initialization_test.cpp'],
    ['publication-test', 'publish and republish canonical generations', 'integration/publication_test.cpp'],
    ['root-authority-test', 'retain opened store authority', 'integration/root_authority_test.cpp'],
    ['locking-test', 'enforce non-blocking store locks', 'integration/locking_test.cpp'],
    ['layout-validation-test', 'refuse invalid canonical layout', 'integration/layout_validation_test.cpp'],
    ['generation-validation-test', 'refuse invalid selected generations', 'integration/generation_validation_test.cpp'],
    ['recovery-refusal-test', 'refuse bound recovery and ignore unselected work', 'integration/recovery_refusal_test.cpp'],
    ['generation-collision-test', 'refuse corrupt existing generation reuse', 'integration/generation_collision_test.cpp'],
    ['publication-outcome-test', 'classify publication mechanism failures', 'integration/publication_outcome_test.cpp'],
    ['special-file-refusal-test', 'refuse blocking special-file corruption', 'integration/special_file_refusal_test.cpp'],
  ]
    dependencies = [libpkgstate_posix_dep]
    if spec[0] == 'publication-test'
      dependencies += [libcrypto_dep]
    endif
    target = executable(
      spec[0],
      spec[2],
      dependencies: dependencies,
    )
    test(spec[1], target, suite: 'integration')
  endforeach

  umbrella = executable(
    'umbrella-header-test',
    'header/umbrella_header_test.cpp',
    dependencies: libpkgstate_posix_dep,
  )
  test('compile the provider umbrella header', umbrella, suite: 'header')
  foreach header : ['export.h', 'canonical_generation_store.h', 'libpkgstate-posix.h']
    target = executable(
      header.replace('.', '-') + '-header-test',
      'header/public_header_test.cpp',
      cpp_args: ['-DLIBPKGSTATE_POSIX_TEST_HEADER="libpkgstate-posix/' + header + '"'],
      dependencies: libpkgstate_posix_dep,
    )
    test('compile public header ' + header, target, suite: 'header')
  endforeach

  if library_kind == 'shared'
    test(
      'verify reviewed provider ABI surface',
      find_program(meson.current_source_dir() / 'check_abi_surface.sh'),
      args: [libpkgstate_posix.full_path(), meson.project_source_root() / 'abi/libpkgstate-posix.exports', find_program('nm').full_path()],
      suite: 'contract',
    )
  endif

  libpkgstate_public_include = libpkgstate_dep.get_variable(
    pkgconfig: 'includedir',
  )
  foreach contract : [
    'bootstrap_client',
    'architecture',
    'abi',
    'ci',
    'documentation',
    'documentation_source',
    'extraction',
    'manpage_normalizer',
    'manpage_source',
    'release',
    'repository',
    'test_layout',
  ]
    contract_args = [
      join_paths(meson.current_source_dir(), 'contracts/check_' + contract + '.sh'),
      meson.project_source_root(),
    ]
    if contract == 'documentation'
      contract_args += [libpkgstate_public_include]
    endif
    test('contract ' + contract, sh, args: contract_args, suite: 'contract')
  endforeach
  test(
    'contract style',
    sh,
    args: [join_paths(meson.current_source_dir(), 'contracts/check_style.sh'), meson.project_source_root(), meson.project_build_root()],
    suite: 'contract',
  )

  pandoc = find_program('pandoc', required: false)
  if pandoc.found()
    test(
      'contract generated manpages',
      sh,
      args: [
        join_paths(meson.current_source_dir(), 'contracts/check_manpage_generated.sh'),
        meson.project_source_root(),
        pandoc.full_path(),
      ],
      suite: 'contract',
    )
  endif

  test(
    'pkgconfig-metadata',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'check_pkgconfig_metadata.sh'),
      meson.project_build_root(),
    ],
    suite: 'contract',
  )

  if get_option('tools').allowed()
    fixture = executable(
      'pkgstate-check-fixture',
      'cli/pkgstate_check_fixture.cpp',
      dependencies: libpkgstate_posix_dep,
    )
    test(
      'initialize empty canonical state explicitly',
      sh,
      args: [
        join_paths(meson.current_source_dir(), 'cli/pkgstate_init_cli_test.sh'),
        pkgstate_init_tool,
        pkgstate_check_tool,
        fixture,
      ],
      suite: 'cli',
    )
    test(
      'verify pkgstate-init manual contract',
      sh,
      args: [
        join_paths(meson.current_source_dir(), 'cli/pkgstate_init_manual_test.sh'),
        meson.project_source_root(),
        pkgstate_init_tool,
      ],
      suite: 'cli',
    )
    test(
      'exercise read-only generation diagnostics',
      sh,
      args: [
        join_paths(meson.current_source_dir(), 'cli/pkgstate_check_cli_test.sh'),
        pkgstate_check_tool,
        fixture,
      ],
      suite: 'cli',
    )
    test(
      'verify pkgstate-check manual contract',
      sh,
      args: [
        join_paths(meson.current_source_dir(), 'cli/pkgstate_check_manual_test.sh'),
        meson.project_source_root(),
        pkgstate_check_tool,
      ],
      suite: 'cli',
    )
  endif
endif
