# icu_capi [![crates.io](https://img.shields.io/crates/v/icu_capi)](https://crates.io/crates/icu_capi)

<!-- cargo-rdme start -->

This crate contains the `extern "C"` FFI for ICU4X, as well as the [Diplomat](https://github.com/rust-diplomat/diplomat)-generated
C and C++ headers. ICU4X is also available for JavaScript/TypeScript through [`npm`](https://www.npmjs.com/package/icu), and for
Dart through [`pub.dev`](https://pub.dev/packages/icu4x).
<p style='font-weight: bold; font-size: 24px;'> 🔗 See the <a target='_blank' href='https://icu4x.unicode.org/
'>ICU4X website</a> for FFI docs and examples</p>

This crate is `no_std`-compatible, but requires an allocator. If you wish to use it in `no_std` mode, you can either
enable the `looping_panic_handler` and `libc_alloc` Cargo features, or write a wrapper crate that defines an
allocator/panic handler.

<div class="stab unstable">
🚧 While the types in this crate are public, APIs from this crate are <em>not intended to be used from Rust</em> and as
such this crate may unpredictably change its Rust API across compatible semver versions.

The <code>extern "C"</code> APIs exposed by this crate, while not directly documented, are stable within the same major
semver version, as are the bindings in the <code>bindings</code> folder.
</div>

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
