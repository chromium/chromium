use super::*;

pick! {
  if #[cfg(target_feature="avx512f")] {
    /// A SIMD vector with eight elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct u64x8 { pub(crate) avx512: m512i }
  } else {
    /// A SIMD vector with eight elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct u64x8 { pub(crate) a : u64x4, pub(crate) b : u64x4 }
  }
}

impl_simd_uint! {
  unsafe {
    T = u64,
    N = 8,
    Simd = u64x8,
    IntSimd = i64x8,
    T_BITS = 64,
    T_BITS_MUL_2 = 128,
    [0, 1, 2, 3, 4, 5, 6, 7],
    optional_type_x86_inner { X86Inner = __m512i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn not(self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: bitxor_m512i(self.avx512, set_splat_i64_m512i(-1)) }
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
        Self { avx512: add_i64_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: sub_i64_m512i(self.avx512, rhs.avx512) }
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
        let arr1: [u64; 8] = cast(self);
        let arr2: [u64; 8] = cast(rhs);
        cast([
          arr1[0].wrapping_mul(arr2[0]),
          arr1[1].wrapping_mul(arr2[1]),
          arr1[2].wrapping_mul(arr2[2]),
          arr1[3].wrapping_mul(arr2[3]),
          arr1[4].wrapping_mul(arr2[4]),
          arr1[5].wrapping_mul(arr2[5]),
          arr1[6].wrapping_mul(arr2[6]),
          arr1[7].wrapping_mul(arr2[7]),
        ])
      } else {
        Self { a: self.a.mul(rhs.a), b: self.b.mul(rhs.b) }
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
        Self { avx512: cmp_op_mask_u64_m512i::<{cmp_int_op!(Eq)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u64_m512i::<{cmp_int_op!(Ne)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u64_m512i::<{cmp_int_op!(Lt)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u64_m512i::<{cmp_int_op!(Nle)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u64_m512i::<{cmp_int_op!(Le)}>(self.avx512, rhs.avx512) }
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
        Self { avx512: cmp_op_mask_u64_m512i::<{cmp_int_op!(Nlt)}>(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.simd_ge(rhs.a),
          b : self.b.simd_ge(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> u64 {
    let array: [u64x4; 2] = cast(self);
    (array[0] + array[1]).reduce_add()
  }


  #[inline]
  pub fn reduce_mul(self) -> u64 {
    let array: [u64x4; 2] = cast(self);
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
        // use f64 move_mask since it is the same size as i64
        movepi64_mask_m512d(cast(self.avx512)) as u32
      } else {
        f64x8::to_bitmask(cast(self))
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        movepi64_mask_m512d(cast(self.avx512)) != 0
      } else {
        let [a, b]: [i64x4; 2] = cast(self);
        (a | b).any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        movepi64_mask_m512d(cast(self.avx512)) == 0b11111111
      } else {
        let [a, b]: [i64x4; 2] = cast(self);
        (a & b).all()
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u64x8) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        Self { avx512: permute_i64_m512i(indices.avx512, self.avx512) }
      } else {
        let self_halfs = cast::<u64x8, [u64x4; 2]>(self);
        let [indices_a, indices_b] = cast::<u64x8, [u64x4; 2]>(indices);

        cast([self_halfs.shuffle(indices_a), self_halfs.shuffle(indices_b)])
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u64x8) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        self.shuffle(indices) & indices.simd_lt(8)
      } else {
        let self_halfs = cast::<u64x8, [u64x4; 2]>(self);
        let [indices_a, indices_b] = cast::<u64x8, [u64x4; 2]>(indices);

        cast([self_halfs.shuffle_zeroing(indices_a), self_halfs.shuffle_zeroing(indices_b)])
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u64x8) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        let self_halfs = cast::<u64x8, [u64x4; 2]>(self);
        let [indices_a, indices_b] = cast::<u64x8, [u64x4; 2]>(indices);

        cast([self_halfs.shuffle_wrapping(indices_a), self_halfs.shuffle_wrapping(indices_b)])
      }
    }
  }

  #[inline]
  fn shuffle(self: [u64x8; 2], indices: u64x8) -> u64x8 {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        u64x8 { avx512: shuffle_abv_i64_all_m512i(self[0].avx512, indices.avx512, self[1].avx512) }
      } else {
        self[0].shuffle_zeroing(indices) | self[1].shuffle_zeroing(indices - 8)
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x8; 2], indices: u64x8) -> u64x8 {
    pick! {
      if #[cfg(all(target_feature = "avx512f"))] {
        self.shuffle(indices) & indices.simd_lt(16)
      } else {
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x8; 2], indices: u64x8) -> u64x8 {
    pick! {
      if #[cfg(target_feature = "avx512f")] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 15)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u64x8; 3], indices: u64x8) -> u64x8 {
    [self[0], self[1]].shuffle_zeroing(indices) | self[2].shuffle_zeroing(indices - 16)
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x8; 3], indices: u64x8) -> u64x8 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x8; 3], indices: u64x8) -> u64x8 {
    self.shuffle(indices % 24)
  }

  #[inline]
  fn shuffle(self: [u64x8; 4], indices: u64x8) -> u64x8 {
    [self[0], self[1]].shuffle_zeroing(indices) | [self[2], self[3]].shuffle_zeroing(indices - 16)
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x8; 4], indices: u64x8) -> u64x8 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x8; 4], indices: u64x8) -> u64x8 {
    self.shuffle(indices & 31)
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [Self; 8]) -> [Self; 8] {
    // Can this be optimized?

    #[inline(always)]
    fn transpose_column(data: &[u64x8; 8], index: usize) -> u64x8 {
      u64x8::new([
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
  fn shl(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        let rhs = bitand_m512i(rhs.avx512, set_splat_i64_m512i(63));
        Self { avx512: shl_each_u64_m512i(self.avx512, rhs) }
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
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = rhs as u64 & 63;
        Self { avx512: shl_all_u64_m512i(self.avx512, shift) }
      } else {
        Self {
          a : self.a.shl(rhs),
          b : self.b.shl(rhs),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        let rhs = bitand_m512i(rhs.avx512, set_splat_i64_m512i(63));
        Self { avx512: shr_each_u64_m512i(self.avx512, rhs) }
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
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = rhs as u64 & 63;
        Self { avx512: shr_all_u64_m512i(self.avx512, shift) }
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
        Self { avx512: max_u64_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: min_u64_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a: self.a.min(rhs.a),
          b: self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u64 {
    let array: [u64x4; 2] = cast(self);
    array[0].max(array[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> u64 {
    let array: [u64x4; 2] = cast(self);
    array[0].min(array[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        Self { avx512: shl_each_u64_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: shl_all_u64_m512i(self.avx512, rhs as u64) }
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
        Self { avx512: shr_each_u64_m512i(self.avx512, rhs.avx512) }
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
        Self { avx512: shr_all_u64_m512i(self.avx512, rhs as u64) }
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
    // TODO(perf): This implementation looks quite bad. Is there a better
    // one? This intentionally avoids `mul_keep_low_high` because getting the
    // high bits of 64-bit multiplication could be slow.

    let self_array = self.to_array();
    let rhs_array = rhs.to_array();

    let result = [
      self_array[0].overflowing_mul(rhs_array[0]),
      self_array[1].overflowing_mul(rhs_array[1]),
      self_array[2].overflowing_mul(rhs_array[2]),
      self_array[3].overflowing_mul(rhs_array[3]),
      self_array[4].overflowing_mul(rhs_array[4]),
      self_array[5].overflowing_mul(rhs_array[5]),
      self_array[6].overflowing_mul(rhs_array[6]),
      self_array[7].overflowing_mul(rhs_array[7]),
    ];
    (
      Self::new([
        result[0].0,
        result[1].0,
        result[2].0,
        result[3].0,
        result[4].0,
        result[5].0,
        result[6].0,
        result[7].0,
      ]),
      Self::new([
        -(result[0].1 as i64) as u64,
        -(result[1].1 as i64) as u64,
        -(result[2].1 as i64) as u64,
        -(result[3].1 as i64) as u64,
        -(result[4].1 as i64) as u64,
        -(result[5].1 as i64) as u64,
        -(result[6].1 as i64) as u64,
        -(result[7].1 as i64) as u64,
      ]),
    )
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `u128x8` type.
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    // TODO(perf): This implementation looks quite bad. Is there a better
    // one?

    let self_array = self.to_array();
    let rhs_array = rhs.to_array();

    let widening_mul = [
      (self_array[0] as u128).wrapping_mul(rhs_array[0] as u128),
      (self_array[1] as u128).wrapping_mul(rhs_array[1] as u128),
      (self_array[2] as u128).wrapping_mul(rhs_array[2] as u128),
      (self_array[3] as u128).wrapping_mul(rhs_array[3] as u128),
      (self_array[4] as u128).wrapping_mul(rhs_array[4] as u128),
      (self_array[5] as u128).wrapping_mul(rhs_array[5] as u128),
      (self_array[6] as u128).wrapping_mul(rhs_array[6] as u128),
      (self_array[7] as u128).wrapping_mul(rhs_array[7] as u128),
    ];

    (
      Self::new([
        widening_mul[0] as u64,
        widening_mul[1] as u64,
        widening_mul[2] as u64,
        widening_mul[3] as u64,
        widening_mul[4] as u64,
        widening_mul[5] as u64,
        widening_mul[6] as u64,
        widening_mul[7] as u64,
      ]),
      Self::new([
        (widening_mul[0] >> 64) as u64,
        (widening_mul[1] >> 64) as u64,
        (widening_mul[2] >> 64) as u64,
        (widening_mul[3] >> 64) as u64,
        (widening_mul[4] >> 64) as u64,
        (widening_mul[5] >> 64) as u64,
        (widening_mul[6] >> 64) as u64,
        (widening_mul[7] >> 64) as u64,
      ]),
    )
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        let arr1: [u64; 8] = cast(self);
        let arr2: [u64; 8] = cast(rhs);
        cast([
          (arr1[0] as u128 * arr2[0] as u128 >> 64) as u64,
          (arr1[1] as u128 * arr2[1] as u128 >> 64) as u64,
          (arr1[2] as u128 * arr2[2] as u128 >> 64) as u64,
          (arr1[3] as u128 * arr2[3] as u128 >> 64) as u64,
          (arr1[4] as u128 * arr2[4] as u128 >> 64) as u64,
          (arr1[5] as u128 * arr2[5] as u128 >> 64) as u64,
          (arr1[6] as u128 * arr2[6] as u128 >> 64) as u64,
          (arr1[7] as u128 * arr2[7] as u128 >> 64) as u64,
        ])
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

/// The following functionality exists only for [`u64x8`], or only for
/// particular types inconsistently.
impl u64x8 {
  /// Returns `[self[0], b[0], self[1], b[1], ...]`, interleaving the low half
  /// of each vector.
  #[inline]
  #[must_use]
  pub fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // `_mm512_unpacklo_epi64` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [aa, _]: [u64x4; 2] = cast(self);
        let [ba, _]: [u64x4; 2] = cast(b);
        cast([aa.unpack_lo(ba), aa.unpack_hi(ba)])
      } else {
        Self { a: self.a.unpack_lo(b.a), b: self.a.unpack_hi(b.a) }
      }
    }
  }

  /// Returns `[self[4], b[4], self[5], b[5], ...]`, interleaving the high half
  /// of each vector.
  #[inline]
  #[must_use]
  pub fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // `_mm512_unpackhi_epi64` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [_, ab]: [u64x4; 2] = cast(self);
        let [_, bb]: [u64x4; 2] = cast(b);
        cast([ab.unpack_lo(bb), ab.unpack_hi(bb)])
      } else {
        Self { a: self.b.unpack_lo(b.b), b: self.b.unpack_hi(b.b) }
      }
    }
  }

  /// The exact per-lane product of `a` and `b` masked to `W` bits.
  ///
  /// Only exact for `W <= 32`, where the product still fits a lane, and
  /// callers guard on that. It is instantiated for wider `W` too, since the
  /// guard is a runtime `if` on a const, so it cannot assert the bound itself.
  #[inline]
  #[must_use]
  fn mul_masked<const W: u32>(a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512f")] {
        // `vpmuludq` reads the low 32 bits of a lane anyway, so at `W == 32` the
        // operand masks are already implied and can be dropped.
        let (a, b) = if W == 32 {
          (a, b)
        } else {
          let mask = Self::splat(add_mul_operand_mask_u64::<W>());
          (a & mask, b & mask)
        };

        Self { avx512: mul_u32_wide_m512i(a.avx512, b.avx512) }
      } else {
        // Lane-wise, so each half is independent.
        Self {
          a: u64x4::mul_masked::<W>(a.a, b.a),
          b: u64x4::mul_masked::<W>(a.b, b.b),
        }
      }
    }
  }

  /// `self + ((a * b) mod 2^W)`, reading only the low `W` bits of each lane of
  /// `a` and `b`. `W` must be in `1..=64`.
  #[inline]
  #[must_use]
  pub fn add_mul_lo<const W: u32>(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512ifma")] {
        // IFMA is fixed at 52 bits; any other width takes the generic path.
        if W == 52 {
          return Self {
            avx512: add_mul_low_u52_m512i(self.avx512, a.avx512, b.avx512),
          };
        }
      }
    }

    // Below 33 bits the whole product fits a lane, so one widening multiply
    // yields both halves and the split is a mask rather than an instruction.
    if W <= 32 {
      let mask = Self::splat(add_mul_operand_mask_u64::<W>());
      return self + (Self::mul_masked::<W>(a, b) & mask);
    }

    let acc = self.to_array();
    let a = a.to_array();
    let b = b.to_array();
    Self::new(core::array::from_fn(|i| {
      add_mul_lo_lane_u64::<W>(acc[i], a[i], b[i])
    }))
  }

  /// `self + ((a * b) >> W)`, reading only the low `W` bits of each lane of `a`
  /// and `b`. `W` must be in `1..=64`.
  #[inline]
  #[must_use]
  pub fn add_mul_hi<const W: u32>(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512ifma")] {
        // IFMA is fixed at 52 bits; any other width takes the generic path.
        if W == 52 {
          return Self {
            avx512: add_mul_high_u52_m512i(self.avx512, a.avx512, b.avx512),
          };
        }
      }
    }

    // See `add_mul_lo`: the whole product is in the lane, so the high half is a
    // shift.
    if W <= 32 {
      return self + (Self::mul_masked::<W>(a, b) >> W);
    }

    let acc = self.to_array();
    let a = a.to_array();
    let b = b.to_array();
    Self::new(core::array::from_fn(|i| {
      add_mul_hi_lane_u64::<W>(acc[i], a[i], b[i])
    }))
  }
}
