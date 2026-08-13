% PKGSTATE_CANONICAL_GENERATION_STORE(3) libpkgstate-posix | Version 3.0.0

# NAME

pkgstate_canonical_generation_store - immutable generation backend

# SYNOPSIS

**#include <libpkgstate-posix/canonical_generation_store.h>**

# DESCRIPTION

**pkgstate::posix::canonical_generation_store** persists complete native
snapshots in immutable
generation directories. One current selector names the authoritative generation.
The store directory is durably bound to exactly one **state_target_binding**.

The normal constructor initializes an empty store when necessary.
**open_existing()** validates an existing store without creating or repairing any
path and is suitable for diagnostics.

# PUBLICATION

Compare-and-publish takes an exclusive directory lock, validates the expected
snapshot, writes and synchronizes a new immutable generation, atomically replaces
the current selector, synchronizes the selection boundary, and returns a typed
receipt. Readers take a shared lock and decode the selected complete generation.

# STORAGE FORMAT

The current receipt-visible format is **libpkgstate-generation-v1**. On-disk
records are canonical length-delimited binary values. Unknown versions, invalid
identities, inconsistent target bindings, and trailing data are rejected.

# SEE ALSO

**pkgstate-generation**(5), **pkgstate_store**(3), **pkgstate-check**(1)
