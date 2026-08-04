# ABI and metadata policy

The first independent release is 3.0.0 with SONAME 3. The exact public C++ export set is `abi/libpkgstate-posix.exports`; hidden visibility and a linker version script reject helper leakage.

Shared pkg-config consumers receive `libpkgstate >=3.0.0`, including the canonical generation-v3 record protocol. `libcrypto` is private and appears only in the static closure. The provider exports no codec symbols. Generation record version, repository release, and C++ SONAME are independent version axes.
