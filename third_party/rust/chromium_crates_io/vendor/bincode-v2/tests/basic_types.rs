mod utils;

use bincode::error::DecodeError;
use core::cell::{Cell, RefCell};
use core::cmp::Reverse;
use core::ops::Bound;
use core::time::Duration;
use std::num::*;
use utils::{the_same, the_same_with_comparer};

#[test]
fn test_numbers() {
    // integer types
    the_same(5u8);
    the_same(5u16);
    the_same(5u32);
    the_same(5u64);
    the_same(5u128);
    the_same(5usize);

    the_same(5i8);
    the_same(5i16);
    the_same(5i32);
    the_same(5i64);
    the_same(5i128);
    the_same(5isize);

    println!("Test {:?}", 5.0f32);
    the_same_with_comparer(5.0f32, |a, b| (a - b).abs() <= f32::EPSILON);
    the_same_with_comparer(5.0f64, |a, b| (a - b).abs() <= f64::EPSILON);

    // bool
    the_same(true);
    the_same(false);

    // utf8 characters
    for char in "aÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö文😀😁😂😃".chars()
    {
        the_same(char);
    }

    // tuples, up to 8
    the_same((1u8,));
    the_same((1u8, 2u8));
    the_same((1u8, 2u8, 3u8));
    the_same((1u8, 2u8, 3u8, 4u8));
    the_same((1u8, 2u8, 3u8, 4u8, 5u8));
    the_same((1u8, 2u8, 3u8, 4u8, 5u8, 6u8));
    the_same((1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8));
    the_same((1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8));

    // arrays
    #[rustfmt::skip]
    #[cfg(not(feature = "serde"))] // serde doesn't support arrays this big
    the_same([
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
        65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
        97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
        113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
        129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144,
        145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
        161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
        177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
        193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208,
        209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
        225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240,
        241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
    ]);

    // Common types
    the_same(Option::<u32>::None);
    the_same(Option::<u32>::Some(1234));

    the_same(Result::<u32, u8>::Ok(1555));
    the_same(Result::<u32, u8>::Err(15));

    the_same(Cell::<u32>::new(15));
    the_same(RefCell::<u32>::new(15));

    the_same(Duration::new(5, 730023852));
    the_same(5u8..10u8);
    the_same(5u8..=10u8);
    the_same(Bound::<usize>::Unbounded);
    the_same(Bound::<usize>::Included(105));
    the_same(Bound::<usize>::Excluded(5));

    // NonZero* types
    the_same(NonZeroU8::new(0));
    the_same(NonZeroU8::new(123));
    the_same(NonZeroU16::new(0));
    the_same(NonZeroU16::new(12345));
    the_same(NonZeroU32::new(0));
    the_same(NonZeroU32::new(12345));
    the_same(NonZeroU64::new(0));
    the_same(NonZeroU64::new(12345));
    the_same(NonZeroU128::new(0));
    the_same(NonZeroU128::new(12345));
    the_same(NonZeroUsize::new(0));
    the_same(NonZeroUsize::new(12345));

    the_same(NonZeroI8::new(0));
    the_same(NonZeroI8::new(123));
    the_same(NonZeroI16::new(0));
    the_same(NonZeroI16::new(12345));
    the_same(NonZeroI32::new(0));
    the_same(NonZeroI32::new(12345));
    the_same(NonZeroI64::new(0));
    the_same(NonZeroI64::new(12345));
    the_same(NonZeroI128::new(0));
    the_same(NonZeroI128::new(12345));
    the_same(NonZeroIsize::new(0));
    the_same(NonZeroIsize::new(12345));

    // Wrapping types
    the_same(Wrapping(5u8));
    the_same(Wrapping(5u16));
    the_same(Wrapping(5u32));
    the_same(Wrapping(5u64));
    the_same(Wrapping(5u128));
    the_same(Wrapping(5usize));

    the_same(Wrapping(5i8));
    the_same(Wrapping(5i16));
    the_same(Wrapping(5i32));
    the_same(Wrapping(5i64));
    the_same(Wrapping(5i128));
    the_same(Wrapping(5isize));

    // Reverse types
    the_same(Reverse(5u8));
    the_same(Reverse(5u16));
    the_same(Reverse(5u32));
    the_same(Reverse(5u64));
    the_same(Reverse(5u128));
    the_same(Reverse(5usize));

    the_same(Reverse(5i8));
    the_same(Reverse(5i16));
    the_same(Reverse(5i32));
    the_same(Reverse(5i64));
    the_same(Reverse(5i128));
    the_same(Reverse(5isize));
}

