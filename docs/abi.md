# ABI and metadata policy

The first independent release is 3.0.0 with SONAME 3. The exact public C++ export set is `abi/libpkgstate-posix.exports`; hidden visibility and a linker version script reject helper leakage.

Shared pkg-config consumers receive `libpkgstate >=3.0.0`. `libcrypto` is private and appears only in the static closure. Generation storage version, repository release, and C++ SONAME are independent version axes.
