#![allow(clippy::unnecessary_cast)]
#![allow(clippy::manual_slice_size_calculation)]

use core::mem::size_of;

use bytemuck::*;

#[test]
fn test_try_cast_slice() {
  // some align4 data
  let u32_slice: &[u32] = &[4, 5, 6];
  // the same data as align1
  let the_bytes: &[u8] = try_cast_slice(u32_slice).unwrap();

  assert_eq!(
    u32_slice.as_ptr() as *const u32 as usize,
    the_bytes.as_ptr() as *const u8 as usize
  );
  assert_eq!(
    u32_slice.len() * size_of::<u32>(),
    the_bytes.len() * size_of::<u8>()
  );

  // by taking one byte off the front, we're definitely mis-aligned for u32.
  let mis_aligned_bytes = &the_bytes[1..];
  assert_eq!(
    try_cast_slice::<u8, u32>(mis_aligned_bytes),
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  );

  // by taking one byte off the end, we're aligned but would have slop bytes for
  // u32
  let the_bytes_len_minus1 = the_bytes.len() - 1;
  let slop_bytes = &the_bytes[..the_bytes_len_minus1];
  assert_eq!(
    try_cast_slice::<u8, u32>(slop_bytes),
    Err(PodCastError::OutputSliceWouldHaveSlop)
  );

  // if we don't mess with it we can up-alignment cast
  try_cast_slice::<u8, u32>(the_bytes).unwrap();
}

#[test]
fn test_try_cast_slice_mut() {
  // some align4 data
  let u32_slice: &mut [u32] = &mut [4, 5, 6];
  let u32_len = u32_slice.len();
  let u32_ptr = u32_slice.as_ptr();

  // the same data as align1
  let the_bytes: &mut [u8] = try_cast_slice_mut(u32_slice).unwrap();
  let the_bytes_len = the_bytes.len();
  let the_bytes_ptr = the_bytes.as_ptr();

  assert_eq!(
    u32_ptr as *const u32 as usize,
    the_bytes_ptr as *const u8 as usize
  );
  assert_eq!(u32_len * size_of::<u32>(), the_bytes_len * size_of::<u8>());

  // by taking one byte off the front, we're definitely mis-aligned for u32.
  let mis_aligned_bytes = &mut the_bytes[1..];
  assert_eq!(
    try_cast_slice_mut::<u8, u32>(mis_aligned_bytes),
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  );

  // by taking one byte off the end, we're aligned but would have slop bytes for
  // u32
  let the_bytes_len_minus1 = the_bytes.len() - 1;
  let slop_bytes = &mut the_bytes[..the_bytes_len_minus1];
  assert_eq!(
    try_cast_slice_mut::<u8, u32>(slop_bytes),
    Err(PodCastError::OutputSliceWouldHaveSlop)
  );

  // if we don't mess with it we can up-alignment cast
  try_cast_slice_mut::<u8, u32>(the_bytes).unwrap();
}

#[test]
fn test_types() {
  let _: i32 = cast(1.0_f32);
  let _: &mut i32 = cast_mut(&mut 1.0_f32);
  let _: &i32 = cast_ref(&1.0_f32);
  let _: &[i32] = cast_slice(&[1.0_f32]);
  let _: &mut [i32] = cast_slice_mut(&mut [1.0_f32]);
  //
  let _: Result<i32, PodCastError> = try_cast(1.0_f32);
  let _: Result<&mut i32, PodCastError> = try_cast_mut(&mut 1.0_f32);
  let _: Result<&i32, PodCastError> = try_cast_ref(&1.0_f32);
  let _: Result<&[i32], PodCastError> = try_cast_slice(&[1.0_f32]);
  let _: Result<&mut [i32], PodCastError> = try_cast_slice_mut(&mut [1.0_f32]);
}

