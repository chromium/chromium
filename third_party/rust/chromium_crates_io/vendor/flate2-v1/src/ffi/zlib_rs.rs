//! Implementation for `zlib_rs` rust backend.
//!
//! Every backend must provide two types:
//!
//! - `Deflate` for compression, implements the `Backend` and `DeflateBackend` trait
//! - `Inflate` for decompression, implements the `Backend` and `InflateBackend` trait
//!
//! Additionally the backend provides a number of constants, and a `ErrorMessage` type.
//!
//! ## Allocation
//!
//! The (de)compression state is not boxed. The C implementations require that the z_stream is
//! pinned in memory (has a fixed address), because their z_stream is self-referential. The most
//! convenient way in rust to guarantee a stable address is to `Box` the data, but it does add an
//! additional allocation.
//!
//! With zlib_rs the state is not self-referential and hence no boxing is needed. The `new` methods
//! internally do allocate space for the (de)compression state.

use core::fmt;

use ::zlib_rs::{DeflateFlush, InflateError, InflateFlush};

pub const MZ_NO_FLUSH: isize = DeflateFlush::NoFlush as isize;
pub const MZ_PARTIAL_FLUSH: isize = DeflateFlush::PartialFlush as isize;
pub const MZ_SYNC_FLUSH: isize = DeflateFlush::SyncFlush as isize;
pub const MZ_FULL_FLUSH: isize = DeflateFlush::FullFlush as isize;
pub const MZ_FINISH: isize = DeflateFlush::Finish as isize;

pub const MZ_DEFAULT_WINDOW_BITS: core::ffi::c_int = 15;

use super::*;
use crate::mem::{compress_failed, decompress_failed};

impl From<::zlib_rs::Status> for crate::mem::Status {
    fn from(value: ::zlib_rs::Status) -> Self {
        match value {
            ::zlib_rs::Status::Ok => crate::mem::Status::Ok,
            ::zlib_rs::Status::BufError => crate::mem::Status::BufError,
            ::zlib_rs::Status::StreamEnd => crate::mem::Status::StreamEnd,
        }
    }
}

#[derive(Clone, Default)]
pub struct ErrorMessage(Option<&'static str>);

impl ErrorMessage {
    pub fn get(&self) -> Option<&str> {
        self.0
    }
}

pub struct Inflate {
    pub(crate) inner: ::zlib_rs::Inflate,
    window_bits: u8,
    // NOTE: these counts do not count the dictionary.
    total_in: u64,
    total_out: u64,
}

impl fmt::Debug for Inflate {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(
            f,
            "zlib_rs inflate internal state. total_in: {}, total_out: {}",
            self.total_in(),
            self.total_out(),
        )
    }
}

impl From<FlushDecompress> for InflateFlush {
    fn from(value: FlushDecompress) -> Self {
        match value {
            FlushDecompress::None => Self::NoFlush,
            FlushDecompress::Sync => Self::SyncFlush,
            FlushDecompress::Finish => Self::Finish,
        }
    }
}

impl InflateBackend for Inflate {
    fn make(zlib_header: bool, window_bits: u8) -> Self {
        Inflate {
            inner: ::zlib_rs::Inflate::new(zlib_header, window_bits),
            window_bits,
            total_in: 0,
            total_out: 0,
        }
    }

    fn decompress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError> {
        let flush = flush.into();

        let total_in_start = self.inner.total_in();
        let total_out_start = self.inner.total_out();

        let result = self.inner.decompress(input, output, flush);

        self.total_in += self.inner.total_in() - total_in_start;
        self.total_out += self.inner.total_out() - total_out_start;

        match result {
            Ok(status) => Ok(status.into()),
            Err(InflateError::NeedDict { dict_id }) => crate::mem::decompress_need_dict(dict_id),
            Err(_) => self.decompress_error(),
        }
    }

    fn decompress_uninit(
        &mut self,
        input: &[u8],
        output: &mut [MaybeUninit<u8>],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError> {
        let flush = flush.into();

        let total_in_start = self.inner.total_in();
        let total_out_start = self.inner.total_out();

        let result = self.inner.decompress_uninit(input, output, flush);

        self.total_in += self.inner.total_in() - total_in_start;
        self.total_out += self.inner.total_out() - total_out_start;

        match result {
            Ok(status) => Ok(status.into()),
            Err(InflateError::NeedDict { dict_id }) => crate::mem::decompress_need_dict(dict_id),
            Err(_) => self.decompress_error(),
        }
    }

    fn reset(&mut self, zlib_header: bool) {
        self.total_in = 0;
        self.total_out = 0;
        if self.window_bits == MZ_DEFAULT_WINDOW_BITS as u8 {
            self.inner.reset(zlib_header);
        } else {
            // zlib_rs::Inflate::reset always restores the default
            // configuration, which would discard the window size and gzip
            // mode selected at construction, so re-create the state instead.
            // Keep only the base-2 logarithm of the window size when
            // resetting a gzip decoder (window_bits + 16) to a raw stream.
            let window_bits = if zlib_header {
                self.window_bits
            } else {
                self.window_bits & 15
            };
            self.inner = ::zlib_rs::Inflate::new(zlib_header, window_bits);
        }
    }
}

impl Backend for Inflate {
    #[inline]
    fn total_in(&self) -> u64 {
        self.total_in
    }

