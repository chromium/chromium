//! This module contains functionality for decompression.

use crate::alloc::boxed::Box;
use crate::alloc::vec;
use crate::alloc::vec::Vec;
use ::core::cmp::min;
use ::core::usize;

pub mod core;
mod output_buffer;
pub mod stream;
use self::core::*;

const TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS: i32 = -4;
const TINFL_STATUS_BAD_PARAM: i32 = -3;
const TINFL_STATUS_ADLER32_MISMATCH: i32 = -2;
const TINFL_STATUS_FAILED: i32 = -1;
const TINFL_STATUS_DONE: i32 = 0;
const TINFL_STATUS_NEEDS_MORE_INPUT: i32 = 1;
const TINFL_STATUS_HAS_MORE_OUTPUT: i32 = 2;

/// Return status codes.
#[repr(i8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum TINFLStatus {
    /// More input data was expected, but the caller indicated that there was more data, so the
    /// input stream is likely truncated.
    FailedCannotMakeProgress = TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS as i8,
    /// One or more of the input parameters were invalid.
    BadParam = TINFL_STATUS_BAD_PARAM as i8,
    /// The decompression went fine, but the adler32 checksum did not match the one
    /// provided in the header.
    Adler32Mismatch = TINFL_STATUS_ADLER32_MISMATCH as i8,
    /// Failed to decompress due to invalid data.
    Failed = TINFL_STATUS_FAILED as i8,
    /// Finished decomression without issues.
    Done = TINFL_STATUS_DONE as i8,
    /// The decompressor needs more input data to continue decompressing.
    NeedsMoreInput = TINFL_STATUS_NEEDS_MORE_INPUT as i8,
    /// There is still pending data that didn't fit in the output buffer.
    HasMoreOutput = TINFL_STATUS_HAS_MORE_OUTPUT as i8,
}

impl TINFLStatus {
    pub fn from_i32(value: i32) -> Option<TINFLStatus> {
        use self::TINFLStatus::*;
        match value {
            TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS => Some(FailedCannotMakeProgress),
            TINFL_STATUS_BAD_PARAM => Some(BadParam),
            TINFL_STATUS_ADLER32_MISMATCH => Some(Adler32Mismatch),
            TINFL_STATUS_FAILED => Some(Failed),
            TINFL_STATUS_DONE => Some(Done),
            TINFL_STATUS_NEEDS_MORE_INPUT => Some(NeedsMoreInput),
            TINFL_STATUS_HAS_MORE_OUTPUT => Some(HasMoreOutput),
            _ => None,
        }
    }
}

/// Decompress the deflate-encoded data in `input` to a vector.
///
/// Returns a status and an integer representing where the decompressor failed on failure.
#[inline]
pub fn decompress_to_vec(input: &[u8]) -> Result<Vec<u8>, TINFLStatus> {
    decompress_to_vec_inner(input, 0, usize::max_value())
}

/// Decompress the deflate-encoded data (with a zlib wrapper) in `input` to a vector.
///
/// Returns a status and an integer representing where the decompressor failed on failure.
#[inline]
pub fn decompress_to_vec_zlib(input: &[u8]) -> Result<Vec<u8>, TINFLStatus> {
    decompress_to_vec_inner(
        input,
        inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER,
        usize::max_value(),
    )
}

/// Decompress the deflate-encoded data in `input` to a vector.
/// The vector is grown to at most `max_size` bytes; if the data does not fit in that size,
/// `TINFLStatus::HasMoreOutput` error is returned.
///
/// Returns a status and an integer representing where the decompressor failed on failure.
#[inline]
pub fn decompress_to_vec_with_limit(input: &[u8], max_size: usize) -> Result<Vec<u8>, TINFLStatus> {
    decompress_to_vec_inner(input, 0, max_size)
}

/// Decompress the deflate-encoded data (with a zlib wrapper) in `input` to a vector.
/// The vector is grown to at most `max_size` bytes; if the data does not fit in that size,
/// `TINFLStatus::HasMoreOutput` error is returned.
///
/// Returns a status and an integer representing where the decompressor failed on failure.
#[inline]
pub fn decompress_to_vec_zlib_with_limit(
    input: &[u8],
    max_size: usize,
) -> Result<Vec<u8>, TINFLStatus> {
    decompress_to_vec_inner(input, inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER, max_size)
}

fn decompress_to_vec_inner(
    input: &[u8],
    flags: u32,
    max_output_size: usize,
) -> Result<Vec<u8>, TINFLStatus> {
    let flags = flags | inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
    let mut ret: Vec<u8> = vec![0; min(input.len().saturating_mul(2), max_output_size)];

    let mut decomp = Box::<DecompressorOxide>::default();

    let mut in_pos = 0;
    let mut out_pos = 0;
    loop {
        // Wrap the whole output slice so we know we have enough of the
        // decompressed data for matches.
        let (status, in_consumed, out_consumed) =
            decompress(&mut decomp, &input[in_pos..], &mut ret, out_pos, flags);
        in_pos += in_consumed;
        out_pos += out_consumed;

        match status {
            TINFLStatus::Done => {
                ret.truncate(out_pos);
                return Ok(ret);
            }

            TINFLStatus::HasMoreOutput => {
                // We need more space, so check if we can resize the buffer and do it.
                let new_len = ret
                    .len()
                    .checked_add(out_pos)
                    .ok_or(TINFLStatus::HasMoreOutput)?;
                if new_len > max_output_size {
                    return Err(TINFLStatus::HasMoreOutput);
                };
                ret.resize(new_len, 0);
            }

            _ => return Err(status),
        }
    }
}

#[cfg(test)]
mod test {
    use super::TINFLStatus;
    use super::{decompress_to_vec_zlib, decompress_to_vec_zlib_with_limit};
    const encoded: [u8; 20] = [
        120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201, 76, 82, 4, 0, 27, 101, 4, 19,
    ];

    #[test]
    fn decompress_vec() {
        let res = decompress_to_vec_zlib(&encoded[..]).unwrap();
        assert_eq!(res.as_slice(), &b"Hello, zlib!"[..]);
    }

    #[test]
    fn decompress_vec_with_high_limit() {
        let res = decompress_to_vec_zlib_with_limit(&encoded[..], 100_000).unwrap();
        assert_eq!(res.as_slice(), &b"Hello, zlib!"[..]);
    }

    #[test]
    fn fail_to_decompress_with_limit() {
        let res = decompress_to_vec_zlib_with_limit(&encoded[..], 8);
        match res {
            Err(TINFLStatus::HasMoreOutput) => (), // expected result
            _ => panic!("Decompression output size limit was not enforced"),
        }
    }
}
