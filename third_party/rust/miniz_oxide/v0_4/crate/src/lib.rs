//! A pure rust replacement for the [miniz](https://github.com/richgel999/miniz)
//! DEFLATE/zlib encoder/decoder.
//! The plan for this crate is to be used as a back-end for the
//! [flate2](https://github.com/alexcrichton/flate2-rs) crate and eventually remove the
//! need to depend on a C library.
//!
//! # Usage
//! ## Simple compression/decompression:
//! ``` rust
//!
//! use miniz_oxide::inflate::decompress_to_vec;
//! use miniz_oxide::deflate::compress_to_vec;
//!
//! fn roundtrip(data: &[u8]) {
//!     let compressed = compress_to_vec(data, 6);
//!     let decompressed = decompress_to_vec(compressed.as_slice()).expect("Failed to decompress!");
//! #   let _ = decompressed;
//! }
//!
//! # roundtrip(b"Test_data test data lalalal blabla");
//!
//! ```

#![allow(warnings)]
#![forbid(unsafe_code)]
#![cfg_attr(any(has_alloc, feature = "rustc-dep-of-std"), no_std)]

// autocfg does not work when building in libstd, so manually enable this for that use case now.
#[cfg(any(has_alloc, feature = "rustc-dep-of-std"))]
extern crate alloc;
#[cfg(all(not(has_alloc), not(feature = "rustc-dep-of-std")))]
use std as alloc;

#[cfg(test)]
extern crate std;

pub mod deflate;
pub mod inflate;
mod shared;

pub use crate::shared::update_adler32 as mz_adler32_oxide;
pub use crate::shared::{MZ_ADLER32_INIT, MZ_DEFAULT_WINDOW_BITS};

/// A list of flush types.
///
/// See [http://www.bolet.org/~pornin/deflate-flush.html] for more in-depth info.
#[repr(i32)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum MZFlush {
    /// Don't force any flushing.
    /// Used when more input data is expected.
    None = 0,
    /// Zlib partial flush.
    /// Currently treated as `Sync`.
    Partial = 1,
    /// Finish compressing the currently buffered data, and output an empty raw block.
    /// Has no use in decompression.
    Sync = 2,
    /// Same as `Sync`, but resets the compression dictionary so that further compressed
    /// data does not depend on data compressed before the flush.
    /// Has no use in decompression.
    Full = 3,
    /// Attempt to flush the remaining data and end the stream.
    Finish = 4,
    /// Not implemented.
    Block = 5,
}

impl MZFlush {
    /// Create an MZFlush value from an integer value.
    ///
    /// Returns `MZError::Param` on invalid values.
    pub fn new(flush: i32) -> Result<Self, MZError> {
        match flush {
            0 => Ok(MZFlush::None),
            1 | 2 => Ok(MZFlush::Sync),
            3 => Ok(MZFlush::Full),
            4 => Ok(MZFlush::Finish),
            _ => Err(MZError::Param),
        }
    }
}

/// A list of miniz successful status codes.
#[repr(i32)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum MZStatus {
    Ok = 0,
    StreamEnd = 1,
    NeedDict = 2,
}

/// A list of miniz failed status codes.
#[repr(i32)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum MZError {
    ErrNo = -1,
    Stream = -2,
    Data = -3,
    Mem = -4,
    Buf = -5,
    Version = -6,
    Param = -10_000,
}

/// How compressed data is wrapped.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum DataFormat {
    /// Wrapped using the [zlib](http://www.zlib.org/rfc-zlib.html) format.
    Zlib,
    /// Raw DEFLATE.
    Raw,
}

impl DataFormat {
    pub(crate) fn from_window_bits(window_bits: i32) -> DataFormat {
        if window_bits > 0 {
            DataFormat::Zlib
        } else {
            DataFormat::Raw
        }
    }

    pub(crate) fn to_window_bits(self) -> i32 {
        match self {
            DataFormat::Zlib => shared::MZ_DEFAULT_WINDOW_BITS,
            DataFormat::Raw => -shared::MZ_DEFAULT_WINDOW_BITS,
        }
    }
}

/// `Result` alias for all miniz status codes both successful and failed.
pub type MZResult = Result<MZStatus, MZError>;

/// A structure containg the result of a call to the inflate or deflate streaming functions.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct StreamResult {
    /// The number of bytes consumed from the input slice.
    pub bytes_consumed: usize,
    /// The number of bytes written to the output slice.
    pub bytes_written: usize,
    /// The return status of the call.
    pub status: MZResult,
}

impl StreamResult {
    #[inline]
    pub(crate) fn error(error: MZError) -> StreamResult {
        StreamResult {
            bytes_consumed: 0,
            bytes_written: 0,
            status: Err(error),
        }
    }
}

impl core::convert::From<StreamResult> for MZResult {
    fn from(res: StreamResult) -> Self {
        res.status
    }
}

impl core::convert::From<&StreamResult> for MZResult {
    fn from(res: &StreamResult) -> Self {
        res.status
    }
}
