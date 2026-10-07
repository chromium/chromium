use super::*;

pick! {
  if #[cfg(target_feature="avx512bw")] {
    /// A SIMD vector with 64 elements of type [`u8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct u8x64 { pub(crate) avx512: m512i }
  } else {
    /// A SIMD vector with 64 elements of type [`u8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(64))]
    pub struct u8x64 { pub(crate) a : u8x32, pub(crate) b : u8x32 }
  }
}

impl_simd_uint! {
  unsafe {
    T = u8,
    N = 64,
    Simd = u8x64,
    IntSimd = i8x64,
    T_BITS = 8,
    T_BITS_MUL_2 = 16,
    [
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
    ],
    optional_type_x86_inner { X86Inner = __m512i },
    optional_type_arm_inner {},
    optional_type_wasm_inner {},
  }

  #[inline]
  fn not(self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: bitxor_m512i(self.avx512, set_splat_i8_m512i(-1)) }
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: add_i8_m512i(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: sub_i8_m512i(self.avx512, rhs.avx512) }
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
    // There is no `_mm512_mullo_epi8` intrinsic, so there is no `avx512bw`
    // optimization. This technically can be done explicitly by converting to
    // `i16` then converting back after multiplication, but that may not
    // actually be faster than auto-vectorization.
    let [self_a, self_b]: [u8x32; 2] = cast(self);
    let [rhs_a, rhs_b]: [u8x32; 2] = cast(rhs);
    cast([self_a * rhs_a, self_b * rhs_b])
  }

  #[inline]
  fn bitand(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
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
      if #[cfg(target_feature="avx512bw")] {
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
      if #[cfg(target_feature="avx512bw")] {
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_u8_m512i::<{cmp_int_op!(Eq)}>(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_u8_m512i::<{cmp_int_op!(Ne)}>(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_u8_m512i::<{cmp_int_op!(Lt)}>(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.simd_lt(rhs.a), b: self.b.simd_lt(rhs.b) }
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_u8_m512i::<{cmp_int_op!(Nle)}>(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.simd_gt(rhs.a), b: self.b.simd_gt(rhs.b) }
      }
    }
  }

  #[inline]
  fn simd_le(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_u8_m512i::<{cmp_int_op!(Le)}>(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.simd_le(rhs.a), b: self.b.simd_le(rhs.b) }
      }
    }
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: cmp_op_mask_u8_m512i::<{cmp_int_op!(Nlt)}>(self.avx512, rhs.avx512) }
      } else {
        Self { a: self.a.simd_ge(rhs.a), b: self.b.simd_ge(rhs.b) }
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> u8 {
    // `_mm512_reduce_add_epi8` does not exist.
    let array: [u8x32; 2] = cast(self);
    (array[0] + array[1]).reduce_add()
  }

  #[inline]
  pub fn reduce_mul(self) -> u8 {
    // `_mm512_reduce_mul_epi8` does not exist.
    let array: [u8x32; 2] = cast(self);
    (array[0] * array[1]).reduce_mul()
  }

  #[inline]
  pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: blend_varying_i8_m512i(if_false.avx512, if_true.avx512, movepi8_mask_m512i(self.avx512)) }
      } else {
        Self {
          a : self.a.select(if_true.a, if_false.a),
          b : self.b.select(if_true.b, if_false.b),
        }
      }
    }
  }

  #[inline]
  pub fn to_bitmask(self) -> u64 {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        movepi8_mask_m512i(self.avx512) as u64
      } else {
        self.a.to_bitmask() as u64 | ((self.b.to_bitmask() as u64) << 32)
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        movepi8_mask_m512i(self.avx512) != 0
      } else {
        (self.a | self.b).any()
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        movepi8_mask_m512i(self.avx512) == u64::MAX
      } else {
        (self.a & self.b).all()
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u8x64) -> Self {
    pick! {
      if #[cfg(target_feature = "avx512vbmi")] {
        Self { avx512: permute_i8_m512i(indices.avx512, self.avx512) }
      } else {
        let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
        let [indices_a, indices_b] = cast::<u8x64, [u8x32; 2]>(indices);

        cast([[self_a, self_b].shuffle(indices_a), [self_a, self_b].shuffle(indices_b)])
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u8x64) -> Self {
    pick! {
      if #[cfg(target_feature = "avx512vbmi")] {
        self.shuffle(indices) & indices.simd_lt(64)
      } else {
        let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
        let [indices_a, indices_b] = cast::<u8x64, [u8x32; 2]>(indices);

        cast([
          [self_a, self_b].shuffle_zeroing(indices_a),
          [self_a, self_b].shuffle_zeroing(indices_b),
        ])
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u8x64) -> Self {
    pick! {
      if #[cfg(target_feature = "avx512vbmi")] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
        let [indices_a, indices_b] = cast::<u8x64, [u8x32; 2]>(indices);

        cast([
          [self_a, self_b].shuffle_wrapping(indices_a),
          [self_a, self_b].shuffle_wrapping(indices_b),
        ])
      }
    }
  }

  #[inline]
  fn shuffle(self: [u8x64; 2], indices: u8x64) -> u8x64 {
    pick! {
      if #[cfg(target_feature = "avx512vbmi")] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm512_permutex2var_epi8;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm512_permutex2var_epi8;
        // TODO(safe_arch): add `_mm512_permutex2var_epi8`.
        u8x64 {
          avx512: unsafe {
            m512i(_mm512_permutex2var_epi8(self[0].avx512.0, indices.avx512.0, self[1].avx512.0))
          },
        }
      } else {
        self[0].shuffle_zeroing(indices) | self[1].shuffle_zeroing(indices - 64)
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u8x64; 2], indices: u8x64) -> u8x64 {
    pick! {
      if #[cfg(target_feature = "avx512vbmi")] {
        self.shuffle(indices) & indices.simd_lt(128)
      } else {
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  fn shuffle_wrapping(self: [u8x64; 2], indices: u8x64) -> u8x64 {
    pick! {
      if #[cfg(target_feature = "avx512vbmi")] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 127)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u8x64; 3], indices: u8x64) -> u8x64 {
    [self[0], self[1]].shuffle_zeroing(indices) | self[2].shuffle_zeroing(indices - 128)
  }

  #[inline]
  fn shuffle_zeroing(self: [u8x64; 3], indices: u8x64) -> u8x64 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u8x64; 3], indices: u8x64) -> u8x64 {
    self.shuffle(indices % 192)
  }

  #[inline]
  fn shuffle(self: [u8x64; 4], indices: u8x64) -> u8x64 {
    [self[0], self[1]].shuffle_zeroing(indices) | [self[2], self[3]].shuffle_zeroing(indices - 128)
  }

  #[inline]
  fn shuffle_zeroing(self: [u8x64; 4], indices: u8x64) -> u8x64 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u8x64; 4], indices: u8x64) -> u8x64 {
    // There are 64*4=256 lanes to pick from, and that is exactly how many
    // indices `u8` can represent, so out of bounds indices are impossible.
    self.shuffle(indices)
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [Self; 64]) -> [Self; 64] {
    // Can this be optimized?

    #[inline(always)]
    fn transpose_column(data: &[u8x64; 64], index: usize) -> u8x64 {
      u8x64::new([
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
        data[16].as_array()[index],
        data[17].as_array()[index],
        data[18].as_array()[index],
        data[19].as_array()[index],
        data[20].as_array()[index],
        data[21].as_array()[index],
        data[22].as_array()[index],
        data[23].as_array()[index],
        data[24].as_array()[index],
        data[25].as_array()[index],
        data[26].as_array()[index],
        data[27].as_array()[index],
        data[28].as_array()[index],
        data[29].as_array()[index],
        data[30].as_array()[index],
        data[31].as_array()[index],
        data[32].as_array()[index],
        data[33].as_array()[index],
        data[34].as_array()[index],
        data[35].as_array()[index],
        data[36].as_array()[index],
        data[37].as_array()[index],
        data[38].as_array()[index],
        data[39].as_array()[index],
        data[40].as_array()[index],
        data[41].as_array()[index],
        data[42].as_array()[index],
        data[43].as_array()[index],
        data[44].as_array()[index],
        data[45].as_array()[index],
        data[46].as_array()[index],
        data[47].as_array()[index],
        data[48].as_array()[index],
        data[49].as_array()[index],
        data[50].as_array()[index],
        data[51].as_array()[index],
        data[52].as_array()[index],
        data[53].as_array()[index],
        data[54].as_array()[index],
        data[55].as_array()[index],
        data[56].as_array()[index],
        data[57].as_array()[index],
        data[58].as_array()[index],
        data[59].as_array()[index],
        data[60].as_array()[index],
        data[61].as_array()[index],
        data[62].as_array()[index],
        data[63].as_array()[index],
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
      transpose_column(&data, 16),
      transpose_column(&data, 17),
      transpose_column(&data, 18),
      transpose_column(&data, 19),
      transpose_column(&data, 20),
      transpose_column(&data, 21),
      transpose_column(&data, 22),
      transpose_column(&data, 23),
      transpose_column(&data, 24),
      transpose_column(&data, 25),
      transpose_column(&data, 26),
      transpose_column(&data, 27),
      transpose_column(&data, 28),
      transpose_column(&data, 29),
      transpose_column(&data, 30),
      transpose_column(&data, 31),
      transpose_column(&data, 32),
      transpose_column(&data, 33),
      transpose_column(&data, 34),
      transpose_column(&data, 35),
      transpose_column(&data, 36),
      transpose_column(&data, 37),
      transpose_column(&data, 38),
      transpose_column(&data, 39),
      transpose_column(&data, 40),
      transpose_column(&data, 41),
      transpose_column(&data, 42),
      transpose_column(&data, 43),
      transpose_column(&data, 44),
      transpose_column(&data, 45),
      transpose_column(&data, 46),
      transpose_column(&data, 47),
      transpose_column(&data, 48),
      transpose_column(&data, 49),
      transpose_column(&data, 50),
      transpose_column(&data, 51),
      transpose_column(&data, 52),
      transpose_column(&data, 53),
      transpose_column(&data, 54),
      transpose_column(&data, 55),
      transpose_column(&data, 56),
      transpose_column(&data, 57),
      transpose_column(&data, 58),
      transpose_column(&data, 59),
      transpose_column(&data, 60),
      transpose_column(&data, 61),
      transpose_column(&data, 62),
      transpose_column(&data, 63),
    ]
  }

  #[inline]
  fn shl(self, rhs: Self) -> Self::Output {
    // There's no dedicated `u8` shift instruction, so we split into halves
    // and let `u8x32` handle it (it widens to `u16` internally where
    // supported).
    let [self_a, self_b]: [u8x32; 2] = cast(self);
    let [rhs_a, rhs_b]: [u8x32; 2] = cast(rhs);
    cast([self_a << rhs_a, self_b << rhs_b])
  }

  #[inline]
  fn shl(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        let values: u16x32 = cast(self);
        let shift = rhs & 7;
        let shifted = values << shift;
        let crossed = u16x32::splat(!((0x00FFu16 << shift) & 0xFF00));
        cast(shifted & crossed)
      } else {
        let [self_a, self_b]: [u8x32; 2] = cast(self);
        cast([self_a << rhs, self_b << rhs])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: Self) -> Self::Output {
    let [self_a, self_b]: [u8x32; 2] = cast(self);
    let [rhs_a, rhs_b]: [u8x32; 2] = cast(rhs);
    cast([self_a >> rhs_a, self_b >> rhs_b])
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        let values: u16x32 = cast(self);
        let shift = rhs & 7;
        let shifted = values >> shift;
        let crossed = u16x32::splat(!((0xFF00u16 >> shift) & 0x00FF));
        cast(shifted & crossed)
      } else {
        let [self_a, self_b]: [u8x32; 2] = cast(self);
        cast([self_a >> rhs, self_b >> rhs])
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: max_u8_m512i(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: min_u8_m512i(self.avx512, rhs.avx512) }
      } else {
        Self {
          a : self.a.min(rhs.a),
          b : self.b.min(rhs.b),
        }
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u8 {
    let array: [u8x32; 2] = cast(self);
    array[0].max(array[1]).reduce_max()
  }

  #[inline]
  pub fn reduce_min(self) -> u8 {
    let array: [u8x32; 2] = cast(self);
    array[0].min(array[1]).reduce_min()
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u8x64, [u8x32; 2]>(rhs);
    cast([self_a.unbounded_shl(rhs_a), self_b.unbounded_shl(rhs_b)])
  }

  #[inline]
  pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        let values: u16x32 = cast(self);
        let shifted = values.unbounded_shl_scalar(rhs);
        let crossed = u16x32::splat(!(0x00FFu16.wrapping_shl(rhs) & 0xFF00));
        cast(shifted & crossed)
      } else {
        let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
        cast([self_a.unbounded_shl_scalar(rhs), self_b.unbounded_shl_scalar(rhs)])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: Self) -> Self {
    let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u8x64, [u8x32; 2]>(rhs);
    cast([self_a.unbounded_shr(rhs_a), self_b.unbounded_shr(rhs_b)])
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        let values: u16x32 = cast(self);
        let shifted = values.unbounded_shr_scalar(rhs);
        let crossed = u16x32::splat(!(0xFF00u16.wrapping_shr(rhs) & 0x00FF));
        cast(shifted & crossed)
      } else {
        let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
        cast([self_a.unbounded_shr_scalar(rhs), self_b.unbounded_shr_scalar(rhs)])
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: add_saturating_u8_m512i(self.avx512, rhs.avx512) }
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
      if #[cfg(target_feature="avx512bw")] {
        Self { avx512: sub_saturating_u8_m512i(self.avx512, rhs.avx512) }
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
    // Cannot have `widening_mul` because there is no `u16x64` type.
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    // There is no `_mm512_mullo_epi8`/`_mm512_mulhi_epu8` intrinsic, so there
    // is no `avx512bw` optimization.

    let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u8x64, [u8x32; 2]>(rhs);

    let result_a = self_a.mul_keep_low_high(rhs_a);
    let result_b = self_b.mul_keep_low_high(rhs_b);
    (cast([result_a.0, result_b.0]), cast([result_a.1, result_b.1]))
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    // There is no `_mm512_mulhi_epu8` intrinsic, so there is no `avx512bw`
    // optimization.

    let [self_a, self_b] = cast::<u8x64, [u8x32; 2]>(self);
    let [rhs_a, rhs_b] = cast::<u8x64, [u8x32; 2]>(rhs);

    cast([self_a.mul_keep_high(rhs_a), self_b.mul_keep_high(rhs_b)])
  }

  optional_fn_deserialize {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
      D: serde_core::Deserializer<'de>,
    {
        crate::simd::deserialize_array(deserializer)
    }
  }
}