    #[inline]
    fn total_out(&self) -> u64 {
        self.total_out
    }
}

impl Inflate {
    fn decompress_error<T>(&self) -> Result<T, DecompressError> {
        decompress_failed(ErrorMessage(self.inner.error_message()))
    }

    pub fn set_dictionary(&mut self, dictionary: &[u8]) -> Result<u32, DecompressError> {
        match self.inner.set_dictionary(dictionary) {
            Ok(v) => Ok(v),
            Err(_) => self.decompress_error(),
        }
    }
}

pub struct Deflate {
    pub(crate) inner: ::zlib_rs::Deflate,
    // NOTE: these counts do not count the dictionary.
    total_in: u64,
    total_out: u64,
}

impl fmt::Debug for Deflate {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(
            f,
            "zlib_rs deflate internal state. total_in: {}, total_out: {}",
            self.total_in(),
            self.total_out(),
        )
    }
}

impl From<FlushCompress> for DeflateFlush {
    fn from(value: FlushCompress) -> Self {
        match value {
            FlushCompress::None => Self::NoFlush,
            FlushCompress::Partial => Self::PartialFlush,
            FlushCompress::Sync => Self::SyncFlush,
            FlushCompress::Full => Self::FullFlush,
            FlushCompress::Finish => Self::Finish,
        }
    }
}

impl DeflateBackend for Deflate {
    fn make(level: Compression, zlib_header: bool, window_bits: u8) -> Self {
        // Check in case the integer value changes at some point.
        debug_assert!(level.level() <= 9);

        Deflate {
            inner: ::zlib_rs::Deflate::new(level.level() as i32, zlib_header, window_bits),
            total_in: 0,
            total_out: 0,
        }
    }

    fn compress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushCompress,
    ) -> Result<Status, CompressError> {
        let flush = flush.into();

        let total_in_start = self.inner.total_in();
        let total_out_start = self.inner.total_out();

        let result = self.inner.compress(input, output, flush);

        self.total_in += self.inner.total_in() - total_in_start;
        self.total_out += self.inner.total_out() - total_out_start;

        match result {
            Ok(status) => Ok(status.into()),
            Err(_) => self.compress_error(),
        }
    }

    fn compress_uninit(
        &mut self,
        input: &[u8],
        output: &mut [MaybeUninit<u8>],
        flush: FlushCompress,
    ) -> Result<Status, CompressError> {
        let flush = flush.into();

        let total_in_start = self.inner.total_in();
        let total_out_start = self.inner.total_out();

        let result = self.inner.compress_uninit(input, output, flush);

        self.total_in += self.inner.total_in() - total_in_start;
        self.total_out += self.inner.total_out() - total_out_start;

        match result {
            Ok(status) => Ok(status.into()),
            Err(_) => self.compress_error(),
        }
    }

    fn reset(&mut self) {
        self.total_in = 0;
        self.total_out = 0;
        self.inner.reset();
    }
}

impl Backend for Deflate {
    #[inline]
    fn total_in(&self) -> u64 {
        self.total_in
    }

    #[inline]
    fn total_out(&self) -> u64 {
        self.total_out
    }
}

impl Deflate {
    fn compress_error<T>(&self) -> Result<T, CompressError> {
        compress_failed(ErrorMessage(self.inner.error_message()))
    }

    pub fn set_dictionary(&mut self, dictionary: &[u8]) -> Result<u32, CompressError> {
        match self.inner.set_dictionary(dictionary) {
            Ok(v) => Ok(v),
            Err(_) => self.compress_error(),
        }
    }

    pub fn set_level(&mut self, level: Compression) -> Result<(), CompressError> {
        use ::zlib_rs::Status;

        match self.inner.set_level(level.level() as i32) {
            Ok(status) => match status {
                Status::Ok => Ok(()),
                Status::BufError => compress_failed(ErrorMessage(Some("insufficient space"))),
                Status::StreamEnd => {
                    unreachable!("zlib-rs is known to never return the StreamEnd status")
                }
            },
            Err(_) => self.compress_error(),
        }
    }
}
