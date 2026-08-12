# Native generation storage

The canonical backend stores one exact `state_target_binding`, one current
selector, and immutable complete generations.

```text
STORE/
  binding
  current
  generations/
    GENERATION/
      snapshot
```

`binding` prevents a pathname from being reused as authority for a different
managed target, root view, backend, state store, or publication domain.

The `binding` and `snapshot` bytes are emitted and validated by the
state-owned `<libpkgstate/generation_codec.h>` protocol. The snapshot is a
canonical length-delimited encoding of the complete native state, including
source-bound native build provenance,
complete installed object metadata, and hard-link topology retained by each
receipt. Every state-owned identity is recomputed while reading. External
identities are decoded as typed references and retained exactly.

Publication writes and synchronizes a new generation before atomically replacing
and synchronizing `current`. Authoritative regular-file opens are non-blocking
before type validation, so special-file corruption such as a FIFO is refused
rather than allowed to stall a reader. Readers reject selector traversal,
malformed data, unknown versions and enum values, duplicate normalized values,
identity mismatches, target mismatches, and trailing bytes.

Current version: 1.
Current receipt-visible identifier: `libpkgstate-generation-v1`.
Only canonical generation 1 is recognized. Every other version is rejected.
