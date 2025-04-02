# icu_capi [![crates.io](https://img.shields.io/crates/v/icu_capi)](https://crates.io/crates/icu_capi)

<!-- cargo-rdme start -->

This crate contains the source of truth for the [Diplomat](https://github.com/rust-diplomat/diplomat)-generated
FFI bindings. This generates the C, C++, JavaScript, and TypeScript bindings. This crate also contains the `extern "C"`
FFI for ICU4X.

While the types in this crate are public, APIs from this crate are *not intended to be used from Rust*
and as such this crate may unpredictably change its Rust API across compatible semver versions. The `extern "C"` APIs exposed
by this crate, while not directly documented, are stable within the same major semver version, as are the bindings exposed under
the `cpp/` and `js/` folders.

This crate may still be explored for documentation on docs.rs, and there are language-specific docs available as well.
C++, Dart, and TypeScript headers contain inline documentation, which is available pre-rendered: [C++], [TypeScript].

This crate is `no_std`-compatible. If you wish to use it in `no_std` mode, you must write a wrapper crate that defines an allocator
and a panic hook in order to compile as a C library.

More information on using ICU4X from C++ can be found in [our tutorial].

[our tutorial]: https://github.com/unicode-org/icu4x/blob/main/tutorials/cpp.md
[TypeScript]: https://unicode-org.github.io/icu4x/tsdoc
[C++]: https://unicode-org.github.io/icu4x/cppdoc

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
