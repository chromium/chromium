use super::*;

pick! {
  if #[cfg(target_feature="avx512f")] {
    /// A SIMD vector with 16 elements of type [`f32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq)]
    #[repr(C, align(64))]
    pub struct f32x16 { pub(crate) avx512: m512 }
  } else {
    /// A SIMD vector with 16 elements of type [`f32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq)]
    #[repr(C, align(64))]
    pub struct f32x16 { pub(crate) a : f32x8, pub(crate) b : f32x8 }
  }
}

macro_rules! const_f32_as_f32x16 {
  ($i:ident, $f:expr) => {
    #[allow(non_upper_case_globals)]
    pub const $i: f32x16 = f32x16::new([$f; 16]);
  };
}

impl_simd_float! {
  unsafe {
    T = f32,
    N = 16,
    Simd = f32x16,
    IntSimd = i32x16,
    UintT = u32,
    UintSimd = u32x16,
    optional_type_x86_inner { X86Inner = __m512 },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }
  old_powf_simd_fn_name = pow_f32x16,

  #[inline]
  fn neg(self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitxor_m512(self.avx512, Self::splat(-0.0).avx512) }
      } else {
        Self {
          a : self.a.neg(),
          b : self.b.neg(),
        }
      }
    }
  }

  #[inline]
  fn not(self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitxor_m512(self.avx512, set_splat_m512(f32::from_bits(u32::MAX))) }
      } else {
        Self {
          a : self.a.not(),
          b : self.b.not(),
        }
      }
    }
  }

  #[inline]
  fn add(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: add_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.add(rhs.a),
          b : self.b.add(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn sub(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: sub_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.sub(rhs.a),
          b : self.b.sub(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn mul(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: mul_m512(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.mul(rhs.a), b: self.b.mul(rhs.b) }
      }
    }
  }

  #[inline]
  fn div(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: div_m512(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.div(rhs.a), b: self.b.div(rhs.b) }
      }
    }
  }

  #[inline]
  fn rem(self, rhs: Self) -> Self::Output {
    Self::new([
      self.to_array()[0] % rhs.to_array()[0],
      self.to_array()[1] % rhs.to_array()[1],
      self.to_array()[2] % rhs.to_array()[2],
      self.to_array()[3] % rhs.to_array()[3],
      self.to_array()[4] % rhs.to_array()[4],
      self.to_array()[5] % rhs.to_array()[5],
      self.to_array()[6] % rhs.to_array()[6],
      self.to_array()[7] % rhs.to_array()[7],
      self.to_array()[8] % rhs.to_array()[8],
      self.to_array()[9] % rhs.to_array()[9],
      self.to_array()[10] % rhs.to_array()[10],
      self.to_array()[11] % rhs.to_array()[11],
      self.to_array()[12] % rhs.to_array()[12],
      self.to_array()[13] % rhs.to_array()[13],
      self.to_array()[14] % rhs.to_array()[14],
      self.to_array()[15] % rhs.to_array()[15],
    ])
  }

  #[inline]
  fn bitand(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitand_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.bitand(rhs.a),
          b : self.b.bitand(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn bitor(self, rhs: Self) -> Self::Output {
    pick! {
    if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitor_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.bitor(rhs.a),
          b : self.b.bitor(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn bitxor(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitxor_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.bitxor(rhs.a),
          b : self.b.bitxor(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn simd_eq(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(EqualOrdered)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_eq(rhs.a),
          b : self.b.simd_eq(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn simd_ne(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(NotEqualUnordered)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ne(rhs.a),
          b : self.b.simd_ne(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(LessThanOrdered)}>(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(GreaterThanOrdered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(LessEqualOrdered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(GreaterEqualOrdered)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> f32 {
    pick! {
      if #[cfg(target_feature="avx512f")]{
        reduce_add_m512(self.avx512)
      } else {
        self.a.reduce_add() + self.b.reduce_add()
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> f32 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // TODO: Add `reduce_mul_m512` to `safe_arch` then make this function
        // safe.
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_reduce_mul_ps;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_reduce_mul_ps;

        unsafe { _mm512_reduce_mul_ps(self.avx512.0) }
      } else {
        self.a.reduce_mul() * self.b.reduce_mul()
      }
    }
  }

  #[inline]
  pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self {
          avx512: bitor_m512(
            bitand_m512(if_one.avx512, self.avx512),
            bitandnot_m512(self.avx512, if_zero.avx512),
          ),
        }
      } else {
        Self {
          a: self.a.bitselect(if_one.a, if_zero.a),
          b: self.b.bitselect(if_one.b, if_zero.b),
        }
      }
    }
  }

  #[inline]
  fn select(self, if_true: Self, if_false: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: blend_varying_m512(if_false.avx512, if_true.avx512, movepi32_mask_m512(self.avx512)) }
      } else {
        Self {
          a : self.a.select(if_true.a, if_false.a),
          b : self.b.select(if_true.b, if_false.b),
        }
      }
    }
  }

  #[inline]
  pub fn to_bitmask(self) -> u32 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        movepi32_mask_m512(self.avx512) as u32
      } else {
        (self.b.to_bitmask() << 8) | self.a.to_bitmask()
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        movepi32_mask_m512(self.avx512) != 0
      } else {
        self.a.any() || self.b.any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        movepi32_mask_m512(self.avx512) == !0_u16
      } else {
        self.a.all() && self.b.all()
      }
    }
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [f32x16; 16]) -> [f32x16; 16] {
    // TODO: Add `_mm512_unpackhi_ps` to `safe_arch`, looks like it is missing,
    // then try adding an optimized `avx512f` implementation.

    #[inline(always)]
    fn transpose_column(data: &[f32x16; 16], index: usize) -> f32x16 {
      f32x16::new([
        data[0].as_array()[index],
        data[1].as_array()[index],
        data[2].as_array()[index],
        data[3].as_array()[index],
        data[4].as_array()[index],
        data[5].as_array()[index],
        data[6].as_array()[index],
        data[7].as_array()[index],
        data[8].as_array()[index],
        data[9].as_array()[index],
        data[10].as_array()[index],
        data[11].as_array()[index],
        data[12].as_array()[index],
        data[13].as_array()[index],
        data[14].as_array()[index],
        data[15].as_array()[index],
      ])
    }

    [
      transpose_column(&data, 0),
      transpose_column(&data, 1),
      transpose_column(&data, 2),
      transpose_column(&data, 3),
      transpose_column(&data, 4),
      transpose_column(&data, 5),
      transpose_column(&data, 6),
      transpose_column(&data, 7),
      transpose_column(&data, 8),
      transpose_column(&data, 9),
      transpose_column(&data, 10),
      transpose_column(&data, 11),
      transpose_column(&data, 12),
      transpose_column(&data, 13),
      transpose_column(&data, 14),
      transpose_column(&data, 15),
    ]
  }

  #[inline]
  pub fn is_nan(self) -> Self {
    pick! {
      if #[cfg(target_feature = "avx512f")] {
        Self { avx512: cmp_op_mask_m512::<{cmp_op!(Unordered)}>(self.avx512, self.avx512) }
      } else {
        Self {
          a: self.a.is_nan(),
          b: self.b.is_nan(),
        }
      }
    }
  }

  #[inline]
  pub fn is_inf(self) -> Self {
    let shifted_inf = u32x16::from(0xFF000000);
    let u: u32x16 = cast(self);
    let shift_u = u << 1_u64;
    let out = (shift_u).simd_eq(shifted_inf);
    cast(out)
  }

  #[inline]
  pub fn is_finite(self) -> Self {
    let shifted_exp_mask = u32x16::splat(0xFF000000);
    let u: u32x16 = cast(self);
    let shift_u = u << 1_u32;
    let out = !(shift_u & shifted_exp_mask).simd_eq(shifted_exp_mask);
    cast(out)
  }

  #[inline]
  pub fn is_sign_positive(self) -> Self {
    const SIGN_MASK: u32x16 = u32x16::splat((-0.0_f32).to_bits());

    let bits = cast::<f32x16, u32x16>(self);
    let sign = bits & SIGN_MASK;
    let result = sign.simd_eq(u32x16::ZERO);
    cast::<u32x16, f32x16>(result)
  }

  #[inline]
  pub fn is_sign_negative(self) -> Self {
    const SIGN_MASK: u32x16 = u32x16::splat((-0.0_f32).to_bits());

    let bits = cast::<f32x16, u32x16>(self);
    let sign = bits & SIGN_MASK;
    let result = sign.simd_eq(SIGN_MASK);
    cast::<u32x16, f32x16>(result)
  }

  #[inline]
  pub fn recip(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // TODO: Add `_mm512_rcp14_ps` to `safe_arch`, looks like it is missing,
        // then consider updating this implementation if the relative error is
        // acceptable.
        1.0 / self
      } else {
        Self {
          a : self.a.recip(),
          b : self.b.recip(),
        }
      }
    }
  }

  #[inline]
  pub fn recip_sqrt(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // TODO: Add `_mm512_rsqrt14_ps` to `safe_arch`, looks like it is
        // missing, then consider updating this implementation if the relative
        // error is acceptable.
        self.sqrt().recip()
      } else {
        Self {
          a : self.a.recip_sqrt(),
          b : self.b.recip_sqrt(),
        }
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // max_m512 seems to do rhs < self ? self : rhs. So if there's any NaN
        // involved, it chooses rhs, so we need to specifically check rhs for
        // NaN.
        rhs.is_nan().select(self, Self { avx512: max_m512(self.avx512, rhs.avx512) })
      } else {
        Self {
          a: self.a.max(rhs.a),
          b: self.b.max(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn fast_max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: max_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.fast_max(rhs.a),
          b: self.b.fast_max(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // min_m512 seems to do rhs > self ? self : rhs. So if there's any NaN
        // involved, it chooses rhs, so we need to specifically check rhs for
        // NaN.
        rhs.is_nan().select(self, Self { avx512: min_m512(self.avx512, rhs.avx512) })
      } else {
        Self {
          a: self.a.min(rhs.a),
          b: self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn fast_min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: min_m512(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.fast_min(rhs.a),
          b: self.b.fast_min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn clamp(self, min: Self, max: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // This works since all bits set is NaN.
        self.fast_clamp(min, max) | min.is_nan() | max.is_nan()
      } else {
        // Some targets have better implementations than the above one.
        Self {
          a: self.a.clamp(min.a, max.a),
          b: self.b.clamp(min.b, max.b),
        }
      }
    }
  }

  #[inline]
  pub fn fast_clamp(self, min: Self, max: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // For both `min_m512` and `max_m512` if any input is NaN, `rhs` gets
        // chosen. For `self` to be chosen, `self` must be the second argument.
        Self { avx512: max_m512(min.avx512, min_m512(max.avx512, self.avx512)) }
      } else {
        Self {
          a: self.a.fast_clamp(min.a, max.a),
          b: self.b.fast_clamp(min.b, max.b),
        }
      }
    }
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        let non_sign_bits = f32x16::from(f32::from_bits(i32::MAX as u32));
        self & non_sign_bits
      } else {
        Self {
          a : self.a.abs(),
          b : self.b.abs(),
        }
      }
    }
  }

  #[inline]
  pub fn floor(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: round_m512::<{round_op!(NegInf)}>(self.avx512) }
      } else {
        Self {
          a : self.a.floor(),
          b : self.b.floor(),
        }
      }
    }
  }

  #[inline]
  pub fn ceil(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: round_m512::<{round_op!(PosInf)}>(self.avx512) }
      } else {
        Self {
          a : self.a.ceil(),
          b : self.b.ceil(),
        }
      }
    }
  }

  #[inline]
  pub fn round(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        const_f32_as_f32x16!(HALF_NEXT_DOWN, 0.5_f32.next_down());
        const_f32_as_f32x16!(BOUNDS_LIMIT, 8388608.0);

        let self_abs = self.abs();

        let adjusted_self = self_abs + Self::HALF;
        let result_abs = Self { avx512: round_m512::<{round_op!(Zero)}>(adjusted_self.avx512) };
        // The addition breaks for `0.5.next_down()` which incorrectly rounds to
        // `1.0`. This resets the result back to `0.0`.
        let result_abs = result_abs & self_abs.simd_ne(HALF_NEXT_DOWN);

        // Large value, infinity and NaN need special handling.
        let bounds_mask: Self = cast(cmp_op_mask_i32_m512i::<{cmp_int_op!(Lt)}>(
          cast(self_abs),
          cast(BOUNDS_LIMIT),
        ));

        // `abs` keeps the original sign.
        bounds_mask.abs().bitselect(result_abs, self)
      } else {
        Self {
          a: self.a.round(),
          b: self.b.round(),
        }
      }
    }
  }

  #[inline]
  pub fn round_int(self) -> i32x16 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // Based on: https://github.com/v8/v8/blob/210987a552a2bf2a854b0baa9588a5959ff3979d/src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h#L489-L504
        let non_nan_mask = self.simd_eq(self);
        let non_nan = self & non_nan_mask;
        let flip_to_max: i32x16 = cast(self.simd_ge(Self::splat(2147483648.0)));
        let cast: i32x16 = cast(convert_to_i32_m512i_from_m512(non_nan.avx512));
        flip_to_max ^ cast
      } else {
        i32x16 {
          a: self.a.round_int(),
          b: self.b.round_int(),
        }
      }
    }
  }

  #[inline]
  pub fn fast_round_int(self) -> i32x16 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        cast(convert_to_i32_m512i_from_m512(self.avx512))
      } else {
        i32x16 {
          a: self.a.fast_round_int(),
          b: self.b.fast_round_int(),
        }
      }
    }
  }

  #[inline]
  pub fn round_ties_even(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: round_m512::<{round_op!(Nearest)}>(self.avx512) }
      } else {
        Self {
          a: self.a.round_ties_even(),
          b: self.b.round_ties_even(),
        }
      }
    }
  }

  #[inline]
  pub fn trunc(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: round_m512::<{round_op!(Zero)}>(self.avx512) }
      } else {
        Self {
          a: self.a.trunc(),
          b: self.b.trunc(),
        }
      }
    }
  }

  #[inline]
  pub fn trunc_int(self) -> i32x16 {
    pick! {
        if #[cfg(target_feature="avx512f")] {
        // Based on: https://github.com/v8/v8/blob/210987a552a2bf2a854b0baa9588a5959ff3979d/src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h#L489-L504
        let non_nan_mask = self.simd_eq(self);
        let non_nan = self & non_nan_mask;
        let flip_to_max: i32x16 = cast(self.simd_ge(Self::splat(2147483648.0)));
        let cast: i32x16 = cast(convert_truncate_m512_i32_m512i(non_nan.avx512));
        flip_to_max ^ cast
      } else {
        cast([
          self.a.trunc_int(),
          self.b.trunc_int(),
        ])
      }
    }
  }

  #[inline]
  pub fn fast_trunc_int(self) -> i32x16 {
    pick! {
      if #[cfg(all(target_feature="avx512f"))] {
        cast(convert_truncate_m512_i32_m512i(self.avx512))
      } else {
        cast([
          self.a.fast_trunc_int(),
          self.b.fast_trunc_int(),
        ])
      }
    }
  }

  ///
  /// # Platform-specific behavior (may change in the future)
  ///
  /// - On `x86`/`x86_64` with AVX-512F+FMA: Uses 512-bit `vfmadd` (single
  ///   rounding, best accuracy)
  /// - On `x86`/`x86_64` with AVX-512F only: Uses `(self * m) + a` (two
  ///   roundings)
  /// - Other platforms: Delegates to [`f32x8`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_add(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_add_m512(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        (self * a) + b
      } else {
        Self {
          a: self.a.mul_add(a.a, b.a),
          b: self.b.mul_add(a.b, b.b),
        }
      }
    }
  }

  ///
  /// # Platform-specific behavior (may change in the future)
  ///
  /// - On `x86`/`x86_64` with AVX-512F+FMA: Uses 512-bit `vfmsub` (single
  ///   rounding, best accuracy)
  /// - On `x86`/`x86_64` with AVX-512F only: Uses `(self * m) - s` (two
  ///   roundings)
  /// - Other platforms: Delegates to [`f32x8`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_sub(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_sub_m512(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        (self * a) - b
      } else {
        Self {
          a: self.a.mul_sub(a.a, b.a),
          b: self.b.mul_sub(a.b, b.b),
        }
      }
    }
  }

  ///
  /// # Platform-specific behavior (may change in the future)
  ///
  /// - On `x86`/`x86_64` with AVX-512F+FMA: Uses 512-bit `vfnmadd` (single
  ///   rounding, best accuracy)
  /// - On `x86`/`x86_64` with AVX-512F only: Uses `a - (self * m)` (two
  ///   roundings)
  /// - Other platforms: Delegates to [`f32x8`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_neg_add(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_neg_add_m512(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        b - (self * a)
      } else {
        Self {
          a: self.a.mul_neg_add(a.a, b.a),
          b: self.b.mul_neg_add(a.b, b.b),
        }
      }
    }
  }

  ///
  /// # Platform-specific behavior (may change in the future)
  ///
  /// - On `x86`/`x86_64` with AVX-512F+FMA: Uses 512-bit `vfnmsub` (single
  ///   rounding, best accuracy)
  /// - On `x86`/`x86_64` with AVX-512F only: Uses `-(self * m) - s` (two
  ///   roundings)
  /// - Other platforms: Delegates to [`f32x8`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_neg_sub(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_neg_sub_m512(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        -(self * a) - b
      } else {
        Self {
          a: self.a.mul_neg_sub(a.a, b.a),
          b: self.b.mul_neg_sub(a.b, b.b),
        }
      }
    }
  }

  #[inline]
  pub fn powf_simd(self, n: Self) -> Self {
    const_f32_as_f32x16!(ln2f_hi, 0.693359375);
    const_f32_as_f32x16!(ln2f_lo, -2.12194440e-4);
    const_f32_as_f32x16!(P0logf, 3.3333331174E-1);
    const_f32_as_f32x16!(P1logf, -2.4999993993E-1);
    const_f32_as_f32x16!(P2logf, 2.0000714765E-1);
    const_f32_as_f32x16!(P3logf, -1.6668057665E-1);
    const_f32_as_f32x16!(P4logf, 1.4249322787E-1);
    const_f32_as_f32x16!(P5logf, -1.2420140846E-1);
    const_f32_as_f32x16!(P6logf, 1.1676998740E-1);
    const_f32_as_f32x16!(P7logf, -1.1514610310E-1);
    const_f32_as_f32x16!(P8logf, 7.0376836292E-2);

    const_f32_as_f32x16!(p2expf, 1.0 / 2.0); // coefficients for Taylor expansion of exp
    const_f32_as_f32x16!(p3expf, 1.0 / 6.0);
    const_f32_as_f32x16!(p4expf, 1.0 / 24.0);
    const_f32_as_f32x16!(p5expf, 1.0 / 120.0);
    const_f32_as_f32x16!(p6expf, 1.0 / 720.0);
    const_f32_as_f32x16!(p7expf, 1.0 / 5040.0);

    let x1 = self.abs();
    let x = x1.fraction_2();
    let mask = x.simd_gt(f32x16::SQRT_2 * f32x16::HALF);
    let x = (!mask).select(x + x, x);

    let x = x - f32x16::ONE;
    let x2 = x * x;
    let lg1 = polynomial_8!(
      x, P0logf, P1logf, P2logf, P3logf, P4logf, P5logf, P6logf, P7logf, P8logf
    );
    let lg1 = lg1 * x2 * x;

    let ef = x1.exponent();
    let ef = mask.select(ef + f32x16::ONE, ef);
    let e1 = (ef * n).round_ties_even();
    let yr = ef.mul_sub(n, e1);

    let lg = f32x16::HALF.mul_neg_add(x2, x) + lg1;
    let x2_err = (f32x16::HALF * x).mul_sub(x, f32x16::HALF * x2);
    let lg_err = f32x16::HALF.mul_add(x2, lg - x) - lg1;

    let e2 = (lg * n * f32x16::LOG2_E).round_ties_even();
    let v = lg.mul_sub(n, e2 * ln2f_hi);
    let v = e2.mul_neg_add(ln2f_lo, v);
    let v = v - (lg_err + x2_err).mul_sub(n, yr * f32x16::LN_2);

    let x = v;
    let e3 = (x * f32x16::LOG2_E).round_ties_even();
    let x = e3.mul_neg_add(f32x16::LN_2, x);
    let x2 = x * x;
    let z = x2.mul_add(
      polynomial_5!(x, p2expf, p3expf, p4expf, p5expf, p6expf, p7expf),
      x + f32x16::ONE,
    );

    let ee = e1 + e2 + e3;
    let ei = cast::<_, i32x16>(ee.round_int());
    let ej = cast::<_, i32x16>(ei + (cast::<_, i32x16>(z) >> 23));

    let overflow = cast::<_, f32x16>(ej.simd_gt(i32x16::splat(0x0FF)))
      | (ee.simd_gt(f32x16::splat(300.0)));
    let underflow = cast::<_, f32x16>(ej.simd_lt(i32x16::splat(0x000)))
      | (ee.simd_lt(f32x16::splat(-300.0)));

    // Add exponent by integer addition
    let z = cast::<_, f32x16>(cast::<_, i32x16>(z) + (ei << 23));
    // Check for overflow/underflow
    let z = underflow.select(f32x16::ZERO, z);
    let z = overflow.select(Self::infinity(), z);

    // Check for self == 0
    let x_zero = self.is_zero_or_subnormal();
    let z = x_zero.select(
      n.simd_lt(f32x16::ZERO).select(
        Self::infinity(),
        n.simd_eq(f32x16::ZERO).select(f32x16::ONE, f32x16::ZERO),
      ),
      z,
    );

    let x_sign = self.is_sign_negative();
    let z = if x_sign.any() {
      // Y into an integer
      let yi = n.simd_eq(n.round_ties_even());

      // Is y odd? If yes flip the sign of the result.
      let y_odd = cast::<i32x16, f32x16>(n.round_int() << 31);

      let z1 = yi
        .select(z | y_odd, self.simd_eq(Self::ZERO).select(z, Self::nan_pow()));

      x_sign.select(z1, z)
    } else {
      z
    };

    let x_finite = self.is_finite();
    let y_finite = n.is_finite();
    let e_finite = ee.is_finite();
    if (x_finite & y_finite & (e_finite | x_zero)).all() {
      return z;
    }

    (self.is_nan() | n.is_nan()).select(self + n, z)
  }

  #[inline]
  pub fn sqrt(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: sqrt_m512(self.avx512) }
      } else {
        Self {
          a : self.a.sqrt(),
          b : self.b.sqrt(),
        }
      }
    }
  }

  /// Calculate the exponent of a packed `f32x16`
  #[inline]
  pub fn exp(self) -> Self {
    const_f32_as_f32x16!(P0, 1.0 / 2.0);
    const_f32_as_f32x16!(P1, 1.0 / 6.0);
    const_f32_as_f32x16!(P2, 1.0 / 24.0);
    const_f32_as_f32x16!(P3, 1.0 / 120.0);
    const_f32_as_f32x16!(P4, 1.0 / 720.0);
    const_f32_as_f32x16!(P5, 1.0 / 5040.0);
    // LN2D_HI/LO: double-double decomposition of ln(2) for exp range reduction,
    // following the approach from fdlibm's e_exp.c (Sun Microsystems,
    // https://www.netlib.org/fdlibm/). The f32 split uses f32-precision constants
    // (0.693359375, -2.12194440e-4) summing to ln(2) with single-precision
    // accuracy; the f64 variants use a full f64 double-double
    // decomposition.
    const_f32_as_f32x16!(LN2D_HI, 0.693359375);
    const_f32_as_f32x16!(LN2D_LO, -2.12194440e-4);
    // max_x = ln(f32::MAX) ≈ 88.7229, max_r = 127 (IEEE max normal exponent)
    // min_x = -149.5 ln(2) ≈ -103.63: min r for vm_pow2n subnormal
    let max_x = f32x16::from(88.723);
    let min_x = f32x16::from(-103.63);
    // x < min_x: e^x underflows to 0 -- skip the entire pipeline
    let finite = self.is_finite();
    let neg_underflow = self.simd_lt(min_x) & finite;
    if neg_underflow.all() {
      return Self::ZERO;
    }
    let max_r = f32x16::from(127.0);
    let r = (self * Self::LOG2_E).round_ties_even();
    let big = r.simd_gt(max_r);
    let r_safe = big.select(max_r, r);
    let excess = r - max_r;
    let excess = big.select(excess, Self::ZERO);
    let scale = Self::vm_pow2n(excess);
    let x = r.mul_neg_add(LN2D_HI, self);
    let x = r.mul_neg_add(LN2D_LO, x);
    let z = polynomial_5!(x, P0, P1, P2, P3, P4, P5);
    let x2 = x * x;
    let z = z.mul_add(x2, x);
    let n2 = Self::vm_pow2n(r_safe);
    let z = (z + Self::ONE) * scale * n2;
    let nan_mask = self.is_nan();
    let mut result = nan_mask.select(Self::nan_pow(), z);
    let pos_overflow = self.simd_gt(max_x) & finite;
    result = pos_overflow.select(Self::infinity(), result);
    result = neg_underflow.select(Self::ZERO, result);
    let pos_inf = !finite & !self.is_sign_negative() & !nan_mask;
    result = pos_inf.select(Self::infinity(), result);
    let neg_inf = !finite & self.is_sign_negative() & !nan_mask;
    result = neg_inf.select(Self::ZERO, result);
    result
  }

  #[inline]
  pub fn exp2(self) -> Self {
    const_f32_as_f32x16!(P2, 1.0 / 2.0);
    const_f32_as_f32x16!(P3, 1.0 / 6.0);
    const_f32_as_f32x16!(P4, 1.0 / 24.0);
    const_f32_as_f32x16!(P5, 1.0 / 120.0);
    const_f32_as_f32x16!(P6, 1.0 / 720.0);
    const_f32_as_f32x16!(P7, 1.0 / 5040.0);

    // max_x = log2(f32::MAX) ≈ 127.99999
    // min_x = log2(f32::MIN_POSITIVE) - 23 ≈ -126 - 23 = -149
    let max_x = f32x16::from(127.99999);
    let min_x = f32x16::from(-149.5);
    let finite = self.is_finite();
    let neg_underflow = self.simd_lt(min_x) & finite;
    if neg_underflow.all() {
      return Self::ZERO;
    }

    let round = self.round_ties_even();
    let max_r = f32x16::from(127.0);
    let big = round.simd_gt(max_r);
    let r_safe = big.select(max_r, round);
    let excess = round - max_r;
    let excess = big.select(excess, Self::ZERO);
    let scale = Self::vm_pow2n(excess);

    let fract = (self - round) * Self::LN_2;
    let fract_partial_exp2 = polynomial_5!(fract, P2, P3, P4, P5, P6, P7);
    let fract2 = fract * fract;
    let fract_exp2 = fract_partial_exp2.mul_add(fract2, fract) + Self::ONE;

    let n2 = Self::vm_pow2n(r_safe);
    let result = fract_exp2 * scale * n2;

    let nan_mask = self.is_nan();
    let mut result = nan_mask.select(Self::nan_pow(), result);
    let pos_overflow = self.simd_gt(max_x) & finite;
    result = pos_overflow.select(Self::infinity(), result);
    result = neg_underflow.select(Self::ZERO, result);
    let pos_inf = !finite & !self.is_sign_negative() & !nan_mask;
    result = pos_inf.select(Self::infinity(), result);
    let neg_inf = !finite & self.is_sign_negative() & !nan_mask;
    result = neg_inf.select(Self::ZERO, result);
    result
  }

  #[inline]
  pub fn ln(self) -> Self {
    const_f32_as_f32x16!(HALF, 0.5);
    const_f32_as_f32x16!(P0, 3.3333331174E-1);
    const_f32_as_f32x16!(P1, -2.4999993993E-1);
    const_f32_as_f32x16!(P2, 2.0000714765E-1);
    const_f32_as_f32x16!(P3, -1.6668057665E-1);
    const_f32_as_f32x16!(P4, 1.4249322787E-1);
    const_f32_as_f32x16!(P5, -1.2420140846E-1);
    const_f32_as_f32x16!(P6, 1.1676998740E-1);
    const_f32_as_f32x16!(P7, -1.1514610310E-1);
    const_f32_as_f32x16!(P8, 7.0376836292E-2);
    const_f32_as_f32x16!(LN2F_HI, 0.693359375);
    const_f32_as_f32x16!(LN2F_LO, -2.12194440e-4);
    const_f32_as_f32x16!(VM_SMALLEST_NORMAL, 1.17549435E-38);

    let x1 = self;
    let x = Self::fraction_2(x1);
    let e = Self::exponent(x1);
    let mask = x.simd_gt(Self::SQRT_2 * HALF);
    let x = (!mask).select(x + x, x);
    let fe = mask.select(e + Self::ONE, e);
    let x = x - Self::ONE;
    let res = polynomial_8!(x, P0, P1, P2, P3, P4, P5, P6, P7, P8);
    let x2 = x * x;
    let res = x2 * x * res;
    let res = fe.mul_add(LN2F_LO, res);
    let res = res + x2.mul_neg_add(HALF, x);
    let res = fe.mul_add(LN2F_HI, res);
    let overflow = !self.is_finite();
    let underflow = x1.simd_lt(VM_SMALLEST_NORMAL);
    let mask = overflow | underflow;
    if !mask.any() {
      res
    } else {
      let is_zero = self.is_zero_or_subnormal();
      let res = underflow.select(Self::nan_log(), res);
      // Note: is_zero_or_subnormal() lumps subnormals (exponent==0) with zero.
      // Both get -Inf here. True subnormal inputs (~1.4e-45..1.175e-38) should
      // produce a finite negative result, but are vanishingly rare in
      // practice.
      let res = is_zero.select(-Self::infinity(), res);
      let res = overflow.select(self, res);
      // This must come *after* overflow.blend to overwrite ln(-∞) = -∞ to NaN
      let res = (!self.is_finite() & self.is_sign_negative())
        .select(Self::nan_log(), res);
      res
    }
  }

  #[inline]
  pub fn cbrt(self) -> Self {
    let a = self.abs();
    let zero = a.simd_eq(Self::ZERO);
    if zero.all() {
      return self; // preserves -0.0
    }
    let inf = a.is_inf();
    let nan = self.is_nan();

    let tiny = a.simd_lt(Self::from(f32::MIN_POSITIVE));
    let a_work = tiny.select(a * Self::from(16777216.0), a);

    let e = Self::exponent(a_work) + Self::ONE;
    let d = Self::fraction_2(a_work);

    // C0..C5 from SLEEF's minimax polynomial for 1/cbrt(d) on [0.5, 1.0)
    // Naoki Shibata et al., "SLEEF: A Portable Vectorized Library of C99
    // Mathematical Functions", https://sleef.org / https://github.com/shibatch/sleef
    // Licensed under the Boost Software License 1.0.
    // These are the f32-precision coefficients; our f64 variants use the f64
    // set.
    const_f32_as_f32x16!(C0, 2.2241257);
    const_f32_as_f32x16!(C1, -3.8095417);
    const_f32_as_f32x16!(C2, 5.8982625);
    const_f32_as_f32x16!(C3, -5.532182);
    const_f32_as_f32x16!(C4, 2.8208892);
    const_f32_as_f32x16!(C5, -0.60156447);
    let mut x = polynomial_5!(d, C0, C1, C2, C3, C4, C5);

    let x2 = x * x;
    let x4 = x2 * x2;
    x = x - d.mul_add(x4, -x) * Self::from(1.0 / 3.0);
    // cbrt(d) = d * x² with refinement
    let mut y = (d * x) * x;
    let yx = y * x;
    let t = Self::from(2.0 / 3.0);
    y = y - t * y * (yx - Self::ONE);

    // Scale by 2^(e/3)
    let three = Self::from(3.0);
    let two = Self::from(2.0);
    let neg = e.simd_lt(Self::ZERO);
    let e_adj = neg.select(e - two, e);
    let k = (e_adj / three).trunc();
    let r = e - three * k;
    const_f32_as_f32x16!(CBRT2, 1.259921);
    const_f32_as_f32x16!(CBRT4, 1.587401);
    y = r.simd_eq(Self::ONE).select(y * CBRT2, y);
    y = r.simd_eq(two).select(y * CBRT4, y);
    y *= Self::vm_pow2n(k);
    y = tiny.select(y / Self::from(256.0_f32), y);

    let result = y.flip_signs(self);
    let result = nan.select(self, result);
    let result = zero.select(self, result);
    let result = inf.select(self, result);
    result
  }

  #[inline]
  pub fn asin(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x16!(P4asinf, 4.2163199048E-2);
    const_f32_as_f32x16!(P3asinf, 2.4181311049E-2);
    const_f32_as_f32x16!(P2asinf, 4.5470025998E-2);
    const_f32_as_f32x16!(P1asinf, 7.4953002686E-2);
    const_f32_as_f32x16!(P0asinf, 1.6666752422E-1);

    let xa = self.abs();
    let big = xa.simd_ge(f32x16::splat(0.5));

    let x1 = f32x16::splat(0.5) * (f32x16::ONE - xa);
    let x2 = xa * xa;
    let x3 = big.select(x1, x2);

    let xb = x1.sqrt();

    let x4 = big.select(xb, xa);

    let z = polynomial_4!(x3, P0asinf, P1asinf, P2asinf, P3asinf, P4asinf);
    let z = z.mul_add(x3 * x4, x4);

    let z1 = z + z;

    // asin
    let z3 = f32x16::FRAC_PI_2 - z1;
    let asin = big.select(z3, z);
    let asin = asin.flip_signs(self);

    asin
  }

  #[inline]
  pub fn acos(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x16!(P4asinf, 4.2163199048E-2);
    const_f32_as_f32x16!(P3asinf, 2.4181311049E-2);
    const_f32_as_f32x16!(P2asinf, 4.5470025998E-2);
    const_f32_as_f32x16!(P1asinf, 7.4953002686E-2);
    const_f32_as_f32x16!(P0asinf, 1.6666752422E-1);

    let xa = self.abs();
    let big = xa.simd_ge(f32x16::splat(0.5));

    let x1 = f32x16::splat(0.5) * (f32x16::ONE - xa);
    let x2 = xa * xa;
    let x3 = big.select(x1, x2);

    let xb = x1.sqrt();

    let x4 = big.select(xb, xa);

    let z = polynomial_4!(x3, P0asinf, P1asinf, P2asinf, P3asinf, P4asinf);
    let z = z.mul_add(x3 * x4, x4);

    let z1 = z + z;

    // acos
    let z3 = self.simd_lt(f32x16::ZERO).select(f32x16::PI - z1, z1);
    let z4 = f32x16::FRAC_PI_2 - z.flip_signs(self);
    let acos = big.select(z3, z4);

    acos
  }

  #[inline]
  pub fn atan(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x16!(P3atanf, 8.05374449538E-2);
    const_f32_as_f32x16!(P2atanf, -1.38776856032E-1);
    const_f32_as_f32x16!(P1atanf, 1.99777106478E-1);
    const_f32_as_f32x16!(P0atanf, -3.33329491539E-1);

    let t = self.abs();

    // small:  z = t / 1.0;
    // medium: z = (t-1.0) / (t+1.0);
    // big:    z = -1.0 / t;
    let notsmal = t.simd_ge(Self::SQRT_2 - Self::ONE);
    let notbig = t.simd_le(Self::SQRT_2 + Self::ONE);

    let mut s = notbig.select(Self::FRAC_PI_4, Self::FRAC_PI_2);
    s = notsmal & s;

    let mut a = notbig & t;
    a = notsmal.select(a - Self::ONE, a);
    let mut b = notbig & Self::ONE;
    b = notsmal.select(b + t, b);
    let z = a / b;

    let zz = z * z;

    // Taylor expansion
    let mut re = polynomial_3!(zz, P0atanf, P1atanf, P2atanf, P3atanf);
    re = re.mul_add(zz * z, z) + s;

    // get sign bit
    re = (self.is_sign_negative()).select(-re, re);

    re
  }

  #[inline]
  pub fn atan2(self, x: Self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x16!(P3atanf, 8.05374449538E-2);
    const_f32_as_f32x16!(P2atanf, -1.38776856032E-1);
    const_f32_as_f32x16!(P1atanf, 1.99777106478E-1);
    const_f32_as_f32x16!(P0atanf, -3.33329491539E-1);

    let y = self;

    // move in first octant
    let x1 = x.abs();
    let y1 = y.abs();
    let swapxy = y1.simd_gt(x1);
    // swap x and y if y1 > x1
    let mut x2 = swapxy.select(y1, x1);
    let mut y2 = swapxy.select(x1, y1);

    // check for special case: x and y are both +/- INF
    let both_infinite = x.is_inf() & y.is_inf();
    if both_infinite.any() {
      let minus_one = -Self::ONE;
      x2 = both_infinite.select(x2 & minus_one, x2);
      y2 = both_infinite.select(y2 & minus_one, y2);
    }

    // x = y = 0 will produce NAN. No problem, fixed below
    let t = y2 / x2;

    // small:  z = t / 1.0;
    // medium: z = (t-1.0) / (t+1.0);
    let notsmal = t.simd_ge(Self::SQRT_2 - Self::ONE);

    let a = notsmal.select(t - Self::ONE, t);
    let b = notsmal.select(t + Self::ONE, Self::ONE);
    let s = notsmal & Self::FRAC_PI_4;
    let z = a / b;

    let zz = z * z;

    // Taylor expansion
    let mut re = polynomial_3!(zz, P0atanf, P1atanf, P2atanf, P3atanf);
    re = re.mul_add(zz * z, z) + s;

    // move back in place
    re = swapxy.select(Self::FRAC_PI_2 - re, re);
    re = ((x | y).simd_eq(Self::ZERO)).select(Self::ZERO, re);
    re = (x.is_sign_negative()).select(Self::PI - re, re);

    // get sign bit
    re = (y.is_sign_negative()).select(-re, re);

    re
  }

  #[inline]
  pub fn sin_cos(self) -> (Self, Self) {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h

    const_f32_as_f32x16!(DP1F, 0.78515625_f32 * 2.0);
    const_f32_as_f32x16!(DP2F, 2.4187564849853515625E-4_f32 * 2.0);
    const_f32_as_f32x16!(DP3F, 3.77489497744594108E-8_f32 * 2.0);

    const_f32_as_f32x16!(P0sinf, -1.6666654611E-1);
    const_f32_as_f32x16!(P1sinf, 8.3321608736E-3);
    const_f32_as_f32x16!(P2sinf, -1.9515295891E-4);

    const_f32_as_f32x16!(P0cosf, 4.166664568298827E-2);
    const_f32_as_f32x16!(P1cosf, -1.388731625493765E-3);
    const_f32_as_f32x16!(P2cosf, 2.443315711809948E-5);

    const_f32_as_f32x16!(TWO_OVER_PI, 2.0 / core::f32::consts::PI);

    let xa = self.abs();

    // Find quadrant
    let y = (xa * TWO_OVER_PI).round_ties_even();
    let q: i32x16 = y.round_int();

    let x = y.mul_neg_add(DP3F, y.mul_neg_add(DP2F, y.mul_neg_add(DP1F, xa)));

    let x2 = x * x;
    let mut s = polynomial_2!(x2, P0sinf, P1sinf, P2sinf) * (x * x2) + x;
    let mut c = polynomial_2!(x2, P0cosf, P1cosf, P2cosf) * (x2 * x2)
      + f32x16::from(0.5).mul_neg_add(x2, f32x16::from(1.0));

    let swap = !(q & i32x16::from(1)).simd_eq(i32x16::from(0));

    let mut overflow: f32x16 = cast(q.simd_gt(i32x16::from(0x2000000)));
    overflow &= xa.is_finite();
    s = overflow.select(f32x16::from(0.0), s);
    c = overflow.select(f32x16::from(1.0), c);

    // calc sin
    let mut sin1 = cast::<_, f32x16>(swap).select(c, s);
    let sign_sin: i32x16 = (q << 30) ^ cast::<_, i32x16>(self);
    sin1 = sin1.flip_signs(cast(sign_sin));

    // calc cos
    let mut cos1 = cast::<_, f32x16>(swap).select(s, c);
    let sign_cos: i32x16 = ((q + i32x16::from(1)) & i32x16::from(2)) << 30;
    cos1 ^= cast::<_, f32x16>(sign_cos);

    // IEEE 754: sin/cos(±∞) = NaN, sin/cos(NaN) = NaN
    let finite = self.is_finite();
    let nan = Self::splat(f32::NAN);
    let sin_final = finite.select(sin1, nan);
    let cos_final = finite.select(cos1, nan);

    (sin_final, cos_final)
  }

  #[inline]
  pub fn asin_acos(self) -> (Self, Self) {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x16!(P4asinf, 4.2163199048E-2);
    const_f32_as_f32x16!(P3asinf, 2.4181311049E-2);
    const_f32_as_f32x16!(P2asinf, 4.5470025998E-2);
    const_f32_as_f32x16!(P1asinf, 7.4953002686E-2);
    const_f32_as_f32x16!(P0asinf, 1.6666752422E-1);

    let xa = self.abs();
    let big = xa.simd_ge(f32x16::splat(0.5));

    let x1 = f32x16::splat(0.5) * (f32x16::ONE - xa);
    let x2 = xa * xa;
    let x3 = big.select(x1, x2);

    let xb = x1.sqrt();

    let x4 = big.select(xb, xa);

    let z = polynomial_4!(x3, P0asinf, P1asinf, P2asinf, P3asinf, P4asinf);
    let z = z.mul_add(x3 * x4, x4);

    let z1 = z + z;

    // acos
    let z3 = self.simd_lt(f32x16::ZERO).select(f32x16::PI - z1, z1);
    let z4 = f32x16::FRAC_PI_2 - z.flip_signs(self);
    let acos = big.select(z3, z4);

    // asin
    let z3 = f32x16::FRAC_PI_2 - z1;
    let asin = big.select(z3, z);
    let asin = asin.flip_signs(self);

    (asin, acos)
  }

  #[inline]
  pub fn exp_m1(self) -> Self {
    // x < -17.329: e^x < 2⁻²⁵, exp_m1(x) = -1.0 exactly (mantissa exhaustion)
    // IEEE simd_lt returns false for NaN, so NaN lanes can't reach here.
    // -inf is < -17.329, and exp_m1(-inf) = -1.0, also correct.
    if self.simd_lt(f32x16::from(-17.329)).all() {
      return f32x16::from(-1.0);
    }
    const_f32_as_f32x16!(P0, 1.0 / 2.0);
    const_f32_as_f32x16!(P1, 1.0 / 6.0);
    const_f32_as_f32x16!(P2, 1.0 / 24.0);
    const_f32_as_f32x16!(P3, 1.0 / 120.0);
    const_f32_as_f32x16!(P4, 1.0 / 720.0);
    const_f32_as_f32x16!(P5, 1.0 / 5040.0);
    // LN2D_HI/LO: double-double decomposition of ln(2) for exp range reduction,
    // following the approach from fdlibm's e_exp.c (Sun Microsystems,
    // https://www.netlib.org/fdlibm/). The f32 split uses f32-precision constants
    // (0.693359375, -2.12194440e-4) summing to ln(2) with single-precision
    // accuracy; the f64 variants use a full f64 double-double
    // decomposition.
    const_f32_as_f32x16!(LN2D_HI, 0.693359375);
    const_f32_as_f32x16!(LN2D_LO, -2.12194440e-4);
    // max_x = ln(f32::MAX) ≈ 88.7229, max_r = 127 (IEEE max normal exponent)
    // min_x = -149.5 ln(2) ≈ -103.63: min r for vm_pow2n subnormal
    let max_x = f32x16::from(88.723);
    let min_x = f32x16::from(-103.63);
    let max_r = f32x16::from(127.0);
    let r = (self * Self::LOG2_E).round_ties_even();
    let big = r.simd_gt(max_r);
    let r_safe = big.select(max_r, r);
    let excess = r - max_r;
    let excess = big.select(excess, Self::ZERO);
    let scale = Self::vm_pow2n(excess);
    let x = r.mul_neg_add(LN2D_HI, self);
    let x = r.mul_neg_add(LN2D_LO, x);
    let z = polynomial_5!(x, P0, P1, P2, P3, P4, P5);
    let x2 = x * x;
    let z = z.mul_add(x2, x);
    let n2 = Self::vm_pow2n(r_safe);
    let exp_val = (z + Self::ONE) * scale * n2;
    let r_is_zero = r.simd_eq(Self::ZERO);
    let z = r_is_zero.select(z, exp_val - Self::ONE);
    let nan_mask = self.is_nan();
    let finite = self.is_finite();
    let mut result = nan_mask.select(Self::nan_pow(), z);
    let pos_overflow = self.simd_gt(max_x) & finite;
    result = pos_overflow.select(Self::infinity(), result);
    let neg_underflow = self.simd_lt(min_x) & finite;
    result = neg_underflow.select(-Self::ONE, result);
    let pos_inf = !finite & !self.is_sign_negative() & !nan_mask;
    result = pos_inf.select(Self::infinity(), result);
    let neg_inf = !finite & self.is_sign_negative() & !nan_mask;
    result = neg_inf.select(-Self::ONE, result);
    let is_zero = self.simd_eq(Self::ZERO);
    result = is_zero.select(self, result);
    result
  }

  #[inline]
  pub fn ln_1p(self) -> Self {
    // Based on the identity ln(1+x) = x·ln(1+x)/((1+x)-1), i.e. x·ln(u)/(u-1)
    // where u = 1+x. From MUSL libc (Rich Felker et al., https://musl.libc.org) src/math/log1pf.c
    // and fdlibm (Sun Microsystems, https://www.netlib.org/fdlibm/) s_log1p.c.
    // When 1+x rounds to 1 exactly (subnormal x), return x directly.
    // When 1+x overflows (+inf), return ln(u) without correction.
    // Mathematically exact: compensates for the rounding loss in 1+x without
    // needing a series threshold.
    let u = self + Self::ONE;
    let eq = u.simd_eq(Self::ONE);
    let ln_u = Self::ln(u);
    let correction = self * (ln_u / (u - Self::ONE));
    let result = eq.select(self, correction);
    let over = u.is_inf();
    over.select(ln_u, result)
  }

  #[inline]
  pub fn sinh(self) -> Self {
    const_f32_as_f32x16!(P0, 1.0);
    const_f32_as_f32x16!(P1, 1.0 / 6.0);
    const_f32_as_f32x16!(P2, 1.0 / 120.0);
    const_f32_as_f32x16!(P3, 1.0 / 5040.0);
    let a = self.abs();
    // |x| < 0.5: Taylor poly; last truncation term < 1 ULP at x=0.5 for both types
    let small = a.simd_lt(f32x16::from(0.5));
    let t = a * a;
    let poly = a * polynomial_3!(t, P0, P1, P2, P3);
    let exp_based = {
      let e = a.exp();
      (e - Self::ONE / e) * Self::HALF
    };
    let result = small.select(poly, exp_based);
    result.flip_signs(self)
  }

  #[inline]
  pub fn cosh(self) -> Self {
    const_f32_as_f32x16!(P0, 1.0);
    const_f32_as_f32x16!(P1, 1.0 / 2.0);
    const_f32_as_f32x16!(P2, 1.0 / 24.0);
    const_f32_as_f32x16!(P3, 1.0 / 720.0);
    let a = self.abs();
    // |x| < 0.5: Taylor poly; last truncation term < 1 ULP at x=0.5 for both types
    let small = a.simd_lt(f32x16::from(0.5));
    let t = a * a;
    let poly = polynomial_3!(t, P0, P1, P2, P3);
    let exp_based = {
      let e = a.exp();
      (e + Self::ONE / e) * Self::HALF
    };
    small.select(poly, exp_based)
  }

  #[inline]
  pub fn tanh(self) -> Self {
    // |x| < 2e-4: tanh(x) ≈ x, error x³/3 < 16·ULP(x)
    // bound: x² < 48·2⁻²³ → x < 2.39e-3; 2e-4 has 10× margin
    // |x| > 9.011: tanh(x) = ±1 to f32 precision (e⁻²ˣ < 2⁻²⁴)
    let a = self.abs();
    let large = a.simd_gt(f32x16::from(9.011));
    if large.all() {
      return Self::ONE.flip_signs(self);
    }
    let small = a.simd_lt(f32x16::from(2e-4));
    let exp_based = {
      let t = (Self::from(-2.0) * a).exp_m1();
      let pos = -t / (t + Self::from(2.0));
      pos.flip_signs(self)
    };
    let result = small.select(self, exp_based);
    large.select(Self::ONE.flip_signs(self), result)
  }
}

