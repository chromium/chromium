# Bincode
<img align="right" src="./logo.svg" />

[![CI](https://github.com/bincode-org/bincode/workflows/CI/badge.svg)](https://github.com/bincode-org/bincode/actions)
[![](https://img.shields.io/crates/v/bincode.svg)](https://crates.io/crates/bincode)
[![](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
<!-- [![](https://img.shields.io/badge/bincode-rustc_1.41.1+-lightgray.svg)](https://blog.rust-lang.org/2020/02/27/Rust-1.41.1.html) -->
[![Matrix](https://img.shields.io/matrix/bincode:matrix.org?label=Matrix%20Chat)](https://matrix.to/#/#bincode:matrix.org)

A compact encoder / decoder pair that uses a binary zero-fluff encoding scheme.
The size of the encoded object will be the same or smaller than the size that
the object takes up in memory in a running Rust program.

In addition to exposing two simple functions
(one that encodes to `Vec<u8>`, and one that decodes from `&[u8]`),
binary-encode exposes a Reader/Writer API that makes it work
perfectly with other stream-based APIs such as Rust files, network streams,
and the [flate2-rs](https://github.com/rust-lang/flate2-rs) compression
library.

## [API Documentation](https://docs.rs/bincode/)

## Bincode in the Wild

* [google/tarpc](https://github.com/google/tarpc): Bincode is used to serialize and deserialize networked RPC messages.
* [servo/webrender](https://github.com/servo/webrender): Bincode records WebRender API calls for record/replay-style graphics debugging.
* [servo/ipc-channel](https://github.com/servo/ipc-channel): IPC-Channel uses Bincode to send structs between processes using a channel-like API.
* [ajeetdsouza/zoxide](https://github.com/ajeetdsouza/zoxide): zoxide uses Bincode to store a database of directories and their access frequencies on disk.

## Example

```rust
use bincode::{config, Decode, Encode};

#[derive(Encode, Decode, PartialEq, Debug)]
struct Entity {
    x: f32,
    y: f32,
}

#[derive(Encode, Decode, PartialEq, Debug)]
struct World(Vec<Entity>);

fn main() {
    let config = config::standard();

    let world = World(vec![Entity { x: 0.0, y: 4.0 }, Entity { x: 10.0, y: 20.5 }]);

    let encoded: Vec<u8> = bincode::encode_to_vec(&world, config).unwrap();

    // The length of the vector is encoded as a varint u64, which in this case gets collapsed to a single byte
    // See the documentation on varint for more info for that.
    // The 4 floats are encoded in 4 bytes each.
    assert_eq!(encoded.len(), 1 + 4 * 4);

    let (decoded, len): (World, usize) = bincode::decode_from_slice(&encoded[..], config).unwrap();

    assert_eq!(world, decoded);
    assert_eq!(len, encoded.len()); // read all bytes
}
```

## Specification

Bincode's format is specified in [docs/spec.md](https://github.com/bincode-org/bincode/blob/trunk/docs/spec.md).

## FAQ

### Is Bincode suitable for storage?

The encoding format is stable, provided the same configuration is used.
This should ensure that later versions can still read data produced by a previous versions of the library if no major version change
has occurred.

Bincode 1 and 2 are completely compatible if the same configuration is used.

Bincode is invariant over byte-order, making an exchange between different
architectures possible. It is also rather space efficient, as it stores no
metadata like struct field names in the output format and writes long streams of
binary data without needing any potentially size-increasing encoding.

As a result, Bincode is suitable for storing data. Be aware that it does not
implement any sort of data versioning scheme or file headers, as these
features are outside the scope of this crate.

### Is Bincode suitable for untrusted inputs?

Bincode attempts to protect against hostile data. There is a maximum size
configuration available (`Configuration::with_limit`), but not enabled in the
default configuration. Enabling it causes pre-allocation size to be limited to
prevent against memory exhaustion attacks.

Deserializing any incoming data will not cause undefined behavior or memory
issues, assuming that the deserialization code for the struct is safe itself.

Bincode can be used for untrusted inputs in the sense that it will not create a
security issues in your application, provided the configuration is changed to enable a
maximum size limit. Malicious inputs will fail upon deserialization.

### What is Bincode's MSRV (minimum supported Rust version)?

Bincode 2.0 has an MSRV of 1.85.0. Any changes to the MSRV are considered a breaking change for semver purposes, except when certain features are enabled. Features affecting MSRV are documented in the crate root.

### Why does bincode not respect `#[repr(u8)]`?

Bincode will encode enum variants as a `u32`. If you're worried about storage size, we can recommend enabling `Configuration::with_variable_int_encoding()`. This option is enabled by default with the `standard` configuration. In this case enum variants will almost always be encoded as a `u8`.

Currently we have not found a compelling case to respect `#[repr(...)]`. You're most likely trying to interop with a format that is similar-but-not-quite-bincode. We only support our own protocol ([spec](https://github.com/bincode-org/bincode/blob/trunk/docs/spec.md)).

If you really want to use bincode to encode/decode a different protocol, consider implementing `Encode` and `Decode` yourself. `bincode-derive` will output the generated implementation in `target/generated/bincode/<name>_Encode.rs` and `target/generated/bincode/<name>_Decode.rs` which should get you started.
