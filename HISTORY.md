# History

## 3.0.0

- Complete the documented public provider contract under Doxygen warnings-as-errors.
- Preserve the exact 12-symbol SONAME-3 provider ABI while keeping initialization and validation helpers private.

- Extract the canonical immutable-generation backend from `libpkgstate`.
- Preserve target binding, locking, publication, recovery refusal, and diagnostic behavior while advancing canonical storage to generation v4.
- Consume the canonical binding and snapshot codec from `libpkgstate` so one semantic owner defines generation-v4 bytes.
- Refuse generation-v3 stores rather than silently discarding the retired
  source-recipe identity.
- Make `libpkgstate` the only public semantic dependency and OpenSSL `libcrypto` a private mechanism dependency.
- Preserve SONAME generation 3 and publish an exact reviewed ELF export manifest.
- Isolate the concrete provider API in `pkgstate::posix` and export only its public construction, observation, destruction, RTTI, and virtual-dispatch surface.
