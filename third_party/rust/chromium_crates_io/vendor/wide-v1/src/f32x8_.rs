use super::*;

pick! {
  if #[cfg(target_feature="avx")] {
    /// A SIMD vector with eight elements of type [`f32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq)]
    #[repr(C, align(32))]
    pub struct f32x8 { pub(crate) avx: m256 }
  } else {
    /// A SIMD vector with eight elements of type [`f32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq)]
    #[repr(C, align(32))]
    pub struct f32x8 { pub(crate) a : f32x4, pub(crate) b : f32x4 }
  }
}

macro_rules! const_f32_as_f32x8 {
  ($i:ident, $f:expr) => {
    #[allow(non_upper_case_globals)]
    pub const $i: f32x8 = f32x8::new([$f; 8]);
  };
}

impl_simd_float! {
  unsafe {
    T = f32,
    N = 8,
    Simd = f32x8,
    IntSimd = i32x8,
    UintT = u32,
    UintSimd = u32x8,
    optional_type_x86_inner { X86Inner = __m256 },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }
  old_powf_simd_fn_name = pow_f32x8,

  #[inline]
  fn neg(self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: bitxor_m256(self.avx, Self::splat(-0.0).avx) }
      } else {
        Self {
          a : self.a.neg(),
          b : self.b.neg(),
        }
      }
    }
  }

  #[inline]
  fn not(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: self.avx.not()  }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: add_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: sub_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: mul_m256(self.avx, rhs.avx) }
      } else {
        Self {
          a : self.a.mul(rhs.a),
          b : self.b.mul(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn div(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: div_m256(self.avx, rhs.avx) }
      } else {
        Self {
          a : self.a.div(rhs.a),
          b : self.b.div(rhs.b),
        }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: bitand_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: bitor_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: bitxor_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(EqualOrdered)}>(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(NotEqualUnordered)}>(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(LessThanOrdered)}>(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(GreaterThanOrdered)}>(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(LessEqualOrdered)}>(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(GreaterEqualOrdered)}>(self.avx, rhs.avx) }
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
      // From https://stackoverflow.com/questions/13219146/how-to-sum-m256-horizontally
      if #[cfg(target_feature="avx")]{
        let hi_quad = extract_m128_from_m256::<1>(self.avx);
        let lo_quad = cast_to_m128_from_m256(self.avx);
        let sum_quad = add_m128(lo_quad,hi_quad);
        let lo_dual = sum_quad;
        let hi_dual = move_high_low_m128(sum_quad,sum_quad);
        let sum_dual = add_m128(lo_dual,hi_dual);
        let lo = sum_dual;
        let hi = shuffle_abi_f32_all_m128::<0b_01>(sum_dual, sum_dual);
        let sum = add_m128_s(lo, hi);
        get_f32_from_m128_s(sum)
      } else {
        self.a.reduce_add() + self.b.reduce_add()
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> f32 {
    pick! {
      // From https://stackoverflow.com/questions/13219146/how-to-sum-m256-horizontally
      if #[cfg(target_feature="avx")] {
        let hi_quad = extract_m128_from_m256::<1>(self.avx);
        let lo_quad = cast_to_m128_from_m256(self.avx);
        let product_quad = mul_m128(lo_quad,hi_quad);
        let lo_dual = product_quad;
        let hi_dual = move_high_low_m128(product_quad, product_quad);
        let product_dual = mul_m128(lo_dual,hi_dual);
        let lo = product_dual;
        let hi = shuffle_abi_f32_all_m128::<0b_01>(product_dual, product_dual);
        let product = mul_m128_s(lo, hi);
        get_f32_from_m128_s(product)
      } else {
        self.a.reduce_mul() * self.b.reduce_mul()
      }
    }
  }

  #[inline]
  pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self {
          avx: bitor_m256(
            bitand_m256(if_one.avx, self.avx),
            bitandnot_m256(self.avx, if_zero.avx),
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
      if #[cfg(target_feature="avx")] {
        Self { avx: blend_varying_m256(if_false.avx, if_true.avx, self.avx) }
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
      if #[cfg(target_feature="avx")] {
        move_mask_m256(self.avx) as u32
      } else {
        (self.b.to_bitmask() << 4) | self.a.to_bitmask()
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx")] {
        move_mask_m256(self.avx) != 0
      } else {
        self.a.any() || self.b.any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx")] {
        move_mask_m256(self.avx) == 0b11111111
      } else {
        self.a.all() && self.b.all()
      }
    }
  }

  ///
  /// Currently this function is only accelerated on `avx`.
  #[inline]
  pub fn transpose(data: [f32x8; 8]) -> [f32x8; 8] {
    pick! {
      if #[cfg(target_feature="avx")] {
        let a0 = unpack_lo_m256(data[0].avx, data[1].avx);
        let a1 = unpack_hi_m256(data[0].avx, data[1].avx);
        let a2 = unpack_lo_m256(data[2].avx, data[3].avx);
        let a3 = unpack_hi_m256(data[2].avx, data[3].avx);
        let a4 = unpack_lo_m256(data[4].avx, data[5].avx);
        let a5 = unpack_hi_m256(data[4].avx, data[5].avx);
        let a6 = unpack_lo_m256(data[6].avx, data[7].avx);
        let a7 = unpack_hi_m256(data[6].avx, data[7].avx);

        pub const fn mm_shuffle(z: i32, y: i32, x: i32, w: i32) -> i32 {
          (z << 6) | (y << 4) | (x << 2) | w
        }

        const SHUFF_LO : i32 = mm_shuffle(1,0,1,0);
        const SHUFF_HI : i32 = mm_shuffle(3,2,3,2);

        // possible todo: intel performance manual suggests alternative with blend to avoid port 5 pressure
        // (since blend runs on a different port than shuffle)
        let b0 = shuffle_m256::<SHUFF_LO>(a0,a2);
        let b1 = shuffle_m256::<SHUFF_HI>(a0,a2);
        let b2 = shuffle_m256::<SHUFF_LO>(a1,a3);
        let b3 = shuffle_m256::<SHUFF_HI>(a1,a3);
        let b4 = shuffle_m256::<SHUFF_LO>(a4,a6);
        let b5 = shuffle_m256::<SHUFF_HI>(a4,a6);
        let b6 = shuffle_m256::<SHUFF_LO>(a5,a7);
        let b7 = shuffle_m256::<SHUFF_HI>(a5,a7);

        [
          f32x8 { avx: permute2z_m256::<0x20>(b0, b4) },
          f32x8 { avx: permute2z_m256::<0x20>(b1, b5) },
          f32x8 { avx: permute2z_m256::<0x20>(b2, b6) },
          f32x8 { avx: permute2z_m256::<0x20>(b3, b7) },
          f32x8 { avx: permute2z_m256::<0x31>(b0, b4) },
          f32x8 { avx: permute2z_m256::<0x31>(b1, b5) },
          f32x8 { avx: permute2z_m256::<0x31>(b2, b6) },
          f32x8 { avx: permute2z_m256::<0x31>(b3, b7) }
        ]
      } else {
        // possible todo: not sure that 128bit SIMD gives us a a lot of speedup here

        #[inline(always)]
        fn transpose_column(data: &[f32x8; 8], index: usize) -> f32x8 {
          f32x8::new([
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
    }
  }

  #[inline]
  pub fn is_nan(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: cmp_op_mask_m256::<{cmp_op!(Unordered)}>(self.avx, self.avx) }
      } else {
        Self {
          a : self.a.is_nan(),
          b : self.b.is_nan(),
        }
      }
    }
  }

  #[inline]
  pub fn is_inf(self) -> Self {
    let shifted_inf = u32x8::from(0xFF000000);
    let u: u32x8 = cast(self);
    let shift_u = u << 1_u64;
    let out = (shift_u).simd_eq(shifted_inf);
    cast(out)
  }

  #[inline]
  pub fn is_finite(self) -> Self {
    let shifted_exp_mask = u32x8::from(0xFF000000);
    let u: u32x8 = cast(self);
    let shift_u = u << 1_u64;
    let out = !(shift_u & shifted_exp_mask).simd_eq(shifted_exp_mask);
    cast(out)
  }

  #[inline]
  pub fn is_sign_positive(self) -> Self {
    const SIGN_MASK: u32x8 = u32x8::splat((-0.0_f32).to_bits());

    let bits = cast::<f32x8, u32x8>(self);
    let sign = bits & SIGN_MASK;
    let result = sign.simd_eq(u32x8::ZERO);
    cast::<u32x8, f32x8>(result)
  }

  #[inline]
  pub fn is_sign_negative(self) -> Self {
    const SIGN_MASK: u32x8 = u32x8::splat((-0.0_f32).to_bits());

    let bits = cast::<f32x8, u32x8>(self);
    let sign = bits & SIGN_MASK;
    let result = sign.simd_eq(SIGN_MASK);
    cast::<u32x8, f32x8>(result)
  }

  #[inline]
  pub fn recip(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: reciprocal_m256(self.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: reciprocal_sqrt_m256(self.avx) }
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
      if #[cfg(target_feature="avx")] {
        // max_m256 seems to do rhs < self ? self : rhs. So if there's any NaN
        // involved, it chooses rhs, so we need to specifically check rhs for
        // NaN.
        rhs.is_nan().select(self, Self { avx: max_m256(self.avx, rhs.avx) })
      } else {
        Self {
          a : self.a.max(rhs.a),
          b : self.b.max(rhs.b),
        }
      }

    }
  }

  #[inline]
  pub fn fast_max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: max_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
        // min_m256 seems to do rhs > self ? self : rhs. So if there's any NaN
        // involved, it chooses rhs, so we need to specifically check rhs for
        // NaN.
        rhs.is_nan().select(self, Self { avx: min_m256(self.avx, rhs.avx) })
      } else {
        Self {
          a : self.a.min(rhs.a),
          b : self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn fast_min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: min_m256(self.avx, rhs.avx) }
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
      if #[cfg(target_feature="avx")] {
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
      if #[cfg(target_feature="avx")] {
        // For both `min_m256` and `max_m256` if any input is NaN, `rhs` gets
        // chosen. For `self` to be chosen, `self` must be the second argument.
        Self { avx: max_m256(min.avx, min_m256(max.avx, self.avx)) }
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
      if #[cfg(target_feature="avx")] {
        let non_sign_bits = f32x8::from(f32::from_bits(i32::MAX as u32));
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
      if #[cfg(target_feature="avx")] {
        Self { avx: floor_m256(self.avx) }
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
      if #[cfg(target_feature="avx")] {
        Self { avx: ceil_m256(self.avx) }
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
      if #[cfg(target_feature="avx2")] {
        const_f32_as_f32x8!(HALF_NEXT_DOWN, 0.5_f32.next_down());
        const_f32_as_f32x8!(BOUNDS_LIMIT, 8388608.0);

        let self_abs = self.abs();

        let adjusted_self = self_abs + Self::HALF;
        let result_abs = Self { avx: round_m256::<{round_op!(Zero)}>(adjusted_self.avx) };
        // The addition breaks for `0.5.next_down()` which incorrectly rounds to
        // `1.0`. This resets the result back to `0.0`.
        let result_abs = result_abs & self_abs.simd_ne(HALF_NEXT_DOWN);

        // Large value, infinity and NaN need special handling.
        let bounds_mask: Self = cast(cmp_gt_mask_i32_m256i(cast(BOUNDS_LIMIT), cast(self_abs)));

        // `abs` keeps the original sign.
        bounds_mask.abs().bitselect(result_abs, self)
      } else {
        let [a, b] = cast::<f32x8, [f32x4; 2]>(self);
        cast([a.round(), b.round()])
      }
    }
  }

  #[inline]
  pub fn round_int(self) -> i32x8 {
    pick! {
      if #[cfg(target_feature="avx")] {
        // Based on: https://github.com/v8/v8/blob/210987a552a2bf2a854b0baa9588a5959ff3979d/src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h#L489-L504
        let non_nan_mask = self.simd_eq(self);
        let non_nan = self & non_nan_mask;
        let flip_to_max: i32x8 = cast(self.simd_ge(Self::splat(2147483648.0)));
        let cast: i32x8 = cast(convert_to_i32_m256i_from_m256(non_nan.avx));
        flip_to_max ^ cast
      } else {
        cast([
          self.a.round_int(),
          self.b.round_int(),
        ])
      }
    }
  }

  #[inline]
  pub fn fast_round_int(self) -> i32x8 {
    pick! {
      if #[cfg(target_feature="avx")] {
        cast(convert_to_i32_m256i_from_m256(self.avx))
      } else {
        cast([
          self.a.fast_round_int(),
          self.b.fast_round_int()])
      }
    }
  }

  #[inline]
  pub fn round_ties_even(self) -> Self {
    pick! {
      // NOTE: Is there an SSE2 version of this? f32x4 version probably translates but I've not had time to figure it out
      if #[cfg(target_feature="avx")] {
        Self { avx: round_m256::<{round_op!(Nearest)}>(self.avx) }
      } else {
        Self {
          a : self.a.round_ties_even(),
          b : self.b.round_ties_even(),
        }
      }
    }
  }

  #[inline]
  pub fn trunc(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx")] {
        Self { avx: round_m256::<{round_op!(Zero)}>(self.avx) }
      } else {
        Self {
          a : self.a.trunc(),
          b : self.b.trunc(),
        }
      }
    }
  }

  #[inline]
  pub fn trunc_int(self) -> i32x8 {
    pick! {
        if #[cfg(target_feature="avx")] {
        // Based on: https://github.com/v8/v8/blob/210987a552a2bf2a854b0baa9588a5959ff3979d/src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h#L489-L504
        let non_nan_mask = self.simd_eq(self);
        let non_nan = self & non_nan_mask;
        let flip_to_max: i32x8 = cast(self.simd_ge(Self::splat(2147483648.0)));
        let cast: i32x8 = cast(convert_truncate_to_i32_m256i_from_m256(non_nan.avx));
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
  pub fn fast_trunc_int(self) -> i32x8 {
    pick! {
      if #[cfg(all(target_feature="avx"))] {
        cast(convert_truncate_to_i32_m256i_from_m256(self.avx))
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
  /// - On `x86`/`x86_64` with AVX+FMA: Uses `vfmadd` (single rounding, best
  ///   accuracy)
  /// - On `x86`/`x86_64` with AVX only: Uses `(self * m) + a` (two roundings)
  /// - Other platforms: Delegates to [`f32x4`] (may use NEON FMA or fallback)
  #[inline]
  pub fn mul_add(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx",target_feature="fma"))] {
        Self { avx: fused_mul_add_m256(self.avx, a.avx, b.avx) }
      } else if #[cfg(target_feature="avx")] {
        // still want to use 256 bit ops
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
  /// - On `x86`/`x86_64` with AVX+FMA: Uses `vfmsub` (single rounding, best
  ///   accuracy)
  /// - On `x86`/`x86_64` with AVX only: Uses `(self * m) - s` (two roundings)
  /// - Other platforms: Delegates to [`f32x4`] (may use NEON FMA or fallback)
  #[inline]
  pub fn mul_sub(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx",target_feature="fma"))] {
        Self { avx: fused_mul_sub_m256(self.avx, a.avx, b.avx) }
      } else if #[cfg(target_feature="avx")] {
        // still want to use 256 bit ops
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
  /// - On `x86`/`x86_64` with AVX+FMA: Uses `vfnmadd` (single rounding, best
  ///   accuracy)
  /// - On `x86`/`x86_64` with AVX only: Uses `a - (self * m)` (two roundings)
  /// - Other platforms: Delegates to [`f32x4`] (may use NEON FMA or fallback)
  #[inline]
  pub fn mul_neg_add(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx",target_feature="fma"))] {
        Self { avx: fused_mul_neg_add_m256(self.avx, a.avx, b.avx) }
      } else if #[cfg(target_feature="avx")] {
        // still want to use 256 bit ops
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
  /// - On `x86`/`x86_64` with AVX+FMA: Uses `vfnmsub` (single rounding, best
  ///   accuracy)
  /// - On `x86`/`x86_64` with AVX only: Uses `-(self * m) - s` (two roundings)
  /// - Other platforms: Delegates to [`f32x4`] (may use NEON FMA or fallback)
  #[inline]
  pub fn mul_neg_sub(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx",target_feature="fma"))] {
        Self { avx: fused_mul_neg_sub_m256(self.avx, a.avx, b.avx) }
      } else if #[cfg(target_feature="avx")] {
        // still want to use 256 bit ops
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
    const_f32_as_f32x8!(ln2f_hi, 0.693359375);
    const_f32_as_f32x8!(ln2f_lo, -2.12194440e-4);
    const_f32_as_f32x8!(P0logf, 3.3333331174E-1);
    const_f32_as_f32x8!(P1logf, -2.4999993993E-1);
    const_f32_as_f32x8!(P2logf, 2.0000714765E-1);
    const_f32_as_f32x8!(P3logf, -1.6668057665E-1);
    const_f32_as_f32x8!(P4logf, 1.4249322787E-1);
    const_f32_as_f32x8!(P5logf, -1.2420140846E-1);
    const_f32_as_f32x8!(P6logf, 1.1676998740E-1);
    const_f32_as_f32x8!(P7logf, -1.1514610310E-1);
    const_f32_as_f32x8!(P8logf, 7.0376836292E-2);

    const_f32_as_f32x8!(p2expf, 1.0 / 2.0); // coefficients for Taylor expansion of exp
    const_f32_as_f32x8!(p3expf, 1.0 / 6.0);
    const_f32_as_f32x8!(p4expf, 1.0 / 24.0);
    const_f32_as_f32x8!(p5expf, 1.0 / 120.0);
    const_f32_as_f32x8!(p6expf, 1.0 / 720.0);
    const_f32_as_f32x8!(p7expf, 1.0 / 5040.0);

    let x1 = self.abs();
    let x = x1.fraction_2();
    let mask = x.simd_gt(f32x8::SQRT_2 * f32x8::HALF);
    let x = (!mask).select(x + x, x);

    let x = x - f32x8::ONE;
    let x2 = x * x;
    let lg1 = polynomial_8!(
      x, P0logf, P1logf, P2logf, P3logf, P4logf, P5logf, P6logf, P7logf, P8logf
    );
    let lg1 = lg1 * x2 * x;

    let ef = x1.exponent();
    let ef = mask.select(ef + f32x8::ONE, ef);
    let e1 = (ef * n).round_ties_even();
    let yr = ef.mul_sub(n, e1);

    let lg = f32x8::HALF.mul_neg_add(x2, x) + lg1;
    let x2_err = (f32x8::HALF * x).mul_sub(x, f32x8::HALF * x2);
    let lg_err = f32x8::HALF.mul_add(x2, lg - x) - lg1;

    let e2 = (lg * n * f32x8::LOG2_E).round_ties_even();
    let v = lg.mul_sub(n, e2 * ln2f_hi);
    let v = e2.mul_neg_add(ln2f_lo, v);
    let v = v - (lg_err + x2_err).mul_sub(n, yr * f32x8::LN_2);

    let x = v;
    let e3 = (x * f32x8::LOG2_E).round_ties_even();
    let x = e3.mul_neg_add(f32x8::LN_2, x);
    let x2 = x * x;
    let z = x2.mul_add(
      polynomial_5!(x, p2expf, p3expf, p4expf, p5expf, p6expf, p7expf),
      x + f32x8::ONE,
    );

    let ee = e1 + e2 + e3;
    let ei = cast::<_, i32x8>(ee.round_int());
    let ej = cast::<_, i32x8>(ei + (cast::<_, i32x8>(z) >> 23));

    let overflow = cast::<_, f32x8>(ej.simd_gt(i32x8::splat(0x0FF)))
      | (ee.simd_gt(f32x8::splat(300.0)));
    let underflow = cast::<_, f32x8>(ej.simd_lt(i32x8::splat(0x000)))
      | (ee.simd_lt(f32x8::splat(-300.0)));

    // Add exponent by integer addition
    let z = cast::<_, f32x8>(cast::<_, i32x8>(z) + (ei << 23));
    // Check for overflow/underflow
    let z = underflow.select(f32x8::ZERO, z);
    let z = overflow.select(Self::infinity(), z);

    // Check for self == 0
    let x_zero = self.is_zero_or_subnormal();
    let z = x_zero.select(
      n.simd_lt(f32x8::ZERO).select(
        Self::infinity(),
        n.simd_eq(f32x8::ZERO).select(f32x8::ONE, f32x8::ZERO),
      ),
      z,
    );

    let x_sign = self.is_sign_negative();
    let z = if x_sign.any() {
      // Y into an integer
      let yi = n.simd_eq(n.round_ties_even());

      // Is y odd? If yes flip the sign of the result.
      let y_odd = cast::<i32x8, f32x8>(n.round_int() << 31);

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
      if #[cfg(target_feature="avx")] {
        Self { avx: sqrt_m256(self.avx) }
      } else {
        Self {
          a : self.a.sqrt(),
          b : self.b.sqrt(),
        }
      }
    }
  }

  /// Calculate the exponent of a packed `f32x8`
  #[inline]
  pub fn exp(self) -> Self {
    const_f32_as_f32x8!(P0, 1.0 / 2.0);
    const_f32_as_f32x8!(P1, 1.0 / 6.0);
    const_f32_as_f32x8!(P2, 1.0 / 24.0);
    const_f32_as_f32x8!(P3, 1.0 / 120.0);
    const_f32_as_f32x8!(P4, 1.0 / 720.0);
    const_f32_as_f32x8!(P5, 1.0 / 5040.0);
    // LN2D_HI/LO: double-double decomposition of ln(2) for exp range reduction,
    // following the approach from fdlibm's e_exp.c (Sun Microsystems,
    // https://www.netlib.org/fdlibm/). The f32 split uses f32-precision constants
    // (0.693359375, -2.12194440e-4) summing to ln(2) with single-precision
    // accuracy; the f64 variants use a full f64 double-double
    // decomposition.
    const_f32_as_f32x8!(LN2D_HI, 0.693359375);
    const_f32_as_f32x8!(LN2D_LO, -2.12194440e-4);
    // max_x = ln(f32::MAX) ≈ 88.7229, max_r = 127 (IEEE max normal exponent)
    // min_x = -149.5 ln(2) ≈ -103.63: min r for vm_pow2n subnormal
    let max_x = f32x8::from(88.723);
    let min_x = f32x8::from(-103.63);
    // x < min_x: e^x underflows to 0 -- skip the entire pipeline
    let finite = self.is_finite();
    let neg_underflow = self.simd_lt(min_x) & finite;
    if neg_underflow.all() {
      return Self::ZERO;
    }
    let max_r = f32x8::from(127.0);
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
    const_f32_as_f32x8!(P2, 1.0 / 2.0);
    const_f32_as_f32x8!(P3, 1.0 / 6.0);
    const_f32_as_f32x8!(P4, 1.0 / 24.0);
    const_f32_as_f32x8!(P5, 1.0 / 120.0);
    const_f32_as_f32x8!(P6, 1.0 / 720.0);
    const_f32_as_f32x8!(P7, 1.0 / 5040.0);

    // max_x = log2(f32::MAX) ≈ 127.99999
    // min_x = log2(f32::MIN_POSITIVE) - 23 ≈ -126 - 23 = -149
    let max_x = f32x8::from(127.99999);
    let min_x = f32x8::from(-149.5);
    let finite = self.is_finite();
    let neg_underflow = self.simd_lt(min_x) & finite;
    if neg_underflow.all() {
      return Self::ZERO;
    }

    let round = self.round_ties_even();
    let max_r = f32x8::from(127.0);
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
    const_f32_as_f32x8!(HALF, 0.5);
    const_f32_as_f32x8!(P0, 3.3333331174E-1);
    const_f32_as_f32x8!(P1, -2.4999993993E-1);
    const_f32_as_f32x8!(P2, 2.0000714765E-1);
    const_f32_as_f32x8!(P3, -1.6668057665E-1);
    const_f32_as_f32x8!(P4, 1.4249322787E-1);
    const_f32_as_f32x8!(P5, -1.2420140846E-1);
    const_f32_as_f32x8!(P6, 1.1676998740E-1);
    const_f32_as_f32x8!(P7, -1.1514610310E-1);
    const_f32_as_f32x8!(P8, 7.0376836292E-2);
    const_f32_as_f32x8!(LN2F_HI, 0.693359375);
    const_f32_as_f32x8!(LN2F_LO, -2.12194440e-4);
    const_f32_as_f32x8!(VM_SMALLEST_NORMAL, 1.17549435E-38);

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
    const_f32_as_f32x8!(C0, 2.2241257);
    const_f32_as_f32x8!(C1, -3.8095417);
    const_f32_as_f32x8!(C2, 5.8982625);
    const_f32_as_f32x8!(C3, -5.532182);
    const_f32_as_f32x8!(C4, 2.8208892);
    const_f32_as_f32x8!(C5, -0.60156447);
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
    const_f32_as_f32x8!(CBRT2, 1.259921);
    const_f32_as_f32x8!(CBRT4, 1.587401);
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
    const_f32_as_f32x8!(P4asinf, 4.2163199048E-2);
    const_f32_as_f32x8!(P3asinf, 2.4181311049E-2);
    const_f32_as_f32x8!(P2asinf, 4.5470025998E-2);
    const_f32_as_f32x8!(P1asinf, 7.4953002686E-2);
    const_f32_as_f32x8!(P0asinf, 1.6666752422E-1);

    let xa = self.abs();
    let big = xa.simd_ge(f32x8::splat(0.5));

    let x1 = f32x8::splat(0.5) * (f32x8::ONE - xa);
    let x2 = xa * xa;
    let x3 = big.select(x1, x2);

    let xb = x1.sqrt();

    let x4 = big.select(xb, xa);

    let z = polynomial_4!(x3, P0asinf, P1asinf, P2asinf, P3asinf, P4asinf);
    let z = z.mul_add(x3 * x4, x4);

    let z1 = z + z;

    // asin
    let z3 = f32x8::FRAC_PI_2 - z1;
    let asin = big.select(z3, z);
    let asin = asin.flip_signs(self);

    asin
  }

  #[inline]
  pub fn acos(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x8!(P4asinf, 4.2163199048E-2);
    const_f32_as_f32x8!(P3asinf, 2.4181311049E-2);
    const_f32_as_f32x8!(P2asinf, 4.5470025998E-2);
    const_f32_as_f32x8!(P1asinf, 7.4953002686E-2);
    const_f32_as_f32x8!(P0asinf, 1.6666752422E-1);

    let xa = self.abs();
    let big = xa.simd_ge(f32x8::splat(0.5));

    let x1 = f32x8::splat(0.5) * (f32x8::ONE - xa);
    let x2 = xa * xa;
    let x3 = big.select(x1, x2);

    let xb = x1.sqrt();

    let x4 = big.select(xb, xa);

    let z = polynomial_4!(x3, P0asinf, P1asinf, P2asinf, P3asinf, P4asinf);
    let z = z.mul_add(x3 * x4, x4);

    let z1 = z + z;

    // acos
    let z3 = self.simd_lt(f32x8::ZERO).select(f32x8::PI - z1, z1);
    let z4 = f32x8::FRAC_PI_2 - z.flip_signs(self);
    let acos = big.select(z3, z4);

    acos
  }

  #[inline]
  pub fn atan(self) -> Self {
    // Based on the Agner Fog "vector class library":
    // https://github.com/vectorclass/version2/blob/master/vectormath_trig.h
    const_f32_as_f32x8!(P3atanf, 8.05374449538E-2);
    const_f32_as_f32x8!(P2atanf, -1.38776856032E-1);
    const_f32_as_f32x8!(P1atanf, 1.99777106478E-1);
    const_f32_as_f32x8!(P0atanf, -3.33329491539E-1);

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
    const_f32_as_f32x8!(P3atanf, 8.05374449538E-2);
    const_f32_as_f32x8!(P2atanf, -1.38776856032E-1);
    const_f32_as_f32x8!(P1atanf, 1.99777106478E-1);
    const_f32_as_f32x8!(P0atanf, -3.33329491539E-1);

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

    const_f32_as_f32x8!(DP1F, 0.78515625_f32 * 2.0);
    const_f32_as_f32x8!(DP2F, 2.4187564849853515625E-4_f32 * 2.0);
    const_f32_as_f32x8!(DP3F, 3.77489497744594108E-8_f32 * 2.0);

    const_f32_as_f32x8!(P0sinf, -1.6666654611E-1);
    const_f32_as_f32x8!(P1sinf, 8.3321608736E-3);
    const_f32_as_f32x8!(P2sinf, -1.9515295891E-4);

    const_f32_as_f32x8!(P0cosf, 4.166664568298827E-2);
    const_f32_as_f32x8!(P1cosf, -1.388731625493765E-3);
    const_f32_as_f32x8!(P2cosf, 2.443315711809948E-5);

    const_f32_as_f32x8!(TWO_OVER_PI, 2.0 / core::f32::consts::PI);

    let xa = self.abs();

    // Find quadrant
    let y = (xa * TWO_OVER_PI).round_ties_even();
    let q: i32x8 = y.round_int();

    let x = y.mul_neg_add(DP3F, y.mul_neg_add(DP2F, y.mul_neg_add(DP1F, xa)));

    let x2 = x * x;
    let mut s = polynomial_2!(x2, P0sinf, P1sinf, P2sinf) * (x * x2) + x;
    let mut c = polynomial_2!(x2, P0cosf, P1cosf, P2cosf) * (x2 * x2)
      + f32x8::from(0.5).mul_neg_add(x2, f32x8::from(1.0));

    let swap = !(q & i32x8::from(1)).simd_eq(i32x8::from(0));

    let mut overflow: f32x8 = cast(q.simd_gt(i32x8::from(0x2000000)));
    overflow &= xa.is_finite();
    s = overflow.select(f32x8::from(0.0), s);
    c = overflow.select(f32x8::from(1.0), c);

    // calc sin
    let mut sin1 = cast::<_, f32x8>(swap).select(c, s);
    let sign_sin: i32x8 = (q << 30) ^ cast::<_, i32x8>(self);
    sin1 = sin1.flip_signs(cast(sign_sin));

    // calc cos
    let mut cos1 = cast::<_, f32x8>(swap).select(s, c);
    let sign_cos: i32x8 = ((q + i32x8::from(1)) & i32x8::from(2)) << 30;
    cos1 ^= cast::<_, f32x8>(sign_cos);

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
    const_f32_as_f32x8!(P4asinf, 4.2163199048E-2);
    const_f32_as_f32x8!(P3asinf, 2.4181311049E-2);
    const_f32_as_f32x8!(P2asinf, 4.5470025998E-2);
    const_f32_as_f32x8!(P1asinf, 7.4953002686E-2);
    const_f32_as_f32x8!(P0asinf, 1.6666752422E-1);

    let xa = self.abs();
    let big = xa.simd_ge(f32x8::splat(0.5));

    let x1 = f32x8::splat(0.5) * (f32x8::ONE - xa);
    let x2 = xa * xa;
    let x3 = big.select(x1, x2);

    let xb = x1.sqrt();

    let x4 = big.select(xb, xa);

    let z = polynomial_4!(x3, P0asinf, P1asinf, P2asinf, P3asinf, P4asinf);
    let z = z.mul_add(x3 * x4, x4);

    let z1 = z + z;

    // acos
    let z3 = self.simd_lt(f32x8::ZERO).select(f32x8::PI - z1, z1);
    let z4 = f32x8::FRAC_PI_2 - z.flip_signs(self);
    let acos = big.select(z3, z4);

    // asin
    let z3 = f32x8::FRAC_PI_2 - z1;
    let asin = big.select(z3, z);
    let asin = asin.flip_signs(self);

    (asin, acos)
  }

  #[inline]
  pub fn exp_m1(self) -> Self {
    // x < -17.329: e^x < 2⁻²⁵, exp_m1(x) = -1.0 exactly (mantissa exhaustion)
    // IEEE simd_lt returns false for NaN, so NaN lanes can't reach here.
    // -inf is < -17.329, and exp_m1(-inf) = -1.0, also correct.
    if self.simd_lt(f32x8::from(-17.329)).all() {
      return f32x8::from(-1.0);
    }
    const_f32_as_f32x8!(P0, 1.0 / 2.0);
    const_f32_as_f32x8!(P1, 1.0 / 6.0);
    const_f32_as_f32x8!(P2, 1.0 / 24.0);
    const_f32_as_f32x8!(P3, 1.0 / 120.0);
    const_f32_as_f32x8!(P4, 1.0 / 720.0);
    const_f32_as_f32x8!(P5, 1.0 / 5040.0);
    // LN2D_HI/LO: double-double decomposition of ln(2) for exp range reduction,
    // following the approach from fdlibm's e_exp.c (Sun Microsystems,
    // https://www.netlib.org/fdlibm/). The f32 split uses f32-precision constants
    // (0.693359375, -2.12194440e-4) summing to ln(2) with single-precision
    // accuracy; the f64 variants use a full f64 double-double
    // decomposition.
    const_f32_as_f32x8!(LN2D_HI, 0.693359375);
    const_f32_as_f32x8!(LN2D_LO, -2.12194440e-4);
    // max_x = ln(f32::MAX) ≈ 88.7229, max_r = 127 (IEEE max normal exponent)
    // min_x = -149.5 ln(2) ≈ -103.63: min r for vm_pow2n subnormal
    let max_x = f32x8::from(88.723);
    let min_x = f32x8::from(-103.63);
    let max_r = f32x8::from(127.0);
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
    const_f32_as_f32x8!(P0, 1.0);
    const_f32_as_f32x8!(P1, 1.0 / 6.0);
    const_f32_as_f32x8!(P2, 1.0 / 120.0);
    const_f32_as_f32x8!(P3, 1.0 / 5040.0);
    let a = self.abs();
    // |x| < 0.5: Taylor poly; last truncation term < 1 ULP at x=0.5 for both types
    let small = a.simd_lt(f32x8::from(0.5));
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
    const_f32_as_f32x8!(P0, 1.0);
    const_f32_as_f32x8!(P1, 1.0 / 2.0);
    const_f32_as_f32x8!(P2, 1.0 / 24.0);
    const_f32_as_f32x8!(P3, 1.0 / 720.0);
    let a = self.abs();
    // |x| < 0.5: Taylor poly; last truncation term < 1 ULP at x=0.5 for both types
    let small = a.simd_lt(f32x8::from(0.5));
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
    let large = a.simd_gt(f32x8::from(9.011));
    if large.all() {
      return Self::ONE.flip_signs(self);
    }
    let small = a.simd_lt(f32x8::from(2e-4));
    let exp_based = {
      let t = (Self::from(-2.0) * a).exp_m1();
      let pos = -t / (t + Self::from(2.0));
      pos.flip_signs(self)
    };
    let result = small.select(self, exp_based);
    large.select(Self::ONE.flip_signs(self), result)
  }
}

/// The following functionality exists only for [`f32x8`], or only for
/// particular types inconsistently.
impl f32x8 {
  #[inline]
  fn vm_pow2n(self) -> Self {
    const_f32_as_f32x8!(pow2_23, 8388608.0);
    const_f32_as_f32x8!(bias, 127.0);
    let a = self + (bias + pow2_23);
    let c = cast::<_, i32x8>(a) << 23;
    let std_result = cast::<_, f32x8>(c);

    let min_exp = f32x8::from(-126.0);
    let is_sub = self.simd_lt(min_exp);
    if is_sub.any() {
      let valid = self.simd_ge(f32x8::from(-149.0));
      let shift_f = self + f32x8::from(149.0);
      let mut shift_i = shift_f.trunc_int();
      shift_i = cast::<_, i32x8>(valid).select(shift_i, i32x8::ZERO);
      let mantissa = i32x8::ONE << shift_i;
      let sub_result = cast::<_, f32x8>(mantissa);
      let sub_result = valid.select(sub_result, f32x8::ZERO);
      is_sub.select(sub_result, std_result)
    } else {
      std_result
    }
  }

  #[inline]
  fn exponent(self) -> f32x8 {
    const_f32_as_f32x8!(pow2_23, 8388608.0);
    const_f32_as_f32x8!(bias, 127.0);
    let a = cast::<_, u32x8>(self);
    let b = a >> 23;
    let c = b | cast::<_, u32x8>(pow2_23);
    let d = cast::<_, f32x8>(c);
    let e = d - (pow2_23 + bias);
    e
  }

  #[inline]
  fn fraction_2(self) -> Self {
    let t1 = cast::<_, u32x8>(self);
    let t2 = cast::<_, u32x8>(
      (t1 & u32x8::from(0x007FFFFF)) | u32x8::from(0x3F000000),
    );
    cast::<_, f32x8>(t2)
  }
  #[inline]
  fn is_zero_or_subnormal(self) -> Self {
    let t = cast::<_, i32x8>(self);
    let t = t & i32x8::splat(0x7F800000);
    let mask = t.simd_eq(i32x8::splat(0));
    cast::<_, f32x8>(mask)
  }
  #[inline]
  fn infinity() -> Self {
    cast::<_, f32x8>(i32x8::splat(0x7F800000))
  }
  #[inline]
  fn nan_log() -> Self {
    cast::<_, f32x8>(i32x8::splat(0x7FC00000 | 0x101 & 0x003FFFFF))
  }
  #[inline]
  fn nan_pow() -> Self {
    cast::<_, f32x8>(i32x8::splat(0x7FC00000 | 0x101 & 0x003FFFFF))
  }

  /// Converts each element from [`i32`] to [`f32`].
  #[inline]
  pub fn from_i32x8(v: i32x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx: convert_to_m256_from_i32_m256i(v.avx2) }
      } else {
        Self::new([
            v.as_array()[0] as f32,
            v.as_array()[1] as f32,
            v.as_array()[2] as f32,
            v.as_array()[3] as f32,
            v.as_array()[4] as f32,
            v.as_array()[5] as f32,
            v.as_array()[6] as f32,
            v.as_array()[7] as f32,
          ])
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
