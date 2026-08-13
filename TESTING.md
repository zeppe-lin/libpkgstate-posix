# Testing libpkgstate-posix

The test tree separates evidence by role:

- `unit` pins provider constants and public type-shape invariants without
  constructing storage;
- `integration` exercises the real `libpkgstate` canonical-store sequence
  against the POSIX filesystem mechanism;
- `header` compiles every public header and the umbrella independently;
- `contract` checks architecture, ABI, release, repository, documentation,
  pkg-config, style, and test-layout invariants; and
- `cli` qualifies the explicit empty-state bootstrap client and the
  installed-state diagnostic client at their opposite authority boundaries.

Integration qualification covers exact target binding, canonical empty
initialization, state-owned binding/snapshot bytes, retained store authority
across configured-path rename/replacement, immutable generation publication,
exact generation reuse, concrete stale refusal without mutation, publication
evidence, read-only existing-store open, shared/exclusive non-blocking lock
behavior, writable/multiply-linked/symlink selector refusal, malformed selector
refusal, non-blocking FIFO refusal for binding, selector, and selected snapshot,
writable or redirected selected-generation refusal, snapshot corruption and
target/identity mismatch, bound incomplete-layout recovery
refusal, unbound empty-initialization completion, unselected crash leftovers,
and corrupt existing-generation collision refusal.

The publication-outcome integration test interposes only the test executable's
`fsync()` symbol and delegates ordinary calls directly to `SYS_fsync`. This
keeps the production library unmodified while deterministically proving the
real provider classifications for failure before selector exposure, visible
publication without durability confirmation, and an indeterminate post-selection
state requiring authoritative recovery. Successful publication still exercises
ordinary kernel `fsync()` calls.

The `pkgstate-init` CLI test starts from an absent nested path, proves one exact
empty generation is admitted, validates it through `pkgstate-check`, proves
repeated same-binding initialization is stable, refuses a mismatched binding
without changing durable state, and refuses an already-populated valid store
without erasing packages.

The `pkgstate-check` fixture publishes packages covering every installation
reason and shared ownership. The diagnostic CLI test snapshots the durable
store before and after inspection, proves the report counts, refuses a
mismatched binding, and proves an absent store is not initialized.

The release matrix contains GCC and Clang shared/static builds, optimized
release, ASan/UBSan, strict Doxygen, `mandoc -Tlint`, staged installation, and
direct `DT_NEEDED` inspection. The documentation contract parses public headers
with the include root of the exact `libpkgstate` dependency resolved by Meson;
it does not rely on an ambient system installation to complete the public
header graph.
