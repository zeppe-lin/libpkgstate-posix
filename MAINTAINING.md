# Maintaining libpkgstate-posix

Before tagging:

1. build GCC and Clang shared and static closures separately;
2. run release and sanitizer configurations;
3. compare exact exports with `abi/libpkgstate-posix.exports` and verify SONAME 3;
4. verify shared metadata exposes only `libpkgstate` and static metadata adds `libcrypto`;
5. inspect `DT_NEEDED` for only `libpkgstate`, `libcrypto`, and platform runtime libraries;
6. run `unit`, `integration`, `header`, `contract`, and `tool` suites; the
   integration suite must exercise generation publication, stale refusal,
   locking, non-blocking special-file corruption refusal, durability outcomes,
   recovery refusal, and reopen;
7. compile every installed public header and an installed consumer;
8. lint manuals, run strict Doxygen, and stage-install documentation and the optional tool;
9. replay the mailbox and compare Git trees.

Generation-v1 bytes are the durable provider protocol. Any change requires explicit migration design and new compatibility qualification.
