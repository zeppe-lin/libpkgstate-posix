# History

## 3.0.0

- Retain the exact opened store-root descriptor across reads and publications so
  pathname rename/replacement cannot redirect an existing store handle.
- Qualify stale refusal and reject writable, multiply-linked, or symlinked
  selectors together with writable selected generations.
- Complete the documented public provider contract under Doxygen warnings-as-errors.
- Preserve the exact 12-symbol SONAME-3 export surface while keeping initialization and validation helpers private.

- Extract the canonical immutable-generation backend from `libpkgstate`.
- Preserve target binding, locking, publication, recovery refusal, and diagnostic behavior in the first canonical storage generation.
- Consume the canonical binding and snapshot codec from `libpkgstate` so one semantic owner defines generation-v1 bytes.
- Reject every unrecognized storage version rather than inventing compatibility with unpublished formats.
- Make `libpkgstate` the only public semantic dependency and OpenSSL `libcrypto` a private mechanism dependency.
- Preserve SONAME generation 3 and publish an exact reviewed ELF export manifest.
- Isolate the concrete provider API in `pkgstate::posix` and export only its public construction, observation, destruction, RTTI, and virtual-dispatch surface.
