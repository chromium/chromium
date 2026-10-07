use std::panic::{UnwindSafe, catch_unwind, resume_unwind};

/// A macro to duplicate code for each SIMD type.
///
/// The inner code has access to type aliases and constants that can be used to
/// write tests.
///
/// ```ignore
/// // Has access to:
/// // - type T
/// // - const N: usize
/// // - type Simd
/// for_simd_types!(|T, N| ...);
///
/// // Has access to:
/// // - type T
/// // - const N: usize
/// // - type Simd
/// // - type Signed
/// // - type SimdSigned
/// for_simd_types!(|T: Float, N| ...);
///
/// // Has access to:
/// // - type T
/// // - const N: usize
/// // - type Simd
/// for_simd_types!(|T: Integer, N| ...);
///
/// // Has access to:
/// // - type T
/// // - const N: usize
/// // - type Simd
/// // - type Unsigned
/// // - type SimdUnsigned
/// for_simd_types!(|T: Signed, N| ...);
///
/// // Has access to:
/// // - type T
/// // - const N: usize
/// // - type Simd
/// for_simd_types!(|T: Unsigned, N| ...);
/// ```
macro_rules! for_simd_types {
  (|T, N| $expr:expr) => {
    for_simd_types!(|T: Float, N| $expr);
    for_simd_types!(|T: Integer, N| $expr);
  };
  (|T: Float, N| $expr:expr) => {
    for_simd_types!(float!(f32, 4, f32x4, i32, i32x4, u32, u32x4, $expr));
    for_simd_types!(float!(f32, 8, f32x8, i32, i32x8, u32, u32x8, $expr));
    for_simd_types!(float!(f32, 16, f32x16, i32, i32x16, u32, u32x16, $expr));
    for_simd_types!(float!(f64, 2, f64x2, i64, i64x2, u64, u64x2, $expr));
    for_simd_types!(float!(f64, 4, f64x4, i64, i64x4, u64, u64x4, $expr));
    for_simd_types!(float!(f64, 8, f64x8, i64, i64x8, u64, u64x8, $expr));
  };
  (|T: Integer, N| $expr:expr) => {
    for_simd_types!(|T: Signed, N| $expr);
    for_simd_types!(|T: Unsigned, N| $expr);
  };
  (|T: Signed, N| $expr:expr) => {
    for_simd_types!(signed!(i8, 16, i8x16, u8, u8x16, i16, (), $expr));
    for_simd_types!(signed!(i8, 32, i8x32, u8, u8x32, i16, (), $expr));
    for_simd_types!(signed!(i8, 64, i8x64, u8, u8x64, i16, (), $expr));
    for_simd_types!(signed!(i16, 8, i16x8, u16, u16x8, i32, (), $expr));
    for_simd_types!(signed!(i16, 16, i16x16, u16, u16x16, i32, (), $expr));
    for_simd_types!(signed!(i16, 32, i16x32, u16, u16x32, i32, (), $expr));
    for_simd_types!(signed!(i32, 4, i32x4, u32, u32x4, i64, (), $expr));
    for_simd_types!(signed!(i32, 8, i32x8, u32, u32x8, i64, (), $expr));
    for_simd_types!(signed!(i32, 16, i32x16, u32, u32x16, i64, (), $expr));
    for_simd_types!(signed!(i64, 2, i64x2, u64, u64x2, i128, (), $expr));
    for_simd_types!(signed!(i64, 4, i64x4, u64, u64x4, i128, (), $expr));
    for_simd_types!(signed!(i64, 8, i64x8, u64, u64x8, i128, (), $expr));
  };
  (|T: Signed, N, DoubleSizedSimd| $expr:expr) => {
    for_simd_types!(signed!(i8, 16, i8x16, u8, u8x16, i16, (i16x16), $expr));
    for_simd_types!(signed!(i8, 32, i8x32, u8, u8x32, i16, (i16x32), $expr));
    // for_simd_types!(signed!(i8, 64, i8x64, u8, u8x64, i16, (i16x64), $expr));
    for_simd_types!(signed!(i16, 8, i16x8, u16, u16x8, i32, (i32x8), $expr));
    for_simd_types!(signed!(i16, 16, i16x16, u16, u16x16, i32, (i32x16), $expr));
    // for_simd_types!(signed!(i16, 32, i16x32, u16, u16x32, i32, (i32x32), $expr));
    for_simd_types!(signed!(i32, 4, i32x4, u32, u32x4, i64, (i64x4), $expr));
    for_simd_types!(signed!(i32, 8, i32x8, u32, u32x8, i64, (i64x8), $expr));
    // for_simd_types!(signed!(i32, 16, i32x16, u32, u32x16, i64, (i64x16), $expr));
    // for_simd_types!(signed!(i64, 2, i64x2, u64, u64x2, i128, (i128x2), $expr));
    // for_simd_types!(signed!(i64, 4, i64x4, u64, u64x4, i128, (i128x4), $expr));
    // for_simd_types!(signed!(i64, 8, i64x8, u64, u64x8, i128, (i128x8), $expr));
  };
  (|T: Unsigned, N| $expr:expr) => {
    for_simd_types!(unsigned!(u8, 16, u8x16, i8, i8x16, u16, (), $expr));
    for_simd_types!(unsigned!(u8, 32, u8x32, i8, i8x32, u16, (), $expr));
    for_simd_types!(unsigned!(u8, 64, u8x64, i8, i8x64, u16, (), $expr));
    for_simd_types!(unsigned!(u16, 8, u16x8, i16, i16x8, u32, (), $expr));
    for_simd_types!(unsigned!(u16, 16, u16x16, i16, i16x16, u32, (), $expr));
    for_simd_types!(unsigned!(u16, 32, u16x32, i16, i16x32, u32, (), $expr));
    for_simd_types!(unsigned!(u32, 4, u32x4, i32, i32x4, u64, (), $expr));
    for_simd_types!(unsigned!(u32, 8, u32x8, i32, i32x8, u64, (), $expr));
    for_simd_types!(unsigned!(u32, 16, u32x16, i32, i32x16, u64, (), $expr));
    for_simd_types!(unsigned!(u64, 2, u64x2, i64, i64x2, u128, (), $expr));
    for_simd_types!(unsigned!(u64, 4, u64x4, i64, i64x4, u128, (), $expr));
    for_simd_types!(unsigned!(u64, 8, u64x8, i64, i64x8, u128, (), $expr));
  };
  (|T: Unsigned, N, DoubleSizedSimd| $expr:expr) => {
    for_simd_types!(unsigned!(u8, 16, u8x16, i8, i8x16, u16, (u16x16), $expr));
    for_simd_types!(unsigned!(u8, 32, u8x32, i8, i8x32, u16, (u16x32), $expr));
    // for_simd_types!(unsigned!(u8, 64, u8x64, i8, i8x64, u16, (u16x64), $expr));
    for_simd_types!(unsigned!(u16, 8, u16x8, i16, i16x8, u32, (u32x8), $expr));
    for_simd_types!(unsigned!(u16, 16, u16x16, i16, i16x16, u32, (u32x16), $expr));
    // for_simd_types!(unsigned!(u16, 32, u16x32, i16, i16x32, u32, (u32x32), $expr));
    for_simd_types!(unsigned!(u32, 4, u32x4, i32, i32x4, u64, (u64x4), $expr));
    for_simd_types!(unsigned!(u32, 8, u32x8, i32, i32x8, u64, (u64x8), $expr));
    // for_simd_types!(unsigned!(u32, 16, u32x16, i32, i32x16, u64, (u64x16), $expr));
    // for_simd_types!(unsigned!(u64, 2, u64x2, i64, i64x2, u128, (u128x2), $expr));
    // for_simd_types!(unsigned!(u64, 4, u64x4, i64, i64x4, u128, (u128x4), $expr));
    // for_simd_types!(unsigned!(u64, 8, u64x8, i64, i64x8, u128, (u128x8), $expr));
  };
  (float!(
    $T:ident,
    $N:literal,
    $Simd:ident,
    $Signed:ident,
    $SimdSigned:ident,
    $Unsigned:ident,
    $SimdUnsigned:ident,
    $expr:expr
  )) => {{
    type Simd = wide::$Simd;
    #[allow(dead_code)]
    type T = $T;
    #[allow(dead_code)]
    const N: usize = $N;
    #[allow(dead_code)]
    type Signed = $Signed;
    #[allow(dead_code)]
    type SimdSigned = wide::$SimdSigned;
    #[allow(dead_code)]
    type Unsigned = $Unsigned;
    #[allow(dead_code)]
    type SimdUnsigned = wide::$SimdUnsigned;
    $crate::utils::for_simd_types_helper(|| $expr, stringify!($T), $N);
  }};
  (signed!(
    $T:ident,
    $N:literal,
    $Simd:ident,
    $Unsigned:ident,
    $SimdUnsigned:ident,
    $DoubleSizedT:ident,
    ($($DoubleSizedSimd:ident)?),
    $expr:expr
  )) => {{
    type Simd = wide::$Simd;
    #[allow(dead_code)]
    type T = $T;
    #[allow(dead_code)]
    const N: usize = $N;
    #[allow(dead_code)]
    type Signed = $T;
    #[allow(dead_code)]
    type SimdSigned = wide::$Simd;
    #[allow(dead_code)]
    type Unsigned = $Unsigned;
    #[allow(dead_code)]
    type SimdUnsigned = wide::$SimdUnsigned;
    #[allow(dead_code)]
    type DoubleSizedT = $DoubleSizedT;
    $(
      #[allow(dead_code)]
      type DoubleSizedSimd = wide::$DoubleSizedSimd;
    )?
    $crate::utils::for_simd_types_helper(|| $expr, stringify!($T), $N);
  }};
  (unsigned!(
    $T:ident,
    $N:literal,
    $Simd:ident,
    $Signed:ident,
    $SimdSigned:ident,
    $DoubleSizedT:ident,
    ($($DoubleSizedSimd:ident)?),
    $expr:expr
  )) => {{
    type Simd = wide::$Simd;
    #[allow(dead_code)]
    type T = $T;
    #[allow(dead_code)]
    const N: usize = $N;
    #[allow(dead_code)]
    type Signed = $Signed;
    #[allow(dead_code)]
    type SimdSigned = wide::$SimdSigned;
    #[allow(dead_code)]
    type Unsigned = $T;
    #[allow(dead_code)]
    type SimdUnsigned = wide::$Simd;
    #[allow(dead_code)]
    type DoubleSizedT = $DoubleSizedT;
    $(
      #[allow(dead_code)]
      type DoubleSizedSimd = wide::$DoubleSizedSimd;
    )?
    $crate::utils::for_simd_types_helper(|| $expr, stringify!($T), $N);
  }};
}
pub(crate) use for_simd_types;

/// Improves error messages by specifying which `T` and `N` failed.
#[doc(hidden)]
pub fn for_simd_types_helper(f: impl FnOnce() + UnwindSafe, t: &str, n: usize) {
  // For WASM, `catch/resume_unwind` leads to:
  // "wasm trap: wasm `unreachable` instruction executed"
  // which hides the actual panic message.
  if cfg!(target_family = "wasm") {
    f();
  } else {
    match catch_unwind(f) {
      Ok(_) => {}
      Err(payload) => {
        println!();
        println!("T: {t}");
        println!("N: {n}");
        resume_unwind(payload);
      }
    }
  }
}
