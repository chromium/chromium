#![no_std]
#![warn(missing_docs, unused_lifetimes)]
#![cfg_attr(docsrs, feature(doc_cfg))]

//! Bincode is a crate for encoding and decoding using a tiny binary
//! serialization strategy.  Using it, you can easily go from having
//! an object in memory, quickly serialize it to bytes, and then
//! deserialize it back just as fast!
//!
//! If you're coming from bincode 1, check out our [migration guide](migration_guide/index.html)
//!
//! # Serde
//!
//! Starting from bincode 2, serde is now an optional dependency. If you want to use serde, please enable the `serde` feature. See [Features](#features) for more information.
//!
//! # Features
//!
//! |Name  |Default?|Affects MSRV?|Supported types for Encode/Decode|Enabled methods                                                  |Other|
//! |------|--------|-------------|-----------------------------------------|-----------------------------------------------------------------|-----|
//! |std   | Yes    | No          |`HashMap` and `HashSet`|`decode_from_std_read` and `encode_into_std_write`|
//! |alloc | Yes    | No          |All common containers in alloc, like `Vec`, `String`, `Box`|`encode_to_vec`|
//! |atomic| Yes    | No          |All `Atomic*` integer types, e.g. `AtomicUsize`, and `AtomicBool`||
//! |derive| Yes    | No          |||Enables the `BorrowDecode`, `Decode` and `Encode` derive macros|
//! |serde | No     | Yes (MSRV reliant on serde)|`Compat` and `BorrowCompat`, which will work for all types that implement serde's traits|serde-specific encode/decode functions in the [serde] module|Note: There are several [known issues](serde/index.html#known-issues) when using serde and bincode|
//!
//! # Which functions to use
//!
//! Bincode has a couple of pairs of functions that are used in different situations.
//!
//! |Situation|Encode|Decode|
//! |---|---|---
//! |You're working with [`fs::File`] or [`net::TcpStream`]|[`encode_into_std_write`]|[`decode_from_std_read`]|
//! |you're working with in-memory buffers|[`encode_to_vec`]|[`decode_from_slice`]|
//! |You want to use a custom [Reader] and [Writer]|[`encode_into_writer`]|[`decode_from_reader`]|
//! |You're working with pre-allocated buffers or on embedded targets|[`encode_into_slice`]|[`decode_from_slice`]|
//!
//! **Note:** If you're using `serde`, use `bincode::serde::...` instead of `bincode::...`
//!
//! # Example
//!
//! ```rust
//! let mut slice = [0u8; 100];
//!
//! // You can encode any type that implements `Encode`.
//! // You can automatically implement this trait on custom types with the `derive` feature.
//! let input = (
//!     0u8,
//!     10u32,
//!     10000i128,
//!     'a',
//!     [0u8, 1u8, 2u8, 3u8]
//! );
//!
//! let length = bincode::encode_into_slice(
//!     input,
//!     &mut slice,
//!     bincode::config::standard()
//! ).unwrap();
//!
//! let slice = &slice[..length];
//! println!("Bytes written: {:?}", slice);
//!
//! // Decoding works the same as encoding.
//! // The trait used is `Decode`, and can also be automatically implemented with the `derive` feature.
//! let decoded: (u8, u32, i128, char, [u8; 4]) = bincode::decode_from_slice(slice, bincode::config::standard()).unwrap().0;
//!
//! assert_eq!(decoded, input);
//! ```
//!
//! [`fs::File`]: std::fs::File
//! [`net::TcpStream`]: std::net::TcpStream
//!

#![doc(html_root_url = "https://docs.rs/bincode/2.0.1")]
#![crate_name = "bincode"]
#![crate_type = "rlib"]

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(any(feature = "std", test))]
extern crate std;

mod atomic;
mod features;
pub(crate) mod utils;
pub(crate) mod varint;

use de::{read::Reader, Decoder};
use enc::write::Writer;

#[cfg(any(
    feature = "alloc",
    feature = "std",
    feature = "derive",
    feature = "serde"
))]
pub use features::*;

pub mod config;
#[macro_use]
pub mod de;
pub mod enc;
pub mod error;

pub use de::{BorrowDecode, Decode};
pub use enc::Encode;

