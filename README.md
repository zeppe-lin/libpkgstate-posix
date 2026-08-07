# libpkgstate-posix

`libpkgstate-posix` provides
`pkgstate::posix::canonical_generation_store`, the concrete
immutable-generation storage mechanism for `libpkgstate`.

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

`libpkgstate` owns installed-state values, stale-safe compare-and-publish semantics, publication requests and receipts, and durable evidence codecs. This repository owns one host mechanism: descriptor-anchored immutable generations whose opened store authority survives pathname rename/replacement, publication locking, durable selector replacement, recovery refusal, and read-only `pkgstate-check` diagnostics. Canonical generation-v1 binding and snapshot bytes are encoded and validated by `libpkgstate`; this provider persists them without reinterpretation.

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

The repository was extracted from the `libpkgstate` provider implementation before publication. It begins with canonical generation-1 storage and uses SONAME 3. No earlier native storage generation is recognized. Release the matching `libpkgstate` generation before this provider.

## Documentation

- `DESIGN.md` — provider invariants and refusal boundaries;
- `STORAGE.md` — immutable-generation layout and durability protocol;
- `TESTING.md` — qualification matrix;
- `docs/architecture.md` — owner/provider placement;
- `docs/abi.md` — ABI and pkg-config policy;
- `man/libpkgstate-posix.3.scdoc` — installed provider overview;
- `man/pkgstate_canonical_generation_store.3.scdoc` — concrete class contract;
- `man/pkgstate-generation.5.scdoc` — generation-v1 storage format; and
- `man/pkgstate-check.1.scdoc` — read-only diagnostic client.

## License

GPL-3.0-or-later. See `COPYING` and `COPYRIGHT`.