/// The following functionality exists only for [`f32x16`], or only for
/// particular types inconsistently.
impl f32x16 {
  #[inline]
  fn vm_pow2n(self) -> Self {
    const_f32_as_f32x16!(pow2_23, 8388608.0);
    const_f32_as_f32x16!(bias, 127.0);
    let a = self + (bias + pow2_23);
    let c = cast::<_, i32x16>(a) << 23;
    let std_result = cast::<_, f32x16>(c);

    let min_exp = f32x16::from(-126.0);
    let is_sub = self.simd_lt(min_exp);
    if is_sub.any() {
      let valid = self.simd_ge(f32x16::from(-149.0));
      let shift_f = self + f32x16::from(149.0);
      let mut shift_i = shift_f.trunc_int();
      shift_i = cast::<_, i32x16>(valid).select(shift_i, i32x16::ZERO);
      let mantissa = i32x16::ONE << shift_i;
      let sub_result = cast::<_, f32x16>(mantissa);
      let sub_result = valid.select(sub_result, f32x16::ZERO);
      is_sub.select(sub_result, std_result)
    } else {
      std_result
    }
  }

  #[inline]
  fn exponent(self) -> Self {
    const_f32_as_f32x16!(pow2_23, 8388608.0);
    const_f32_as_f32x16!(bias, 127.0);
    let a = cast::<_, u32x16>(self);
    let b = a >> 23;
    let c = b | cast::<_, u32x16>(pow2_23);
    let d = cast::<_, f32x16>(c);
    let e = d - (pow2_23 + bias);
    e
  }