use config::Config;

/// Encode the given value into the given slice. Returns the amount of bytes that have been written.
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn encode_into_slice<E: enc::Encode, C: Config>(
    val: E,
    dst: &mut [u8],
    config: C,
) -> Result<usize, error::EncodeError> {
    let writer = enc::write::SliceWriter::new(dst);
    let mut encoder = enc::EncoderImpl::<_, C>::new(writer, config);
    val.encode(&mut encoder)?;
    Ok(encoder.into_writer().bytes_written())
}

/// Encode the given value into a custom [Writer].
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn encode_into_writer<E: enc::Encode, W: Writer, C: Config>(
    val: E,
    writer: W,
    config: C,
) -> Result<(), error::EncodeError> {
    let mut encoder = enc::EncoderImpl::<_, C>::new(writer, config);
    val.encode(&mut encoder)?;
    Ok(())
}

/// Attempt to decode a given type `D` from the given slice. Returns the decoded output and the amount of bytes read.
///
/// Note that this does not work with borrowed types like `&str` or `&[u8]`. For that use [borrow_decode_from_slice].
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn decode_from_slice<D: de::Decode<()>, C: Config>(
    src: &[u8],
    config: C,
) -> Result<(D, usize), error::DecodeError> {
    decode_from_slice_with_context(src, config, ())
}

/// Attempt to decode a given type `D` from the given slice with `Context`. Returns the decoded output and the amount of bytes read.
///
/// Note that this does not work with borrowed types like `&str` or `&[u8]`. For that use [borrow_decode_from_slice].
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn decode_from_slice_with_context<Context, D: de::Decode<Context>, C: Config>(
    src: &[u8],
    config: C,
    context: Context,
) -> Result<(D, usize), error::DecodeError> {
    let reader = de::read::SliceReader::new(src);
    let mut decoder = de::DecoderImpl::<_, C, Context>::new(reader, config, context);
    let result = D::decode(&mut decoder)?;
    let bytes_read = src.len() - decoder.reader().slice.len();
    Ok((result, bytes_read))
}

/// Attempt to decode a given type `D` from the given slice. Returns the decoded output and the amount of bytes read.
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn borrow_decode_from_slice<'a, D: de::BorrowDecode<'a, ()>, C: Config>(
    src: &'a [u8],
    config: C,
) -> Result<(D, usize), error::DecodeError> {
    borrow_decode_from_slice_with_context(src, config, ())
}

/// Attempt to decode a given type `D` from the given slice with `Context`. Returns the decoded output and the amount of bytes read.
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn borrow_decode_from_slice_with_context<
    'a,
    Context,
    D: de::BorrowDecode<'a, Context>,
    C: Config,
>(
    src: &'a [u8],
    config: C,
    context: Context,
) -> Result<(D, usize), error::DecodeError> {
    let reader = de::read::SliceReader::new(src);
    let mut decoder = de::DecoderImpl::<_, C, Context>::new(reader, config, context);
    let result = D::borrow_decode(&mut decoder)?;
    let bytes_read = src.len() - decoder.reader().slice.len();
    Ok((result, bytes_read))
}

/// Attempt to decode a given type `D` from the given [Reader].
///
/// See the [config] module for more information on configurations.
///
/// [config]: config/index.html
pub fn decode_from_reader<D: de::Decode<()>, R: Reader, C: Config>(
    reader: R,
    config: C,
) -> Result<D, error::DecodeError> {
    let mut decoder = de::DecoderImpl::<_, C, ()>::new(reader, config, ());
    D::decode(&mut decoder)
}

// TODO: Currently our doctests fail when trying to include the specs because the specs depend on `derive` and `alloc`.
// But we want to have the specs in the docs always
#[cfg(all(feature = "alloc", feature = "derive", doc))]
pub mod spec {
    #![doc = include_str!("../docs/spec.md")]
}

#[cfg(doc)]
pub mod migration_guide {
    #![doc = include_str!("../docs/migration_guide.md")]
}

// Test the examples in readme.md
#[cfg(all(feature = "alloc", feature = "derive", doctest))]
mod readme {
    #![doc = include_str!("../readme.md")]
}
