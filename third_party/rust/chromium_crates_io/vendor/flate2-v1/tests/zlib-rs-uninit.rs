#![cfg(all(feature = "zlib-rs", not(feature = "any_c_zlib")))]

use flate2::{Compress, Compression, Decompress, FlushCompress, FlushDecompress};
use std::mem::MaybeUninit;

#[test]
fn uninit_compression_matches_initialized_compression() {
    let input = highly_compressible_input();

    let mut initialized = vec![0; 4096];
    let mut expected = Compress::new(Compression::default(), true);
    let expected_status = expected
        .compress(&input, &mut initialized, FlushCompress::Finish)
        .unwrap();

    let mut uninitialized = vec![MaybeUninit::uninit(); initialized.len()];
    let mut actual = Compress::new(Compression::default(), true);
    let actual_status = actual
        .compress_uninit(&input, &mut uninitialized, FlushCompress::Finish)
        .unwrap();

    assert_eq!(actual_status, expected_status);
    assert_eq!(actual.total_in(), expected.total_in());
    assert_eq!(actual.total_out(), expected.total_out());
    assert_eq!(
        initialized_prefix(&uninitialized, actual.total_out() as usize),
        &initialized[..expected.total_out() as usize]
    );
}

#[test]
fn uninit_decompression_matches_initialized_decompression() {
    let input = highly_compressible_input();

    let mut compressed = Vec::with_capacity(4096);
    Compress::new(Compression::default(), true)
        .compress_vec(&input, &mut compressed, FlushCompress::Finish)
        .unwrap();

    let mut initialized = vec![0; 4096];
    let mut expected = Decompress::new(true);
    let expected_status = expected
        .decompress(&compressed, &mut initialized, FlushDecompress::Finish)
        .unwrap();

    let mut uninitialized = vec![MaybeUninit::uninit(); initialized.len()];
    let mut actual = Decompress::new(true);
    let actual_status = actual
        .decompress_uninit(&compressed, &mut uninitialized, FlushDecompress::Finish)
        .unwrap();

    assert_eq!(actual_status, expected_status);
    assert_eq!(actual.total_in(), expected.total_in());
    assert_eq!(actual.total_out(), expected.total_out());
    assert_eq!(
        initialized_prefix(&uninitialized, actual.total_out() as usize),
        &initialized[..expected.total_out() as usize]
    );
}

fn highly_compressible_input() -> Vec<u8> {
    b"hello world ".repeat(100)
}

fn initialized_prefix(bytes: &[MaybeUninit<u8>], len: usize) -> &[u8] {
    // SAFETY: The compression APIs report the number of bytes they initialized.
    unsafe { std::slice::from_raw_parts(bytes.as_ptr().cast(), len) }
}
