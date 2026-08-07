# ABI and metadata policy

The first independent release is 3.0.0 with SONAME 3. The exact public
`pkgstate::posix` C++ export set is `abi/libpkgstate-posix.exports`; hidden
visibility and a linker version script reject helper leakage. The 3.0.0 tag is
the first independent ABI and object-layout freeze; pre-tag extraction heads are
qualification inputs, not compatibility promises.

Shared pkg-config consumers receive `libpkgstate >=3.0.0`, including the canonical generation-v1 record protocol. `libcrypto` is private and appears only in the static closure. The provider exports no codec symbols. Generation record version, repository release, and C++ SONAME are independent version axes.
