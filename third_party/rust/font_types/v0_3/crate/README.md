# font-types

This crate contains definitions of the basic types in the [OpenType
spec][opentype], as well as informal but useful types (such as a distinct type
for a glyph ID) and traits for encoding and decoding these types as big-endian
bytes.

These types are intended to be general purpose, and useable by other Rust
projects that work with font data.

[opentype]: https://docs.microsoft.com/en-us/typography/opentype/

## Safety

Unsafe code is forbidden by a `#![forbid(unsafe_code)]` attribute in the root
of the library.
