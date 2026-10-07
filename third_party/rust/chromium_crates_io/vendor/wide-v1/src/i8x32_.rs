use super::*;

pick! {
  if #[cfg(target_feature="avx2")] {
    /// A SIMD vector with 32 elements of type [`i8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct i8x32 { avx: m256i }
  } else {
    /// A SIMD vector with 32 elements of type [`i8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct i8x32 { a : i8x16, b : i8x16 }
  }
}

impl_simd_int! {
  unsafe {
    T = i8,
    N = 32,
    Simd = i8x32,
    UintSimd = u8x32,
    T_BITS = 8,
    T_BITS_MUL_2 = 16,
    BitmaskType = u32,
    [
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
      21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    ],
    optional_type_x86_inner { X86Inner = __m256i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    rhs.simd_gt(self)
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx : cmp_gt_mask_i8_m256i(self.avx,rhs.avx) }
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
      if #[cfg(target_feature="avx2")] {
        !self.simd_gt(rhs)
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
      if #[cfg(target_feature="avx2")] {
        !self.simd_lt(rhs)
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u8x32) -> Self::Output {
    // For x86, this technically can be done explicitly by converting to `i16`
    // or `i32` then converting back after multiplication, but that may not
    // actually be faster than auto-vectorization.
    let [self_a, self_b]: [i8x16; 2] = cast(self);
    let [rhs_a, rhs_b]: [u8x16; 2] = cast(rhs);
    cast([self_a >> rhs_a, self_b >> rhs_b])
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    // For x86, this technically can be done explicitly by converting
    // to `i16` or `i32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    let [self_a, self_b]: [i8x16; 2] = cast(self);
    cast([self_a >> rhs, self_b >> rhs])
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: max_i8_m256i(self.avx,rhs.avx) }
      } else {
        Self {
          a : self.a.max(rhs.a),
          b : self.b.max(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: min_i8_m256i(self.avx,rhs.avx) }
      } else {
        Self {
          a : self.a.min(rhs.a),
          b : self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i8 {
    let array: [i8x16; 2] = cast(self);
    array[0].max(array[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> i8 {
    let array: [i8x16; 2] = cast(self);
    array[0].min(array[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u8x32) -> Self {
    // For x86, this technically can be done explicitly by converting to `i16`
    // or `i32` then converting back after multiplication, but that may not
    // actually be faster than auto-vectorization.
    let [self_a, self_b] = cast::<i8x32, [i8x16; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u8x32, [u8x16; 2]>(rhs);
    cast([self_a.unbounded_shr(rhs_a), self_b.unbounded_shr(rhs_b)])
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    // For x86, this technically can be done explicitly by converting
    // to `i16` or `i32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    let [self_a, self_b] = cast::<i8x32, [i8x16; 2]>(self);
    cast([self_a.unbounded_shr_scalar(rhs), self_b.unbounded_shr_scalar(rhs)])
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: add_saturating_i8_m256i(self.avx, rhs.avx) }
      } else {
        Self {
          a : self.a.saturating_add(rhs.a),
          b : self.b.saturating_add(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn saturating_sub(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: sub_saturating_i8_m256i(self.avx, rhs.avx) }
      } else {
        Self {
          a : self.a.saturating_sub(rhs.a),
          b : self.b.saturating_sub(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn overflowing_mul(self, rhs: Self) -> (Self, Self) {
    let (low, high) = self.mul_keep_low_high(rhs);
    let low = cast::<u8x32, i8x32>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> i16x32 {
      // x86 has no `_mm256_mul_epi8` intrinsic so there is no `avx2`
      // optimization.

      let [self_a, self_b] = cast::<i8x32, [i8x16; 2]>(self);
      let [rhs_a, rhs_b] = cast::<i8x32, [i8x16; 2]>(rhs);

      cast([self_a.widening_mul(rhs_a), self_b.widening_mul(rhs_b)])
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u8x32, i8x32) {
    // x86 has no `_mm256_mul_epi8` intrinsic so there is no `avx2`
    // optimization.

    let [self_a, self_b] = cast::<i8x32, [i8x16; 2]>(self);
    let [rhs_a, rhs_b] = cast::<i8x32, [i8x16; 2]>(rhs);

    let result_a = self_a.mul_keep_low_high(rhs_a);
    let result_b = self_b.mul_keep_low_high(rhs_b);
    (cast([result_a.0, result_b.0]), cast([result_a.1, result_b.1]))
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    // x86 has no `_mm256_mul_epi8` intrinsic so there is no `avx2`
    // optimization.

    let [self_a, self_b] = cast::<i8x32, [i8x16; 2]>(self);
    let [rhs_a, rhs_b] = cast::<i8x32, [i8x16; 2]>(rhs);

    cast([self_a.mul_keep_high(rhs_a), self_b.mul_keep_high(rhs_b)])
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: abs_i8_m256i(self.avx) }
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

/// The following functionality exists only for [`i8x32`], or only for
/// particular types inconsistently.
impl i8x32 {
  /// Returns a new vector with lanes selected from the lanes of the first input
  /// vector a specified in the second input vector `rhs`.
  /// The indices i in range `[0, 15]` select the i-th element of `self`. For
  /// indices outside of the range the resulting lane is `0`.
  ///
  /// This note that is the equivalent of two parallel swizzle operations on the
  /// two halves of the vector, and the indexes each refer to the
  /// corresponding half.
  #[inline]
  pub fn swizzle_half(self, rhs: i8x32) -> i8x32 {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: shuffle_av_i8z_half_m256i(self.avx, add_saturating_u8_m256i(rhs.avx, set_splat_i8_m256i(0x70))) }
      } else {
          Self {
            a : self.a.shuffle_zeroing(rhs.a.cast_unsigned()),
            b : self.b.shuffle_zeroing(rhs.b.cast_unsigned()),
          }
      }
    }
  }

  /// Indices in the range `[0, 15]` will select the i-th element of `self`. If
  /// the high bit of any element of `rhs` is set (negative) then the
  /// corresponding output lane is guaranteed to be zero. Otherwise if the
  /// element of `rhs` is within the range `[32, 127]` then the output lane is
  /// either `0` or `self[rhs[i] % 16]` depending on the implementation.
  ///
  /// This is the equivalent to two parallel swizzle operations on the two
  /// halves of the vector, and the indexes each refer to their corresponding
  /// half.
  #[inline]
  pub fn swizzle_half_relaxed(self, rhs: i8x32) -> i8x32 {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: shuffle_av_i8z_half_m256i(self.avx, rhs.avx) }
      } else {
        Self {
          a : self.a.shuffle(rhs.a.cast_unsigned()),
          b : self.b.shuffle(rhs.b.cast_unsigned()),
        }
      }
    }
  }

  /// Full 32-entry byte table lookup.
  ///
  /// * An index (interpreted as unsigned) in `[0, 31]` selects `self[index]`.
  /// * Any index `>= 32` (including negative `i8` values) yields `0`.
  ///
  /// Unlike [`swizzle_half`](Self::swizzle_half), indices address the entire
  /// 32-byte vector, not just their own 16-byte half.
  ///
  /// This function has been deprecated and replaced with [`shuffle_zeroing`].
  ///
  /// [`shuffle_zeroing`]: Self::shuffle_zeroing
  #[inline]
  #[deprecated(since = "1.7.0", note = "replaced with `shuffle_zeroing`")]
  pub fn swizzle(self, rhs: i8x32) -> i8x32 {
    self.shuffle_zeroing(rhs.cast_unsigned())
  }

  /// Like [`swizzle`](Self::swizzle), but out-of-range indices (unsigned
  /// `>= 32`) yield an implementation-defined result (`0` or `self[index %
  /// 32]`). Prefer this when you know all indices are in range; it can be
  /// cheaper.
  ///
  /// This function has been deprecated and replaced with [`shuffle`].
  ///
  /// [`shuffle`]: Self::shuffle
  #[inline]
  #[deprecated(since = "1.7.0", note = "replaced with `shuffle`")]
  pub fn swizzle_relaxed(self, rhs: i8x32) -> i8x32 {
    self.shuffle(rhs.cast_unsigned())
  }
}
