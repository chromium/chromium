# Serialization Specification

_NOTE_: This specification is primarily defined in the context of Rust, but aims to be implementable across different programming languages.

## Definitions

- **Variant**: A specific constructor or case of an enum type.
- **Variant Payload**: The associated data of a specific enum variant.
- **Discriminant**: A unique identifier for an enum variant, typically represented as an integer.
- **Basic Types**: Primitive types that have a direct, well-defined binary representation.

## Endianness

By default, this serialization format uses little-endian byte order for basic numeric types. This means multi-byte values are encoded with their least significant byte first.

Endianness can be configured with the following methods, allowing for big-endian serialization when required:

- [`with_big_endian`](https://docs.rs/bincode/2/bincode/config/struct.Configuration.html#method.with_big_endian)
- [`with_little_endian`](https://docs.rs/bincode/2/bincode/config/struct.Configuration.html#method.with_little_endian)

### Byte Order Considerations

- Multi-byte values (integers, floats) are affected by endianness
- Single-byte values (u8, i8) are not affected
- Struct and collection serialization order is not changed by endianness

## Basic Types

### Boolean Encoding

- Encoded as a single byte
- `false` is represented by `0`
- `true` is represented by `1`
- During deserialization, values other than 0 and 1 will result in an error [`DecodeError::InvalidBooleanValue`](https://docs.rs/bincode/2/bincode/error/enum.DecodeError.html#variant.InvalidBooleanValue)

### Numeric Types

- Encoded based on the configured [IntEncoding](#intencoding)
- Signed integers use 2's complement representation
- Floating point types use IEEE 754-2008 standard
  - `f32`: 4 bytes (binary32)
  - `f64`: 8 bytes (binary64)

#### Floating Point Special Values

- Subnormal numbers are preserved
  - Also known as denormalized numbers
  - Maintain their exact bit representation
- `NaN` values are preserved
  - Both quiet and signaling `NaN` are kept as-is
  - Bit pattern of `NaN` is maintained exactly
- No normalization or transformation of special values occurs
- Serialization and deserialization do not alter the bit-level representation
- Consistent with IEEE 754-2008 standard for floating-point arithmetic

### Character Encoding

- `char` is encoded as a 32-bit unsigned integer representing its Unicode Scalar Value
- Valid Unicode Scalar Value range:
  - 0x0000 to 0xD7FF (Basic Multilingual Plane)
  - 0xE000 to 0x10FFFF (Supplementary Planes)
- Surrogate code points (0xD800 to 0xDFFF) are not valid
- Invalid Unicode characters can be acquired via unsafe code, this is handled as:
  - during serialization: data is written as-is
  - during deserialization: an error is raised [`DecodeError::InvalidCharEncoding`](https://docs.rs/bincode/2/bincode/error/enum.DecodeError.html#variant.InvalidCharEncoding)
- No additional metadata or encoding scheme beyond the raw code point value

All tuples have no additional bytes, and are encoded in their specified order, e.g.

```rust
let tuple = (u32::min_value(), i32::max_value()); // 8 bytes
let encoded = bincode::encode_to_vec(tuple, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    0,   0,   0,   0,  // 4 bytes for first type:  u32
    255, 255, 255, 127 // 4 bytes for second type: i32
]);
```

## IntEncoding

Bincode currently supports 2 different types of `IntEncoding`. With the default config, `VarintEncoding` is selected.

### VarintEncoding

Encoding an unsigned integer v (of any type excepting u8/i8) works as follows:

1. If `u < 251`, encode it as a single byte with that value.
1. If `251 <= u < 2**16`, encode it as a literal byte 251, followed by a u16 with value `u`.
1. If `2**16 <= u < 2**32`, encode it as a literal byte 252, followed by a u32 with value `u`.
1. If `2**32 <= u < 2**64`, encode it as a literal byte 253, followed by a u64 with value `u`.
1. If `2**64 <= u < 2**128`, encode it as a literal byte 254, followed by a u128 with value `u`.

`usize` is being encoded/decoded as a `u64` and `isize` is being encoded/decoded as a `i64`.

See the documentation of [VarintEncoding](https://docs.rs/bincode/2/bincode/config/struct.Configuration.html#method.with_variable_int_encoding) for more information.

### FixintEncoding

- Fixed size integers are encoded directly
- Enum discriminants are encoded as u32
- Lengths and usize are encoded as u64

See the documentation of [FixintEncoding](https://docs.rs/bincode/2/bincode/config/struct.Configuration.html#method.with_fixed_int_encoding) for more information.

## Enums

Enums are encoded with their variant first, followed by optionally the variant fields. The variant index is based on the `IntEncoding` during serialization.

Both named and unnamed fields are serialized with their values only, and therefore encode to the same value.

```rust
#[derive(bincode::Encode)]
pub enum SomeEnum {
    A,
    B(u32),
    C { value: u32 },
}

// SomeEnum::A
let encoded = bincode::encode_to_vec(SomeEnum::A, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    0, 0, 0, 0, // first variant, A
    // no extra bytes because A has no fields
]);

// SomeEnum::B(0)
let encoded = bincode::encode_to_vec(SomeEnum::B(0), bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    1, 0, 0, 0, // second variant, B
    0, 0, 0, 0  // B has 1 unnamed field, which is an u32, so 4 bytes
]);

// SomeEnum::C { value: 0u32 }
let encoded = bincode::encode_to_vec(SomeEnum::C { value: 0u32 }, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    2, 0, 0, 0, // third variant, C
    0, 0, 0, 0  // C has 1 named field which is a u32, so 4 bytes
]);
```

### Options

`Option<T>` is always serialized using a single byte for the discriminant, even in `Fixint` encoding (which normally uses a `u32` for discriminant).

```rust
let data: Option<u32> = Some(123);
let encoded = bincode::encode_to_vec(data, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    1, 123, 0, 0, 0  // the Some(..) tag is the leading 1
]);

let data: Option<u32> = None;
let encoded = bincode::encode_to_vec(data, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    0 // the None tag is simply 0
]);
```

# Collections

## General Collection Serialization

Collections are encoded with their length value first, followed by each entry of the collection. The length value is based on the configured `IntEncoding`.

### Serialization Considerations

- Length is always serialized first
- Entries are serialized in the order they are returned from the iterator implementation.
  - Iteration order depends on the collection type
    - Ordered collections (e.g., `Vec`): Iteration from lowest to highest index
    - Unordered collections (e.g., `HashMap`): Implementation-defined iteration order
- Duplicate keys are not checked in bincode, but may be resulting in an error when decoding a container from a list of pairs.

### Handling of Specific Collection Types

#### Linear Collections (`Vec`, Arrays, etc.)

- Serialized by iterating from lowest to highest index
- Length prefixed
- Each item serialized sequentially

```rust
let list = vec![0u8, 1u8, 2u8];
let encoded = bincode::encode_to_vec(list, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    3, 0, 0, 0, 0, 0, 0, 0, // length of 3u64
    0, // entry 0
    1, // entry 1
    2, // entry 2
]);
```

#### Key-Value Collections (`HashMap`, etc.)

- Serialized as a sequence of key-value pairs
- Iteration order is implementation-defined
- Each entry is a tuple of (key, value)

### Special Collection Considerations

- Bincode will serialize the entries based on the iterator order.
- Deserialization is deterministic but the collection implementation might not guarantee the same order as serialization.

**Note**: Fixed-length arrays do not have their length encoded. See [Arrays](#arrays) for details.

# String and &str

## Encoding Principles

- Strings are encoded as UTF-8 byte sequences
- No null terminator is added
- No Byte Order Mark (BOM) is written
- Unicode non-characters are preserved

### Encoding Details

- Length is encoded first using the configured `IntEncoding`
- Raw UTF-8 bytes follow the length
- Supports the full range of valid UTF-8 sequences
- `U+0000` and other code points can appear freely within the string

### Unicode Handling

- During serialization, the string is encoded as a sequence of the given bytes.
  - Rust strings are UTF-8 encoded by default, but this is not enforced by bincode
- No normalization or transformation of text
- If an invalid UTF-8 sequence is encountered during decoding, an [`DecodeError::Utf8`](https://docs.rs/bincode/2/bincode/error/enum.DecodeError.html#variant.Utf8) error is raised

```rust
let str = "Hello 🌍"; // Mixed ASCII and Unicode

let encoded = bincode::encode_to_vec(str, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    10, 0, 0, 0, 0, 0, 0, 0, // length of the string, 10 bytes
    b'H', b'e', b'l', b'l', b'o', b' ', 0xF0, 0x9F, 0x8C, 0x8D // UTF-8 encoded string
]);
```

### Comparison with Other Types

- Treated similarly to `Vec<u8>` in serialization
- See [Collections](#collections) for more information about length and entry encoding

# Arrays

Array length is never encoded.

Note that `&[T]` is encoded as a [Collection](#collections).

```rust
let arr: [u8; 5] = [10, 20, 30, 40, 50];
let encoded = bincode::encode_to_vec(arr, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    10, 20, 30, 40, 50, // the bytes
]);

```

This applies to any type `T` that implements `Encode`/`Decode`

```rust
#[derive(bincode::Encode)]
struct Foo {
    first: u8,
    second: u8
};

let arr: [Foo; 2] = [
    Foo {
        first: 10,
        second: 20,
    },
    Foo {
        first: 30,
        second: 40,
    },
];

let encoded = bincode::encode_to_vec(&arr, bincode::config::legacy()).unwrap();
assert_eq!(encoded.as_slice(), &[
    10, 20, // First Foo
    30, 40, // Second Foo
]);
```

## TupleEncoding

Tuple fields are serialized in first-to-last declaration order, with no additional metadata.

- No length prefix is added
- Fields are encoded sequentially
- No padding or alignment adjustments are made
- Order of serialization is deterministic and matches the tuple's declaration order

## StructEncoding

Struct fields are serialized in first-to-last declaration order, with no metadata representing field names.

- No length prefix is added
- Fields are encoded sequentially
- No padding or alignment adjustments are made
- Order of serialization is deterministic and matches the struct's field declaration order
- Both named and unnamed fields are serialized identically

## EnumEncoding

Enum variants are encoded with a discriminant followed by optional variant payload.

### Discriminant Allocation

- Discriminants are automatically assigned by the derive macro in declaration order
  - First variant starts at 0
  - Subsequent variants increment by 1
- Explicit discriminant indices are currently not supported
- Discriminant is always represented as a `u32` during serialization. See [Discriminant Representation](#discriminant-representation) for more details.
- Maintains the original enum variant semantics during encoding

### Variant Payload Encoding

- Tuple variants: Fields serialized in declaration order
- Struct variants: Fields serialized in declaration order
- Unit variants: No additional data encoded

### Discriminant Representation

- Always encoded as a `u32`
- Encoding method depends on the configured `IntEncoding`
  - `VarintEncoding`: Variable-length encoding
  - `FixintEncoding`: Fixed 4-byte representation

### Handling of Variant Payloads

- Payload is serialized immediately after the discriminant
- No additional metadata about field names or types
- Payload structure matches the variant's definition
