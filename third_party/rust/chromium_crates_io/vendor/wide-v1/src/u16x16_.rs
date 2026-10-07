use super::*;

pick! {
  if #[cfg(target_feature="avx2")] {
    /// A SIMD vector with 16 elements of type [`u16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct u16x16 { pub(crate) avx2: m256i }
  } else {
    /// A SIMD vector with 16 elements of type [`u16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct u16x16 { pub(crate) a : u16x8, pub(crate) b : u16x8 }
  }
}

impl_simd_uint! {
  unsafe {
    T = u16,
    N = 16,
    Simd = u16x16,
    IntSimd = i16x16,
    T_BITS = 16,
    T_BITS_MUL_2 = 32,
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
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
        Self { avx2: add_i16_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: sub_i16_m256i(self.avx2, rhs.avx2) }
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
        // non-widening multiplication is the same for unsigned and signed
        Self { avx2: mul_i16_keep_low_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: cmp_eq_mask_i16_m256i(self.avx2, rhs.avx2) }
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
    // no gt, so just reverse to get same answer
    Self::simd_gt(rhs, self)
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature = "avx2")] {
        let bias = m256i::from([0x8000u16; 16]);
        let a_biased = sub_i16_m256i(self.avx2, bias);
        let b_biased = sub_i16_m256i(rhs.avx2, bias);
        let mask = cmp_gt_mask_i16_m256i(a_biased, b_biased);

        Self { avx2: mask }
      } else {
        Self {
          a: self.a.simd_gt(rhs.a),
          b: self.b.simd_gt(rhs.b),
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
  pub fn reduce_add(self) -> u16 {
    let array: [u16x8; 2] = cast(self);
    (array[0] + array[1]).reduce_add()
  }

  #[inline]
  pub fn reduce_mul(self) -> u16 {
    let array: [u16x8; 2] = cast(self);
    (array[0] * array[1]).reduce_mul()
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
        Self { avx2: blend_varying_i8_m256i(if_false.avx2, if_true.avx2, self.avx2) }
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
      if #[cfg(target_feature="sse2")] {
          let [a,b] = cast::<_,[m128i;2]>(self);
          move_mask_i8_m128i( pack_i16_to_i8_m128i(a,b)) as u32
        } else {
        self.a.to_bitmask() | (self.b.to_bitmask() << 8)
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        ((move_mask_i8_m256i(self.avx2) as u32) & 0b10101010101010101010101010101010) != 0
      } else {
        (self.a | self.b).any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        ((move_mask_i8_m256i(self.avx2) as u32) & 0b10101010101010101010101010101010) == 0b10101010101010101010101010101010
      } else {
        (self.a & self.b).all()
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u16x16) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512bw", target_feature = "avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_permutexvar_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_permutexvar_epi16;
        // TODO(safe_arch): Add `_mm256_permutexvar_epi16`
        Self { avx2: unsafe { m256i(_mm256_permutexvar_epi16(indices.avx2.0, self.avx2.0)) } }
      } else if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        let self_bytes = cast::<u16x16, u8x32>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x32, u16x16>(self_bytes.shuffle(byte_indices))
      } else {
        let self_array = self.to_array();
        let indices_array = indices.to_array();

        let mut result = [0; 16];
        for i in 0..16 {
          let index = indices_array[i] as usize;
          if index < 16 {
            result[i] = self_array[index];
          }
        }

        Self::new(result)
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u16x16) -> Self {
    pick! {
      if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        // Even if the `u8x32::shuffle` implementation is zeroing, our 16-bit to
        // 8-bit can trigger an overflow causing incorrect behavior
        self.shuffle(indices) & indices.simd_lt(16)
      } else {
        // The fallback branch of `shuffle` already has the behavior we want
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u16x16) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512bw", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 15)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u16x16; 2], indices: u16x16) -> u16x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512bw", target_feature = "avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_permutex2var_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_permutex2var_epi16;
        // TODO(safe_arch): add `_mm256_permutex2var_epi16`.
        u16x16 {
          avx2: unsafe {
            m256i(_mm256_permutex2var_epi16(self[0].avx2.0, indices.avx2.0, self[1].avx2.0))
          },
        }
      } else {
        let self_bytes = cast::<[u16x16; 2], [u8x32; 2]>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x32, u16x16>(self_bytes.shuffle(byte_indices))
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u16x16; 2], indices: u16x16) -> u16x16 {
    // Even if the `u8x32` shuffle is zeroing, our 16-bit to 8-bit index
    // conversion breaks for out of bounds indices.
    self.shuffle(indices) & indices.simd_lt(32)
  }

  #[inline]
  fn shuffle_wrapping(self: [u16x16; 2], indices: u16x16) -> u16x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512bw", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 31)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u16x16; 3], indices: u16x16) -> u16x16 {
    let self_bytes = cast::<[u16x16; 3], [u8x32; 3]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x32, u16x16>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u16x16; 3], indices: u16x16) -> u16x16 {
    // Even if the `u8x32` shuffle is zeroing, our 16-bit to 8-bit index
    // conversion breaks for out of bounds indices.
    self.shuffle(indices) & indices.simd_lt(48)
  }

  #[inline]
  fn shuffle_wrapping(self: [u16x16; 3], indices: u16x16) -> u16x16 {
    self.shuffle(indices % 48)
  }

  #[inline]
  fn shuffle(self: [u16x16; 4], indices: u16x16) -> u16x16 {
    let self_bytes = cast::<[u16x16; 4], [u8x32; 4]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x32, u16x16>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u16x16; 4], indices: u16x16) -> u16x16 {
    // Even if the `u8x32` shuffle is zeroing, our 16-bit to 8-bit index
    // conversion breaks for out of bounds indices.
    self.shuffle(indices) & indices.simd_lt(64)
  }

  #[inline]
  fn shuffle_wrapping(self: [u16x16; 4], indices: u16x16) -> u16x16 {
    self.shuffle(indices & 63)
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [Self; 16]) -> [Self; 16] {
    // Can this be optimized?

    #[inline(always)]
    fn transpose_column(data: &[u16x16; 16], index: usize) -> u16x16 {
      u16x16::new([
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
  fn shl(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(all(target_feature="avx512bw", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_sllv_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_sllv_epi16;

        // Mask `rhs` to 15 to match `wrapping_shl`.
        let rhs = bitand_m256i(rhs.avx2, set_splat_i16_m256i(15));
        // TODO(safe_arch): Add `_mm256_sllv_epi16`.
        cast(unsafe { _mm256_sllv_epi16(self.avx2.0, rhs.0) })
      } else {
        let [self_a, self_b]: [u16x8; 2] = cast(self);
        let [rhs_a, rhs_b]: [u16x8; 2] = cast(rhs);

        cast([self_a << rhs_a, self_b << rhs_b])
      }
    }
  }

  #[inline]
  fn shl(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // Use `rhs % 16` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 15, 0]);
        Self { avx2: shl_all_u16_m256i(self.avx2, shift) }
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
      if #[cfg(all(target_feature="avx512bw", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_srlv_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_srlv_epi16;

        // Mask `rhs` to 15 to match `wrapping_shr`.
        let rhs = bitand_m256i(rhs.avx2, set_splat_i16_m256i(15));
        // TODO(safe_arch): Add `_mm256_srlv_epi16`.
        cast(unsafe { _mm256_srlv_epi16(self.avx2.0, rhs.0) })
      } else {
        let [self_a, self_b]: [u16x8; 2] = cast(self);
        let [rhs_a, rhs_b]: [u16x8; 2] = cast(rhs);

        cast([self_a >> rhs_a, self_b >> rhs_b])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // Use `rhs % 16` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 15, 0]);
        Self { avx2: shr_all_u16_m256i(self.avx2, shift) }
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
        Self { avx2: max_u16_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: min_u16_m256i(self.avx2, rhs.avx2) }
      } else {
        Self {
          a : self.a.min(rhs.a),
          b : self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u16 {
    let array: [u16x8; 2] = cast(self);
    array[0].max(array[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> u16 {
    let array: [u16x8; 2] = cast(self);
    array[0].min(array[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512bw", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_sllv_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_sllv_epi16;

        // TODO(safe_arch): Add `_mm256_sllv_epi16`.
        cast(unsafe { _mm256_sllv_epi16(self.avx2.0, rhs.avx2.0) })
      } else {
        let [self_a, self_b] = cast::<u16x16, [u16x8; 2]>(self);
        let [rhs_a, rhs_b] = cast::<u16x16, [u16x8; 2]>(rhs);

        cast([self_a.unbounded_shl(rhs_a), self_b.unbounded_shl(rhs_b)])
      }
    }
  }

  #[inline]
  pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shl_all_u16_m256i(self.avx2, cast([rhs as u64, 0])) }
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
      if #[cfg(all(target_feature="avx512bw", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_srlv_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_srlv_epi16;

        // TODO(safe_arch): Add `_mm256_srlv_epi16`.
        cast(unsafe { _mm256_srlv_epi16(self.avx2.0, rhs.avx2.0) })
      } else {
        let [self_a, self_b] = cast::<u16x16, [u16x8; 2]>(self);
        let [rhs_a, rhs_b] = cast::<u16x16, [u16x8; 2]>(rhs);

        cast([self_a.unbounded_shr(rhs_a), self_b.unbounded_shr(rhs_b)])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shr_all_u16_m256i(self.avx2, cast([rhs as u64, 0])) }
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
        Self { avx2: add_saturating_u16_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: sub_saturating_u16_m256i(self.avx2, rhs.avx2) }
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
    let overflow = high.simd_ne(Self::ZERO);
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> u32x16 {
      // x86 has no `_mm256_mul_epu16` intrinsic so there is no `avx2`
      // optimization.

      let [self_a, self_b] = cast::<u16x16, [u16x8; 2]>(self);
      let [rhs_a, rhs_b] = cast::<u16x16, [u16x8; 2]>(rhs);

      cast([self_a.widening_mul(rhs_a), self_b.widening_mul(rhs_b)])
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    // x86 has no `_mm256_mul_epu16` intrinsic so there is no `avx2`
    // optimization.

    let [self_a, self_b] = cast::<u16x16, [u16x8; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u16x16, [u16x8; 2]>(rhs);

    let result_a = self_a.mul_keep_low_high(rhs_a);
    let result_b = self_b.mul_keep_low_high(rhs_b);
    (cast([result_a.0, result_b.0]), cast([result_a.1, result_b.1]))
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    // x86 has no `_mm256_mul_epu16` intrinsic so there is no `avx2`
    // optimization.

    let [self_a, self_b] = cast::<u16x16, [u16x8; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u16x16, [u16x8; 2]>(rhs);

    cast([self_a.mul_keep_high(rhs_a), self_b.mul_keep_high(rhs_b)])
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`u16x16`], or only for
/// particular types inconsistently.
impl u16x16 {
  /// A helper for shuffle functions that turns indices of 16-bit lanes into
  /// byte indices that can be used with 8-bit shuffle intrinsics.
  ///
  /// This turns each 16-bit lane `i` into two 8-bit lanes `[2*i, 2*i + 1]`.
  ///
  /// This assumes `self` has already been reduced to the table's lane count,
  /// which may be at most 128 lanes so that `2 * i` still fits in a byte.
  #[allow(dead_code)]
  #[inline]
  fn to_byte_indices(self) -> u8x32 {
    // The byte offset of the lane, broadcast to every byte of the lane.
    let base = self.unbounded_shl_scalar(1);
    let base = base | base.unbounded_shl_scalar(8);

    // Then the offset of each byte within its lane. These bits are free because
    // every byte of `base` is a multiple of two. `from_ne_bytes` keeps this
    // correct on big endian, where the bytes of a lane are the other way around.
    const WITHIN_LANE: u16x16 = u16x16::splat(u16::from_ne_bytes([0, 1]));

    cast::<u16x16, u8x32>(base | WITHIN_LANE)
  }
}

impl From<u8x16> for u16x16 {
  /// widens and sign extends to u16x16
  #[inline]
  fn from(v: u8x16) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        u16x16 { avx2:convert_to_i16_m256i_from_u8_m128i(v.sse) }
      } else if #[cfg(target_feature="sse2")] {
        u16x16 {
          a: u16x8 { sse: shr_imm_u16_m128i::<8>( unpack_low_i8_m128i(v.sse, v.sse)) },
          b: u16x8 { sse: shr_imm_u16_m128i::<8>( unpack_high_i8_m128i(v.sse, v.sse)) },
        }
      } else {

        u16x16::new([
          v.as_array()[0] as u16,
          v.as_array()[1] as u16,
          v.as_array()[2] as u16,
          v.as_array()[3] as u16,
          v.as_array()[4] as u16,
          v.as_array()[5] as u16,
          v.as_array()[6] as u16,
          v.as_array()[7] as u16,
          v.as_array()[8] as u16,
          v.as_array()[9] as u16,
          v.as_array()[10] as u16,
          v.as_array()[11] as u16,
          v.as_array()[12] as u16,
          v.as_array()[13] as u16,
          v.as_array()[14] as u16,
          v.as_array()[15] as u16,
          ])
      }
    }
  }
}
