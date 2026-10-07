use super::*;

pick! {
  if #[cfg(target_feature="avx512bw")] {
    /// A SIMD vector with 32 elements of type [`i16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct i16x32 { pub(crate) avx512: m512i }
  } else {
    /// A SIMD vector with 32 elements of type [`i16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct i16x32 { pub(crate) a : i16x16, pub(crate) b : i16x16 }
  }
}

impl_simd_int! {
  unsafe {
    T = i16,
    N = 32,
    Simd = i16x32,
    UintSimd = u16x32,
    T_BITS = 16,
    T_BITS_MUL_2 = 32,
    BitmaskType = u32,
    [
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
      21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    ],
    optional_type_x86_inner { X86Inner = __m512i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_i16_m512i::<{cmp_int_op!(Lt)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : rhs.a.simd_gt(self.a),
          b : rhs.b.simd_gt(self.b),
        }
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_i16_m512i::<{cmp_int_op!(Nle)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_gt(rhs.a),
          b : self.b.simd_gt(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn simd_le(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_i16_m512i::<{cmp_int_op!(Le)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_le(rhs.a),
          b : self.b.simd_le(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_i16_m512i::<{cmp_int_op!(Nlt)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u16x32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_srav_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_srav_epi16;

        // Mask `rhs` to 15 to match `wrapping_shr`.
        let rhs = bitand_m512i(rhs.avx512, set_splat_i16_m512i(15));
        // TODO(safe_arch): Add `_mm512_srav_epi16`.
        Self { avx512: m512i(unsafe { _mm512_srav_epi16(self.avx512.0, rhs.0) }) }
      } else {
        let [self_a, self_b]: [i16x16; 2] = cast(self);
        let [rhs_a, rhs_b]: [u16x16; 2] = cast(rhs);

        cast([self_a >> rhs_a, self_b >> rhs_b])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        // Use `rhs % 16` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = rhs as u16 & 15;
        Self { avx512: shr_all_i16_m512i(self.avx512, shift) }
      } else {
        Self {
          a : self.a.shr(rhs),
          b : self.b.shr(rhs),
        }
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: max_i16_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.max(rhs.a),
          b: self.b.max(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: min_i16_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.min(rhs.a),
          b: self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i16 {
    let arr: [i16x16; 2] = cast(self);
    arr[0].max(arr[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> i16 {
    let arr: [i16x16; 2] = cast(self);
    arr[0].min(arr[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u16x32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_srav_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_srav_epi16;

        // TODO(safe_arch): Add `_mm512_srav_epi16`.
        Self { avx512: m512i(unsafe { _mm512_srav_epi16(self.avx512.0, rhs.avx512.0) }) }
      } else {
        let [self_a, self_b] = cast::<i16x32, [i16x16; 2]>(self);
        let [rhs_a, rhs_b] = cast::<u16x32, [u16x16; 2]>(rhs);

        cast([self_a.unbounded_shr(rhs_a), self_b.unbounded_shr(rhs_b)])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        // `u32 as u16` truncates the higher half so we need to manually
        // saturate.
        Self { avx512: shr_all_i16_m512i(self.avx512, rhs.min(u16::MAX as u32) as u16) }
      } else {
        Self {
          a: self.a.unbounded_shr_scalar(rhs),
          b: self.b.unbounded_shr_scalar(rhs),
        }
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: add_saturating_i16_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.saturating_add(rhs.a),
          b: self.b.saturating_add(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn saturating_sub(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: sub_saturating_i16_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.saturating_sub(rhs.a),
          b: self.b.saturating_sub(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn overflowing_mul(self, rhs: Self) -> (Self, Self) {
    let (low, high) = self.mul_keep_low_high(rhs);
    let low = cast::<u16x32, i16x32>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `i32x32` type.
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u16x32, i16x32) {
    // x86 has no `_mm512_mul_epi16` intrinsic so there is no `avx512`
    // optimization.

    let [self_a, self_b] = cast::<i16x32, [i16x16; 2]>(self);
    let [rhs_a, rhs_b] = cast::<i16x32, [i16x16; 2]>(rhs);

    let result_a = self_a.mul_keep_low_high(rhs_a);
    let result_b = self_b.mul_keep_low_high(rhs_b);
    (cast([result_a.0, result_b.0]), cast([result_a.1, result_b.1]))
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    // x86 has no `_mm512_mul_epi16` intrinsic so there is no `avx512`
    // optimization.

    let [self_a, self_b] = cast::<i16x32, [i16x16; 2]>(self);
    let [rhs_a, rhs_b] = cast::<i16x32, [i16x16; 2]>(rhs);

    cast([self_a.mul_keep_high(rhs_a), self_b.mul_keep_high(rhs_b)])
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: abs_i16_m512i(self.avx512) }
      } else {
        Self {
          a : self.a.abs(),
          b : self.b.abs(),
        }
      }
    }
  }

  #[inline]
  pub fn is_positive(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        // `neon` has dedicated greater-than-zero intrinsics.
        Self {
          a: self.a.is_positive(),
          b: self.b.is_positive(),
        }
      } else {
        self.simd_gt(Self::ZERO)
      }
    }
  }

  #[inline]
  pub fn is_negative(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        // `neon` has dedicated less-than-zero intrinsics.
        Self {
          a: self.a.is_negative(),
          b: self.b.is_negative(),
        }
      } else {
        self.simd_lt(Self::ZERO)
      }
    }
  }

  optional_fn_deserialize {}
}

impl From<i8x32> for i16x32 {
  /// widen with sign extend from i8 to i16
  #[inline]
  fn from(i: i8x32) -> Self {
    i16x32::from_i8x32(i)
  }
}

/// The following functionality exists only for [`i16x32`], or only for
/// particular types inconsistently.
impl i16x32 {
  /// Converts each element from [`i8`] to [`i16`].
  #[inline]
  #[must_use]
  pub fn from_i8x32(v: i8x32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: convert_to_i16_m512i_from_i8_m256i(cast(v)) }
      } else {
        let [a, b]: [i8x16; 2] = cast(v);
        Self {
          a: i16x16::from_i8x16(a),
          b: i16x16::from_i8x16(b),
        }
      }
    }
  }

  /// Partially computes the dot product.
  ///
  /// First this multiplies the input 16-bit integers, producing intermediate
  /// 32-bit integers. Then this horizontally adds adjacent pairs, resulting in
  /// sixteen 32-bit integers.
  #[inline]
  #[must_use]
  pub fn dot(self, rhs: Self) -> i32x16 {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        i32x16 { avx512: mul_i16_horizontal_add_m512i(self.avx512, rhs.avx512) }
      } else {
        i32x16 {
          a : self.a.dot(rhs.a),
          b : self.b.dot(rhs.b),
        }
      }
    }
  }
}
