use super::*;

pick! {
  if #[cfg(target_feature="avx512f")] {
    /// A SIMD vector with 16 elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct u32x16 { pub(crate) avx512: m512i }
  } else {
    /// A SIMD vector with 16 elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct u32x16 { pub(crate) a : u32x8, pub(crate) b : u32x8 }
  }
}

impl_simd_uint! {
  unsafe {
    T = u32,
    N = 16,
    Simd = u32x16,
    IntSimd = i32x16,
    T_BITS = 32,
    T_BITS_MUL_2 = 64,
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
    optional_type_x86_inner { X86Inner = __m512i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn not(self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitxor_m512i(self.avx512, set_splat_i32_m512i(-1)) }
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
        Self { avx512: add_i32_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: sub_i32_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: mul_i32_keep_low_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.mul(rhs.a),
          b : self.b.mul(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn bitand(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitand_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: bitor_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: bitxor_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u32_m512i::<{cmp_int_op!(Eq)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u32_m512i::<{cmp_int_op!(Ne)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u32_m512i::<{cmp_int_op!(Lt)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u32_m512i::<{cmp_int_op!(Nle)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u32_m512i::<{cmp_int_op!(Le)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u32_m512i::<{cmp_int_op!(Nlt)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> u32 {
    let array: [u32x8; 2] = cast(self);
    (array[0] + array[1]).reduce_add()
  }

  #[inline]
  pub fn reduce_mul(self) -> u32 {
    let array: [u32x8; 2] = cast(self);
    (array[0] * array[1]).reduce_mul()
  }

  #[inline]
  pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self {
          avx512: bitor_m512i(
            bitand_m512i(if_one.avx512, self.avx512),
            bitandnot_m512i(self.avx512, if_zero.avx512),
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
        Self { avx512: blend_varying_i8_m512i(if_false.avx512,if_true.avx512,movepi8_mask_m512i(self.avx512)) }
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
      if #[cfg(target_feature="avx512dq")] {
        movepi32_mask_m512i(self.avx512) as u32
      } else {
        f32x16::to_bitmask(cast(self))
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        movepi32_mask_m512i(self.avx512) != 0
      } else {
        let [a, b]: [u32x8; 2] = cast(self);
        (a | b).any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        movepi32_mask_m512i(self.avx512) == 0xFFFF
      } else {
        let [a, b]: [u32x8; 2] = cast(self);
        (a & b).all()
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u32x16) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        Self { avx512: permute_i32_m512i(indices.avx512, self.avx512) }
      } else {
        let self_halfs = cast::<u32x16, [u32x8; 2]>(self);
        let [indices_a, indices_b] = cast::<u32x16, [u32x8; 2]>(indices);

        cast([self_halfs.shuffle(indices_a), self_halfs.shuffle(indices_b)])
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u32x16) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        self.shuffle(indices) & indices.simd_lt(16)
      } else {
        let self_halfs = cast::<u32x16, [u32x8; 2]>(self);
        let [indices_a, indices_b] = cast::<u32x16, [u32x8; 2]>(indices);

        cast([self_halfs.shuffle_zeroing(indices_a), self_halfs.shuffle_zeroing(indices_b)])
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u32x16) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        let self_halfs = cast::<u32x16, [u32x8; 2]>(self);
        let [indices_a, indices_b] = cast::<u32x16, [u32x8; 2]>(indices);

        cast([self_halfs.shuffle_wrapping(indices_a), self_halfs.shuffle_wrapping(indices_b)])
      }
    }
  }

  #[inline]
  fn shuffle(self: [u32x16; 2], indices: u32x16) -> u32x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        u32x16 { avx512: shuffle_abv_i32_all_m512i(self[0].avx512, indices.avx512, self[1].avx512) }
      } else {
        self[0].shuffle_zeroing(indices) | self[1].shuffle_zeroing(indices - 16)
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x16; 2], indices: u32x16) -> u32x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        self.shuffle(indices) & indices.simd_lt(32)
      } else {
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x16; 2], indices: u32x16) -> u32x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 31)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u32x16; 3], indices: u32x16) -> u32x16 {
    [self[0], self[1]].shuffle_zeroing(indices) | self[2].shuffle_zeroing(indices - 32)
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x16; 3], indices: u32x16) -> u32x16 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x16; 3], indices: u32x16) -> u32x16 {
    self.shuffle(indices % 48)
  }

  #[inline]
  fn shuffle(self: [u32x16; 4], indices: u32x16) -> u32x16 {
    [self[0], self[1]].shuffle_zeroing(indices) | [self[2], self[3]].shuffle_zeroing(indices - 32)
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x16; 4], indices: u32x16) -> u32x16 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x16; 4], indices: u32x16) -> u32x16 {
    self.shuffle(indices & 63)
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [Self; 16]) -> [Self; 16] {
    // Can this be optimized?

    #[inline(always)]
    fn transpose_column(data: &[u32x16; 16], index: usize) -> u32x16 {
      u32x16::new([
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
  fn shl(self, rhs: u32x16) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        let shift_by = bitand_m512i(rhs.avx512, set_splat_i32_m512i(31));
        Self { avx512: shl_each_u32_m512i(self.avx512, shift_by) }
      } else {
        Self {
          a : self.a.shl(rhs.a),
          b : self.b.shl(rhs.b),
        }
      }
    }
  }

  #[inline]
  fn shl(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = rhs & 31;
        Self { avx512: shl_all_u32_m512i(self.avx512, shift) }
      } else {
        Self {
          a : self.a.shl(rhs),
          b : self.b.shl(rhs),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32x16) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        let shift_by = bitand_m512i(rhs.avx512, set_splat_i32_m512i(31));
        Self { avx512: shr_each_u32_m512i(self.avx512, shift_by ) }
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
      if #[cfg(target_feature="avx512f")] {
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = rhs & 31;
        Self { avx512: shr_all_u32_m512i(self.avx512, shift) }
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
        Self { avx512: max_u32_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: min_u32_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.min(rhs.a),
          b: self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u32 {
    let array: [u32x8; 2] = cast(self);
    array[0].max(array[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> u32 {
    let array: [u32x8; 2] = cast(self);
    array[0].min(array[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: shl_each_u32_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.unbounded_shl(rhs.a),
          b: self.b.unbounded_shl(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: shl_all_u32_m512i(self.avx512, rhs) }
      } else {
        Self {
          a: self.a.unbounded_shl_scalar(rhs),
          b: self.b.unbounded_shl_scalar(rhs),
        }
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: shr_each_u32_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: shr_all_u32_m512i(self.avx512, rhs) }
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
        let overflow = result.simd_lt(self);
        // Return `MAX` (all bits set) if overflow occurs.
        result | overflow
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
        let no_overflow = result.simd_le(self);
        // Return `0` (no bits set) if overflow occurs.
        result & no_overflow
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
    let overflow = high.simd_ne(Self::ZERO);
    (low, overflow)
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `u64x16` type.
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    pick! {
      if #[cfg(all(target_feature="avx512f", target_feature="avx512dq"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::{_mm512_unpackhi_epi64, _mm512_unpacklo_epi64};
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::{_mm512_unpackhi_epi64, _mm512_unpacklo_epi64};

        let even_wide_mul = mul_u32_wide_m512i(self.avx512, rhs.avx512);
        let odd_wide_mul = mul_u32_wide_m512i(
          shuffle_i32_m512i::<0b_00_11_00_01>(self.avx512),
          shuffle_i32_m512i::<0b_00_11_00_01>(rhs.avx512),
        );

        let ll_hh_1 = unpack_low_i32_m512i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m512i(even_wide_mul, odd_wide_mul);
        // TODO(safe_arch): Add `_mm512_unpacklo_epi64` and `_mm512_unpackhi_epi64`.
        (
          Self {
            avx512: m512i(unsafe { _mm512_unpacklo_epi64(ll_hh_1.0, ll_hh_2.0) }),
          },
          Self {
            avx512: m512i(unsafe { _mm512_unpackhi_epi64(ll_hh_1.0, ll_hh_2.0) }),
          },
        )
      } else {
        let [self_a, self_b] = cast::<u32x16, [u32x8; 2]>(self);
        let [rhs_a, rhs_b] = cast::<u32x16, [u32x8; 2]>(rhs);

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
      if #[cfg(target_feature="avx512f")] {
        let alo = extract_m256i32_from_m512i::<0>(self.avx512);
        let ahi = extract_m256i32_from_m512i::<1>(self.avx512);
        let blo = extract_m256i32_from_m512i::<0>(rhs.avx512);
        let bhi = extract_m256i32_from_m512i::<1>(rhs.avx512);

        let lo_res: m256i = {
          let a8 = u32x8 { avx2: alo };
          let b8 = u32x8 { avx2: blo };
          a8.mul_keep_high(b8).avx2
        };
        let hi_res: m256i = {
          let a8 = u32x8 { avx2: ahi };
          let b8 = u32x8 { avx2: bhi };
          a8.mul_keep_high(b8).avx2
        };

        let zero = zeroed_m512i();
        let with_lo = insert_m256i32_to_m512i::<0>(zero, lo_res);
        let combined = insert_m256i32_to_m512i::<1>(with_lo, hi_res);

        Self { avx512: combined }
      } else {
        Self {
          a: self.a.mul_keep_high(rhs.a),
          b: self.b.mul_keep_high(rhs.b),
        }
      }
    }
  }

  optional_fn_deserialize {}
}

impl From<u16x16> for u32x16 {
  /// Widens and zero-extends each u16 lane to u32
  #[inline]
  fn from(v: u16x16) -> Self {
    pick! {
      if #[cfg(target_feature = "avx512f")] {
        Self {
          avx512: convert_to_u32_m512i_from_u16_m256i(v.avx2)
        }
      } else if #[cfg(target_feature = "avx2")] {
        let lo: m128i = extract_m128i_from_m256i::<0>(v.avx2);
        let hi: m128i = extract_m128i_from_m256i::<1>(v.avx2);
        Self {
          a: u32x8 { avx2: convert_to_i32_m256i_from_u16_m128i(lo) },
          b: u32x8 { avx2: convert_to_i32_m256i_from_u16_m128i(hi) },
        }
      } else if #[cfg(target_feature = "sse2")] {
        Self {
          a: u32x8 {
            a: u32x4 {
              sse: shr_imm_u32_m128i::<16>(unpack_low_i16_m128i(v.a.sse, v.a.sse))
            },
            b: u32x4 {
              sse: shr_imm_u32_m128i::<16>(unpack_high_i16_m128i(v.a.sse, v.a.sse))
            },
          },
          b: u32x8 {
            a: u32x4 {
              sse: shr_imm_u32_m128i::<16>(unpack_low_i16_m128i(v.b.sse, v.b.sse))
            },
            b: u32x4 {
              sse: shr_imm_u32_m128i::<16>(unpack_high_i16_m128i(v.b.sse, v.b.sse))
            },
          },
        }
      } else {
        // Portable fallback
        let arr = v.as_array();
        Self::new([
          arr[0] as u32,  arr[1] as u32,  arr[2] as u32,  arr[3] as u32,
          arr[4] as u32,  arr[5] as u32,  arr[6] as u32,  arr[7] as u32,
          arr[8] as u32,  arr[9] as u32,  arr[10] as u32, arr[11] as u32,
          arr[12] as u32, arr[13] as u32, arr[14] as u32, arr[15] as u32,
        ])
      }
    }
  }
}

/// The following functionality exists only for [`u32x16`], or only for
/// particular types inconsistently.
impl u32x16 {
  /// Returns `[self[0], b[0], self[1], b[1], ...]`, interleaving the low half
  /// of each vector.
  #[inline]
  #[must_use]
  pub fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // `_mm512_unpacklo_epi32` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [aa, _]: [u32x8; 2] = cast(self);
        let [ba, _]: [u32x8; 2] = cast(b);
        cast([aa.unpack_lo(ba), aa.unpack_hi(ba)])
      } else {
        Self { a: self.a.unpack_lo(b.a), b: self.a.unpack_hi(b.a) }
      }
    }
  }

  /// Returns `[self[8], b[8], self[9], b[9], ...]`, interleaving the high half
  /// of each vector.
  #[inline]
  #[must_use]
  pub fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // `_mm512_unpackhi_epi32` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [_, ab]: [u32x8; 2] = cast(self);
        let [_, bb]: [u32x8; 2] = cast(b);
        cast([ab.unpack_lo(bb), ab.unpack_hi(bb)])
      } else {
        Self { a: self.b.unpack_lo(b.b), b: self.b.unpack_hi(b.b) }
      }
    }
  }

  /// `self + ((a * b) mod 2^W)`, reading only the low `W` bits of each lane of
  /// `a` and `b`. `W` must be in `1..=32`.
  ///
  /// There is no IFMA equivalent at this width, and below 17 bits no widening
  /// multiply is needed either: the whole product fits a lane, so the ordinary
  /// lane multiply already yields both halves.
  #[inline]
  #[must_use]
  pub fn add_mul_lo<const W: u32>(self, a: Self, b: Self) -> Self {
    if W <= 16 {
      let mask = Self::splat(add_mul_operand_mask_u32::<W>());
      return self + (((a & mask) * (b & mask)) & mask);
    }

    let acc = self.to_array();
    let a = a.to_array();
    let b = b.to_array();
    Self::new(core::array::from_fn(|i| {
      add_mul_lo_lane_u32::<W>(acc[i], a[i], b[i])
    }))
  }

  /// `self + ((a * b) >> W)`, reading only the low `W` bits of each lane of `a`
  /// and `b`. `W` must be in `1..=32`.
  #[inline]
  #[must_use]
  pub fn add_mul_hi<const W: u32>(self, a: Self, b: Self) -> Self {
    // See `add_mul_lo`: the whole product is in the lane, so the high half is a
    // shift.
    if W <= 16 {
      let mask = Self::splat(add_mul_operand_mask_u32::<W>());
      return self + (((a & mask) * (b & mask)) >> W);
    }

    let acc = self.to_array();
    let a = a.to_array();
    let b = b.to_array();
    Self::new(core::array::from_fn(|i| {
      add_mul_hi_lane_u32::<W>(acc[i], a[i], b[i])
    }))
  }
}
