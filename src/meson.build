# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
public_inc = include_directories('../include')
private_inc = include_directories('.')

elf_export_generator = find_program(meson.project_source_root() / 'tools/generate-elf-export-script.sh')
export_script_name = 'libpkgstate-posix.exports.map'
export_script_path = meson.current_build_dir() / export_script_name
export_script = configure_file(
  input: '../abi/libpkgstate-posix.exports',
  output: export_script_name,
  command: [elf_export_generator, '@INPUT@'],
  capture: true,
)

libpkgstate_posix = library(
  'pkgstate-posix',
  ['canonical_generation_store.cpp'],
  include_directories: [public_inc, private_inc],
  dependencies: [libpkgstate_dep, libcrypto_dep],
  cpp_args: visibility_cpp_args + ['-DPKGSTATE_POSIX_BUILDING_LIBRARY', '-D_FILE_OFFSET_BITS=64'],
  gnu_symbol_visibility: 'hidden',
  link_args: ['-Wl,--version-script=' + export_script_path],
  link_depends: [export_script],
  install: true,
  version: meson.project_version(),
  soversion: '3',
)

consumer_deps = [libpkgstate_dep]
if want_static
  consumer_deps += [libcrypto_dep]
endif
libpkgstate_posix_dep = declare_dependency(
  link_with: libpkgstate_posix,
  include_directories: public_inc,
  dependencies: consumer_deps,
  compile_args: ['-D_FILE_OFFSET_BITS=64'],
  version: meson.project_version(),
)
meson.override_dependency('libpkgstate-posix', libpkgstate_posix_dep)

install_headers(
  '../include/libpkgstate-posix/export.h',
  '../include/libpkgstate-posix/canonical_generation_store.h',
  '../include/libpkgstate-posix/libpkgstate-posix.h',
  subdir: 'libpkgstate-posix',
)

pkgconf.generate(
  libpkgstate_posix,
  name: 'libpkgstate-posix',
  filebase: 'libpkgstate-posix',
  description: 'POSIX immutable-generation storage for libpkgstate',
  subdirs: '.',
  requires: ['libpkgstate >= 3.0.0'],
  requires_private: [libcrypto_dep],
)
