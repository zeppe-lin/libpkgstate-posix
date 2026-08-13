# Qualification details

Provider qualification is layered rather than concentrated in one filesystem
scenario. Unit qualification owns only provider constants and C++ surface
shape. Integration qualification uses the real `pkgstate::canonical_store`
sequence and the state-owned generation codec; it does not duplicate state
semantics with mocks.

The mechanism suite proves that a named store cannot be redirected by
renaming/replacing its configured pathname after opening, final symlinks are
refused, lock acquisition never waits, current selection names a complete
immutable generation, writable or multiply-linked metadata is refused,
selector and generation redirection is refused, malformed/corrupt selected
state is rejected, unselected crash leftovers are not authoritative, and a
bound incomplete store is never silently reconstructed.

Publication qualification proves generation-before-selector ordering, exact
reuse of an already valid immutable generation, stale refusal without durable
mutation, evidence binding to the stored generation bytes, and the concrete
backend outcome boundary. A test-local `fsync()` interposer injects failures
without changing production code: pre-selection failure yields
`failed_before_publication`, failed selector-directory durability followed by a
successful reread yields `published_durability_unconfirmed`, and failed
post-selection durability with an unavailable authoritative reread yields
`indeterminate`.

The CLI suite treats both reference clients as ordinary callers.
`pkgstate-init` invokes the provider's open-or-initialize constructor, requires
the resulting authoritative snapshot to be empty, proves stable replay for the
same exact binding, and refuses mismatched or populated stores without erasing
state. `pkgstate-check` opens only through `open_existing()`, reports a state
containing shared ownership and every installation-reason class, and proves
diagnostics neither initialize nor mutate storage.
