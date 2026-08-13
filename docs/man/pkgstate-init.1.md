% PKGSTATE-INIT(1) libpkgstate-posix | Version 3.1.0

# NAME

pkgstate-init - initialize empty native installed state

# SYNOPSIS

**pkgstate-init** **--canonical-store** _path_ \
               **--managed-target** _identity_ \
               **--state-store** _identity_ \
               **--root-view** _identity_ \
               **--state-backend** _identity_ \
               **--publication-domain** _identity_

**pkgstate-init** {**-V** | **-h**}

# DESCRIPTION

**pkgstate-init** explicitly invokes the POSIX provider's canonical
open-or-initialize authority for one exact **state_target_binding**. It creates
and durably selects the canonical empty generation when initialization is
needed, or validates an already-bound empty store.

The command admits only an empty canonical snapshot. It does not erase packages,
repair foreign or incompatible state, import a historical package database,
construct installed-state truth from retained evidence, or publish a non-empty
generation.

A valid store that already contains installed packages is refused. A store bound
to different target identities is refused by the provider.

# OPTIONS

**-c**, **--canonical-store**=_path_
Initialize or validate the canonical generation store at _path_.

**--managed-target**=_identity_
Managed-target identity to bind.

**--state-store**=_identity_
Durable state-store identity to bind.

**--root-view**=_identity_
Logical target root-view identity to bind.

**--state-backend**=_identity_
State-backend identity to bind.

**--publication-domain**=_identity_
Publication and locking-domain identity to bind.

**-V**, **--version**
Print version and exit.

**-h**, **--help**
Print usage and exit.

# OUTPUT

On success the command reports the storage format, store coordinate, exact
target-binding identity, selected snapshot identity, ownership-inventory
identity, and **packages=0**.

# EXIT STATUS

0
An empty store with the exact supplied binding is authoritative.

1
Initialization or validation failed, the binding mismatched, or the canonical
store already contains installed packages.

2
Command-line usage is invalid.

# SEE ALSO

**pkgstate-check**(1), **pkgstate-generation**(5),
**pkgstate_canonical_generation_store**(3)
