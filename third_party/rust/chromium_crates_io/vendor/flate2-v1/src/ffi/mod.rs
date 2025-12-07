//! This module contains backend-specific code.

use crate::mem::{CompressError, DecompressError, FlushCompress, FlushDecompress, Status};
use crate::Compression;
use std::mem::MaybeUninit;

fn initialize_buffer(output: &mut [MaybeUninit<u8>]) -> &mut [u8] {
    // SAFETY: Here we zero-initialize the output and cast it to [u8]
    unsafe {
        output.as_mut_ptr().write_bytes(0, output.len());
        &mut *(output as *mut [MaybeUninit<u8>] as *mut [u8])
    }
}

/// Traits specifying the interface of the backends.
///
/// Sync + Send are added as a condition to ensure they are available
/// for the frontend.
pub trait Backend: Sync + Send {
    fn total_in(&self) -> u64;
    fn total_out(&self) -> u64;
}

pub trait InflateBackend: Backend {
    fn make(zlib_header: bool, window_bits: u8) -> Self;
    fn decompress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError>;
    fn decompress_uninit(
        &mut self,
        input: &[u8],
        output: &mut [MaybeUninit<u8>],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError> {
        self.decompress(input, initialize_buffer(output), flush)
    }
    fn reset(&mut self, zlib_header: bool);
}

pub trait DeflateBackend: Backend {
    fn make(level: Compression, zlib_header: bool, window_bits: u8) -> Self;
    fn compress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushCompress,
    ) -> Result<Status, CompressError>;
    fn compress_uninit(
        &mut self,
        input: &[u8],
        output: &mut [MaybeUninit<u8>],
        flush: FlushCompress,
    ) -> Result<Status, CompressError> {
        self.compress(input, initialize_buffer(output), flush)
    }
    fn reset(&mut self);
}

// Default to Rust implementation unless explicitly opted in to a different backend.
#[cfg(feature = "any_zlib")]
mod c;
#[cfg(feature = "any_zlib")]
pub use self::c::*;

#[cfg(all(not(feature = "any_zlib"), feature = "miniz_oxide"))]
mod rust;
#[cfg(all(not(feature = "any_zlib"), feature = "miniz_oxide"))]
pub use self::rust::*;

impl std::fmt::Debug for ErrorMessage {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        self.get().fmt(f)
    }
}
