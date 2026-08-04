# Architecture

`libpkgstate-posix` is a mechanism provider for the abstract `libpkgstate::canonical_store` contract. It owns descriptor-anchored generation storage and no package-management policy. The dependency arrow points inward:

```text
libpkgstate-posix -> libpkgstate
```

`libpkgstate` does not depend on this provider. Controllers select and construct a provider at the composition root.
