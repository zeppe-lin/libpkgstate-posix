# libpkgstate-posix

`libpkgstate-posix` provides the concrete immutable-generation storage mechanism for `libpkgstate`.

```text
state_publication_request
        |
        v
libpkgstate compare-and-publish semantics
        |
        v
libpkgstate-posix generation store
        |
        v
one durable target-bound canonical snapshot
```

## Authority

`libpkgstate` owns installed-state values, stale-safe compare-and-publish semantics, publication requests and receipts, and durable evidence codecs. This repository owns one host mechanism: descriptor-anchored immutable generations, publication locking, durable selector replacement, generation-v3 encoding, and read-only `pkgstate-check` diagnostics.

It does not parse package sources, admit builds, inspect images, plan or apply operations, construct publication requests, reinterpret stale requests, repair state, import historical databases, or choose retry policy.

## Build

Shared and static closures use separate build directories:

```sh
meson setup build-shared -Ddefault_library=shared -Dlink_mode=shared
meson compile -C build-shared
meson test -C build-shared --print-errorlogs

meson setup build-static -Ddefault_library=static -Dlink_mode=static
meson compile -C build-static
meson test -C build-static --print-errorlogs
```

The optional `pkgstate-check` client is built by default and installed only with `-Dinstall_tools=true`.

## Release lineage

The 3.0 repository was extracted from the untagged `libpkgstate` 3.0 refinement after `libpkgstate` 2.5.1. The library preserves the generation-v3 storage protocol and uses SONAME 3. Release `libpkgstate` 3.0 before this provider.

## License

GPL-3.0-or-later. See `COPYING` and `COPYRIGHT`.
