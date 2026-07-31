#[test]
pub fn test_cast_array() {
  let x = [0u32, 1u32, 2u32];
  let _: [u16; 6] = bytemuck::cast(x);
}

#[cfg(feature = "min_const_generics")]
#[test]
pub fn test_cast_long_array() {
  let x = [0u32; 65];
  let _: [u16; 130] = bytemuck::cast(x);
}

#[cfg(feature = "min_const_generics")]
#[test]
pub fn test_cast_nouninit_array() {
  assert_eq!(
    bytemuck::try_cast::<[char; 65], [u32; 65]>(['a'; 65]),
    Ok([97; 65])
  );

  assert_eq!(
    bytemuck::try_cast::<[bool; 65], [u8; 65]>([true; 65]),
    Ok([1; 65])
  );

  use core::num::*;
  macro_rules! test_no_uninit {
    ($ty:ty: $primitive:ty) => {
      assert_eq!(
        bytemuck::try_cast::<[$ty; 65], [$primitive; 65]>(
          [<$ty>::new(1).unwrap(); 65]
        ),
        Ok([1; 65])
      );
    };
  }

  test_no_uninit!(NonZeroU8: u8);
  test_no_uninit!(NonZeroI8: i8);
  test_no_uninit!(NonZeroU16: u16);
  test_no_uninit!(NonZeroI16: i16);
  test_no_uninit!(NonZeroU32: u32);
  test_no_uninit!(NonZeroI32: i32);
  test_no_uninit!(NonZeroU64: u64);
  test_no_uninit!(NonZeroI64: i64);
  test_no_uninit!(NonZeroU128: u128);
  test_no_uninit!(NonZeroI128: i128);
  test_no_uninit!(NonZeroUsize: usize);
  test_no_uninit!(NonZeroIsize: isize);
}