#[test]
fn test_bytes_of() {
  assert_eq!(bytes_of(&0xaabbccdd_u32), &0xaabbccdd_u32.to_ne_bytes());
  assert_eq!(
    bytes_of_mut(&mut 0xaabbccdd_u32),
    &mut 0xaabbccdd_u32.to_ne_bytes()
  );
  let mut a = 0xaabbccdd_u32;
  let a_addr = &a as *const _ as usize;
  // ensure addresses match.
  assert_eq!(bytes_of(&a).as_ptr() as usize, a_addr);
  assert_eq!(bytes_of_mut(&mut a).as_ptr() as usize, a_addr);
}

#[test]
fn test_try_from_bytes() {
  let u32s = [0xaabbccdd, 0x11223344_u32];
  let bytes = bytemuck::cast_slice::<u32, u8>(&u32s);
  assert_eq!(try_from_bytes::<u32>(&bytes[..4]), Ok(&u32s[0]));
  assert_eq!(
    try_from_bytes::<u32>(&bytes[..5]),
    Err(PodCastError::SizeMismatch)
  );
  assert_eq!(
    try_from_bytes::<u32>(&bytes[..3]),
    Err(PodCastError::SizeMismatch)
  );
  assert_eq!(
    try_from_bytes::<u32>(&bytes[1..5]),
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  );
}

#[test]
fn test_try_from_bytes_mut() {
  let mut abcd = 0xaabbccdd;
  let mut u32s = [abcd, 0x11223344_u32];
  let bytes = bytemuck::cast_slice_mut::<u32, u8>(&mut u32s);
  assert_eq!(try_from_bytes_mut::<u32>(&mut bytes[..4]), Ok(&mut abcd));
  assert_eq!(try_from_bytes_mut::<u32>(&mut bytes[..4]), Ok(&mut abcd));
  assert_eq!(
    try_from_bytes_mut::<u32>(&mut bytes[..5]),
    Err(PodCastError::SizeMismatch)
  );
  assert_eq!(
    try_from_bytes_mut::<u32>(&mut bytes[..3]),
    Err(PodCastError::SizeMismatch)
  );
  assert_eq!(
    try_from_bytes::<u32>(&bytes[1..5]),
    Err(PodCastError::TargetAlignmentGreaterAndInputNotAligned)
  );
}

#[test]
fn test_from_bytes() {
  let abcd = 0xaabbccdd_u32;
  let aligned_bytes = bytemuck::bytes_of(&abcd);
  assert_eq!(from_bytes::<u32>(aligned_bytes), &abcd);
  assert!(core::ptr::eq(from_bytes(aligned_bytes), &abcd));
}

#[test]
fn test_from_bytes_mut() {
  let mut a = 0xaabbccdd_u32;
  let a_addr = &a as *const _ as usize;
  let aligned_bytes = bytemuck::bytes_of_mut(&mut a);
  assert_eq!(*from_bytes_mut::<u32>(aligned_bytes), 0xaabbccdd_u32);
  assert_eq!(
    from_bytes_mut::<u32>(aligned_bytes) as *const u32 as usize,
    a_addr
  );
}

// like #[should_panic], but can be a part of another test, instead of requiring
// it to be it's own test.
macro_rules! should_panic {
  ($ex:expr) => {
    assert!(
      std::panic::catch_unwind(|| {
        let _ = $ex;
      })
      .is_err(),
      concat!("should have panicked: `", stringify!($ex), "`")
    );
  };
}

#[test]
fn test_panics() {
  should_panic!(cast_slice::<u8, u32>(&[1u8, 2u8]));
  should_panic!(cast_slice_mut::<u8, u32>(&mut [1u8, 2u8]));
  should_panic!(from_bytes::<u32>(&[1u8, 2]));
  should_panic!(from_bytes::<u32>(&[1u8, 2, 3, 4, 5]));
  should_panic!(from_bytes_mut::<u32>(&mut [1u8, 2]));
  should_panic!(from_bytes_mut::<u32>(&mut [1u8, 2, 3, 4, 5]));
  // use cast_slice on some u32s to get some align>=4 bytes, so we can know
  // we'll give from_bytes unaligned ones.
  let aligned_bytes = bytemuck::cast_slice::<u32, u8>(&[0, 0]);
  should_panic!(from_bytes::<u32>(&aligned_bytes[1..5]));
}
