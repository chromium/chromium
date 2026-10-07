use super::*;

pick! {
  if #[cfg(target_feature="avx2")] {
    /// A SIMD vector with eight elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct u32x8 { pub(crate) avx2: m256i }
  } else {
    /// A SIMD vector with eight elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(32))]
    pub struct u32x8 { pub(crate) a : u32x4, pub(crate) b : u32x4 }
  }
}

impl_simd_uint! {
  unsafe {
    T = u32,
    N = 8,
    Simd = u32x8,
    IntSimd = i32x8,
    T_BITS = 32,
    T_BITS_MUL_2 = 64,
    [0, 1, 2, 3, 4, 5, 6, 7],
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
        Self { avx2: add_i32_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: sub_i32_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: mul_i32_keep_low_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: cmp_eq_mask_i32_m256i(self.avx2, rhs.avx2 ) }
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
        let highbit = u32x8::splat(1 << 31);
        Self { avx2: cmp_gt_mask_i32_m256i((self ^ highbit).avx2, (rhs ^ highbit).avx2 ) }
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
    self.simd_eq(rhs) | self.simd_lt(rhs)
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    self.simd_eq(rhs) | self.simd_gt(rhs)
  }

  #[inline]
  pub fn reduce_add(self) -> u32 {
    let array: [u32x4; 2] = cast(self);
    (array[0] + array[1]).reduce_add()
  }

  #[inline]
  pub fn reduce_mul(self) -> u32 {
    let array: [u32x4; 2] = cast(self);
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
      if #[cfg(target_feature="avx2")] {
        // use f32 move_mask since it is the same size as i32
        move_mask_m256(cast(self.avx2)) as u32
      } else {
        self.a.to_bitmask() | (self.b.to_bitmask() << 4)
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        move_mask_m256(cast(self.avx2)) != 0
      } else {
        (self.a | self.b).any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        move_mask_m256(cast(self.avx2)) == 0b11111111
      } else {
        (self.a & self.b).all()
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u32x8) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm256_permutexvar_epi32;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm256_permutexvar_epi32;
        // TODO(safe_arch): Add `_mm256_permutexvar_epi32`
        Self { avx2: unsafe { m256i(_mm256_permutexvar_epi32(indices.avx2.0, self.avx2.0)) } }
      } else if #[cfg(target_feature = "avx2")] {
        Self { avx2: shuffle_av_i32_all_m256i(self.avx2, indices.avx2) }
      } else if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        let self_bytes = cast::<u32x8, u8x32>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x32, u32x8>(self_bytes.shuffle(byte_indices))
      } else {
        let self_array = self.to_array();
        let indices_array = indices.to_array();

        let mut result = [0; 8];
        for i in 0..8 {
          let index = indices_array[i] as usize;
          if index < 8 {
            result[i] = self_array[index];
          }
        }

        Self::new(result)
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u32x8) -> Self {
    pick! {
      if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        // Even if the `u8x32::shuffle` implementation is zeroing, our 32-bit to
        // 8-bit can trigger an overflow causing incorrect behavior
        self.shuffle(indices) & indices.simd_lt(8)
      } else {
        // The fallback branch of `shuffle` already has the behavior we want
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u32x8) -> Self {
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
  fn shuffle(self: [u32x8; 2], indices: u32x8) -> u32x8 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        u32x8 { avx2: shuffle_abv_i32_all_m256i(self[0].avx2, indices.avx2, self[1].avx2) }
      } else {
        let self_bytes = cast::<[u32x8; 2], [u8x32; 2]>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x32, u32x8>(self_bytes.shuffle(byte_indices))
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x8; 2], indices: u32x8) -> u32x8 {
    // Even if the `u8x32::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(16)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x8; 2], indices: u32x8) -> u32x8 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 15)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u32x8; 3], indices: u32x8) -> u32x8 {
    let self_bytes = cast::<[u32x8; 3], [u8x32; 3]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x32, u32x8>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x8; 3], indices: u32x8) -> u32x8 {
    // Even if the `u8x32::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(24)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x8; 3], indices: u32x8) -> u32x8 {
    self.shuffle(indices % 24)
  }

  #[inline]
  fn shuffle(self: [u32x8; 4], indices: u32x8) -> u32x8 {
    let self_bytes = cast::<[u32x8; 4], [u8x32; 4]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x32, u32x8>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x8; 4], indices: u32x8) -> u32x8 {
    // Even if the `u8x32::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(32)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x8; 4], indices: u32x8) -> u32x8 {
    self.shuffle(indices & 31)
  }

  ///
  /// Currently this function is only accelerated on `avx2`.
  #[inline]
  pub fn transpose(data: [Self; 8]) -> [Self; 8] {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let a0 = unpack_low_i32_m256i(data[0].avx2, data[1].avx2);
        let a1 = unpack_high_i32_m256i(data[0].avx2, data[1].avx2);
        let a2 = unpack_low_i32_m256i(data[2].avx2, data[3].avx2);
        let a3 = unpack_high_i32_m256i(data[2].avx2, data[3].avx2);
        let a4 = unpack_low_i32_m256i(data[4].avx2, data[5].avx2);
        let a5 = unpack_high_i32_m256i(data[4].avx2, data[5].avx2);
        let a6 = unpack_low_i32_m256i(data[6].avx2, data[7].avx2);
        let a7 = unpack_high_i32_m256i(data[6].avx2, data[7].avx2);

        pub const fn mm_shuffle(z: i32, y: i32, x: i32, w: i32) -> i32 {
          (z << 6) | (y << 4) | (x << 2) | w
        }

        const SHUFF_LO : i32 = mm_shuffle(1,0,1,0);
        const SHUFF_HI : i32 = mm_shuffle(3,2,3,2);

        // possible todo: intel performance manual suggests alternative with blend to avoid port 5 pressure
        // (since blend runs on a different port than shuffle)
        let b0 = cast::<m256,m256i>(shuffle_m256::<SHUFF_LO>(cast(a0),cast(a2)));
        let b1 = cast::<m256,m256i>(shuffle_m256::<SHUFF_HI>(cast(a0),cast(a2)));
        let b2 = cast::<m256,m256i>(shuffle_m256::<SHUFF_LO>(cast(a1),cast(a3)));
        let b3 = cast::<m256,m256i>(shuffle_m256::<SHUFF_HI>(cast(a1),cast(a3)));
        let b4 = cast::<m256,m256i>(shuffle_m256::<SHUFF_LO>(cast(a4),cast(a6)));
        let b5 = cast::<m256,m256i>(shuffle_m256::<SHUFF_HI>(cast(a4),cast(a6)));
        let b6 = cast::<m256,m256i>(shuffle_m256::<SHUFF_LO>(cast(a5),cast(a7)));
        let b7 = cast::<m256,m256i>(shuffle_m256::<SHUFF_HI>(cast(a5),cast(a7)));

        [
          u32x8 { avx2: permute2z_m256i::<0x20>(b0, b4) },
          u32x8 { avx2: permute2z_m256i::<0x20>(b1, b5) },
          u32x8 { avx2: permute2z_m256i::<0x20>(b2, b6) },
          u32x8 { avx2: permute2z_m256i::<0x20>(b3, b7) },
          u32x8 { avx2: permute2z_m256i::<0x31>(b0, b4) },
          u32x8 { avx2: permute2z_m256i::<0x31>(b1, b5) },
          u32x8 { avx2: permute2z_m256i::<0x31>(b2, b6) },
          u32x8 { avx2: permute2z_m256i::<0x31>(b3, b7) }
        ]
      } else {
        // possible todo: not sure that 128bit SIMD gives us a a lot of speedup here

        #[inline(always)]
        fn transpose_column(data: &[u32x8; 8], index: usize) -> u32x8 {
          u32x8::new([
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
  fn shl(self, rhs: u32x8) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // ensure same behavior as scalar wrapping_shl
        let shift_by = bitand_m256i(rhs.avx2, set_splat_i32_m256i(31));
        Self { avx2: shl_each_u32_m256i(self.avx2, shift_by) }
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
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 31, 0]);
        Self { avx2: shl_all_u32_m256i(self.avx2, shift) }
      } else {
        Self {
          a : self.a.shl(rhs),
          b : self.b.shl(rhs),
        }
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32x8) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // ensure same behavior as scalar wrapping_shr
        let shift_by = bitand_m256i(rhs.avx2, set_splat_i32_m256i(31));
        Self { avx2: shr_each_u32_m256i(self.avx2, shift_by ) }
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
        Self { avx2: shr_all_u32_m256i(self.avx2, shift) }
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
        Self { avx2: max_u32_m256i(self.avx2, rhs.avx2 ) }
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
        Self { avx2: min_u32_m256i(self.avx2, rhs.avx2 ) }
      } else {
        Self {
          a : self.a.min(rhs.a),
          b : self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u32 {
    let array: [u32x4; 2] = cast(self);
    array[0].max(array[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> u32 {
    let array: [u32x4; 2] = cast(self);
    array[0].min(array[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2: shl_each_u32_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: shl_all_u32_m256i(self.avx2, cast([rhs as u64, 0])) }
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
        Self { avx2: shr_each_u32_m256i(self.avx2, rhs.avx2) }
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
        Self { avx2: shr_all_u32_m256i(self.avx2, cast([rhs as u64, 0])) }
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
    let (low, high) = self.mul_keep_low_high(rhs);
    let overflow = high.simd_ne(Self::ZERO);
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> u64x8 {
      pick! {
        if #[cfg(all(target_feature="avx512f", target_feature="avx2"))] {
          const SHUFFLE_INDICES: m512i = i64x8::new([0, 4, 1, 5, 2, 6, 3, 7]).avx512;

          let even_wide_mul = mul_u64_low_bits_m256i(self.avx2, rhs.avx2);
          let odd_wide_mul = mul_u64_low_bits_m256i(
            shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(self.avx2),
            shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(rhs.avx2),
          );
          let even_then_odd = cast::<[m256i; 2], m512i>([even_wide_mul, odd_wide_mul]);
          u64x8 {
            avx512: permute_i64_m512i(SHUFFLE_INDICES, even_then_odd),
          }
        } else {
          let [self_a, self_b] = cast::<u32x8, [u32x4; 2]>(self);
          let [rhs_a, rhs_b] = cast::<u32x8, [u32x4; 2]>(rhs);

          cast([self_a.widening_mul(rhs_a), self_b.widening_mul(rhs_b)])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let even_wide_mul = mul_u64_low_bits_m256i(self.avx2, rhs.avx2);
        let odd_wide_mul = mul_u64_low_bits_m256i(
          shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(self.avx2),
          shuffle_ai_i32_half_m256i::<0b_00_11_00_01>(rhs.avx2),
        );
        let ll_hh_1 = unpack_low_i32_m256i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m256i(even_wide_mul, odd_wide_mul);
        (
          Self { avx2: unpack_low_i64_m256i(ll_hh_1, ll_hh_2) },
          Self { avx2: unpack_high_i64_m256i(ll_hh_1, ll_hh_2) },
        )
      } else {
        let [self_a, self_b] = cast::<u32x8, [u32x4; 2]>(self);
        let [rhs_a, rhs_b] = cast::<u32x8, [u32x4; 2]>(rhs);

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
  pub fn mul_keep_high(self, rhs: u32x8) -> u32x8 {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let a : [u32;8]= cast(self);
        let b : [u32;8]= cast(rhs);

        // let the compiler shuffle the values around, it does the right thing
        let r1 : [u32;8] = cast(mul_u64_low_bits_m256i(cast([a[0], 0, a[1], 0, a[2], 0, a[3], 0]), cast([b[0], 0, b[1], 0, b[2], 0, b[3], 0])));
        let r2 : [u32;8] = cast(mul_u64_low_bits_m256i(cast([a[4], 0, a[5], 0, a[6], 0, a[7], 0]), cast([b[4], 0, b[5], 0, b[6], 0, b[7], 0])));

        cast([r1[1], r1[3], r1[5], r1[7], r2[1], r2[3], r2[5], r2[7]])
      } else {
        Self {
          a : self.a.mul_keep_high(rhs.a),
          b : self.b.mul_keep_high(rhs.b),
        }
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`u32x8`], or only for
/// particular types inconsistently.
impl u32x8 {
  /// A helper for shuffle functions that turns indices of 32-bit lanes into
  /// byte indices that can be used with 8-bit shuffle intrinsics.
  ///
  /// This turns each 32-bit lane `i` into four 8-bit lanes
  /// `[4*i, 4*i + 1, 4*i + 2, 4*i + 3]`.
  ///
  /// This assumes `self` has already been reduced to the table's lane count,
  /// which may be at most 64 lanes so that `4 * i` still fits in a byte.
  #[allow(dead_code)]
  #[inline]
  fn to_byte_indices(self) -> u8x32 {
    // The byte offset of the lane, broadcast to every byte of the lane.
    let base = self.unbounded_shl_scalar(2);
    let base = base | base.unbounded_shl_scalar(8);
    let base = base | base.unbounded_shl_scalar(16);

    // Then the offset of each byte within its lane. These bits are free because
    // every byte of `base` is a multiple of four. `from_ne_bytes` keeps this
    // correct on big endian, where the bytes of a lane are the other way around.
    const WITHIN_LANE: u32x8 = u32x8::splat(u32::from_ne_bytes([0, 1, 2, 3]));

    cast::<u32x8, u8x32>(base | WITHIN_LANE)
  }
}

impl From<u16x8> for u32x8 {
  /// widens and zero extends to u32x8
  #[inline]
  fn from(v: u16x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { avx2:convert_to_i32_m256i_from_u16_m128i(v.sse) }
      } else if #[cfg(target_feature="sse2")] {
        Self {
          a: u32x4 { sse: shr_imm_u32_m128i::<16>( unpack_low_i16_m128i(v.sse, v.sse)) },
          b: u32x4 { sse: shr_imm_u32_m128i::<16>( unpack_high_i16_m128i(v.sse, v.sse)) },
        }
      } else {
        u32x8::new([
          u32::from(v.as_array()[0]),
          u32::from(v.as_array()[1]),
          u32::from(v.as_array()[2]),
          u32::from(v.as_array()[3]),
          u32::from(v.as_array()[4]),
          u32::from(v.as_array()[5]),
          u32::from(v.as_array()[6]),
          u32::from(v.as_array()[7]),
        ])
      }
    }
  }
}

/// The following functionality exists only for [`u32x8`], or only for
/// particular types inconsistently.
impl u32x8 {
  /// Returns `[self[0], b[0], self[1], b[1], self[2], b[2], self[3], b[3]]`,
  /// interleaving the low half of each vector.
  #[inline]
  #[must_use]
  pub fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // `unpack_low_i32_m256i` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [aa, _]: [u32x4; 2] = cast(self);
        let [ba, _]: [u32x4; 2] = cast(b);
        cast([aa.unpack_lo(ba), aa.unpack_hi(ba)])
      } else {
        Self { a: self.a.unpack_lo(b.a), b: self.a.unpack_hi(b.a) }
      }
    }
  }

  /// Returns `[self[4], b[4], self[5], b[5], self[6], b[6], self[7], b[7]]`,
  /// interleaving the high half of each vector.
  #[inline]
  #[must_use]
  pub fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // `unpack_high_i32_m256i` cannot be used because it acts within each
        // 128-bit lane, which is a different operation.
        let [_, ab]: [u32x4; 2] = cast(self);
        let [_, bb]: [u32x4; 2] = cast(b);
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
