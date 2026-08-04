# Architecture

`libpkgstate-posix` is a mechanism provider for the abstract
`pkgstate::canonical_store` contract. Its concrete API is isolated in the
`pkgstate::posix` namespace. It owns descriptor-anchored generation storage and
no package-management policy or state-record codec. The dependency arrow points
inward:

```text
libpkgstate-posix -> libpkgstate
```

`libpkgstate` does not depend on this provider. The provider consumes the state-owned generation codec; controllers select and construct the provider at the composition root.
