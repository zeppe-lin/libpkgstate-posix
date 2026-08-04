# POSIX generation-store design

## Boundary

The provider implements `pkgstate::canonical_store`; it does not override its non-virtual compare-and-publish sequence. The semantic owner rereads current state under the provider transaction, rejects stale requests, derives the only permitted result, and passes that exact snapshot to the provider publication primitive.

## Mechanism

One store directory is durably bound to one `state_target_binding`. Complete snapshots are encoded into immutable generation directories. A current selector names the authoritative generation. Reads take a non-blocking shared publication lock; compare-and-publish takes a non-blocking exclusive lock. New generation bytes and directory metadata are synchronized before selector replacement is exposed and synchronized.

The provider never edits a selected generation, silently rebases a request, imports another format, repairs missing authority, waits for locks, or claims target-filesystem/state atomicity.

## Placement

The generation-v3 codec and filesystem mechanism stay together because the codec is private to this exact storage provider. State-owned publication evidence codecs remain in `libpkgstate`; foreign source/build/plan/apply translations remain in their independent adapters.