  #[inline]
  fn fraction_2(self) -> Self {
    let t1 = cast::<_, u32x16>(self);
    let t2 = cast::<_, u32x16>(
      (t1 & u32x16::from(0x007FFFFF)) | u32x16::from(0x3F000000),
    );
    cast::<_, f32x16>(t2)
  }

  #[inline]
  fn is_zero_or_subnormal(self) -> Self {
    let t = cast::<_, i32x16>(self);
    let t = t & i32x16::splat(0x7F800000);
    let mask = t.simd_eq(i32x16::splat(0));
    cast::<_, f32x16>(mask)
  }

  #[inline]
  fn infinity() -> Self {
    cast::<_, f32x16>(i32x16::splat(0x7F800000))
  }

  #[inline]
  fn nan_log() -> Self {
    cast::<_, f32x16>(i32x16::splat(0x7FC00000 | 0x101 & 0x003FFFFF))
  }

  #[inline]
  fn nan_pow() -> Self {
    cast::<_, f32x16>(i32x16::splat(0x7FC00000 | 0x101 & 0x003FFFFF))
  }

  /// Converts each element from [`i32`] to [`f32`].
  #[inline]
  pub fn from_i32x16(v: i32x16) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: convert_to_m512_from_i32_m512i(v.avx512) }
      } else {
        Self {
          a: f32x8::from_i32x8(v.a),
          b: f32x8::from_i32x8(v.b),
        }
      }
    }
  }

  /// Returns a [mask] that checks if each element has a negative sign,
  /// including `-0.0`, NaNs with negative sign bit and negative infinity.
  ///
  /// Note that this function has a misleading name. If the sign bit is set, the
  /// result has all bits set, not just the sign bit. This function has been
  /// renamed to [`is_sign_negative`].
  ///
  /// [mask]: crate#masks
  /// [`is_sign_negative`]: Self::is_sign_negative
  #[inline]
  #[must_use]
  #[deprecated(since = "1.4.0", note = "renamed to `is_sign_negative`")]
  pub fn sign_bit(self) -> Self {
    self.is_sign_negative()
  }
}
