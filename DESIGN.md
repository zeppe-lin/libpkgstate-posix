# POSIX generation-store design

## Boundary

The provider implements `pkgstate::canonical_store`; it does not override its non-virtual compare-and-publish sequence. The semantic owner rereads current state under the provider transaction, rejects stale requests, derives the only permitted result, and passes that exact snapshot to the provider publication primitive.

## Mechanism

One store directory is durably bound to one `state_target_binding`. Complete snapshots are encoded by the state-owned generation-v4 codec and persisted in immutable generation directories. A current selector names the authoritative generation. Reads take a non-blocking shared publication lock; compare-and-publish takes a non-blocking exclusive lock. New generation bytes and directory metadata are synchronized before selector replacement is exposed and synchronized.

The provider never edits a selected generation, silently rebases a request, imports another format, repairs missing authority, waits for locks, or claims target-filesystem/state atomicity.

## Placement

Canonical binding and snapshot bytes are state-domain protocol and remain in
`libpkgstate`. Filesystem paths, descriptors, locks, immutable directories,
selector replacement, fsync ordering, recovery refusal, and diagnostics are
provider mechanism and remain here. This repository consumes the core codec and
contains no competing copy. Foreign source/build/plan/apply translations remain
in their independent adapters.
