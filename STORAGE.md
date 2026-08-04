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

The `snapshot` record is a canonical length-delimited binary encoding of the
complete native snapshot, including source-bound native build provenance,
complete installed object metadata, and hard-link topology retained by each
receipt. Every state-owned identity is recomputed while reading. External
identities are decoded as typed references and retained exactly.

Publication writes and synchronizes a new generation before atomically replacing
and synchronizing `current`. Readers reject selector traversal, malformed data,
unknown versions and enum values, duplicate normalized values, identity
mismatches, target mismatches, and trailing bytes.

Current version: 3.
Current receipt-visible identifier: `libpkgstate-generation-v3`.
Generation-v1 and generation-v2 bytes are not reinterpreted as version 3.
