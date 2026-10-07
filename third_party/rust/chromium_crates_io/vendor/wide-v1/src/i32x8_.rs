use super::*;

pick! {
  if #[cfg(target_feature="avx2")] {
    /// A SIMD vector with eight elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct i32x8 { pub(crate) avx2: m256i }
  } else {
    /// A SIMD vector with eight elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct i32x8 { pub(crate) a : i32x4, pub(crate) b : i32x4}
  }
}

impl_simd_int! {
  unsafe {
    T = i32,
    N = 8,
    Simd = i32x8,
    UintSimd = u32x8,
    T_BITS = 32,
    T_BITS_MUL_2 = 64,
    BitmaskType = u32,
    [0, 1, 2, 3, 4, 5, 6, 7],
    optional_type_x86_inner { X86Inner = __m256i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: cmp_gt_mask_i32_m256i(rhs.avx2, self.avx2) }
      } else {
        Self {
          a : self.a.simd_lt(rhs.a),
          b : self.b.simd_lt(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: cmp_gt_mask_i32_m256i(self.avx2, rhs.avx2) }
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
  fn shr(self, rhs: u32x8) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // ensure same behavior as scalar
        let shift_by = bitand_m256i(rhs.avx2, set_splat_i32_m256i(31));
        Self { avx2: shr_each_i32_m256i(self.avx2, shift_by ) }
      } else {
        Self {
          a : self.a.shr(rhs.a),
          b : self.b.shr(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 31, 0]);
        Self { avx2: shr_all_i32_m256i(self.avx2, shift) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: max_i32_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: min_i32_m256i(self.avx2, rhs.avx2) }
      } else {
        Self {
          a : self.a.min(rhs.a),
          b : self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i32 {
    let arr: [i32x4; 2] = cast(self);
    arr[0].max(arr[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> i32 {
    let arr: [i32x4; 2] = cast(self);
    arr[0].min(arr[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u32x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shr_each_i32_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shr_all_i32_m256i(self.avx2, cast([rhs as u64, 0])) }
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
      if #[cfg(target_feature="avx2")] {
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
      if #[cfg(target_feature="avx2")] {
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
    let low = cast::<u32x8, i32x8>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> i64x8 {
      pick! {
        if #[cfg(all(target_feature="avx512f", target_feature="avx2"))] {
          const SHUFFLE_INDICES: m512i = i64x8::new([0, 4, 1, 5, 2, 6, 3, 7]).avx512;

          let even_wide_mul = mul_i64_low_bits_m256i(self.avx2, rhs.avx2);
          let odd_wide_mul = mul_i64_low_bits_m256i(
            shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(self.avx2),
            shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(rhs.avx2),
          );
          let even_then_odd = cast::<[m256i; 2], m512i>([even_wide_mul, odd_wide_mul]);
          i64x8 {
            avx512: permute_i64_m512i(SHUFFLE_INDICES, even_then_odd),
          }
        } else if #[cfg(target_feature="avx2")] {
          let even_wide_mul = mul_i64_low_bits_m256i(self.avx2, rhs.avx2);
          let odd_wide_mul = mul_i64_low_bits_m256i(
            shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(self.avx2),
            shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(rhs.avx2),
          );
          let m0145 = unpack_low_i64_m256i(even_wide_mul, odd_wide_mul);
          let m2367 = unpack_high_i64_m256i(even_wide_mul, odd_wide_mul);

          cast([
            shuffle_abi_i128z_all_m256i::<0b_0010_0000>(m0145, m2367),
            shuffle_abi_i128z_all_m256i::<0b_0011_0001>(m0145, m2367),
          ])
        } else {
          let [self_a, self_b] = cast::<i32x8, [i32x4; 2]>(self);
          let [rhs_a, rhs_b] = cast::<i32x8, [i32x4; 2]>(rhs);

          cast([self_a.widening_mul(rhs_a), self_b.widening_mul(rhs_b)])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u32x8, i32x8) {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let even_wide_mul = mul_i64_low_bits_m256i(self.avx2, rhs.avx2);
        let odd_wide_mul = mul_i64_low_bits_m256i(
          shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(self.avx2),
          shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(rhs.avx2),
        );
        let ll_hh_1 = unpack_low_i32_m256i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m256i(even_wide_mul, odd_wide_mul);

        (
          u32x8 { avx2: unpack_low_i64_m256i(ll_hh_1, ll_hh_2) },
          i32x8 { avx2: unpack_high_i64_m256i(ll_hh_1, ll_hh_2) },
        )
      } else {
        let [self_a, self_b] = cast::<i32x8, [i32x4; 2]>(self);
        let [rhs_a, rhs_b] = cast::<i32x8, [i32x4; 2]>(rhs);

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
      if #[cfg(target_feature="avx2")] {
        let even_wide_mul = mul_i64_low_bits_m256i(self.avx2, rhs.avx2);
        let odd_wide_mul = mul_i64_low_bits_m256i(
          shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(self.avx2),
          shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(rhs.avx2),
        );
        let ll_hh_1 = unpack_low_i32_m256i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m256i(even_wide_mul, odd_wide_mul);

        Self { avx2: unpack_high_i64_m256i(ll_hh_1, ll_hh_2) }
      } else {
        let [self_a, self_b] = cast::<i32x8, [i32x4; 2]>(self);
        let [rhs_a, rhs_b] = cast::<i32x8, [i32x4; 2]>(rhs);

        cast([self_a.mul_keep_high(rhs_a), self_b.mul_keep_high(rhs_b)])
      }
    }
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: abs_i32_m256i(self.avx2) }
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

impl From<i16x8> for i32x8 {
  #[inline]
  fn from(value: i16x8) -> Self {
    i32x8::from_i16x8(value)
  }
}

/// The following functionality exists only for [`i32x8`], or only for
/// particular types inconsistently.
impl i32x8 {
  /// Converts each element from [`i16`] to [`i32`].
  #[inline]
  #[must_use]
  pub fn from_i16x8(v: i16x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        i32x8 { avx2:convert_to_i32_m256i_from_i16_m128i(v.sse) }
      } else if #[cfg(target_feature="sse2")] {
        i32x8 {
          a: i32x4 { sse: shr_imm_i32_m128i::<16>( unpack_low_i16_m128i(v.sse, v.sse)) },
          b: i32x4 { sse: shr_imm_i32_m128i::<16>( unpack_high_i16_m128i(v.sse, v.sse)) },
        }
      } else {
        i32x8::new([
          i32::from(v.as_array()[0]),
          i32::from(v.as_array()[1]),
          i32::from(v.as_array()[2]),
          i32::from(v.as_array()[3]),
          i32::from(v.as_array()[4]),
          i32::from(v.as_array()[5]),
          i32::from(v.as_array()[6]),
          i32::from(v.as_array()[7]),
        ])
      }
    }
  }

  /// Converts each element from [`u16`] to [`i32`].
  #[inline]
  #[must_use]
  pub fn from_u16x8(v: u16x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        i32x8 { avx2:convert_to_i32_m256i_from_u16_m128i(v.sse) }
      } else if #[cfg(target_feature="sse2")] {
        i32x8 {
          a: i32x4 { sse: shr_imm_u32_m128i::<16>( unpack_low_i16_m128i(v.sse, v.sse)) },
          b: i32x4 { sse: shr_imm_u32_m128i::<16>( unpack_high_i16_m128i(v.sse, v.sse)) },
        }
      } else {
        i32x8::new([
          i32::from(v.as_array()[0]),
          i32::from(v.as_array()[1]),
          i32::from(v.as_array()[2]),
          i32::from(v.as_array()[3]),
          i32::from(v.as_array()[4]),
          i32::from(v.as_array()[5]),
          i32::from(v.as_array()[6]),
          i32::from(v.as_array()[7]),
        ])
      }
    }
  }

  /// Converts each element from [`i32`] to [`f32`].
  #[inline]
  #[must_use]
  pub fn round_float(self) -> f32x8 {
    pick! {
      if #[cfg(target_feature="avx2")] {
        cast(convert_to_m256_from_i32_m256i(self.avx2))
      } else {
        cast([
          self.a.round_float(),
          self.b.round_float(),
        ])
      }
    }
  }
}