#[test]
fn test_refcell_already_borrowed() {
    let cell = RefCell::new(5u32);
    // first get a mutable reference to the cell
    let _mutable_guard = cell.borrow_mut();
    // now try to encode it
    let mut slice = [0u8; 10];
    let result = bincode::encode_into_slice(&cell, &mut slice, bincode::config::standard())
        .expect_err("Encoding a borrowed refcell should fail");

    match result {
        bincode::error::EncodeError::RefCellAlreadyBorrowed { .. } => {} // ok
        x => panic!("Expected a RefCellAlreadyBorrowed error, found {:?}", x),
    }
}

#[test]
fn test_slice() {
    let mut buffer = [0u8; 32];
    let input: &[u8] = &[1, 2, 3, 4, 5, 6, 7];
    bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(&buffer[..8], &[7, 1, 2, 3, 4, 5, 6, 7]);

    let (output, len): (&[u8], usize) =
        bincode::borrow_decode_from_slice(&buffer[..8], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, 8);
}

#[test]
fn test_option_slice() {
    let mut buffer = [0u8; 32];
    let input: Option<&[u8]> = Some(&[1, 2, 3, 4, 5, 6, 7]);
    let n = bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(&buffer[..n], &[1, 7, 1, 2, 3, 4, 5, 6, 7]);

    let (output, len): (Option<&[u8]>, usize) =
        bincode::borrow_decode_from_slice(&buffer[..n], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, n);

    let mut buffer = [0u8; 32];
    let input: Option<&[u8]> = None;
    let n = bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(&buffer[..n], &[0]);

    let (output, len): (Option<&[u8]>, usize) =
        bincode::borrow_decode_from_slice(&buffer[..n], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, n);
}

#[test]
fn test_str() {
    let mut buffer = [0u8; 32];
    let input: &str = "Hello world";
    bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(
        &buffer[..12],
        &[11, 72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100]
    );

    let (output, len): (&str, usize) =
        bincode::borrow_decode_from_slice(&buffer[..12], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, 12);
}

#[test]
fn test_option_str() {
    let mut buffer = [0u8; 32];
    let input: Option<&str> = Some("Hello world");
    let n = bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(
        &buffer[..n],
        &[1, 11, 72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100]
    );

    let (output, len): (Option<&str>, usize) =
        bincode::borrow_decode_from_slice(&buffer[..n], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, n);

    let mut buffer = [0u8; 32];
    let input: Option<&str> = None;
    let n = bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(&buffer[..n], &[0]);

    let (output, len): (Option<&str>, usize) =
        bincode::borrow_decode_from_slice(&buffer[..n], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, n);
}

#[test]
fn test_array() {
    let mut buffer = [0u8; 32];
    let input: [u8; 10] = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100];
    bincode::encode_into_slice(input, &mut buffer, bincode::config::standard()).unwrap();
    assert_eq!(&buffer[..10], &[10, 20, 30, 40, 50, 60, 70, 80, 90, 100]);

    let (output, len): ([u8; 10], usize) =
        bincode::decode_from_slice(&buffer[..11], bincode::config::standard()).unwrap();
    assert_eq!(input, output);
    assert_eq!(len, 10);

    let mut buffer = [0u8; 32];
    let input: [u8; 1] = [1];
    let config = bincode::config::standard()
        .with_fixed_int_encoding()
        .with_little_endian();
    let len = bincode::encode_into_slice(input, &mut buffer, config).unwrap();
    assert_eq!(len, 1);
    assert_eq!(&buffer[..1], &[1]);

    let (output, len): ([u8; 1], usize) = bincode::decode_from_slice(&buffer[..9], config).unwrap();
    assert_eq!(len, 1);
    assert_eq!(input, output);
}

#[test]
fn test_duration_out_of_range() {
    let mut input = [0u8; 14];

    bincode::encode_into_slice(
        (u64::MAX, u32::MAX),
        &mut input,
        bincode::config::standard(),
    )
    .unwrap();

    let result: Result<(std::time::Duration, usize), _> =
        bincode::decode_from_slice(&input, bincode::config::standard());

    match result {
        Err(DecodeError::InvalidDuration {
            secs: u64::MAX,
            nanos: u32::MAX,
        }) => {}
        Err(e) => panic!("Expected InvalidDuration, got {:?}", e),
        Ok(_) => panic!("Expected the decode to fail"),
    }
}

#[test]
fn test_duration_wrapping() {
    let mut input = [0u8; 14];

    bincode::encode_into_slice(
        (u64::MAX - 4, u32::MAX),
        &mut input,
        bincode::config::standard(),
    )
    .unwrap();

    let (result, _): (std::time::Duration, _) =
        bincode::decode_from_slice(&input, bincode::config::standard()).unwrap();

    assert_eq!(result.as_secs(), u64::MAX);

    assert_eq!(result.subsec_nanos(), 294967295);
}
