# Integration

Construct the provider only after selecting the exact canonical store path and `state_target_binding`. Pass it through the abstract `pkgstate::canonical_store` surface. Application mutation leases and installed-state publication locks are distinct domains; orchestration owns their lock order.

Release order is `libpkgstate` 3.0.0, then `libpkgstate-posix` 3.0.0. State adapters do not depend on this provider.
