# Qualification

Release qualification covers exact target binding, retained store authority across configured-path rename/replacement, empty initialization, immutable generation publication, concrete stale refusal, read-only existing-store open, writable/multiply-linked/symlink selector refusal, writable selected-generation refusal, corrupted or incomplete layout refusal, shared/exclusive non-blocking lock behavior, selector replacement, durability classifications, installed consumers, every public header, exact ELF exports, pkg-config closure, manuals, and `pkgstate-check` non-mutation.

The matrix contains GCC and Clang shared/static builds, optimized release, ASan/UBSan, strict Doxygen, `mandoc -Tlint`, staged installation, and direct `DT_NEEDED` inspection.

The documentation contract parses public headers with the include root of the
exact `libpkgstate` dependency resolved by Meson; it does not rely on an
ambient system installation to complete the public header graph.
