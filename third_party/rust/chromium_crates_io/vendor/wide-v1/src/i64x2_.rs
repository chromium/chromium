use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with two elements of type [`i64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i64x2 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with two elements of type [`i64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct i64x2 { pub(crate) simd: v128 }

    impl Default for i64x2 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i64x2 {
      fn eq(&self, other: &Self) -> bool {
        u64x2_all_true(i64x2_eq(self.simd, other.simd))
      }
    }

    impl Eq for i64x2 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with two elements of type [`i64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct i64x2 { pub(crate) neon : int64x2_t }

    impl Default for i64x2 {
      #[inline]
      fn default() -> Self {
        unsafe { Self { neon: vdupq_n_s64(0)} }
      }
    }

    impl PartialEq for i64x2 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe {
          vgetq_lane_s64(self.neon,0) == vgetq_lane_s64(other.neon,0) && vgetq_lane_s64(self.neon,1) == vgetq_lane_s64(other.neon,1)
        }
      }
    }

    impl Eq for i64x2 { }
  } else {
    /// A SIMD vector with two elements of type [`i64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i64x2 { arr: [i64;2] }
  }
}

impl_simd_int! {
  unsafe {
    T = i64,
    N = 2,
    Simd = i64x2,
    UintSimd = u64x2,
    T_BITS = 64,
    T_BITS_MUL_2 = 128,
    BitmaskType = u32,
    [0, 1],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = int64x2_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.2")] {
        // only has gt, so flip arguments around to get lt
        Self { sse: cmp_gt_mask_i64_m128i( rhs.sse, self.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_lt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s64_u64(vcltq_s64(self.neon, rhs.neon)) }}
      } else {
        let s: [i64;2] = cast(self);
        let r: [i64;2] = cast(rhs);
        cast([
          if s[0] < r[0] { -1_i64 } else { 0 },
          if s[1] < r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.2")] {
        Self { sse: cmp_gt_mask_i64_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_gt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s64_u64(vcgtq_s64(self.neon, rhs.neon)) }}
      } else {
        let s: [i64;2] = cast(self);
        let r: [i64;2] = cast(rhs);
        cast([
          if s[0] > r[0] { -1_i64 } else { 0 },
          if s[1] > r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn simd_le(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        !self.simd_gt(rhs)
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_le(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_gt(rhs)
      } else {
        let s: [i64;2] = cast(self);
        let r: [i64;2] = cast(rhs);
        cast([
          if s[0] <= r[0] { -1_i64 } else { 0 },
          if s[1] <= r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        !self.simd_lt(rhs)
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_ge(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_lt(rhs)
      } else {
        let s: [i64;2] = cast(self);
        let r: [i64;2] = cast(rhs);
        cast([
          if s[0] >= r[0] { -1_i64 } else { 0 },
          if s[1] >= r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u64x2) -> Self::Output {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // mask the shift count to 63 to have same behavior on all platforms
          // no right shift, have to pass negative value to left shift on neon
          let shift_by = vnegq_s64(vreinterpretq_s64_u64(vandq_u64(rhs.neon, vmovq_n_u64(63))));
          Self { neon: vshlq_s64(self.neon, shift_by) }
        }
      } else {
        let arr: [i64; 2] = cast(self);
        let rhs: [u64; 2] = cast(rhs);
        cast([
          arr[0].wrapping_shr(rhs[0] as u32),
          arr[1].wrapping_shr(rhs[1] as u32),
        ])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_shr(self.simd, rhs) }
      } else {
        let arr: [i64; 2] = cast(self);
        cast([
          arr[0].wrapping_shr(rhs),
          arr[1].wrapping_shr(rhs),
        ])
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    self.simd_gt(rhs).select(self, rhs)
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    self.simd_lt(rhs).select(self, rhs)
  }

  #[inline]
  pub fn reduce_max(self) -> i64 {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let array: [i64; 2] = cast(self);
        array[0].max(array[1])
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { vgetq_lane_s64(self.neon, 0).max(vgetq_lane_s64(self.neon, 1)) }
      } else {
        self.arr[0].max(self.arr[1])
      }
    }
  }

  #[inline]
  pub fn reduce_min(self) -> i64 {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let array: [i64; 2] = cast(self);
        array[0].min(array[1])
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { vgetq_lane_s64(self.neon, 0).min(vgetq_lane_s64(self.neon, 1)) }
      } else {
        self.arr[0].min(self.arr[1])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u64x2) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_s64(self.neon, vnegq_s64(vreinterpretq_s64_u64(rhs.min(u64x2::splat(64)).neon))) }
        }
      } else {
        // Cannot use scalar `unbounded_shl` because it takes `u32`, which is
        // smaller than `u64`.
        cast::<u64x2, i64x2>(rhs.simd_lt(64)).select(
          self >> cast::<u64x2, i64x2>(rhs),
          self.is_negative(),
        )
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="simd128")] {
        if rhs < 64 { Self { simd: i64x2_shr(self.simd, rhs) } } else { self.is_negative() }
      } else {
        let self_array = self.to_array();

        Self::new([
          self_array[0].unbounded_shr(rhs),
          self_array[1].unbounded_shr(rhs),
        ])
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let result = self + rhs;
        let overflow = (!(self ^ rhs) & (self ^ result)).is_negative();
        let negative = self.is_negative();

        // If overflow occurs return `MAX` if positive or `MIN` if negative.
        overflow.select(Self::MAX ^ negative, result)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqaddq_s64(self.neon, rhs.neon) } }
      } else {
        Self {
          arr: [
            self.arr[0].saturating_add(rhs.arr[0]),
            self.arr[1].saturating_add(rhs.arr[1]),
          ],
        }
      }
    }
  }

  #[inline]
  pub fn saturating_sub(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let result = self - rhs;
        let overflow = ((self ^ rhs) & (self ^ result)).is_negative();
        let negative = self.is_negative();

        // If overflow occurs return `MAX` if positive or `MIN` if negative.
        overflow.select(Self::MAX ^ negative, result)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_s64(self.neon, rhs.neon) } }
      } else {
        Self {
          arr: [
            self.arr[0].saturating_sub(rhs.arr[0]),
            self.arr[1].saturating_sub(rhs.arr[1]),
          ],
        }
      }
    }
  }

  #[inline]
  pub fn overflowing_mul(self, rhs: Self) -> (Self, Self) {
    // TODO(perf): This implementation looks quite bad. Is there a better
    // one? This intentionally avoids `mul_keep_low_high` because getting the
    // high bits of 64-bit multiplication could be slow.

    let self_array = self.to_array();
    let rhs_array = rhs.to_array();

    let result = [
      self_array[0].overflowing_mul(rhs_array[0]),
      self_array[1].overflowing_mul(rhs_array[1]),
    ];
    (
      Self::new([result[0].0, result[1].0]),
      Self::new([-(result[0].1 as i64), -(result[1].1 as i64)]),
    )
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `i128x2` type.
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u64x2, i64x2) {
    // TODO(perf): This implementation looks quite bad. Is there a better
    // one?

    let self_array = self.to_array();
    let rhs_array = rhs.to_array();

    let widening_mul = [
      (self_array[0] as i128).wrapping_mul(rhs_array[0] as i128),
      (self_array[1] as i128).wrapping_mul(rhs_array[1] as i128),
    ];

    (
      u64x2::new([
        widening_mul[0] as u64,
        widening_mul[1] as u64,
      ]),
      i64x2::new([
        (widening_mul[0] >> 64) as i64,
        (widening_mul[1] >> 64) as i64,
      ]),
    )
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    let self_array = self.to_array();
    let rhs_array = rhs.to_array();

    Self::new([
      ((self_array[0] as i128).wrapping_mul(rhs_array[0] as i128) >> 64) as i64,
      ((self_array[1] as i128).wrapping_mul(rhs_array[1] as i128) >> 64) as i64,
    ])
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      // x86 doesn't have this builtin
      if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_abs(self.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vabsq_s64(self.neon) }}
      } else {
        let arr: [i64; 2] = cast(self);
        cast(
          [
            arr[0].wrapping_abs(),
            arr[1].wrapping_abs(),
          ])
      }
    }
  }

  #[inline]
  pub fn is_positive(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s64_u64(vcgtzq_s64(self.neon)) } }
      } else {
        self.simd_gt(Self::ZERO)
      }
    }
  }

  #[inline]
  pub fn is_negative(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s64_u64(vcltzq_s64(self.neon)) } }
      } else {
        self.simd_lt(Self::ZERO)
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`i64x2`], or only for
/// particular types inconsistently.
impl i64x2 {
  /// Converts each element from [`i64`] to [`f64`].
  #[inline]
  #[must_use]
  pub fn round_float(self) -> f64x2 {
    let arr: [i64; 2] = cast(self);
    cast([arr[0] as f64, arr[1] as f64])
  }

  // Sometimes used for `transpose`.
  #[must_use]
  #[inline]
  #[allow(dead_code)]
  pub(crate) fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: unpack_low_i64_m128i(self.sse, b.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_shuffle::<0, 2>(self.simd, b.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        Self { neon: unsafe { vzip1q_s64(self.neon, b.neon) } }
      } else {
        Self::new([self.as_array()[0], b.as_array()[0]])
      }
    }
  }

  // Sometimes used for `transpose`.
  #[must_use]
  #[inline]
  #[allow(dead_code)]
  pub(crate) fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: unpack_high_i64_m128i(self.sse, b.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_shuffle::<1, 3>(self.simd, b.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        Self { neon: unsafe { vzip2q_s64(self.neon, b.neon) } }
      } else {
        Self::new([self.as_array()[1], b.as_array()[1]])
      }
    }
  }
}
