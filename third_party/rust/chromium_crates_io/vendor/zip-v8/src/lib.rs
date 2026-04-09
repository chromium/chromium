#![doc = include_str!("../README.md")]
#![cfg_attr(docsrs, feature(doc_cfg))]
#![warn(clippy::all, rust_2018_idioms)]
#![deny(
    missing_docs,
    clippy::all,
    clippy::missing_panics_doc,
    clippy::cargo,
    clippy::panic,
    clippy::cast_lossless,
    clippy::decimal_literal_representation,
    clippy::must_use_candidate
)]
#![cfg_attr(not(test), deny(clippy::unwrap_used, clippy::expect_used))]
#![allow(unexpected_cfgs)] // Needed for cfg(fuzzing)
#![allow(clippy::multiple_crate_versions)] // https://github.com/rust-lang/rust-clippy/issues/16440
pub use crate::compression::{CompressionMethod, SUPPORTED_COMPRESSION_METHODS};
pub use crate::datetime::DateTime;
pub use crate::read::HasZipMetadata;
pub use crate::read::{ZipArchive, ZipReadOptions};
pub use crate::spec::{ZIP64_BYTES_THR, ZIP64_ENTRY_THR};
pub use crate::types::{AesMode, System};
pub use crate::write::ZipWriter;

#[cfg(feature = "aes-crypto")]
mod aes;
#[cfg(feature = "aes-crypto")]
pub use aes::AesSalt;
#[cfg(feature = "aes-crypto")]
mod aes_ctr;
mod compression;
mod cp437;
mod crc32;
mod datetime;
pub mod extra_fields;
mod path;
pub mod read;
pub mod result;
mod spec;
mod types;
pub mod write;
mod zipcrypto;
pub use extra_fields::ExtraField;
#[cfg(feature = "legacy-zip")]
mod legacy;

#[doc = "Unstable APIs\n\
\
All APIs accessible by importing this module are unstable; They may be changed in patch \
releases. You MUST use an exact version specifier in `Cargo.toml`, to indicate the version of this \
API you're using:\n\
\
```toml\n
[dependencies]\n
zip = \"="]
#[doc=env!("CARGO_PKG_VERSION")]
#[doc = "\"\n\
```"]
pub mod unstable;
