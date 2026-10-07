use super::*;

pick! {
  if #[cfg(target_feature="avx512f")] {
    /// A SIMD vector with 16 elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct i32x16 { pub(crate) avx512: m512i }
  } else {
    /// A SIMD vector with 16 elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct i32x16 { pub(crate) a : i32x8, pub(crate) b : i32x8 }
  }
}

impl_simd_int! {
  unsafe {
    T = i32,
    N = 16,
    Simd = i32x16,
    UintSimd = u32x16,
    T_BITS = 32,
    T_BITS_MUL_2 = 64,
    BitmaskType = u32,
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
    optional_type_x86_inner { X86Inner = __m512i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_i32_m512i::<{cmp_int_op!(Lt)}>(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_i32_m512i::<{cmp_int_op!(Nle)}>(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_i32_m512i::<{cmp_int_op!(Le)}>(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_i32_m512i::<{cmp_int_op!(Nlt)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32x16) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_srav_epi32;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_srav_epi32;

        // Mask `rhs` to 31 to match `wrapping_shr`.
        let rhs = bitand_m512i(rhs.avx512, set_splat_i32_m512i(31));
        // TODO(safe_arch): Add `_mm512_srav_epi32`.
        Self { avx512: m512i(unsafe { _mm512_srav_epi32(self.avx512.0, rhs.0) }) }
      } else {
        Self {
          a: self.a >> rhs.a,
          b: self.b >> rhs.b,
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = rhs & 31;
        Self { avx512: shr_all_i32_m512i(self.avx512, shift) }
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
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: max_i32_m512i(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: min_i32_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.min(rhs.a),
          b: self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i32 {
    let arr: [i32x8; 2] = cast(self);
    arr[0].max(arr[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> i32 {
    let arr: [i32x8; 2] = cast(self);
    arr[0].min(arr[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u32x16) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_srav_epi32;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_srav_epi32;

        // TODO(safe_arch): Add `_mm512_srav_epi32`.
        Self { avx512: m512i(unsafe { _mm512_srav_epi32(self.avx512.0, rhs.avx512.0) }) }
      } else {
        Self {
          a: self.a.unbounded_shr(rhs.a),
          b: self.b.unbounded_shr(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: shr_all_i32_m512i(self.avx512, rhs) }
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
      if #[cfg(target_feature="avx512f")] {
        let result = self + rhs;
        let overflow = (!(self ^ rhs) & (self ^ result)).is_negative();
        let negative = self.is_negative();

        // If overflow occurs return `MAX` if positive or `MIN` if negative.
        overflow.select(Self::MAX ^ negative, result)
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
      if #[cfg(target_feature="avx512f")] {
        let result = self - rhs;
        let overflow = ((self ^ rhs) & (self ^ result)).is_negative();
        let negative = self.is_negative();

        // If overflow occurs return `MAX` if positive or `MIN` if negative.
        overflow.select(Self::MAX ^ negative, result)
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
    let low = cast::<u32x16, i32x16>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `i64x16` type.
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u32x16, i32x16) {
    pick! {
      if #[cfg(all(target_feature="avx512f", target_feature="avx512dq"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::{_mm512_unpackhi_epi64, _mm512_unpacklo_epi64};
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::{_mm512_unpackhi_epi64, _mm512_unpacklo_epi64};

        let even_wide_mul = mul_i32_wide_m512i(self.avx512, rhs.avx512);
        let odd_wide_mul = mul_i32_wide_m512i(
          shuffle_i32_m512i::<0b_00_11_00_01>(self.avx512),
          shuffle_i32_m512i::<0b_00_11_00_01>(rhs.avx512),
        );
        let ll_hh_1 = unpack_low_i32_m512i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m512i(even_wide_mul, odd_wide_mul);
        // TODO(safe_arch): Add `_mm512_unpacklo_epi64` and `_mm512_unpackhi_epi64`.
        (
          u32x16 {
            avx512: m512i(unsafe { _mm512_unpacklo_epi64(ll_hh_1.0, ll_hh_2.0) }),
          },
          i32x16 {
            avx512: m512i(unsafe { _mm512_unpackhi_epi64(ll_hh_1.0, ll_hh_2.0) }),
          },
        )
      } else {
        let [self_a, self_b] = cast::<i32x16, [i32x8; 2]>(self);
        let [rhs_a, rhs_b] = cast::<i32x16, [i32x8; 2]>(rhs);

        let result_a = self_a.mul_keep_low_high(rhs_a);
        let result_b = self_b.mul_keep_low_high(rhs_b);
        (
          cast([result_a.0, result_b.0]),
          cast([result_a.1, result_b.1]),
        )
      }
    }
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f", target_feature="avx512dq"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_unpackhi_epi64;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_unpackhi_epi64;

        let even_wide_mul = mul_i32_wide_m512i(self.avx512, rhs.avx512);
        let odd_wide_mul = mul_i32_wide_m512i(
          shuffle_i32_m512i::<0b_00_11_00_01>(self.avx512),
          shuffle_i32_m512i::<0b_00_11_00_01>(rhs.avx512),
        );
        let ll_hh_1 = unpack_low_i32_m512i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m512i(even_wide_mul, odd_wide_mul);
        // TODO(safe_arch): Add `_mm512_unpackhi_epi64`.
        Self {
          avx512: m512i(unsafe { _mm512_unpackhi_epi64(ll_hh_1.0, ll_hh_2.0) }),
        }
      } else {
        let [self_a, self_b] = cast::<i32x16, [i32x8; 2]>(self);
        let [rhs_a, rhs_b] = cast::<i32x16, [i32x8; 2]>(rhs);

        cast([self_a.mul_keep_high(rhs_a), self_b.mul_keep_high(rhs_b)])
      }
    }
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: abs_i32_m512i(self.avx512) }
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

impl From<i16x16> for i32x16 {
  #[inline]
  fn from(value: i16x16) -> Self {
    i32x16::from_i16x16(value)
  }
}

/// The following functionality exists only for [`i32x16`], or only for
/// particular types inconsistently.
impl i32x16 {
  /// Converts each element from [`i16`] to [`i32`].
  #[inline]
  #[must_use]
  pub fn from_i16x16(v: i16x16) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: convert_to_i32_m512i_from_i16_m256i(v.avx2) }
      } else {
        let [a, b]: [i16x8; 2] = cast(v);
        Self {
          a: i32x8::from_i16x8(a),
          b: i32x8::from_i16x8(b),
        }
      }
    }
  }

  /// Converts each element from [`i32`] to [`f32`].
  #[inline]
  #[must_use]
  pub fn round_float(self) -> f32x16 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        cast(convert_to_m512_from_i32_m512i(self.avx512))
      } else {
        f32x16 {
          a: self.a.round_float(),
          b: self.b.round_float(),
        }
      }
    }
  }
}
