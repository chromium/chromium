use super::*;

pick! {
  if #[cfg(target_feature="avx2")] {
    /// A SIMD vector with four elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct u64x4 { pub(crate) avx2: m256i }
  } else {
    /// A SIMD vector with four elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct u64x4 { pub(crate) a : u64x2, pub(crate) b : u64x2 }
  }
}

impl_simd_uint! {
  unsafe {
    T = u64,
    N = 4,
    Simd = u64x4,
    IntSimd = i64x4,
    T_BITS = 64,
    T_BITS_MUL_2 = 128,
    [0, 1, 2, 3],
    optional_type_x86_inner { X86Inner = __m256i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn not(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: self.avx2.not()  }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: add_i64_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: sub_i64_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        let arr1: [i64; 4] = cast(self);
        let arr2: [i64; 4] = cast(rhs);
        cast([
          arr1[0].wrapping_mul(arr2[0]),
          arr1[1].wrapping_mul(arr2[1]),
          arr1[2].wrapping_mul(arr2[2]),
          arr1[3].wrapping_mul(arr2[3]),
        ])
      } else {
        Self { a: self.a.mul(rhs.a), b: self.b.mul(rhs.b) }
      }
    }
  }

  #[inline]
  fn bitand(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: bitand_m256i(self.avx2, rhs.avx2) }
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
    if #[cfg(target_feature="avx2")] {
        Self { avx2: bitor_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: bitxor_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: cmp_eq_mask_i64_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        !self.simd_eq(rhs)
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
    // lt is just gt the other way around
    rhs.simd_gt(self)
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // no unsigned gt than so inverting the high bit will get the correct result
        let highbit = u64x4::splat(1 << 63);
        Self { avx2: cmp_gt_mask_i64_m256i((self ^ highbit).avx2, (rhs ^ highbit).avx2) }
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
  pub fn reduce_add(self) -> u64 {
    pick! {
      if #[cfg(all(target_arch="x86_64", target_feature="avx2"))] {
        let zwxx  = shuffle_ai_i64_all_m256i::<0b00_00_11_10>(self.avx2);
        let xz_yw = add_i64_m256i(zwxx, self.avx2);
        let yw_xz  = shuffle_ai_i64_all_m256i::<0b00_00_00_01>(xz_yw);
        let sum = add_i64_m256i(xz_yw, yw_xz);
        extract_i64_from_m256i::<0>(sum).cast_unsigned()
      } else {
        let array: [u64; 4] = cast(self);
        array[0]
          .wrapping_add(array[1])
          .wrapping_add(array[2])
          .wrapping_add(array[3])
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> u64 {
    let array: [u64; 4] = cast(self);
    array[0]
      .wrapping_mul(array[1])
      .wrapping_mul(array[2])
      .wrapping_mul(array[3])
  }

  #[inline]
  pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self {
          avx2: bitor_m256i(
            bitand_m256i(if_one.avx2, self.avx2),
            bitandnot_m256i(self.avx2, if_zero.avx2),
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: blend_varying_i8_m256i(if_false.avx2,if_true.avx2,self.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        // use f64 move_mask since it is the same size as i64
        move_mask_m256d(cast(self.avx2)) as u32
      } else {
        self.a.to_bitmask() | (self.b.to_bitmask() << 2)
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        move_mask_m256d(cast(self.avx2)) != 0
      } else {
        (self.a | self.b).any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        move_mask_m256d(cast(self.avx2)) == 0b1111
      } else {
        (self.a & self.b).all()
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u64x4) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512f", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_permutexvar_epi64;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_permutexvar_epi64;
        // TODO(safe_arch): Add `_mm256_permutexvar_epi64`.
        Self { avx2: m256i(unsafe { _mm256_permutexvar_epi64(indices.avx2.0, self.avx2.0) }) }
      } else if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        let self_bytes = cast::<u64x4, u8x32>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x32, u64x4>(self_bytes.shuffle(byte_indices))
      } else {
        let self_array = self.to_array();
        let indices_array = indices.to_array();

        let mut result = [0; 4];
        for i in 0..4 {
          let index = indices_array[i] as usize;
          if index < 4 {
            result[i] = self_array[index];
          }
        }

        Self::new(result)
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u64x4) -> Self {
    pick! {
      if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        // Even if the `u8x32::shuffle` implementation is zeroing, our 64-bit to
        // 8-bit can trigger an overflow causing incorrect behavior
        self.shuffle(indices) & indices.simd_lt(4)
      } else {
        // The fallback branch of `shuffle` already has the behavior we want
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u64x4) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 3)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u64x4; 2], indices: u64x4) -> u64x4 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        u64x4 { avx2: shuffle_abv_i64_all_m256i(self[0].avx2, indices.avx2, self[1].avx2) }
      } else {
        let self_bytes = cast::<[u64x4; 2], [u8x32; 2]>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x32, u64x4>(self_bytes.shuffle(byte_indices))
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x4; 2], indices: u64x4) -> u64x4 {
    // Even if the `u8x32::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(8)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x4; 2], indices: u64x4) -> u64x4 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 7)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u64x4; 3], indices: u64x4) -> u64x4 {
    let self_bytes = cast::<[u64x4; 3], [u8x32; 3]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x32, u64x4>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x4; 3], indices: u64x4) -> u64x4 {
    // Even if the `u8x32::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(12)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x4; 3], indices: u64x4) -> u64x4 {
    self.shuffle(indices % 12)
  }

  #[inline]
  fn shuffle(self: [u64x4; 4], indices: u64x4) -> u64x4 {
    let self_bytes = cast::<[u64x4; 4], [u8x32; 4]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x32, u64x4>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x4; 4], indices: u64x4) -> u64x4 {
    // Even if the `u8x32::shuffle` implementation is zeroing, our 64-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(16)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x4; 4], indices: u64x4) -> u64x4 {
    self.shuffle(indices & 15)
  }

  ///
  /// Currently this function is only accelerated on `avx2`.
  #[inline]
  pub fn transpose(data: [Self; 4]) -> [Self; 4] {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // Can this be optimized?
        // TODO: Once unpack functions are added, remove these casts
        let a = data[0].cast_signed().unpack_lo(data[2].cast_signed());
        let b = data[1].cast_signed().unpack_lo(data[3].cast_signed());
        let c = data[0].cast_signed().unpack_hi(data[2].cast_signed());
        let d = data[1].cast_signed().unpack_hi(data[3].cast_signed());
        [
          a.unpack_lo(b).cast_unsigned(),
          a.unpack_hi(b).cast_unsigned(),
          c.unpack_lo(d).cast_unsigned(),
          c.unpack_hi(d).cast_unsigned(),
        ]
      } else {
        #[inline(always)]
        fn transpose_column(data: &[u64x4; 4], index: usize) -> u64x4 {
          u64x4::new([
            data[0].as_array()[index],
            data[1].as_array()[index],
            data[2].as_array()[index],
            data[3].as_array()[index],
          ])
        }

        [
          transpose_column(&data, 0),
          transpose_column(&data, 1),
          transpose_column(&data, 2),
          transpose_column(&data, 3),
        ]
      }
    }
  }

  #[inline]
  fn shl(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // mask the shift count to 63 to have same behavior on all platforms
        let shift_by = rhs & Self::splat(63);
        Self { avx2: shl_each_u64_m256i(self.avx2, shift_by.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 63, 0]);
        Self { avx2: shl_all_u64_m256i(self.avx2, shift) }
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
      if #[cfg(target_feature="avx2")] {
        // mask the shift count to 63 to have same behavior on all platforms
        let shift_by = rhs & Self::splat(63);
        Self { avx2: shr_each_u64_m256i(self.avx2, shift_by.avx2) }
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
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 63, 0]);
        Self { avx2: shr_all_u64_m256i(self.avx2, shift) }
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
    self.simd_gt(rhs).select(self, rhs)
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    self.simd_lt(rhs).select(self, rhs)
  }

  #[inline]
  pub fn reduce_max(self) -> u64 {
    let array: [u64; 4] = cast(self);
    array[0].max(array[1]).max(array[2]).max(array[3])
  }

  #[inline]
  pub fn reduce_min(self) -> u64 {
    let array: [u64; 4] = cast(self);
    array[0].min(array[1]).min(array[2]).min(array[3])
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shl_each_u64_m256i(self.avx2, rhs.avx2) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shl_all_u64_m256i(self.avx2, cast([rhs as u64, 0])) }
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
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shr_each_u64_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: shr_all_u64_m256i(self.avx2, cast([rhs as u64, 0])) }
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
      if #[cfg(target_feature="avx2")] {
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
    ];
    (
      Self::new([result[0].0, result[1].0, result[2].0, result[3].0]),
      Self::new([
        -(result[0].1 as i64) as u64,
        -(result[1].1 as i64) as u64,
        -(result[2].1 as i64) as u64,
        -(result[3].1 as i64) as u64,
      ]),
    )
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `u128x4` type.
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
    ];

    (
      Self::new([
        widening_mul[0] as u64,
        widening_mul[1] as u64,
        widening_mul[2] as u64,
        widening_mul[3] as u64,
      ]),
      Self::new([
        (widening_mul[0] >> 64) as u64,
        (widening_mul[1] >> 64) as u64,
        (widening_mul[2] >> 64) as u64,
        (widening_mul[3] >> 64) as u64,
      ]),
    )
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let arr1: [u64; 4] = cast(self);
        let arr2: [u64; 4] = cast(rhs);
        cast([
          (arr1[0] as u128 * arr2[0] as u128 >> 64) as u64,
          (arr1[1] as u128 * arr2[1] as u128 >> 64) as u64,
          (arr1[2] as u128 * arr2[2] as u128 >> 64) as u64,
          (arr1[3] as u128 * arr2[3] as u128 >> 64) as u64,
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

/// The following functionality exists only for [`u64x4`], or only for
/// particular types inconsistently.
impl u64x4 {
  /// A helper for shuffle functions that turns indices of 64-bit lanes into
  /// byte indices that can be used with 8-bit shuffle intrinsics.
  ///
  /// This turns each 64-bit lane `i` into eight 8-bit lanes
  /// `[8*i, 8*i + 1, 8*i + 2, 8*i + 3, ...]`.
  ///
  /// This assumes `self` has already been reduced to the table's lane count,
  /// which may be at most 32 lanes so that `8 * i` still fits in a byte.
  #[allow(dead_code)]
  #[inline]
  fn to_byte_indices(self) -> u8x32 {
    // The byte offset of the lane, broadcast to every byte of the lane.
    let base = self.unbounded_shl_scalar(3);
    let base = base | base.unbounded_shl_scalar(8);
    let base = base | base.unbounded_shl_scalar(16);
    let base = base | base.unbounded_shl_scalar(32);

    // Then the offset of each byte within its lane. These bits are free because
    // every byte of `base` is a multiple of eight. `from_ne_bytes` keeps this
    // correct on big endian, where the bytes of a lane are the other way around.
    const WITHIN_LANE: u64x4 =
      u64x4::splat(u64::from_ne_bytes([0, 1, 2, 3, 4, 5, 6, 7]));

    cast::<u64x4, u8x32>(base | WITHIN_LANE)
  }

  /// Returns `[self[0], b[0], self[1], b[1]]`, interleaving the low half of
  /// each vector.
  #[inline]
  #[must_use]
  pub fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // `unpack_low_i64_m256i` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [aa, _]: [u64x2; 2] = cast(self);
        let [ba, _]: [u64x2; 2] = cast(b);
        cast([aa.unpack_lo(ba), aa.unpack_hi(ba)])
      } else {
        Self { a: self.a.unpack_lo(b.a), b: self.a.unpack_hi(b.a) }
      }
    }
  }

  /// Returns `[self[2], b[2], self[3], b[3]]`, interleaving the high half of
  /// each vector.
  #[inline]
  #[must_use]
  pub fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // `unpack_high_i64_m256i` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [_, ab]: [u64x2; 2] = cast(self);
        let [_, bb]: [u64x2; 2] = cast(b);
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
  pub(crate) fn mul_masked<const W: u32>(a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // `vpmuludq` reads the low 32 bits of a lane anyway, so at `W == 32` the
        // operand masks are already implied and can be dropped. A scalar
        // multiply has no such truncation, so the other arm always masks.
        let (a, b) = if W == 32 {
          (a, b)
        } else {
          let mask = Self::splat(add_mul_operand_mask_u64::<W>());
          (a & mask, b & mask)
        };

        Self { avx2: mul_u64_low_bits_m256i(a.avx2, b.avx2) }
      } else {
        let mask = add_mul_operand_mask_u64::<W>();
        let a = a.to_array();
        let b = b.to_array();
        Self::new(core::array::from_fn(|i| (a[i] & mask) * (b[i] & mask)))
      }
    }
  }

  /// `self + ((a * b) mod 2^W)`, reading only the low `W` bits of each lane of
  /// `a` and `b`. `W` must be in `1..=64`.
  #[inline]
  #[must_use]
  pub fn add_mul_lo<const W: u32>(self, a: Self, b: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512ifma", target_feature="avx512vl"))] {
        // IFMA is fixed at 52 bits; any other width takes the generic path.
        if W == 52 {
          return Self {
            avx2: add_mul_low_u52_m256i(self.avx2, a.avx2, b.avx2),
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
      if #[cfg(all(target_feature="avx512ifma", target_feature="avx512vl"))] {
        // IFMA is fixed at 52 bits; any other width takes the generic path.
        if W == 52 {
          return Self {
            avx2: add_mul_high_u52_m256i(self.avx2, a.avx2, b.avx2),
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
