use super::*;

pick! {
  if #[cfg(target_feature="avx512f")] {
    /// A SIMD vector with eight elements of type [`f64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq)]
    #[repr(C, align(64))]
    pub struct f64x8 { pub(crate) avx512: m512d }
  } else {
    /// A SIMD vector with eight elements of type [`f64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq)]
    #[repr(C, align(64))]
    pub struct f64x8 { pub(crate) a : f64x4, pub(crate) b : f64x4 }
  }
}

macro_rules! const_f64_as_f64x8 {
  ($i:ident, $f:expr) => {
    #[allow(non_upper_case_globals)]
    pub const $i: f64x8 = f64x8::new([$f; 8]);
  };
}

impl_simd_float! {
  unsafe {
    T = f64,
    N = 8,
    Simd = f64x8,
    IntSimd = i64x8,
    UintT = u64,
    UintSimd = u64x8,
    optional_type_x86_inner { X86Inner = __m512d },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }
  old_powf_simd_fn_name = pow_f64x8,

  #[inline]
  fn neg(self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitxor_m512d(self.avx512, Self::splat(-0.0).avx512) }
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
        Self { avx512: bitxor_m512d(self.avx512, set_splat_m512d(f64::from_bits(u64::MAX))) }
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
        Self { avx512: add_m512d(self.avx512, rhs.avx512) }
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
        Self { avx512: sub_m512d(self.avx512, rhs.avx512) }
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
        Self { avx512: mul_m512d(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.mul(rhs.a), b: self.b.mul(rhs.b) }
      }
    }
  }

  #[inline]
  fn div(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: div_m512d(self.avx512, rhs.avx512) }
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
    ])
  }

  #[inline]
  fn bitand(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitand_m512d(self.avx512, rhs.avx512) }
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
        Self { avx512: bitor_m512d(self.avx512, rhs.avx512) }
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
        Self { avx512: bitxor_m512d(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(EqualOrdered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(NotEqualUnordered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(LessThanOrdered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(GreaterThanOrdered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(LessEqualOrdered)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(GreaterEqualOrdered)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> f64 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // From https://stackoverflow.com/questions/49941645/get-sum-of-values-stored-in-m256d-with-sse-avx
        let lo = cast_to_m256d_from_m512d(self.avx512);
        let hi = extract_m256d_from_m512d::<1>(self.avx512);
        let v  = add_m256d(lo, hi);                // [a0+a4, a1+a5, a2+a6, a3+a7]
        let t  = add_horizontal_m256d(v, v);       // [s01, s23, s01, s23]
        let lo = cast_to_m128d_from_m256d(t);      // s01
        let hi = extract_m128d_from_m256d::<1>(t); // s23
        let s  = add_m128d(lo, hi);                // [sum, ...]
        get_f64_from_m128d_s(s)
      } else {
        self.a.reduce_add() + self.b.reduce_add()
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> f64 {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // From https://stackoverflow.com/questions/49941645/get-sum-of-values-stored-in-m256d-with-sse-avx
        let lo = cast_to_m256d_from_m512d(self.avx512);
        let hi = extract_m256d_from_m512d::<1>(self.avx512);
        let v  = mul_m256d(lo, hi);
        let lo = cast_to_m128d_from_m256d(v);
        let hi = extract_m128d_from_m256d::<1>(v);
        let lo = mul_m128d(lo,hi);
        let hi64 = unpack_high_m128d(lo,lo);
        let product = mul_m128d_s(lo,hi64);
        get_f64_from_m128d_s(product)
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
          avx512: bitor_m512d(
            bitand_m512d(if_one.avx512, self.avx512),
            bitandnot_m512d(self.avx512, if_zero.avx512),
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
        Self { avx512: blend_varying_m512d(if_false.avx512, if_true.avx512, movepi64_mask_m512d(self.avx512)) }
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
        movepi64_mask_m512d(self.avx512) as u32
      } else {
        (self.b.to_bitmask() << 4) | self.a.to_bitmask()
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        movepi64_mask_m512d(self.avx512) != 0
      } else {
        self.a.any() || self.b.any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        movepi64_mask_m512d(self.avx512) == 0b11111111
      } else {
        self.a.all() && self.b.all()
      }
    }
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [f64x8; 8]) -> [f64x8; 8] {
    // Can this be optimized?

    #[inline(always)]
    fn transpose_column(data: &[f64x8; 8], index: usize) -> f64x8 {
      f64x8::new([
        data[0].as_array()[index],
        data[1].as_array()[index],
        data[2].as_array()[index],
        data[3].as_array()[index],
        data[4].as_array()[index],
        data[5].as_array()[index],
        data[6].as_array()[index],
        data[7].as_array()[index],
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
    ]
  }

  #[inline]
  pub fn is_nan(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: cmp_op_mask_m512d::<{cmp_op!(Unordered)}>(self.avx512, self.avx512) }
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
    let shifted_inf = u64x8::from(0xFFE0000000000000);
    let u: u64x8 = cast(self);
    let shift_u = u << 1_u64;
    let out = (shift_u).simd_eq(shifted_inf);
    cast(out)
  }

  #[inline]
  pub fn is_finite(self) -> Self {
    let shifted_exp_mask = u64x8::splat(0xFFE0000000000000);
    let u: u64x8 = cast(self);
    let shift_u = u << 1_u64;
    let out = !(shift_u & shifted_exp_mask).simd_eq(shifted_exp_mask);
    cast(out)
  }

  #[inline]
  pub fn is_sign_positive(self) -> Self {
    const SIGN_MASK: u64x8 = u64x8::splat((-0.0_f64).to_bits());

    let bits = cast::<f64x8, u64x8>(self);
    let sign = bits & SIGN_MASK;
    let result = sign.simd_eq(u64x8::ZERO);
    cast::<u64x8, f64x8>(result)
  }

  #[inline]
  pub fn is_sign_negative(self) -> Self {
    const SIGN_MASK: u64x8 = u64x8::splat((-0.0_f64).to_bits());

    let bits = cast::<f64x8, u64x8>(self);
    let sign = bits & SIGN_MASK;
    let result = sign.simd_eq(SIGN_MASK);
    cast::<u64x8, f64x8>(result)
  }

  #[inline]
  pub fn recip(self) -> Self {
    // There does not seem to be a `recip` intrinsic for any architecture. The
    // closest is `_mm512_rcp14_pd` which has relative error.
    Self::ONE / self
  }

  #[inline]
  pub fn recip_sqrt(self) -> Self {
    // There does not seem to be a `recip_sqrt` intrinsic for any architecture.
    // The closest is `_mm512_rsqrt14_pd` which has relative error.
    Self::ONE / self.sqrt()
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        rhs.is_nan().select(self, Self { avx512: max_m512d(self.avx512, rhs.avx512) })
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
        Self { avx512: max_m512d(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.fast_max(rhs.a),
          b : self.b.fast_max(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        rhs.is_nan().select(self, Self { avx512: min_m512d(self.avx512, rhs.avx512) })
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
        Self { avx512: min_m512d(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.fast_min(rhs.a),
          b : self.b.fast_min(rhs.b),
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
        // For both `min_m512d` and `max_m512d` if any input is NaN, `rhs` gets
        // chosen. For `self` to be chosen, `self` must be the second argument.
        Self { avx512: max_m512d(min.avx512, min_m512d(max.avx512, self.avx512)) }
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
        let non_sign_bits = f64x8::from(f64::from_bits(i64::MAX as u64));
        self & non_sign_bits
      } else {
        Self {
          a: self.a.abs(),
          b: self.b.abs(),
        }
      }
    }
  }

  #[inline]
  pub fn floor(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: round_m512d::<{round_op!(NegInf)}>(self.avx512) }
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
        Self { avx512: round_m512d::<{round_op!(PosInf)}>(self.avx512) }
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
        const_f64_as_f64x8!(HALF_NEXT_DOWN, 0.5_f64.next_down());
        const_f64_as_f64x8!(BOUNDS_LIMIT, 4503599627370496.0);

        let self_abs = self.abs();

        let adjusted_self = self_abs + Self::HALF;
        let result_abs = Self { avx512: round_m512d::<{round_op!(Zero)}>(adjusted_self.avx512) };
        // The addition breaks for `0.5.next_down()` which incorrectly rounds to
        // `1.0`. This resets the result back to `0.0`.
        let result_abs = result_abs & self_abs.simd_ne(HALF_NEXT_DOWN);

        // Large value, infinity and NaN need special handling.
        let bounds_mask: Self = cast(cmp_op_mask_i64_m512i::<{cmp_int_op!(Lt)}>(
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
  pub fn round_int(self) -> i64x8 {
    pick! {
      if #[cfg(target_feature="avx512dq")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_cvtpd_epi64;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_cvtpd_epi64;

        // Based on: https://github.com/v8/v8/blob/210987a552a2bf2a854b0baa9588a5959ff3979d/src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h#L489-L504
        let non_nan_mask = self.simd_eq(self);
        let non_nan = self & non_nan_mask;
        let flip_to_max: i64x8 = cast(self.simd_ge(Self::splat(9223372036854775808.0)));

        // TODO(safe_arch): Add `_mm512_cvtpd_epi64`.
        let cast: i64x8 = cast(m512i(unsafe { _mm512_cvtpd_epi64(non_nan.avx512.0) }));
        flip_to_max ^ cast
      } else {
        let [a,b]: [f64x4;2] = cast(self);
        cast([
          a.round_int(),
          b.round_int(),
        ])
      }
    }
  }

  #[inline]
  pub fn fast_round_int(self) -> i64x8 {
    pick! {
      if #[cfg(target_feature="avx512dq")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_cvtpd_epi64;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_cvtpd_epi64;

        // TODO(safe_arch): Add `_mm512_cvtpd_epi64`.
        cast(m512i(unsafe { _mm512_cvtpd_epi64(self.avx512.0) }))
      } else {

        let [a,b]: [f64x4;2] = cast(self);
        cast([
          a.fast_round_int(),
          b.fast_round_int(),
        ])
      }
    }
  }

  #[inline]
  pub fn round_ties_even(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: round_m512d::<{round_op!(Nearest)}>(self.avx512) }
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
        Self { avx512: round_m512d::<{round_op!(Zero)}>(self.avx512) }
      } else {
        Self {
          a: self.a.trunc(),
          b: self.b.trunc(),
        }
      }
    }
  }

  #[inline]
  pub fn trunc_int(self) -> i64x8 {
    pick! {
      if #[cfg(target_feature="avx512dq")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_cvttpd_epi64;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_cvttpd_epi64;

        // Based on: https://github.com/v8/v8/blob/210987a552a2bf2a854b0baa9588a5959ff3979d/src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h#L489-L504
        let non_nan_mask = self.simd_eq(self);
        let non_nan = self & non_nan_mask;
        let flip_to_max: i64x8 = cast(self.simd_ge(Self::splat(9223372036854775808.0)));

        // TODO(safe_arch): Add `_mm512_cvttpd_epi64`.
        let cast: i64x8 = cast(m512i(unsafe { _mm512_cvttpd_epi64(non_nan.avx512.0) }));
        flip_to_max ^ cast
      } else {
        let [a,b]: [f64x4;2] = cast(self);
        cast([
          a.trunc_int(),
          b.trunc_int(),
        ])
      }
    }
  }

  #[inline]
  pub fn fast_trunc_int(self) -> i64x8 {
    pick! {
      if #[cfg(target_feature="avx512dq")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_cvttpd_epi64;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_cvttpd_epi64;

        // TODO(safe_arch): Add `_mm512_cvttpd_epi64`.
        cast(m512i(unsafe { _mm512_cvttpd_epi64(self.avx512.0) }))
      } else {
        let [a,b]: [f64x4;2] = cast(self);
        cast([
          a.fast_trunc_int(),
          b.fast_trunc_int(),
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
  /// - Other platforms: Delegates to [`f64x4`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_add(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_add_m512d(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        (self * a) + b
      } else {
        Self {
          a : self.a.mul_add(a.a, b.a),
          b : self.b.mul_add(a.b, b.b),
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
  /// - Other platforms: Delegates to [`f64x4`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_sub(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_sub_m512d(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        (self * a) - b
      } else {
        Self {
          a : self.a.mul_sub(a.a, b.a),
          b : self.b.mul_sub(a.b, b.b),
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
  /// - Other platforms: Delegates to [`f64x4`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_neg_add(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
        Self { avx512: fused_mul_neg_add_m512d(self.avx512, a.avx512, b.avx512) }
      } else if #[cfg(target_feature="avx512f")] {
        // still want to use 512 bit ops
        b - (self * a)
      } else {
        Self {
          a : self.a.mul_neg_add(a.a, b.a),
          b : self.b.mul_neg_add(a.b, b.b),
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
  /// - Other platforms: Delegates to [`f64x4`] (inherits its FMA behavior)
  #[inline]
  pub fn mul_neg_sub(self, a: Self, b: Self) -> Self {
    pick! {
       if #[cfg(all(target_feature="avx512f",target_feature="fma"))] {
         Self { avx512: fused_mul_neg_sub_m512d(self.avx512, a.avx512, b.avx512) }
        } else if #[cfg(target_feature="avx512f")] {
          // still want to use 512 bit ops
          -(self * a) - b
        } else {
         Self {
           a : self.a.mul_neg_sub(a.a, b.a),
           b : self.b.mul_neg_sub(a.b, b.b),
         }
       }
    }
  }

  #[inline]
  pub fn powf_simd(self, n: Self) -> Self {
    const_f64_as_f64x8!(ln2d_hi, 0.693145751953125);
    const_f64_as_f64x8!(ln2d_lo, 1.42860682030941723212E-6);
    const_f64_as_f64x8!(P0log, 2.0039553499201281259648E1);
    const_f64_as_f64x8!(P1log, 5.7112963590585538103336E1);
    const_f64_as_f64x8!(P2log, 6.0949667980987787057556E1);
    const_f64_as_f64x8!(P3log, 2.9911919328553073277375E1);
    const_f64_as_f64x8!(P4log, 6.5787325942061044846969E0);
    const_f64_as_f64x8!(P5log, 4.9854102823193375972212E-1);
    const_f64_as_f64x8!(P6log, 4.5270000862445199635215E-5);
    const_f64_as_f64x8!(Q0log, 6.0118660497603843919306E1);
    const_f64_as_f64x8!(Q1log, 2.1642788614495947685003E2);
    const_f64_as_f64x8!(Q2log, 3.0909872225312059774938E2);
    const_f64_as_f64x8!(Q3log, 2.2176239823732856465394E2);
    const_f64_as_f64x8!(Q4log, 8.3047565967967209469434E1);
    const_f64_as_f64x8!(Q5log, 1.5062909083469192043167E1);

    // Taylor expansion constants
    const_f64_as_f64x8!(p2, 1.0 / 2.0); // coefficients for Taylor expansion of exp
    const_f64_as_f64x8!(p3, 1.0 / 6.0);
    const_f64_as_f64x8!(p4, 1.0 / 24.0);
    const_f64_as_f64x8!(p5, 1.0 / 120.0);
    const_f64_as_f64x8!(p6, 1.0 / 720.0);
    const_f64_as_f64x8!(p7, 1.0 / 5040.0);
    const_f64_as_f64x8!(p8, 1.0 / 40320.0);
    const_f64_as_f64x8!(p9, 1.0 / 362880.0);
    const_f64_as_f64x8!(p10, 1.0 / 3628800.0);
    const_f64_as_f64x8!(p11, 1.0 / 39916800.0);
    const_f64_as_f64x8!(p12, 1.0 / 479001600.0);
    const_f64_as_f64x8!(p13, 1.0 / 6227020800.0);

    let x1 = self.abs();
    let x = x1.fraction_2();
    let mask = x.simd_gt(f64x8::SQRT_2 * f64x8::HALF);
    let x = (!mask).select(x + x, x);
    let x = x - f64x8::ONE;
    let x2 = x * x;
    let px = polynomial_6!(x, P0log, P1log, P2log, P3log, P4log, P5log, P6log);
    let px = px * x * x2;
    let qx = polynomial_6n!(x, Q0log, Q1log, Q2log, Q3log, Q4log, Q5log);
    let lg1 = px / qx;

    let ef = x1.exponent();
    let ef = mask.select(ef + f64x8::ONE, ef);
    let e1 = (ef * n).round_ties_even();
    let yr = ef.mul_sub(n, e1);

    let lg = f64x8::HALF.mul_neg_add(x2, x) + lg1;
    let x2err = (f64x8::HALF * x).mul_sub(x, f64x8::HALF * x2);
    let lg_err = f64x8::HALF.mul_add(x2, lg - x) - lg1;

    let e2 = (lg * n * f64x8::LOG2_E).round_ties_even();
    let v = lg.mul_sub(n, e2 * ln2d_hi);
    let v = e2.mul_neg_add(ln2d_lo, v);
    let v = v - (lg_err + x2err).mul_sub(n, yr * f64x8::LN_2);

    let x = v;
    let e3 = (x * f64x8::LOG2_E).round_ties_even();
    let x = e3.mul_neg_add(f64x8::LN_2, x);
    let z =
      polynomial_13!(x, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)
        + f64x8::ONE;
    let ee = e1 + e2 + e3;
    let ei = cast::<_, i64x8>(ee.round_int());
    let ej = cast::<_, i64x8>(ei + (cast::<_, i64x8>(z) >> 52));

    let overflow = cast::<_, f64x8>(!ej.simd_lt(i64x8::splat(0x07FF)))
      | ee.simd_gt(f64x8::splat(3000.0));
    let underflow = cast::<_, f64x8>(!ej.simd_gt(i64x8::splat(0x000)))
      | ee.simd_lt(f64x8::splat(-3000.0));

    // Add exponent by integer addition
    let z = cast::<_, f64x8>(cast::<_, i64x8>(z) + (ei << 52));

    // Check for overflow/underflow
    let z = if (overflow | underflow).any() {
      let z = underflow.select(f64x8::ZERO, z);
      overflow.select(Self::infinity(), z)
    } else {
      z
    };

    // Check for self == 0
    let x_zero = self.is_zero_or_subnormal();
    let z = x_zero.select(
      n.simd_lt(f64x8::ZERO).select(
        Self::infinity(),
        n.simd_eq(f64x8::ZERO).select(f64x8::ONE, f64x8::ZERO),
      ),
      z,
    );

    let x_sign = self.is_sign_negative();

    let z = if x_sign.any() {
      // Y into an integer
      let yi = n.simd_eq(n.round_ties_even());
      // Is y odd? If yes flip the sign of the result.
      let y_odd = cast::<i64x8, f64x8>(n.round_int() << 63);

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
        Self { avx512: sqrt_m512d(self.avx512) }
      } else {
        Self {
          a : self.a.sqrt(),
          b : self.b.sqrt(),
        }
      }
    }
  }

  /// Calculate the exponent of a packed `f64x8`
  #[inline]
  pub fn exp(self) -> Self {
    const_f64_as_f64x8!(P2, 1.0 / 2.0);
    const_f64_as_f64x8!(P3, 1.0 / 6.0);
    const_f64_as_f64x8!(P4, 1.0 / 24.0);
    const_f64_as_f64x8!(P5, 1.0 / 120.0);
    const_f64_as_f64x8!(P6, 1.0 / 720.0);
    const_f64_as_f64x8!(P7, 1.0 / 5040.0);
    const_f64_as_f64x8!(P8, 1.0 / 40320.0);
    const_f64_as_f64x8!(P9, 1.0 / 362880.0);
    const_f64_as_f64x8!(P10, 1.0 / 3628800.0);
    const_f64_as_f64x8!(P11, 1.0 / 39916800.0);
    const_f64_as_f64x8!(P12, 1.0 / 479001600.0);
    const_f64_as_f64x8!(P13, 1.0 / 6227020800.0);
    // LN2D_HI/LO: double-double decomposition of ln(2) for exp range reduction,
    // following fdlibm's approach (Sun Microsystems, https://www.netlib.org/fdlibm/ e_exp.c).
    // Values chosen so LN2D_HI + LN2D_LO = ln(2) to full f64 precision.
    const_f64_as_f64x8!(LN2D_HI, 0.693145751953125);
    const_f64_as_f64x8!(LN2D_LO, 1.42860682030941723212E-6);
    let max_x = f64x8::from(709.783);
    let min_x = f64x8::from(-744.79);
    let finite = self.is_finite();
    // x < min_x: e^x underflows to 0 -- skip the entire pipeline
    let neg_underflow = self.simd_lt(min_x) & finite;
    if neg_underflow.all() {
      return Self::ZERO;
    }
    let max_r = f64x8::from(1023.0);
    let r = (self * Self::LOG2_E).round_ties_even();
    let big = r.simd_gt(max_r);
    let r_safe = big.select(max_r, r);
    let excess = r - max_r;
    let excess = big.select(excess, Self::ZERO);
    let scale = Self::vm_pow2n(excess);
    let x = r.mul_neg_add(LN2D_HI, self);
    let x = r.mul_neg_add(LN2D_LO, x);
    let z =
      polynomial_13!(x, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
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
    const_f64_as_f64x8!(P2, 1.0 / 2.0);
    const_f64_as_f64x8!(P3, 1.0 / 6.0);
    const_f64_as_f64x8!(P4, 1.0 / 24.0);
    const_f64_as_f64x8!(P5, 1.0 / 120.0);
    const_f64_as_f64x8!(P6, 1.0 / 720.0);
    const_f64_as_f64x8!(P7, 1.0 / 5040.0);
    const_f64_as_f64x8!(P8, 1.0 / 40320.0);
    const_f64_as_f64x8!(P9, 1.0 / 362880.0);
    const_f64_as_f64x8!(P10, 1.0 / 3628800.0);

    // max_x = log2(f64::MAX) ≈ 1023.9999999999999
    // min_x = log2(f64::MIN_POSITIVE) - 52 ≈ -1022 - 52 = -1074
    let max_x = f64x8::from(1023.9999999999999);
    let min_x = f64x8::from(-1074.5);
    let finite = self.is_finite();
    let neg_underflow = self.simd_lt(min_x) & finite;
    if neg_underflow.all() {
      return Self::ZERO;
    }

    let round = self.round_ties_even();
    let max_r = f64x8::from(1023.0);
    let big = round.simd_gt(max_r);
    let r_safe = big.select(max_r, round);
    let excess = round - max_r;
    let excess = big.select(excess, Self::ZERO);
    let scale = Self::vm_pow2n(excess);

    let fract = (self - round) * Self::LN_2;
    let fract_partial_exp2 =
      polynomial_8!(fract, P2, P3, P4, P5, P6, P7, P8, P9, P10);
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
    const_f64_as_f64x8!(HALF, 0.5);
    const_f64_as_f64x8!(P0, 7.70838733755885391666E0);
    const_f64_as_f64x8!(P1, 1.79368678507819816313E1);
    const_f64_as_f64x8!(P2, 1.44989225341610930846E1);
    const_f64_as_f64x8!(P3, 4.70579119878881725854E0);
    const_f64_as_f64x8!(P4, 4.97494994976747001425E-1);
    const_f64_as_f64x8!(P5, 1.01875663804580931796E-4);

    const_f64_as_f64x8!(Q0, 2.31251620126765340583E1);
    const_f64_as_f64x8!(Q1, 7.11544750618563894466E1);
    const_f64_as_f64x8!(Q2, 8.29875266912776603211E1);
    const_f64_as_f64x8!(Q3, 4.52279145837532221105E1);
    const_f64_as_f64x8!(Q4, 1.12873587189167450590E1);
    // LN2F_HI/LO from fdlibm (Freely Distributable LIBM)
    // Sun Microsystems, Inc. https://www.netlib.org/fdlibm/
    // e_log.c: bit-exact double-double decomposition of ln(2) for f64.
    // Replaced the original f32-literals (0.693359375, -2.12194440e-4)
    // which had ~10 significant digits, causing ~630 ULP error in f64 ln.
    const_f64_as_f64x8!(LN2F_HI, f64::from_bits(0x3FE62E42FEE00000));
    const_f64_as_f64x8!(LN2F_LO, f64::from_bits(0x3DEA39EF35793C76));
    const_f64_as_f64x8!(VM_SQRT2, 1.414213562373095048801);
    const_f64_as_f64x8!(VM_SMALLEST_NORMAL, 2.2250738585072014E-308);

    let x1 = self;
    let x = Self::fraction_2(x1);
    let e = Self::exponent(x1);
    let mask = x.simd_gt(VM_SQRT2 * HALF);
    let x = (!mask).select(x + x, x);
    let fe = mask.select(e + Self::ONE, e);
    let x = x - Self::ONE;
    let px = polynomial_5!(x, P0, P1, P2, P3, P4, P5);
    let x2 = x * x;
    let px = x2 * x * px;
    let qx = polynomial_5n!(x, Q0, Q1, Q2, Q3, Q4);
    let res = px / qx;
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
      // Both get -Inf here. True subnormal inputs (~5e-324..2.225e-308) should
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

    const SUBN_SCALE: f64 = 1.8014398509481984e16;
    const SUBN_CBRT: f64 = 262144.0;
    let tiny = a.simd_lt(Self::from(f64::MIN_POSITIVE));
    let a = tiny.select(a * Self::from(SUBN_SCALE), a);

    let e = Self::exponent(a) + Self::ONE;
    let d = Self::fraction_2(a);

    // C0..C5 from SLEEF's minimax polynomial for 1/cbrt(d) on [0.5, 1.0)
    // Naoki Shibata et al., "SLEEF: A Portable Vectorized Library of C99
    // Mathematical Functions", https://sleef.org / https://github.com/shibatch/sleef
    // Licensed under the Boost Software License 1.0.
    const_f64_as_f64x8!(C0, 2.2307275302496609725722);
    const_f64_as_f64x8!(C1, -3.85841935510444988821632);
    const_f64_as_f64x8!(C2, 6.03990368989458747961407);
    const_f64_as_f64x8!(C3, -5.73353060922947843636166);
    const_f64_as_f64x8!(C4, 2.96155103020039511818595);
    const_f64_as_f64x8!(C5, -0.640245898480692909870982);
    let mut x = polynomial_5!(d, C0, C1, C2, C3, C4, C5);

    // Newton for 1/cbrt: x = x - (d * x^4 - x) / 3.
    let x2 = x * x;
    let x4 = x2 * x2;
    x = x - d.mul_add(x4, -x) * Self::from(1.0 / 3.0);

    // cbrt(d) = d * x^2, then polish
    let mut y = (d * x) * x;
    let yx = y * x;
    let t = Self::from(2.0 / 3.0);
    y = y - t * y * (yx - Self::ONE);

    // Scale by 2^(e/3) = 2^k * 2^(r/3)
    let three = Self::from(3.0);
    let two = Self::from(2.0);
    let neg = e.simd_lt(Self::ZERO);
    let e_adj = neg.select(e - two, e);
    let k = (e_adj / three).trunc();
    let r = e - three * k;
    const_f64_as_f64x8!(CBRT2, 1.2599210498948732);
    const_f64_as_f64x8!(CBRT4, 1.5874010519681994);
    y = r.simd_eq(Self::ONE).select(y * CBRT2, y);
    y = r.simd_eq(two).select(y * CBRT4, y);
    y *= Self::vm_pow2n(k);
    y = tiny.select(y / Self::from(SUBN_CBRT), y);

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
    const_f64_as_f64x8!(R4asin, 2.967721961301243206100E-3);
    const_f64_as_f64x8!(R3asin, -5.634242780008963776856E-1);
    const_f64_as_f64x8!(R2asin, 6.968710824104713396794E0);
    const_f64_as_f64x8!(R1asin, -2.556901049652824852289E1);
    const_f64_as_f64x8!(R0asin, 2.853665548261061424989E1);

    const_f64_as_f64x8!(S3asin, -2.194779531642920639778E1);
    const_f64_as_f64x8!(S2asin, 1.470656354026814941758E2);
    const_f64_as_f64x8!(S1asin, -3.838770957603691357202E2);
    const_f64_as_f64x8!(S0asin, 3.424398657913078477438E2);

    const_f64_as_f64x8!(P5asin, 4.253011369004428248960E-3);
    const_f64_as_f64x8!(P4asin, -6.019598008014123785661E-1);
    const_f64_as_f64x8!(P3asin, 5.444622390564711410273E0);
    const_f64_as_f64x8!(P2asin, -1.626247967210700244449E1);
    const_f64_as_f64x8!(P1asin, 1.956261983317594739197E1);
    const_f64_as_f64x8!(P0asin, -8.198089802484824371615E0);

    const_f64_as_f64x8!(Q4asin, -1.474091372988853791896E1);
    const_f64_as_f64x8!(Q3asin, 7.049610280856842141659E1);
    const_f64_as_f64x8!(Q2asin, -1.471791292232726029859E2);
    const_f64_as_f64x8!(Q1asin, 1.395105614657485689735E2);
    const_f64_as_f64x8!(Q0asin, -4.918853881490881290097E1);

    let xa = self.abs();

    let big = xa.simd_ge(f64x8::splat(0.625));

    let x1 = big.select(f64x8::splat(1.0) - xa, xa * xa);

    let x2 = x1 * x1;
    let x3 = x2 * x1;
    let x4 = x2 * x2;
    let x5 = x4 * x1;

    let do_big = big.any();
    let do_small = !big.all();

    let mut rx = f64x8::default();
    let mut sx = f64x8::default();
    let mut px = f64x8::default();
    let mut qx = f64x8::default();

    if do_big {
      rx = x3.mul_add(R3asin, x2 * R2asin)
        + x4.mul_add(R4asin, x1.mul_add(R1asin, R0asin));
      sx =
        x3.mul_add(S3asin, x4) + x2.mul_add(S2asin, x1.mul_add(S1asin, S0asin));
    }
    if do_small {
      px = x3.mul_add(P3asin, P0asin)
        + x4.mul_add(P4asin, x1 * P1asin)
        + x5.mul_add(P5asin, x2 * P2asin);
      qx = x4.mul_add(Q4asin, x5)
        + x3.mul_add(Q3asin, x1 * Q1asin)
        + x2.mul_add(Q2asin, Q0asin);
    };

    let vx = big.select(rx, px);
    let wx = big.select(sx, qx);

    let y1 = vx / wx * x1;

    let mut z1 = f64x8::default();
    let mut z2 = f64x8::default();
    if do_big {
      let xb = (x1 + x1).sqrt();
      z1 = xb.mul_add(y1, xb);
    }

    if do_small {
      z2 = xa.mul_add(y1, xa);
    }

    // asin
    let z3 = f64x8::FRAC_PI_2 - z1;
    let asin = big.select(z3, z2);
    let asin = asin.flip_signs(self);

    asin
  }

  #[inline]
  pub fn acos(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f64_as_f64x8!(R4asin, 2.967721961301243206100E-3);
    const_f64_as_f64x8!(R3asin, -5.634242780008963776856E-1);
    const_f64_as_f64x8!(R2asin, 6.968710824104713396794E0);
    const_f64_as_f64x8!(R1asin, -2.556901049652824852289E1);
    const_f64_as_f64x8!(R0asin, 2.853665548261061424989E1);

    const_f64_as_f64x8!(S3asin, -2.194779531642920639778E1);
    const_f64_as_f64x8!(S2asin, 1.470656354026814941758E2);
    const_f64_as_f64x8!(S1asin, -3.838770957603691357202E2);
    const_f64_as_f64x8!(S0asin, 3.424398657913078477438E2);

    const_f64_as_f64x8!(P5asin, 4.253011369004428248960E-3);
    const_f64_as_f64x8!(P4asin, -6.019598008014123785661E-1);
    const_f64_as_f64x8!(P3asin, 5.444622390564711410273E0);
    const_f64_as_f64x8!(P2asin, -1.626247967210700244449E1);
    const_f64_as_f64x8!(P1asin, 1.956261983317594739197E1);
    const_f64_as_f64x8!(P0asin, -8.198089802484824371615E0);

    const_f64_as_f64x8!(Q4asin, -1.474091372988853791896E1);
    const_f64_as_f64x8!(Q3asin, 7.049610280856842141659E1);
    const_f64_as_f64x8!(Q2asin, -1.471791292232726029859E2);
    const_f64_as_f64x8!(Q1asin, 1.395105614657485689735E2);
    const_f64_as_f64x8!(Q0asin, -4.918853881490881290097E1);

    let xa = self.abs();

    let big = xa.simd_ge(f64x8::splat(0.625));

    let x1 = big.select(f64x8::splat(1.0) - xa, xa * xa);

    let x2 = x1 * x1;
    let x3 = x2 * x1;
    let x4 = x2 * x2;
    let x5 = x4 * x1;

    let do_big = big.any();
    let do_small = !big.all();

    let mut rx = f64x8::default();
    let mut sx = f64x8::default();
    let mut px = f64x8::default();
    let mut qx = f64x8::default();

    if do_big {
      rx = x3.mul_add(R3asin, x2 * R2asin)
        + x4.mul_add(R4asin, x1.mul_add(R1asin, R0asin));
      sx =
        x3.mul_add(S3asin, x4) + x2.mul_add(S2asin, x1.mul_add(S1asin, S0asin));
    }
    if do_small {
      px = x3.mul_add(P3asin, P0asin)
        + x4.mul_add(P4asin, x1 * P1asin)
        + x5.mul_add(P5asin, x2 * P2asin);
      qx = x4.mul_add(Q4asin, x5)
        + x3.mul_add(Q3asin, x1 * Q1asin)
        + x2.mul_add(Q2asin, Q0asin);
    };

    let vx = big.select(rx, px);
    let wx = big.select(sx, qx);

    let y1 = vx / wx * x1;

    let mut z1 = f64x8::default();
    let mut z2 = f64x8::default();
    if do_big {
      let xb = (x1 + x1).sqrt();
      z1 = xb.mul_add(y1, xb);
    }

    if do_small {
      z2 = xa.mul_add(y1, xa);
    }

    // acos
    let z3 = self.simd_lt(f64x8::ZERO).select(f64x8::PI - z1, z1);
    let z4 = f64x8::FRAC_PI_2 - z2.flip_signs(self);
    let acos = big.select(z3, z4);

    acos
  }

  #[inline]
  pub fn atan(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f64_as_f64x8!(MORE_BITS, 6.123233995736765886130E-17);
    const_f64_as_f64x8!(MORE_BITS_O2, 6.123233995736765886130E-17 * 0.5);
    const_f64_as_f64x8!(T3PO8, core::f64::consts::SQRT_2 + 1.0);

    const_f64_as_f64x8!(P4atan, -8.750608600031904122785E-1);
    const_f64_as_f64x8!(P3atan, -1.615753718733365076637E1);
    const_f64_as_f64x8!(P2atan, -7.500855792314704667340E1);
    const_f64_as_f64x8!(P1atan, -1.228866684490136173410E2);
    const_f64_as_f64x8!(P0atan, -6.485021904942025371773E1);

    const_f64_as_f64x8!(Q4atan, 2.485846490142306297962E1);
    const_f64_as_f64x8!(Q3atan, 1.650270098316988542046E2);
    const_f64_as_f64x8!(Q2atan, 4.328810604912902668951E2);
    const_f64_as_f64x8!(Q1atan, 4.853903996359136964868E2);
    const_f64_as_f64x8!(Q0atan, 1.945506571482613964425E2);

    let t = self.abs();

    // small:  t < 0.66
    // medium: t <= t <= 2.4142 (1+sqrt(2))
    // big:    t > 2.4142
    let notbig = t.simd_le(T3PO8);
    let notsmal = t.simd_ge(Self::splat(0.66));

    let mut s = notbig.select(Self::FRAC_PI_4, Self::FRAC_PI_2);
    s = notsmal & s;
    let mut fac = notbig.select(MORE_BITS_O2, MORE_BITS);
    fac = notsmal & fac;

    // small:  z = t / 1.0;
    // medium: z = (t-1.0) / (t+1.0);
    // big:    z = -1.0 / t;
    let mut a = notbig & t;
    a = notsmal.select(a - Self::ONE, a);
    let mut b = notbig & Self::ONE;
    b = notsmal.select(b + t, b);
    let z = a / b;

    let zz = z * z;

    let px = polynomial_4!(zz, P0atan, P1atan, P2atan, P3atan, P4atan);
    let qx = polynomial_5n!(zz, Q0atan, Q1atan, Q2atan, Q3atan, Q4atan);

    let mut re = (px / qx).mul_add(z * zz, z);
    re += s + fac;

    // get sign bit
    re = (self.is_sign_negative()).select(-re, re);

    re
  }

  #[inline]
  pub fn atan2(self, x: Self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f64_as_f64x8!(MORE_BITS, 6.123233995736765886130E-17);
    const_f64_as_f64x8!(MORE_BITS_O2, 6.123233995736765886130E-17 * 0.5);
    const_f64_as_f64x8!(T3PO8, core::f64::consts::SQRT_2 + 1.0);

    const_f64_as_f64x8!(P4atan, -8.750608600031904122785E-1);
    const_f64_as_f64x8!(P3atan, -1.615753718733365076637E1);
    const_f64_as_f64x8!(P2atan, -7.500855792314704667340E1);
    const_f64_as_f64x8!(P1atan, -1.228866684490136173410E2);
    const_f64_as_f64x8!(P0atan, -6.485021904942025371773E1);

    const_f64_as_f64x8!(Q4atan, 2.485846490142306297962E1);
    const_f64_as_f64x8!(Q3atan, 1.650270098316988542046E2);
    const_f64_as_f64x8!(Q2atan, 4.328810604912902668951E2);
    const_f64_as_f64x8!(Q1atan, 4.853903996359136964868E2);
    const_f64_as_f64x8!(Q0atan, 1.945506571482613964425E2);

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

    // x = y = 0 gives NAN here
    let t = y2 / x2;

    // small:  t < 0.66
    // medium: t <= t <= 2.4142 (1+sqrt(2))
    // big:    t > 2.4142
    let notbig = t.simd_le(T3PO8);
    let notsmal = t.simd_ge(Self::splat(0.66));

    let mut s = notbig.select(Self::FRAC_PI_4, Self::FRAC_PI_2);
    s = notsmal & s;
    let mut fac = notbig.select(MORE_BITS_O2, MORE_BITS);
    fac = notsmal & fac;

    // small:  z = t / 1.0;
    // medium: z = (t-1.0) / (t+1.0);
    // big:    z = -1.0 / t;
    let mut a = notbig & t;
    a = notsmal.select(a - Self::ONE, a);
    let mut b = notbig & Self::ONE;
    b = notsmal.select(b + t, b);
    let z = a / b;

    let zz = z * z;

    let px = polynomial_4!(zz, P0atan, P1atan, P2atan, P3atan, P4atan);
    let qx = polynomial_5n!(zz, Q0atan, Q1atan, Q2atan, Q3atan, Q4atan);

    let mut re = (px / qx).mul_add(z * zz, z);
    re += s + fac;

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

    const_f64_as_f64x8!(P0sin, -1.66666666666666307295E-1);
    const_f64_as_f64x8!(P1sin, 8.33333333332211858878E-3);
    const_f64_as_f64x8!(P2sin, -1.98412698295895385996E-4);
    const_f64_as_f64x8!(P3sin, 2.75573136213857245213E-6);
    const_f64_as_f64x8!(P4sin, -2.50507477628578072866E-8);
    const_f64_as_f64x8!(P5sin, 1.58962301576546568060E-10);

    const_f64_as_f64x8!(P0cos, 4.16666666666665929218E-2);
    const_f64_as_f64x8!(P1cos, -1.38888888888730564116E-3);
    const_f64_as_f64x8!(P2cos, 2.48015872888517045348E-5);
    const_f64_as_f64x8!(P3cos, -2.75573141792967388112E-7);
    const_f64_as_f64x8!(P4cos, 2.08757008419747316778E-9);
    const_f64_as_f64x8!(P5cos, -1.13585365213876817300E-11);

    const_f64_as_f64x8!(DP1, 7.853981554508209228515625E-1 * 2.);
    const_f64_as_f64x8!(DP2, 7.94662735614792836714E-9 * 2.);
    const_f64_as_f64x8!(DP3, 3.06161699786838294307E-17 * 2.);

    const_f64_as_f64x8!(TWO_OVER_PI, 2.0 / core::f64::consts::PI);

    let xa = self.abs();

    let y = (xa * TWO_OVER_PI).round_ties_even();
    let q = y.round_int();

    let x = y.mul_neg_add(DP3, y.mul_neg_add(DP2, y.mul_neg_add(DP1, xa)));

    let x2 = x * x;
    let mut s = polynomial_5!(x2, P0sin, P1sin, P2sin, P3sin, P4sin, P5sin);
    let mut c = polynomial_5!(x2, P0cos, P1cos, P2cos, P3cos, P4cos, P5cos);
    s = (x * x2).mul_add(s, x);
    c =
      (x2 * x2).mul_add(c, x2.mul_neg_add(f64x8::from(0.5), f64x8::from(1.0)));

    let swap = !((q & i64x8::from(1)).simd_eq(i64x8::from(0)));

    let mut overflow: f64x8 = cast(q.simd_gt(i64x8::from(0x80000000000000)));
    overflow &= xa.is_finite();
    s = overflow.select(f64x8::from(0.0), s);
    c = overflow.select(f64x8::from(1.0), c);

    // calc sin
    let mut sin1 = cast::<_, f64x8>(swap).select(c, s);
    let sign_sin: i64x8 = (q << 62) ^ cast::<_, i64x8>(self);
    sin1 = sin1.flip_signs(cast(sign_sin));

    // calc cos
    let mut cos1 = cast::<_, f64x8>(swap).select(s, c);
    let sign_cos: i64x8 = ((q + i64x8::from(1)) & i64x8::from(2)) << 62;
    cos1 ^= cast::<_, f64x8>(sign_cos);

    // IEEE 754: sin/cos(±∞) = NaN, sin/cos(NaN) = NaN
    let finite = self.is_finite();
    let nan = Self::splat(f64::NAN);
    let sin_final = finite.select(sin1, nan);
    let cos_final = finite.select(cos1, nan);

    (sin_final, cos_final)
  }

  #[inline]
  pub fn asin_acos(self) -> (Self, Self) {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f64_as_f64x8!(R4asin, 2.967721961301243206100E-3);
    const_f64_as_f64x8!(R3asin, -5.634242780008963776856E-1);
    const_f64_as_f64x8!(R2asin, 6.968710824104713396794E0);
    const_f64_as_f64x8!(R1asin, -2.556901049652824852289E1);
    const_f64_as_f64x8!(R0asin, 2.853665548261061424989E1);

    const_f64_as_f64x8!(S3asin, -2.194779531642920639778E1);
    const_f64_as_f64x8!(S2asin, 1.470656354026814941758E2);
    const_f64_as_f64x8!(S1asin, -3.838770957603691357202E2);
    const_f64_as_f64x8!(S0asin, 3.424398657913078477438E2);

    const_f64_as_f64x8!(P5asin, 4.253011369004428248960E-3);
    const_f64_as_f64x8!(P4asin, -6.019598008014123785661E-1);
    const_f64_as_f64x8!(P3asin, 5.444622390564711410273E0);
    const_f64_as_f64x8!(P2asin, -1.626247967210700244449E1);
    const_f64_as_f64x8!(P1asin, 1.956261983317594739197E1);
    const_f64_as_f64x8!(P0asin, -8.198089802484824371615E0);

    const_f64_as_f64x8!(Q4asin, -1.474091372988853791896E1);
    const_f64_as_f64x8!(Q3asin, 7.049610280856842141659E1);
    const_f64_as_f64x8!(Q2asin, -1.471791292232726029859E2);
    const_f64_as_f64x8!(Q1asin, 1.395105614657485689735E2);
    const_f64_as_f64x8!(Q0asin, -4.918853881490881290097E1);

    let xa = self.abs();

    let big = xa.simd_ge(f64x8::splat(0.625));

    let x1 = big.select(f64x8::splat(1.0) - xa, xa * xa);

    let x2 = x1 * x1;
    let x3 = x2 * x1;
    let x4 = x2 * x2;
    let x5 = x4 * x1;

    let do_big = big.any();
    let do_small = !big.all();

    let mut rx = f64x8::default();
    let mut sx = f64x8::default();
    let mut px = f64x8::default();
    let mut qx = f64x8::default();

    if do_big {
      rx = x3.mul_add(R3asin, x2 * R2asin)
        + x4.mul_add(R4asin, x1.mul_add(R1asin, R0asin));
      sx =
        x3.mul_add(S3asin, x4) + x2.mul_add(S2asin, x1.mul_add(S1asin, S0asin));
    }

    if do_small {
      px = x3.mul_add(P3asin, P0asin)
        + x4.mul_add(P4asin, x1 * P1asin)
        + x5.mul_add(P5asin, x2 * P2asin);
      qx = x4.mul_add(Q4asin, x5)
        + x3.mul_add(Q3asin, x1 * Q1asin)
        + x2.mul_add(Q2asin, Q0asin);
    };

    let vx = big.select(rx, px);
    let wx = big.select(sx, qx);

    let y1 = vx / wx * x1;

    let mut z1 = f64x8::default();
    let mut z2 = f64x8::default();
    if do_big {
      let xb = (x1 + x1).sqrt();
      z1 = xb.mul_add(y1, xb);
    }

    if do_small {
      z2 = xa.mul_add(y1, xa);
    }

    // asin
    let z3 = f64x8::FRAC_PI_2 - z1;
    let asin = big.select(z3, z2);
    let asin = asin.flip_signs(self);

    // acos
    let z3 = self.simd_lt(f64x8::ZERO).select(f64x8::PI - z1, z1);
    let z4 = f64x8::FRAC_PI_2 - z2.flip_signs(self);
    let acos = big.select(z3, z4);

    (asin, acos)
  }

  #[inline]
  pub fn exp_m1(self) -> Self {
    const_f64_as_f64x8!(P2, 1.0 / 2.0);
    const_f64_as_f64x8!(P3, 1.0 / 6.0);
    const_f64_as_f64x8!(P4, 1.0 / 24.0);
    const_f64_as_f64x8!(P5, 1.0 / 120.0);
    const_f64_as_f64x8!(P6, 1.0 / 720.0);
    const_f64_as_f64x8!(P7, 1.0 / 5040.0);
    const_f64_as_f64x8!(P8, 1.0 / 40320.0);
    const_f64_as_f64x8!(P9, 1.0 / 362880.0);
    const_f64_as_f64x8!(P10, 1.0 / 3628800.0);
    const_f64_as_f64x8!(P11, 1.0 / 39916800.0);
    const_f64_as_f64x8!(P12, 1.0 / 479001600.0);
    const_f64_as_f64x8!(P13, 1.0 / 6227020800.0);
    // LN2D_HI/LO: double-double decomposition of ln(2) for exp range reduction,
    // following fdlibm's approach (Sun Microsystems, https://www.netlib.org/fdlibm/ e_exp.c).
    const_f64_as_f64x8!(LN2D_HI, 0.693145751953125);
    const_f64_as_f64x8!(LN2D_LO, 1.42860682030941723212E-6);
    // x < -37.429: e^x < 2⁻⁵⁴, exp_m1(x) = -1.0 exactly (mantissa exhaustion)
    // IEEE simd_lt returns false for NaN, so NaN lanes can't reach here.
    // -inf is < -37.429, and exp_m1(-inf) = -1.0, also correct.
    if self.simd_lt(f64x8::from(-37.429)).all() {
      return f64x8::from(-1.0);
    }
    // max_x = ln(f64::MAX) ≈ 709.7827129, max_r = 1023 (IEEE max normal
    // exponent) min_x = -1074.5 ln(2) ≈ -744.79: min r for vm_pow2n to
    // construct subnormal
    let max_x = f64x8::from(709.783);
    let min_x = f64x8::from(-744.79);
    let max_r = f64x8::from(1023.0);
    let r = (self * Self::LOG2_E).round_ties_even();
    let big = r.simd_gt(max_r);
    let r_safe = big.select(max_r, r);
    let excess = r - max_r;
    let excess = big.select(excess, Self::ZERO);
    let scale = Self::vm_pow2n(excess);
    let x = r.mul_neg_add(LN2D_HI, self);
    let x = r.mul_neg_add(LN2D_LO, x);
    let z =
      polynomial_13!(x, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
    let n2 = Self::vm_pow2n(r_safe);
    let exp_val = (z + Self::ONE) * scale * n2;
    // When r == 0, z is already e^x - 1 from the Taylor poly.
    // Computing (z+1) - 1 would lose low bits for small x (catastrophic
    // cancellation at z ~ 0), so keep z directly.
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
    // where u = 1+x. From MUSL libc (Rich Felker et al., https://musl.libc.org) src/math/log1p.c
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
    const_f64_as_f64x8!(P0, 1.0);
    const_f64_as_f64x8!(P1, 1.0 / 6.0);
    const_f64_as_f64x8!(P2, 1.0 / 120.0);
    const_f64_as_f64x8!(P3, 1.0 / 5040.0);
    const_f64_as_f64x8!(P4, 1.0 / 362880.0);
    const_f64_as_f64x8!(P5, 1.0 / 39916800.0);
    const_f64_as_f64x8!(P6, 1.0 / 6227020800.0);
    let a = self.abs();
    // |x| < 0.5: Taylor poly; last truncation term < 1 ULP at x=0.5 for both types
    let small = a.simd_lt(f64x8::from(0.5));
    let t = a * a;
    let poly = a * polynomial_6!(t, P0, P1, P2, P3, P4, P5, P6);
    let exp_based = {
      let e = a.exp();
      (e - Self::ONE / e) * Self::HALF
    };
    let result = small.select(poly, exp_based);
    result.flip_signs(self)
  }

  #[inline]
  pub fn cosh(self) -> Self {
    const_f64_as_f64x8!(P0, 1.0);
    const_f64_as_f64x8!(P1, 1.0 / 2.0);
    const_f64_as_f64x8!(P2, 1.0 / 24.0);
    const_f64_as_f64x8!(P3, 1.0 / 720.0);
    const_f64_as_f64x8!(P4, 1.0 / 40320.0);
    const_f64_as_f64x8!(P5, 1.0 / 3628800.0);
    const_f64_as_f64x8!(P6, 1.0 / 479001600.0);
    const_f64_as_f64x8!(P7, 1.0 / 87178291200.0);
    let a = self.abs();
    // |x| < 0.5: Taylor poly; last truncation term < 1 ULP at x=0.5 for both types
    let small = a.simd_lt(f64x8::from(0.5));
    let t = a * a;
    let poly = polynomial_7!(t, P0, P1, P2, P3, P4, P5, P6, P7);
    let exp_based = {
      let e = a.exp();
      (e + Self::ONE / e) * Self::HALF
    };
    small.select(poly, exp_based)
  }

  #[inline]
  pub fn tanh(self) -> Self {
    // |x| < 5e-8: tanh(x) ≈ x, error x³/3 < 16·ULP(x)
    // bound: x² < 48·2⁻⁵² → x < 1.03e-7; 5e-8 has 2× margin
    // |x| > 19.062: tanh(x) = ±1 to f64 precision (e⁻²ˣ < 2⁻⁵⁴)
    let a = self.abs();
    let large = a.simd_gt(f64x8::from(19.062));
    if large.all() {
      return Self::ONE.flip_signs(self);
    }
    let small = a.simd_lt(f64x8::from(5e-8));
    let exp_based = {
      let t = (Self::from(-2.0) * a).exp_m1();
      let pos = -t / (t + Self::from(2.0));
      pos.flip_signs(self)
    };
    let result = small.select(self, exp_based);
    large.select(Self::ONE.flip_signs(self), result)
  }
}

/// The following functionality exists only for [`f64x8`], or only for
/// particular types inconsistently.
impl f64x8 {
  #[inline]
  fn vm_pow2n(self) -> Self {
    const_f64_as_f64x8!(pow2_52, 4503599627370496.0);
    const_f64_as_f64x8!(bias, 1023.0);
    let a = self + (bias + pow2_52);
    let c = cast::<_, i64x8>(a) << 52;
    let std_result = cast::<_, f64x8>(c);

    let min_exp = f64x8::from(-1022.0);
    let is_sub = self.simd_lt(min_exp);
    if is_sub.any() {
      let valid = self.simd_ge(f64x8::from(-1074.0));
      let shift_f = self + f64x8::from(1074.0);
      let mut shift_i = shift_f.trunc_int();
      shift_i = cast::<_, i64x8>(valid).select(shift_i, i64x8::ZERO);
      let mantissa = i64x8::ONE << shift_i;
      let sub_result = cast::<_, f64x8>(mantissa);
      let sub_result = valid.select(sub_result, f64x8::ZERO);
      is_sub.select(sub_result, std_result)
    } else {
      std_result
    }
  }

  #[inline]
  fn exponent(self) -> f64x8 {
    const_f64_as_f64x8!(pow2_52, 4503599627370496.0);
    const_f64_as_f64x8!(bias, 1023.0);
    let a = cast::<_, u64x8>(self);
    let b = a >> 52;
    let c = b | cast::<_, u64x8>(pow2_52);
    let d = cast::<_, f64x8>(c);
    let e = d - (pow2_52 + bias);
    e
  }

  #[inline]
  fn fraction_2(self) -> Self {
    let t1 = cast::<_, u64x8>(self);
    let t2 = cast::<_, u64x8>(
      (t1 & u64x8::from(0x000FFFFFFFFFFFFF)) | u64x8::from(0x3FE0000000000000),
    );
    cast::<_, f64x8>(t2)
  }
  #[inline]
  fn is_zero_or_subnormal(self) -> Self {
    let t = cast::<_, i64x8>(self);
    let t = t & i64x8::splat(0x7FF0000000000000);
    let mask = t.simd_eq(i64x8::splat(0));
    cast::<_, f64x8>(mask)
  }
  #[inline]
  fn infinity() -> Self {
    cast::<_, f64x8>(i64x8::splat(0x7FF0000000000000))
  }
  #[inline]
  fn nan_log() -> Self {
    cast::<_, f64x8>(i64x8::splat(0x7FF8000000000000 | 0x101 << 29))
  }
  #[inline]
  fn nan_pow() -> Self {
    cast::<_, f64x8>(i64x8::splat(0x7FF8000000000000 | 0x101 << 29))
  }

  /// Converts each element from [`i32`] to [`f64`].
  #[inline]
  pub fn from_i32x8(v: i32x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: convert_to_m512d_from_i32_m256i(v.avx2) }
      } else {
        Self::new([
          v.as_array()[0] as f64,
          v.as_array()[1] as f64,
          v.as_array()[2] as f64,
          v.as_array()[3] as f64,
          v.as_array()[4] as f64,
          v.as_array()[5] as f64,
          v.as_array()[6] as f64,
          v.as_array()[7] as f64,
        ])
      }
    }
  }
}

impl From<i32x8> for f64x8 {
  #[inline]
  fn from(v: i32x8) -> Self {
    Self::from_i32x8(v)
  }
}
