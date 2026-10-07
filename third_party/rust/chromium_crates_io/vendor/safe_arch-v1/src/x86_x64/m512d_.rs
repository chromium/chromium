//! This module is for the `m512d` wrapper type, its bonus methods, and all
//! necessary trait impls.
//!
//! Intrinsics should _not_ be in this module! They should all be free-functions
//! in the other modules, sorted by CPU target feature.

use super::*;

/// The data for a 512-bit AVX-512 register of eight `f64` values.
///
/// * This is _very similar to_ having `[f64; 8]`. The main difference is that
///   it's aligned to 64 instead of just 8, and of course you can perform
///   various intrinsic operations on it.
#[repr(transparent)]
#[allow(non_camel_case_types)]
pub struct m512d(pub __m512d);

#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::Zeroable for m512d {}
#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::Pod for m512d {}
#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::TransparentWrapper<__m512d> for m512d {}

impl m512d {
  /// Transmutes the `m512d` to an array.
  ///
  /// Same as `m.into()`, just lets you be more explicit about what's happening.
  #[must_use]
  #[inline(always)]
  pub fn to_array(self) -> [f64; 8] {
    self.into()
  }

  /// Transmutes an array into `m512d`.
  ///
  /// Same as `m512d::from(arr)`, it just lets you be more explicit about what's
  /// happening.
  #[must_use]
  #[inline(always)]
  pub fn from_array(f: [f64; 8]) -> Self {
    f.into()
  }

  //

  /// Converts into the bit patterns of these doubles (`[u64;8]`).
  ///
  /// Like [`f64::to_bits`](f64::to_bits), but all eight lanes at once.
  #[must_use]
  #[inline(always)]
  pub fn to_bits(self) -> [u64; 8] {
    unsafe { core::mem::transmute(self) }
  }

  /// Converts from the bit patterns of these doubles (`[u64;8]`).
  ///
  /// Like [`f64::from_bits`](f64::from_bits), but all eight lanes at once.
  #[must_use]
  #[inline(always)]
  pub fn from_bits(bits: [u64; 8]) -> Self {
    unsafe { core::mem::transmute(bits) }
  }
}

impl Clone for m512d {
  #[inline(always)]
  fn clone(&self) -> Self {
    *self
  }
}
impl Copy for m512d {}

impl Default for m512d {
  #[inline(always)]
  fn default() -> Self {
    unsafe { core::mem::zeroed() }
  }
}

impl From<[f64; 8]> for m512d {
  #[inline(always)]
  fn from(arr: [f64; 8]) -> Self {
    // Safety: because this semantically moves the value from the input position
    // (align8) to the output position (align64) it is fine to increase our
    // required alignment without worry.
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512d> for [f64; 8] {
  #[inline(always)]
  fn from(m: m512d) -> Self {
    // We can of course transmute to a lower alignment
    unsafe { core::mem::transmute(m) }
  }
}

//
// PLEASE KEEP ALL THE FORMAT IMPL JUNK AT THE END OF THE FILE
//

impl Debug for m512d {
  /// Debug formats each double.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:?}", m512d::default());
  /// assert_eq!(&f, "m512d(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "m512d(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Debug::fmt(double, f)?;
    }
    write!(f, ")")
  }
}

impl Display for m512d {
  /// Display formats each double, and leaves the type name off of the font.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{}", m512d::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Display::fmt(double, f)?;
    }
    write!(f, ")")
  }
}

impl Binary for m512d {
  /// Binary formats each double's bit pattern (via [`f64::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:b}", m512d::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Binary::fmt(&double.to_bits(), f)?;
    }
    write!(f, ")")
  }
}

impl LowerExp for m512d {
  /// LowerExp formats each double.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:e}", m512d::default());
  /// assert_eq!(&f, "(0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      LowerExp::fmt(double, f)?;
    }
    write!(f, ")")
  }
}

impl UpperExp for m512d {
  /// UpperExp formats each double.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:E}", m512d::default());
  /// assert_eq!(&f, "(0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      UpperExp::fmt(double, f)?;
    }
    write!(f, ")")
  }
}

impl LowerHex for m512d {
  /// LowerHex formats each double's bit pattern (via [`f64::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:x}", m512d::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      LowerHex::fmt(&double.to_bits(), f)?;
    }
    write!(f, ")")
  }
}

impl UpperHex for m512d {
  /// UpperHex formats each double's bit pattern (via [`f64::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:X}", m512d::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      UpperHex::fmt(&double.to_bits(), f)?;
    }
    write!(f, ")")
  }
}

impl Octal for m512d {
  /// Octal formats each double's bit pattern (via [`f64::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:o}", m512d::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, double) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Octal::fmt(&double.to_bits(), f)?;
    }
    write!(f, ")")
  }
}
