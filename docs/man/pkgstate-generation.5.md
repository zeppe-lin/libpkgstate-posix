% PKGSTATE-GENERATION(5) libpkgstate-posix | Version 3.0.0

# NAME

pkgstate-generation - native immutable installed-state storage format

# DESCRIPTION

A canonical generation store contains:

**binding**
One canonical record naming the store's exact **state_target_binding**.

**current**
The name of the selected immutable generation.

**generations/**
Complete immutable generation directories.

Each generation contains one canonical binary snapshot record encoded by
**pkgstate_generation_codec**(3). The format stores
all package source records, installed control, installation receipts, complete
recorded object metadata, manifests, publication references, and target binding
required to reconstruct the exact native snapshot and verify every state-owned
identity.

# VERSION

The current format version is 1 and its public identifier is
**libpkgstate-generation-v1**. Only canonical generation 1 is recognized; every other version is rejected.
Migration between storage versions belongs to a separate
explicit tool.

# DURABILITY

Publication creates and synchronizes a complete generation before atomically
replacing and synchronizing the current selector. Generation directories are
immutable after selection.

# VALIDATION

Readers reject malformed lengths, unknown enum values, invalid UTF-8-independent
identifier bytes, duplicate normalized values, identity mismatches, target
mismatches, trailing data, and selector traversal.

# SEE ALSO

**pkgstate_generation_codec**(3), **pkgstate_canonical_generation_store**(3),
**pkgstate-check**(1)
