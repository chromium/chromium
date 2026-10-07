//! This module is for the `m512i` wrapper type, its bonus methods, and all
//! necessary trait impls.
//!
//! Intrinsics should _not_ be in this module! They should all be free-functions
//! in the other modules, sorted by CPU target feature.

use super::*;

/// The data for a 512-bit AVX-512 register of integer data.
///
/// * The exact layout to view the type as depends on the operation used.
/// * `From` and `Into` impls are provided for all the relevant signed integer
///   array types.
/// * Formatting impls print as sixteen `i32` values just because they have to
///   pick something. If you want an alternative you can turn it into an array
///   and print as you like.
#[repr(transparent)]
#[allow(non_camel_case_types)]
pub struct m512i(pub __m512i);

#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::Zeroable for m512i {}
#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::Pod for m512i {}
#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::TransparentWrapper<__m512i> for m512i {}

impl m512i {
  /// Transmutes the `m512i` to an array.
  ///
  /// Same as `m.into()`, just lets you be more explicit about what's happening.
  #[must_use]
  #[inline(always)]
  pub fn to_array(self) -> [i32; 16] {
    self.into()
  }

  /// Transmutes an array into `m512i`.
  ///
  /// Same as `m512i::from(arr)`, it just lets you be more explicit about what's
  /// happening.
  #[must_use]
  #[inline(always)]
  pub fn from_array(f: [i32; 16]) -> Self {
    f.into()
  }

  //

  /// Converts into the bit patterns of these doubles (`[u64;8]`).
  ///
  /// Like [`f64::to_bits`](f64::to_bits), but all eight lanes at once.
  #[must_use]
  #[inline(always)]
  pub fn to_bits(self) -> [i32; 16] {
    unsafe { core::mem::transmute(self) }
  }

  /// Converts from the bit patterns of these doubles (`[u64;8]`).
  ///
  /// Like [`f64::from_bits`](f64::from_bits), but all eight lanes at once.
  #[must_use]
  #[inline(always)]
  pub fn from_bits(bits: [i32; 16]) -> Self {
    unsafe { core::mem::transmute(bits) }
  }
}

impl Clone for m512i {
  #[inline(always)]
  fn clone(&self) -> Self {
    *self
  }
}
impl Copy for m512i {}

impl Default for m512i {
  #[inline(always)]
  fn default() -> Self {
    unsafe { core::mem::zeroed() }
  }
}

// 8-bit

impl From<[i8; 64]> for m512i {
  #[inline(always)]
  fn from(arr: [i8; 64]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [i8; 64] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

impl From<[u8; 64]> for m512i {
  #[inline(always)]
  fn from(arr: [u8; 64]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [u8; 64] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

// 16-bit

impl From<[i16; 32]> for m512i {
  #[inline(always)]
  fn from(arr: [i16; 32]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [i16; 32] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

impl From<[u16; 32]> for m512i {
  #[inline(always)]
  fn from(arr: [u16; 32]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [u16; 32] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

// 32-bit

impl From<[i32; 16]> for m512i {
  #[inline(always)]
  fn from(arr: [i32; 16]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [i32; 16] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

impl From<[u32; 16]> for m512i {
  #[inline(always)]
  fn from(arr: [u32; 16]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [u32; 16] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

// 64-bit

impl From<[i64; 8]> for m512i {
  #[inline(always)]
  fn from(arr: [i64; 8]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [i64; 8] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

impl From<[u64; 8]> for m512i {
  #[inline(always)]
  fn from(arr: [u64; 8]) -> Self {
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512i> for [u64; 8] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

// 128-bit

impl From<[i128; 4]> for m512i {
  #[inline(always)]
  fn from(i: [i128; 4]) -> Self {
    unsafe { core::mem::transmute(i) }
  }
}

impl From<m512i> for [i128; 4] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

impl From<[u128; 4]> for m512i {
  #[inline(always)]
  fn from(u: [u128; 4]) -> Self {
    unsafe { core::mem::transmute(u) }
  }
}

impl From<m512i> for [u128; 4] {
  #[inline(always)]
  fn from(m: m512i) -> Self {
    unsafe { core::mem::transmute(m) }
  }
}

//
// PLEASE KEEP ALL THE FORMAT IMPL JUNK AT THE END OF THE FILE
//

impl Debug for m512i {
  /// Debug formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:?}", m512i::default());
  /// assert_eq!(&f, "m512i(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "m512i(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Debug::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl Display for m512i {
  /// Display formats each `i32`, and leaves the type name off of the font.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{}", m512i::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Display::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl Binary for m512i {
  /// Binary formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:b}", m512i::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Binary::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl LowerExp for m512i {
  /// LowerExp formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:e}", m512i::default());
  /// assert_eq!(
  ///   &f,
  ///   "(0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0)"
  /// );
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      LowerExp::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl UpperExp for m512i {
  /// UpperExp formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:E}", m512i::default());
  /// assert_eq!(
  ///   &f,
  ///   "(0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0)"
  /// );
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      UpperExp::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl LowerHex for m512i {
  /// LowerHex formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:x}", m512i::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      LowerHex::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl UpperHex for m512i {
  /// UpperHex formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:X}", m512i::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      UpperHex::fmt(int, f)?;
    }
    write!(f, ")")
  }
}

impl Octal for m512i {
  /// Octal formats each `i32`.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:o}", m512i::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, int) in <[i32; 16]>::from(*self).iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Octal::fmt(int, f)?;
    }
    write!(f, ")")
  }
}
