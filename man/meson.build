# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
scdoc = find_program('scdoc', required: get_option('man_pages'))
if scdoc.found()
  foreach page : [
    ['libpkgstate-posix.3', '3'],
    ['pkgstate_canonical_generation_store.3', '3'],
    ['pkgstate-generation.5', '5'],
  ]
    custom_target(
      page[0],
      input: page[0] + '.scdoc',
      output: page[0],
      command: [scdoc], feed: true, capture: true,
      install: true,
      install_dir: get_option('mandir') / ('man' + page[1]),
    )
  endforeach
  custom_target(
    'pkgstate-check.1',
    input: 'pkgstate-check.1.scdoc',
    output: 'pkgstate-check.1',
    command: [scdoc], feed: true, capture: true,
    install: get_option('install_tools'),
    install_dir: get_option('mandir') / 'man1',
  )
endif
