//! This module is for the `m512` wrapper type, its bonus methods, and all
//! necessary trait impls.
//!
//! Intrinsics should _not_ be in this module! They should all be free-functions
//! in the other modules, sorted by CPU target feature.

use super::*;

/// The data for a 512-bit AVX-512 register of sixteen `f32` lanes.
///
/// * This is _very similar to_ having `[f32; 16]`. The main difference is that
///   it's aligned to 64 instead of just 4, and of course you can perform
///   various intrinsic operations on it.
#[repr(transparent)]
#[allow(non_camel_case_types)]
pub struct m512(pub __m512);

#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::Zeroable for m512 {}
#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::Pod for m512 {}
#[cfg(feature = "bytemuck")]
unsafe impl bytemuck::TransparentWrapper<__m512> for m512 {}

impl m512 {
  /// Transmutes the `m512` to an array.
  ///
  /// Same as `m.into()`, just lets you be more explicit about what's happening.
  #[must_use]
  #[inline(always)]
  pub fn to_array(self) -> [f32; 16] {
    self.into()
  }

  /// Transmutes an array into `m512`.
  ///
  /// Same as `m512::from(arr)`, it just lets you be more explicit about what's
  /// happening.
  #[must_use]
  #[inline(always)]
  pub fn from_array(f: [f32; 16]) -> Self {
    f.into()
  }

  /// Converts into the bit patterns of these floats (`[u32;16]`).
  ///
  /// Like [`f32::to_bits`](f32::to_bits), but all sixteen lanes at once.
  #[must_use]
  #[inline(always)]
  pub fn to_bits(self) -> [u32; 16] {
    unsafe { core::mem::transmute(self) }
  }

  /// Converts from the bit patterns of these floats (`[u32;16]`).
  ///
  /// Like [`f32::from_bits`](f32::from_bits), but all sixteen lanes at once.
  #[must_use]
  #[inline(always)]
  pub fn from_bits(bits: [u32; 16]) -> Self {
    unsafe { core::mem::transmute(bits) }
  }
}

impl Clone for m512 {
  #[inline(always)]
  fn clone(&self) -> Self {
    *self
  }
}
impl Copy for m512 {}

impl Default for m512 {
  #[inline(always)]
  fn default() -> Self {
    unsafe { core::mem::zeroed() }
  }
}

impl From<[f32; 16]> for m512 {
  #[inline(always)]
  fn from(arr: [f32; 16]) -> Self {
    // Safety: because this semantically moves the value from the input position
    // (align4) to the output position (align64) it is fine to increase our
    // required alignment without worry.
    unsafe { core::mem::transmute(arr) }
  }
}

impl From<m512> for [f32; 16] {
  #[inline(always)]
  fn from(m: m512) -> Self {
    // We can of course transmute to a lower alignment
    unsafe { core::mem::transmute(m) }
  }
}

//
// PLEASE KEEP ALL THE FORMAT IMPL JUNK AT THE END OF THE FILE
//

impl Debug for m512 {
  /// Debug formats each float.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:?}", m512::default());
  /// assert_eq!(
  ///   &f,
  ///   "m512(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)"
  /// );
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "m512(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Debug::fmt(float, f)?;
    }
    write!(f, ")")
  }
}

impl Display for m512 {
  /// Display formats each float, and leaves the type name off of the font.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{}", m512::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Display::fmt(float, f)?;
    }
    write!(f, ")")
  }
}

impl Binary for m512 {
  /// Binary formats each float's bit pattern (via [`f32::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:b}", m512::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Binary::fmt(&float.to_bits(), f)?;
    }
    write!(f, ")")
  }
}

impl LowerExp for m512 {
  /// LowerExp formats each float.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:e}", m512::default());
  /// assert_eq!(
  ///   &f,
  ///   "(0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0, 0e0)"
  /// );
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      LowerExp::fmt(float, f)?;
    }
    write!(f, ")")
  }
}

impl UpperExp for m512 {
  /// UpperExp formats each float.
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:E}", m512::default());
  /// assert_eq!(
  ///   &f,
  ///   "(0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0, 0E0)"
  /// );
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      UpperExp::fmt(float, f)?;
    }
    write!(f, ")")
  }
}

impl LowerHex for m512 {
  /// LowerHex formats each float's bit pattern (via [`f32::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:x}", m512::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      LowerHex::fmt(&float.to_bits(), f)?;
    }
    write!(f, ")")
  }
}

impl UpperHex for m512 {
  /// UpperHex formats each float's bit pattern (via [`f32::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:X}", m512::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      UpperHex::fmt(&float.to_bits(), f)?;
    }
    write!(f, ")")
  }
}

impl Octal for m512 {
  /// Octal formats each float's bit pattern (via [`f32::to_bits`]).
  /// ```
  /// # use safe_arch::*;
  /// let f = format!("{:o}", m512::default());
  /// assert_eq!(&f, "(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)");
  /// ```
  fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
    write!(f, "(")?;
    for (i, float) in self.to_array().iter().enumerate() {
      if i != 0 {
        write!(f, ", ")?;
      }
      Octal::fmt(&float.to_bits(), f)?;
    }
    write!(f, ")")
  }
}
