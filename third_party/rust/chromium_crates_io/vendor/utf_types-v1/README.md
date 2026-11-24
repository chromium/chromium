# utf_types

[![crates.io](https://img.shields.io/crates/v/utf_types.svg)](https://crates.io/crates/utf_types)
[![docs.rs](https://docs.rs/utf_types/badge.svg)](https://docs.rs/utf_types/)

Types for UTF-8 and UTF-16 code units and code unit sequences

## Licensing

TL;DR: `Apache-2.0 OR MIT`

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/utf_types/blob/main/COPYRIGHT).

## Documentation

Generated [API documentation](https://docs.rs/utf_types/) is available
online.

## Roadmap

Currently, the types in this crate cover the needs of `utf8_iter` and
of `utf16_iter` separately. In the future, for `encoding_rs`, this crate is
expected to gain 16-bit types corresponding to the value spaces of 1, 2, and
3-byte UTF-8 sequences as well as conversions between the UTF-8 and UTF-16
types.

The crate is missing constructors from some obvious source types, such as
`char` and arrays/slices of `u8`. Those can be added as use cases arise.

## Release Notes

### 1.0.0

* Initial version
