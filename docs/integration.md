# Integration

Construct the provider only after selecting the exact canonical store path and `state_target_binding`. Pass it through the abstract `pkgstate::canonical_store` surface. Application mutation leases and installed-state publication locks are distinct domains; orchestration owns their lock order.

The provider calls the state-owned generation codec and implements only persistence mechanics. Current qualification uses the published `libpkgstate` 3.1.0 owner while the public compatibility floor remains 3.0.0. State adapters do not depend on this provider.
