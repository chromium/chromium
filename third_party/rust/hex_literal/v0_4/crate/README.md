# [RustCrypto]: hex-literal

[![Crate][crate-image]][crate-link]
[![Docs][docs-image]][docs-link]
![Apache 2.0/MIT Licensed][license-image]
![MSRV][rustc-image]
[![Build Status][build-image]][build-link]

This crate provides the `hex!` macro for converting hexadecimal string literals to a byte array at compile time.

It accepts the following characters in the input string:

- `'0'...'9'`, `'a'...'f'`, `'A'...'F'` — hex characters which will be used in construction of the output byte array
- `' '`, `'\r'`, `'\n'`, `'\t'` — formatting characters which will be ignored

# Examples
```rust
use hex_literal::hex;

// The macro can be used in const contexts
const DATA: [u8; 4] = hex!("01020304");
assert_eq!(DATA, [1, 2, 3, 4]);

// Both upper and lower hex values are supported
assert_eq!(hex!("a1 b2 c3 d4"), [0xA1, 0xB2, 0xC3, 0xD4]);
assert_eq!(hex!("E5 E6 90 92"), [0xE5, 0xE6, 0x90, 0x92]);
assert_eq!(hex!("0a0B 0C0d"), [10, 11, 12, 13]);

// Multi-line literals
let bytes1 = hex!("
    00010203 04050607
    08090a0b 0c0d0e0f
");
assert_eq!(
    bytes1,
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
);

// It's possible to use several literals
// (results will be concatenated)
let bytes2 = hex!(
    "00010203 04050607" // first half
    "08090a0b 0c0d0e0f" // second half
);
assert_eq!(bytes1, bytes2);
```

Using an unsupported character inside literals will result in a compilation error:
```rust,compile_fail
hex_literal::hex!("АА"); // Cyrillic "А"
hex_literal::hex!("11　22"); // Japanese space
```

Сomments inside literals are not supported:
```rust,compile_fail
hex_literal::hex!("0123 // foo");
```

Each literal must contain an even number of hex characters:
```rust,compile_fail
hex_literal::hex!(
    "01234"
    "567"
);
```

## Minimum Supported Rust Version

Rust **1.57** or newer.

In the future, we reserve the right to change MSRV (i.e. MSRV is out-of-scope for this crate's SemVer guarantees), however when we do it will be accompanied by a minor version bump.

## License

Licensed under either of:

* [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
* [MIT license](http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.

[//]: # (badges)

[crate-image]: https://img.shields.io/crates/v/hex-literal.svg
[crate-link]: https://crates.io/crates/hex-literal
[docs-image]: https://docs.rs/hex-literal/badge.svg
[docs-link]: https://docs.rs/hex-literal/
[license-image]: https://img.shields.io/badge/license-Apache2.0/MIT-blue.svg
[rustc-image]: https://img.shields.io/badge/rustc-1.57+-blue.svg
[build-image]: https://github.com/RustCrypto/utils/actions/workflows/hex-literal.yml/badge.svg
[build-link]: https://github.com/RustCrypto/utils/actions/workflows/hex-literal.yml

[//]: # (general links)

[RustCrypto]: https://github.com/RustCrypto
