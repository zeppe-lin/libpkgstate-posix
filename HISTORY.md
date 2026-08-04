# History

## 3.0.0

- Extract the canonical immutable-generation backend from `libpkgstate`.
- Preserve generation-v3 bytes, target binding, locking, publication, recovery refusal, and diagnostic behavior.
- Consume the canonical binding and snapshot codec from `libpkgstate` so one semantic owner defines generation-v3 bytes.
- Make `libpkgstate` the only public semantic dependency and OpenSSL `libcrypto` a private mechanism dependency.
- Preserve SONAME generation 3 and publish an exact reviewed ELF export manifest.
- Isolate the concrete provider API in `pkgstate::posix` and export only its public construction, observation, destruction, RTTI, and virtual-dispatch surface.
