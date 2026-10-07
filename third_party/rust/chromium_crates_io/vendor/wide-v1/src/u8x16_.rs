use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with 16 elements of type [`u8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct u8x16 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with 16 elements of type [`u8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct u8x16 { pub(crate) simd: v128 }

    impl Default for u8x16 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for u8x16 {
      fn eq(&self, other: &Self) -> bool {
        u8x16_all_true(u8x16_eq(self.simd, other.simd))
      }
    }

    impl Eq for u8x16 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with 16 elements of type [`u8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct u8x16 { pub(crate) neon : uint8x16_t }

    impl Default for u8x16 {
      #[inline]
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for u8x16 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe { vminvq_u8(vceqq_u8(self.neon, other.neon))==u8::MAX }
      }
    }

    impl Eq for u8x16 { }
  } else {
    /// A SIMD vector with 16 elements of type [`u8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct u8x16 { pub(crate) arr: [u8;16] }
  }
}

impl_simd_uint! {
  unsafe {
    T = u8,
    N = 16,
    Simd = u8x16,
    IntSimd = i8x16,
    T_BITS = 8,
    T_BITS_MUL_2 = 16,
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = uint8x16_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn not(self) -> Self::Output {
    self ^ cast::<u128, u8x16>(u128::MAX)
  }

  #[inline]
  fn add(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: add_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_add(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vaddq_u8(self.neon, rhs.neon) } }
      } else {
        Self { arr: [
          self.arr[0].wrapping_add(rhs.arr[0]),
          self.arr[1].wrapping_add(rhs.arr[1]),
          self.arr[2].wrapping_add(rhs.arr[2]),
          self.arr[3].wrapping_add(rhs.arr[3]),
          self.arr[4].wrapping_add(rhs.arr[4]),
          self.arr[5].wrapping_add(rhs.arr[5]),
          self.arr[6].wrapping_add(rhs.arr[6]),
          self.arr[7].wrapping_add(rhs.arr[7]),
          self.arr[8].wrapping_add(rhs.arr[8]),
          self.arr[9].wrapping_add(rhs.arr[9]),
          self.arr[10].wrapping_add(rhs.arr[10]),
          self.arr[11].wrapping_add(rhs.arr[11]),
          self.arr[12].wrapping_add(rhs.arr[12]),
          self.arr[13].wrapping_add(rhs.arr[13]),
          self.arr[14].wrapping_add(rhs.arr[14]),
          self.arr[15].wrapping_add(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  fn sub(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: sub_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_sub(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vsubq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_sub(rhs.arr[0]),
          self.arr[1].wrapping_sub(rhs.arr[1]),
          self.arr[2].wrapping_sub(rhs.arr[2]),
          self.arr[3].wrapping_sub(rhs.arr[3]),
          self.arr[4].wrapping_sub(rhs.arr[4]),
          self.arr[5].wrapping_sub(rhs.arr[5]),
          self.arr[6].wrapping_sub(rhs.arr[6]),
          self.arr[7].wrapping_sub(rhs.arr[7]),
          self.arr[8].wrapping_sub(rhs.arr[8]),
          self.arr[9].wrapping_sub(rhs.arr[9]),
          self.arr[10].wrapping_sub(rhs.arr[10]),
          self.arr[11].wrapping_sub(rhs.arr[11]),
          self.arr[12].wrapping_sub(rhs.arr[12]),
          self.arr[13].wrapping_sub(rhs.arr[13]),
          self.arr[14].wrapping_sub(rhs.arr[14]),
          self.arr[15].wrapping_sub(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  fn mul(self, rhs: Self) -> Self::Output {
    // For x86 and wasm, this technically can be done explicitly by converting
    // to `i16` then converting back after multiplication, but that may not
    // actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vmulq_u8(self.neon, rhs.neon) } }
      } else {
        let self_array: [u8; 16] = cast(self);
        let rhs_array: [u8; 16] = cast(rhs);

        Self::new([
          self_array[0].wrapping_mul(rhs_array[0]),
          self_array[1].wrapping_mul(rhs_array[1]),
          self_array[2].wrapping_mul(rhs_array[2]),
          self_array[3].wrapping_mul(rhs_array[3]),
          self_array[4].wrapping_mul(rhs_array[4]),
          self_array[5].wrapping_mul(rhs_array[5]),
          self_array[6].wrapping_mul(rhs_array[6]),
          self_array[7].wrapping_mul(rhs_array[7]),
          self_array[8].wrapping_mul(rhs_array[8]),
          self_array[9].wrapping_mul(rhs_array[9]),
          self_array[10].wrapping_mul(rhs_array[10]),
          self_array[11].wrapping_mul(rhs_array[11]),
          self_array[12].wrapping_mul(rhs_array[12]),
          self_array[13].wrapping_mul(rhs_array[13]),
          self_array[14].wrapping_mul(rhs_array[14]),
          self_array[15].wrapping_mul(rhs_array[15]),
        ])
      }
    }
  }

  #[inline]
  fn bitand(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: bitand_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: v128_and(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vandq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitand(rhs.arr[0]),
          self.arr[1].bitand(rhs.arr[1]),
          self.arr[2].bitand(rhs.arr[2]),
          self.arr[3].bitand(rhs.arr[3]),
          self.arr[4].bitand(rhs.arr[4]),
          self.arr[5].bitand(rhs.arr[5]),
          self.arr[6].bitand(rhs.arr[6]),
          self.arr[7].bitand(rhs.arr[7]),
          self.arr[8].bitand(rhs.arr[8]),
          self.arr[9].bitand(rhs.arr[9]),
          self.arr[10].bitand(rhs.arr[10]),
          self.arr[11].bitand(rhs.arr[11]),
          self.arr[12].bitand(rhs.arr[12]),
          self.arr[13].bitand(rhs.arr[13]),
          self.arr[14].bitand(rhs.arr[14]),
          self.arr[15].bitand(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  fn bitor(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: bitor_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: v128_or(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vorrq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitor(rhs.arr[0]),
          self.arr[1].bitor(rhs.arr[1]),
          self.arr[2].bitor(rhs.arr[2]),
          self.arr[3].bitor(rhs.arr[3]),
          self.arr[4].bitor(rhs.arr[4]),
          self.arr[5].bitor(rhs.arr[5]),
          self.arr[6].bitor(rhs.arr[6]),
          self.arr[7].bitor(rhs.arr[7]),
          self.arr[8].bitor(rhs.arr[8]),
          self.arr[9].bitor(rhs.arr[9]),
          self.arr[10].bitor(rhs.arr[10]),
          self.arr[11].bitor(rhs.arr[11]),
          self.arr[12].bitor(rhs.arr[12]),
          self.arr[13].bitor(rhs.arr[13]),
          self.arr[14].bitor(rhs.arr[14]),
          self.arr[15].bitor(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  fn bitxor(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: bitxor_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: v128_xor(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: veorq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitxor(rhs.arr[0]),
          self.arr[1].bitxor(rhs.arr[1]),
          self.arr[2].bitxor(rhs.arr[2]),
          self.arr[3].bitxor(rhs.arr[3]),
          self.arr[4].bitxor(rhs.arr[4]),
          self.arr[5].bitxor(rhs.arr[5]),
          self.arr[6].bitxor(rhs.arr[6]),
          self.arr[7].bitxor(rhs.arr[7]),
          self.arr[8].bitxor(rhs.arr[8]),
          self.arr[9].bitxor(rhs.arr[9]),
          self.arr[10].bitxor(rhs.arr[10]),
          self.arr[11].bitxor(rhs.arr[11]),
          self.arr[12].bitxor(rhs.arr[12]),
          self.arr[13].bitxor(rhs.arr[13]),
          self.arr[14].bitxor(rhs.arr[14]),
          self.arr[15].bitxor(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  fn simd_eq(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_eq_mask_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_eq(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vceqq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] == rhs.arr[0] { u8::MAX } else { 0 },
          if self.arr[1] == rhs.arr[1] { u8::MAX } else { 0 },
          if self.arr[2] == rhs.arr[2] { u8::MAX } else { 0 },
          if self.arr[3] == rhs.arr[3] { u8::MAX } else { 0 },
          if self.arr[4] == rhs.arr[4] { u8::MAX } else { 0 },
          if self.arr[5] == rhs.arr[5] { u8::MAX } else { 0 },
          if self.arr[6] == rhs.arr[6] { u8::MAX } else { 0 },
          if self.arr[7] == rhs.arr[7] { u8::MAX } else { 0 },
          if self.arr[8] == rhs.arr[8] { u8::MAX } else { 0 },
          if self.arr[9] == rhs.arr[9] { u8::MAX } else { 0 },
          if self.arr[10] == rhs.arr[10] { u8::MAX } else { 0 },
          if self.arr[11] == rhs.arr[11] { u8::MAX } else { 0 },
          if self.arr[12] == rhs.arr[12] { u8::MAX } else { 0 },
          if self.arr[13] == rhs.arr[13] { u8::MAX } else { 0 },
          if self.arr[14] == rhs.arr[14] { u8::MAX } else { 0 },
          if self.arr[15] == rhs.arr[15] { u8::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_ne(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        !self.simd_eq(rhs)
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_ne(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_eq(rhs)
      } else {
        Self { arr: [
          if self.arr[0] != rhs.arr[0] { u8::MAX } else { 0 },
          if self.arr[1] != rhs.arr[1] { u8::MAX } else { 0 },
          if self.arr[2] != rhs.arr[2] { u8::MAX } else { 0 },
          if self.arr[3] != rhs.arr[3] { u8::MAX } else { 0 },
          if self.arr[4] != rhs.arr[4] { u8::MAX } else { 0 },
          if self.arr[5] != rhs.arr[5] { u8::MAX } else { 0 },
          if self.arr[6] != rhs.arr[6] { u8::MAX } else { 0 },
          if self.arr[7] != rhs.arr[7] { u8::MAX } else { 0 },
          if self.arr[8] != rhs.arr[8] { u8::MAX } else { 0 },
          if self.arr[9] != rhs.arr[9] { u8::MAX } else { 0 },
          if self.arr[10] != rhs.arr[10] { u8::MAX } else { 0 },
          if self.arr[11] != rhs.arr[11] { u8::MAX } else { 0 },
          if self.arr[12] != rhs.arr[12] { u8::MAX } else { 0 },
          if self.arr[13] != rhs.arr[13] { u8::MAX } else { 0 },
          if self.arr[14] != rhs.arr[14] { u8::MAX } else { 0 },
          if self.arr[15] != rhs.arr[15] { u8::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Convert from u8 to i8.
        let offset = Self::splat(0x80);
        let self_i8 = self.bitxor(offset).sse;
        let rhs_i8 = rhs.bitxor(offset).sse;
        Self { sse: cmp_lt_mask_i8_m128i(self_i8, rhs_i8) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_lt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vcltq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] < rhs.arr[0] { u8::MAX } else { 0 },
          if self.arr[1] < rhs.arr[1] { u8::MAX } else { 0 },
          if self.arr[2] < rhs.arr[2] { u8::MAX } else { 0 },
          if self.arr[3] < rhs.arr[3] { u8::MAX } else { 0 },
          if self.arr[4] < rhs.arr[4] { u8::MAX } else { 0 },
          if self.arr[5] < rhs.arr[5] { u8::MAX } else { 0 },
          if self.arr[6] < rhs.arr[6] { u8::MAX } else { 0 },
          if self.arr[7] < rhs.arr[7] { u8::MAX } else { 0 },
          if self.arr[8] < rhs.arr[8] { u8::MAX } else { 0 },
          if self.arr[9] < rhs.arr[9] { u8::MAX } else { 0 },
          if self.arr[10] < rhs.arr[10] { u8::MAX } else { 0 },
          if self.arr[11] < rhs.arr[11] { u8::MAX } else { 0 },
          if self.arr[12] < rhs.arr[12] { u8::MAX } else { 0 },
          if self.arr[13] < rhs.arr[13] { u8::MAX } else { 0 },
          if self.arr[14] < rhs.arr[14] { u8::MAX } else { 0 },
          if self.arr[15] < rhs.arr[15] { u8::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Convert from u8 to i8.
        let offset = Self::splat(0x80);
        let self_i8 = self.bitxor(offset).sse;
        let rhs_i8 = rhs.bitxor(offset).sse;
        Self { sse: cmp_gt_mask_i8_m128i(self_i8, rhs_i8) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_gt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vcgtq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] > rhs.arr[0] { u8::MAX } else { 0 },
          if self.arr[1] > rhs.arr[1] { u8::MAX } else { 0 },
          if self.arr[2] > rhs.arr[2] { u8::MAX } else { 0 },
          if self.arr[3] > rhs.arr[3] { u8::MAX } else { 0 },
          if self.arr[4] > rhs.arr[4] { u8::MAX } else { 0 },
          if self.arr[5] > rhs.arr[5] { u8::MAX } else { 0 },
          if self.arr[6] > rhs.arr[6] { u8::MAX } else { 0 },
          if self.arr[7] > rhs.arr[7] { u8::MAX } else { 0 },
          if self.arr[8] > rhs.arr[8] { u8::MAX } else { 0 },
          if self.arr[9] > rhs.arr[9] { u8::MAX } else { 0 },
          if self.arr[10] > rhs.arr[10] { u8::MAX } else { 0 },
          if self.arr[11] > rhs.arr[11] { u8::MAX } else { 0 },
          if self.arr[12] > rhs.arr[12] { u8::MAX } else { 0 },
          if self.arr[13] > rhs.arr[13] { u8::MAX } else { 0 },
          if self.arr[14] > rhs.arr[14] { u8::MAX } else { 0 },
          if self.arr[15] > rhs.arr[15] { u8::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_le(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Convert from u8 to i8.
        let offset = Self::splat(0x80);
        let self_i8 = self.bitxor(offset).sse;
        let rhs_i8 = rhs.bitxor(offset).sse;
        // a <= b  is equivalent to  !(b < a)  or  !(a > b)
        let gt_mask = u8x16 { sse: cmp_gt_mask_i8_m128i(self_i8, rhs_i8) };
        Self { sse: gt_mask.bitxor(u8x16::splat(0xFF)).sse }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_le(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vcleq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] <= rhs.arr[0] { u8::MAX } else { 0 },
          if self.arr[1] <= rhs.arr[1] { u8::MAX } else { 0 },
          if self.arr[2] <= rhs.arr[2] { u8::MAX } else { 0 },
          if self.arr[3] <= rhs.arr[3] { u8::MAX } else { 0 },
          if self.arr[4] <= rhs.arr[4] { u8::MAX } else { 0 },
          if self.arr[5] <= rhs.arr[5] { u8::MAX } else { 0 },
          if self.arr[6] <= rhs.arr[6] { u8::MAX } else { 0 },
          if self.arr[7] <= rhs.arr[7] { u8::MAX } else { 0 },
          if self.arr[8] <= rhs.arr[8] { u8::MAX } else { 0 },
          if self.arr[9] <= rhs.arr[9] { u8::MAX } else { 0 },
          if self.arr[10] <= rhs.arr[10] { u8::MAX } else { 0 },
          if self.arr[11] <= rhs.arr[11] { u8::MAX } else { 0 },
          if self.arr[12] <= rhs.arr[12] { u8::MAX } else { 0 },
          if self.arr[13] <= rhs.arr[13] { u8::MAX } else { 0 },
          if self.arr[14] <= rhs.arr[14] { u8::MAX } else { 0 },
          if self.arr[15] <= rhs.arr[15] { u8::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Convert from u8 to i8.
        let offset = Self::splat(0x80);
        let self_i8 = self.bitxor(offset).sse;
        let rhs_i8 = rhs.bitxor(offset).sse;
        // a >= b  is equivalent to  !(b > a)  or  !(a < b)
        let lt_mask = u8x16 { sse: cmp_lt_mask_i8_m128i(self_i8, rhs_i8) };
        Self { sse: lt_mask.bitxor(u8x16::splat(0xFF)).sse }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_ge(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vcgeq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] >= rhs.arr[0] { u8::MAX } else { 0 },
          if self.arr[1] >= rhs.arr[1] { u8::MAX } else { 0 },
          if self.arr[2] >= rhs.arr[2] { u8::MAX } else { 0 },
          if self.arr[3] >= rhs.arr[3] { u8::MAX } else { 0 },
          if self.arr[4] >= rhs.arr[4] { u8::MAX } else { 0 },
          if self.arr[5] >= rhs.arr[5] { u8::MAX } else { 0 },
          if self.arr[6] >= rhs.arr[6] { u8::MAX } else { 0 },
          if self.arr[7] >= rhs.arr[7] { u8::MAX } else { 0 },
          if self.arr[8] >= rhs.arr[8] { u8::MAX } else { 0 },
          if self.arr[9] >= rhs.arr[9] { u8::MAX } else { 0 },
          if self.arr[10] >= rhs.arr[10] { u8::MAX } else { 0 },
          if self.arr[11] >= rhs.arr[11] { u8::MAX } else { 0 },
          if self.arr[12] >= rhs.arr[12] { u8::MAX } else { 0 },
          if self.arr[13] >= rhs.arr[13] { u8::MAX } else { 0 },
          if self.arr[14] >= rhs.arr[14] { u8::MAX } else { 0 },
          if self.arr[15] >= rhs.arr[15] { u8::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> u8 {
    #[allow(dead_code)]
    const SHUFFLE_1: [u8; 16] =
      [8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_2: [u8; 16] =
      [4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_3: [u8; 16] =
      [2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_4: [u8; 16] =
      [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    pick! {
      if #[cfg(target_feature="ssse3")] {
        let rhs = shuffle_av_i8z_all_m128i(self.sse, m128i::from(SHUFFLE_1));
        let sum = add_i8_m128i(self.sse, rhs);
        let rhs = shuffle_av_i8z_all_m128i(sum, m128i::from(SHUFFLE_2));
        let sum = add_i8_m128i(sum, rhs);
        let rhs = shuffle_av_i8z_all_m128i(sum, m128i::from(SHUFFLE_3));
        let sum = add_i8_m128i(sum, rhs);
        let rhs = shuffle_av_i8z_all_m128i(sum, m128i::from(SHUFFLE_4));
        let sum = add_i8_m128i(sum, rhs);
        get_i32_from_m128i_s(sum) as u8
      } else if #[cfg(target_feature="simd128")] {
        let rhs = u8x16_shuffle::<8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7>(self.simd, self.simd);
        let sum = u8x16_add(self.simd, rhs);
        let rhs = u8x16_shuffle::<4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0>(sum, sum);
        let sum = u8x16_add(sum, rhs);
        let rhs = u8x16_shuffle::<2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(sum, sum);
        let sum = u8x16_add(sum, rhs);
        let rhs = u8x16_shuffle::<1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(sum, sum);
        let sum = u8x16_add(sum, rhs);
        u8x16_extract_lane::<0>(sum)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Use `transmute` instead of `cast` because `uint8x16_t` does not
          // implement `bytemuck::Pod`.
          let rhs = vqtbl1q_u8(self.neon, core::mem::transmute(SHUFFLE_1));
          let sum = vaddq_u8(self.neon, rhs);
          let rhs = vqtbl1q_u8(sum, core::mem::transmute(SHUFFLE_2));
          let sum = vaddq_u8(sum, rhs);
          let rhs = vqtbl1q_u8(sum, core::mem::transmute(SHUFFLE_3));
          let sum = vaddq_u8(sum, rhs);
          let rhs = vqtbl1q_u8(sum, core::mem::transmute(SHUFFLE_4));
          let sum = vaddq_u8(sum, rhs);
          vgetq_lane_u8(sum, 0)
        }
      } else {
        let array: [u8; 16] = cast(self);
        array.into_iter().reduce(u8::wrapping_add).unwrap()
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> u8 {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        const HIGH_64: [u8; 16] = [8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0];
        const HIGH_32: [u8; 16] = [4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0];
        const HIGH_16: [u8; 16] = [2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        const HIGH_8: [u8; 16] = [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

        unsafe {
          // Use `transmute` instead of `cast` because `int8x16_t` does not
          // implement `bytemuck::Pod`.
          let high_64 = vqtbl1q_u8(self.neon, core::mem::transmute(HIGH_64));
          let reduce_64 = vmulq_u8(self.neon, high_64);
          let high_32 = vqtbl1q_u8(reduce_64, core::mem::transmute(HIGH_32));
          let reduce_32 = vmulq_u8(reduce_64, high_32);
          let high_16 = vqtbl1q_u8(reduce_32, core::mem::transmute(HIGH_16));
          let reduce_16 = vmulq_u8(reduce_32, high_16);
          let high_8 = vqtbl1q_u8(reduce_16, core::mem::transmute(HIGH_8));
          let reduce_8 = vmulq_u8(reduce_16, high_8);
          vgetq_lane_u8::<0>(reduce_8)
        }
      } else {
        self.to_array().into_iter().reduce(u8::wrapping_mul).unwrap()
      }
    }
  }

  #[inline]
  pub fn bitselect(self, if_one: Self, if_zero: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self {
          sse: bitor_m128i(
            bitand_m128i(if_one.sse, self.sse),
            bitandnot_m128i(self.sse, if_zero.sse),
          ),
        }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: v128_bitselect(if_one.simd, if_zero.simd, self.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vbslq_u8(self.neon, if_one.neon, if_zero.neon) }}
      } else {
        generic_bit_blend(self, if_one, if_zero)
      }
    }
  }

  #[inline]
  fn select(self, if_true: Self, if_false: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: blend_varying_i8_m128i(if_false.sse, if_true.sse, self.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: v128_bitselect(if_true.simd, if_false.simd, self.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vbslq_u8(self.neon, if_true.neon, if_false.neon) }}
      } else {
        generic_bit_blend(self, if_true, if_false)
      }
    }
  }

  #[inline]
  pub fn to_bitmask(self) -> u32 {
    pick! {
      if #[cfg(target_feature="sse2")] {
        move_mask_i8_m128i(self.sse) as u32
      } else if #[cfg(target_feature="simd128")] {
        u8x16_bitmask(self.simd) as u32
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // set all to 1 if top bit is set, else 0
          let masked = vcltzq_s8(self.cast_signed().neon);

          // select the right bit out of each lane
          let selectbit : uint8x16_t = core::mem::transmute([1u8, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128]);
          let out = vandq_u8(masked, selectbit);

          // interleave the lanes so that a 16-bit sum accumulates the bits in the right order
          let table : uint8x16_t = core::mem::transmute([0u8, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15]);
          let r = vqtbl1q_u8(out, table);

          // horizontally add the 16-bit lanes
          vaddvq_u16(vreinterpretq_u16_u8(r)) as u32
        }
       } else {
        ((self.arr[0].cast_signed() < 0) as u32) |
        ((self.arr[1].cast_signed() < 0) as u32) << 1 |
        ((self.arr[2].cast_signed() < 0) as u32) << 2 |
        ((self.arr[3].cast_signed() < 0) as u32) << 3 |
        ((self.arr[4].cast_signed() < 0) as u32) << 4 |
        ((self.arr[5].cast_signed() < 0) as u32) << 5 |
        ((self.arr[6].cast_signed() < 0) as u32) << 6 |
        ((self.arr[7].cast_signed() < 0) as u32) << 7 |
        ((self.arr[8].cast_signed() < 0) as u32) << 8 |
        ((self.arr[9].cast_signed() < 0) as u32) << 9 |
        ((self.arr[10].cast_signed() < 0) as u32) << 10 |
        ((self.arr[11].cast_signed() < 0) as u32) << 11 |
        ((self.arr[12].cast_signed() < 0) as u32) << 12 |
        ((self.arr[13].cast_signed() < 0) as u32) << 13 |
        ((self.arr[14].cast_signed() < 0) as u32) << 14 |
        ((self.arr[15].cast_signed() < 0) as u32) << 15
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="sse2")] {
        move_mask_i8_m128i(self.sse) != 0
      } else if #[cfg(target_feature="simd128")] {
        u8x16_bitmask(self.simd) != 0
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {
          vminvq_s8(self.cast_signed().neon) < 0
        }
      } else {
        let v : [u64;2] = cast(self);
        ((v[0] | v[1]) & 0x8080808080808080) != 0
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="sse2")] {
        move_mask_i8_m128i(self.sse) == 0b1111_1111_1111_1111
      } else if #[cfg(target_feature="simd128")] {
        u8x16_bitmask(self.simd) == 0b1111_1111_1111_1111
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {
          vmaxvq_s8(self.cast_signed().neon) < 0
        }
      } else {
        let v : [u64;2] = cast(self);
        (v[0] & v[1] & 0x8080808080808080) == 0x8080808080808080
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u8x16) -> Self {
    pick! {
      if #[cfg(target_feature="ssse3")] {
        Self { sse: shuffle_av_i8z_all_m128i(self.sse, indices.sse) }
      } else if #[cfg(target_feature="relaxed-simd")] {
        Self { simd: u8x16_relaxed_swizzle(self.simd, indices.simd) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_swizzle(self.simd, indices.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe { Self { neon: vqtbl1q_u8(self.neon, indices.neon) } }
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
  pub fn shuffle_zeroing(self, indices: u8x16) -> Self {
    pick! {
      if #[cfg(target_feature="ssse3")] {
        Self {
          sse: shuffle_av_i8z_all_m128i(
            self.sse,
            add_saturating_u8_m128i(indices.sse, set_splat_i8_m128i(0x70)),
          ),
        }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_swizzle(self.simd, indices.simd) }
      } else {
        // The remaining branches of `shuffle` already have the behavior we want
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u8x16) -> Self {
    pick! {
      if #[cfg(all(target_feature = "avx512vbmi", target_feature = "avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm_permutexvar_epi8;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm_permutexvar_epi8;

        // TODO(safe_arch): add `_mm_permutexvar_epi8`.
        Self { sse: unsafe { m128i(_mm_permutexvar_epi8(indices.sse.0, self.sse.0)) } }
      } else {
        self.shuffle(indices & 15)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u8x16; 2], indices: u8x16) -> u8x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512vbmi", target_feature = "avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm_permutex2var_epi8;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm_permutex2var_epi8;

        // TODO(safe_arch): add `_mm_permutex2var_epi8`.
        u8x16 {
          sse: unsafe {
            m128i(_mm_permutex2var_epi8(self[0].sse.0, indices.sse.0, self[1].sse.0))
          },
        }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        let table = uint8x16x2_t(self[0].neon, self[1].neon);
        unsafe { u8x16 { neon: vqtbl2q_u8(table, indices.neon) } }
      } else {
        self[0].shuffle_zeroing(indices) | self[1].shuffle_zeroing(indices - 16)
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u8x16; 2], indices: u8x16) -> u8x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512vbmi", target_feature = "avx512vl"))] {
        self.shuffle(indices) & indices.simd_lt(32)
      } else {
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  fn shuffle_wrapping(self: [u8x16; 2], indices: u8x16) -> u8x16 {
    pick! {
      if #[cfg(all(target_feature = "avx512vbmi", target_feature = "avx512vl"))] {
        // `avx512` shuffle intrinsics are wrapping
        self.shuffle(indices)
      } else {
        self.shuffle(indices & 31)
      }
    }
  }

  #[inline]
  fn shuffle(self: [u8x16; 3], indices: u8x16) -> u8x16 {
    pick! {
      if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        let table = uint8x16x3_t(self[0].neon, self[1].neon, self[2].neon);
        unsafe { u8x16 { neon: vqtbl3q_u8(table, indices.neon) } }
      } else {
        [self[0], self[1]].shuffle_zeroing(indices) | self[2].shuffle_zeroing(indices - 32)
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u8x16; 3], indices: u8x16) -> u8x16 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u8x16; 3], indices: u8x16) -> u8x16 {
    self.shuffle(indices % 48)
  }

  #[inline]
  fn shuffle(self: [u8x16; 4], indices: u8x16) -> u8x16 {
    pick! {
      if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        let table = uint8x16x4_t(self[0].neon, self[1].neon, self[2].neon, self[3].neon);
        unsafe { u8x16 { neon: vqtbl4q_u8(table, indices.neon) } }
      } else {
        [self[0], self[1]].shuffle_zeroing(indices)
          | [self[2], self[3]].shuffle_zeroing(indices - 32)
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u8x16; 4], indices: u8x16) -> u8x16 {
    self.shuffle(indices)
  }

  #[inline]
  fn shuffle_wrapping(self: [u8x16; 4], indices: u8x16) -> u8x16 {
    self.shuffle(indices & 63)
  }

  ///
  /// Currently this function is never accelerated.
  #[inline]
  pub fn transpose(data: [Self; 16]) -> [Self; 16] {
    // Can this be optimized?

    #[inline(always)]
    fn transpose_column(data: &[u8x16; 16], index: usize) -> u8x16 {
      u8x16::new([
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
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that may
    // not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Mask `rhs` to 7 to match `wrapping_shl`.
          let shift_by = vreinterpretq_s8_u8(vandq_u8(rhs.neon, vmovq_n_u8(7)));
          Self { neon: vshlq_u8(self.neon, shift_by) }
        }
      } else {
        let self_array: [u8; 16] = cast(self);
        let rhs_array: [u8; 16] = cast(rhs);

        Self::new([
          self_array[0].wrapping_shl(rhs_array[0] as u32),
          self_array[1].wrapping_shl(rhs_array[1] as u32),
          self_array[2].wrapping_shl(rhs_array[2] as u32),
          self_array[3].wrapping_shl(rhs_array[3] as u32),
          self_array[4].wrapping_shl(rhs_array[4] as u32),
          self_array[5].wrapping_shl(rhs_array[5] as u32),
          self_array[6].wrapping_shl(rhs_array[6] as u32),
          self_array[7].wrapping_shl(rhs_array[7] as u32),
          self_array[8].wrapping_shl(rhs_array[8] as u32),
          self_array[9].wrapping_shl(rhs_array[9] as u32),
          self_array[10].wrapping_shl(rhs_array[10] as u32),
          self_array[11].wrapping_shl(rhs_array[11] as u32),
          self_array[12].wrapping_shl(rhs_array[12] as u32),
          self_array[13].wrapping_shl(rhs_array[13] as u32),
          self_array[14].wrapping_shl(rhs_array[14] as u32),
          self_array[15].wrapping_shl(rhs_array[15] as u32),
        ])
      }
    }
  }

  #[inline]
  fn shl(self, rhs: u32) -> Self::Output {
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(target_feature="simd128")] {
        // Mask `rhs` to 7 to match `wrapping_shl`.
        Self { simd: u8x16_shl(self.simd, rhs & 7) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Mask `rhs` to 7 to match `wrapping_shl`.
        unsafe { Self { neon: vshlq_u8(self.neon, vmovq_n_s8(rhs as i8 & 7)) } }
      } else {
        let values: u16x8 = cast(self);
        let shift = rhs & 7;
        let shifted = values << shift;
        let crossed = u16x8::splat(!((0x00FFu16 << shift) & 0xFF00));
        cast(shifted & crossed)
      }
    }
  }

  #[inline]
  fn shr(self, rhs: Self) -> Self::Output {
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that may
    // not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Mask `rhs` to 7 to match `wrapping_shr`, and negate it because
          // there is no shift-right intrinsic.
          let neg_rhs = vnegq_s8(vreinterpretq_s8_u8(vandq_u8(rhs.neon, vmovq_n_u8(7))));
          Self { neon: vshlq_u8(self.neon, neg_rhs) }
        }
      } else {
        let self_array: [u8; 16] = cast(self);
        let rhs_array: [u8; 16] = cast(rhs);

        Self::new([
          self_array[0].wrapping_shr(rhs_array[0] as u32),
          self_array[1].wrapping_shr(rhs_array[1] as u32),
          self_array[2].wrapping_shr(rhs_array[2] as u32),
          self_array[3].wrapping_shr(rhs_array[3] as u32),
          self_array[4].wrapping_shr(rhs_array[4] as u32),
          self_array[5].wrapping_shr(rhs_array[5] as u32),
          self_array[6].wrapping_shr(rhs_array[6] as u32),
          self_array[7].wrapping_shr(rhs_array[7] as u32),
          self_array[8].wrapping_shr(rhs_array[8] as u32),
          self_array[9].wrapping_shr(rhs_array[9] as u32),
          self_array[10].wrapping_shr(rhs_array[10] as u32),
          self_array[11].wrapping_shr(rhs_array[11] as u32),
          self_array[12].wrapping_shr(rhs_array[12] as u32),
          self_array[13].wrapping_shr(rhs_array[13] as u32),
          self_array[14].wrapping_shr(rhs_array[14] as u32),
          self_array[15].wrapping_shr(rhs_array[15] as u32),
        ])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(target_feature="simd128")] {
        // Mask `rhs` to 7 to match `wrapping_shr`.
        Self { simd: u8x16_shr(self.simd, rhs & 7) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Mask `rhs` to 7 to match `wrapping_shr`, and negate it because
        // there is no shift-right intrinsic.
        unsafe { Self { neon: vshlq_u8(self.neon, vmovq_n_s8(-(rhs as i8 & 7))) } }
      } else {
        let values: u16x8 = cast(self);
        let shift = rhs & 7;
        let shifted = values >> shift;
        let crossed = u16x8::splat(!((0xFF00u16 >> shift) & 0x00FF));
        cast(shifted & crossed)
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: max_u8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_max(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmaxq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].max(rhs.arr[0]),
          self.arr[1].max(rhs.arr[1]),
          self.arr[2].max(rhs.arr[2]),
          self.arr[3].max(rhs.arr[3]),
          self.arr[4].max(rhs.arr[4]),
          self.arr[5].max(rhs.arr[5]),
          self.arr[6].max(rhs.arr[6]),
          self.arr[7].max(rhs.arr[7]),
          self.arr[8].max(rhs.arr[8]),
          self.arr[9].max(rhs.arr[9]),
          self.arr[10].max(rhs.arr[10]),
          self.arr[11].max(rhs.arr[11]),
          self.arr[12].max(rhs.arr[12]),
          self.arr[13].max(rhs.arr[13]),
          self.arr[14].max(rhs.arr[14]),
          self.arr[15].max(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: min_u8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_min(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vminq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].min(rhs.arr[0]),
          self.arr[1].min(rhs.arr[1]),
          self.arr[2].min(rhs.arr[2]),
          self.arr[3].min(rhs.arr[3]),
          self.arr[4].min(rhs.arr[4]),
          self.arr[5].min(rhs.arr[5]),
          self.arr[6].min(rhs.arr[6]),
          self.arr[7].min(rhs.arr[7]),
          self.arr[8].min(rhs.arr[8]),
          self.arr[9].min(rhs.arr[9]),
          self.arr[10].min(rhs.arr[10]),
          self.arr[11].min(rhs.arr[11]),
          self.arr[12].min(rhs.arr[12]),
          self.arr[13].min(rhs.arr[13]),
          self.arr[14].min(rhs.arr[14]),
          self.arr[15].min(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u8 {
    #[allow(dead_code)]
    const SHUFFLE_1: [i8; 16] =
      [8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_2: [i8; 16] =
      [4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_3: [i8; 16] =
      [2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_4: [i8; 16] =
      [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    pick! {
      if #[cfg(target_feature="ssse3")] {
        let rhs = shuffle_av_i8z_all_m128i(self.sse, m128i::from(SHUFFLE_1));
        let max = max_u8_m128i(self.sse, rhs);
        let rhs = shuffle_av_i8z_all_m128i(max, m128i::from(SHUFFLE_2));
        let max = max_u8_m128i(max, rhs);
        let rhs = shuffle_av_i8z_all_m128i(max, m128i::from(SHUFFLE_3));
        let max = max_u8_m128i(max, rhs);
        let rhs = shuffle_av_i8z_all_m128i(max, m128i::from(SHUFFLE_4));
        let max = max_u8_m128i(max, rhs);
        get_i32_from_m128i_s(max) as u8
      } else if #[cfg(target_feature="simd128")] {
        let rhs = u8x16_shuffle::<8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7>(self.simd, self.simd);
        let max = u8x16_max(self.simd, rhs);
        let rhs = u8x16_shuffle::<4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0>(max, max);
        let max = u8x16_max(max, rhs);
        let rhs = u8x16_shuffle::<2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(max, max);
        let max = u8x16_max(max, rhs);
        let rhs = u8x16_shuffle::<1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(max, max);
        let max = u8x16_max(max, rhs);
        u8x16_extract_lane::<0>(max)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Use `transmute` instead of `cast` because `uint8x16_t` does not
          // implement `bytemuck::Pod`.
          let rhs = vqtbl1q_u8(self.neon, core::mem::transmute(SHUFFLE_1));
          let max = vmaxq_u8(self.neon, rhs);
          let rhs = vqtbl1q_u8(max, core::mem::transmute(SHUFFLE_2));
          let max = vmaxq_u8(max, rhs);
          let rhs = vqtbl1q_u8(max, core::mem::transmute(SHUFFLE_3));
          let max = vmaxq_u8(max, rhs);
          let rhs = vqtbl1q_u8(max, core::mem::transmute(SHUFFLE_4));
          let max = vmaxq_u8(max, rhs);
          vgetq_lane_u8(max, 0)
        }
      } else {
        let array: [u8; 16] = cast(self);
        array.into_iter().reduce(u8::max).unwrap()
      }
    }
  }

  #[inline]
  pub fn reduce_min(self) -> u8 {
    #[allow(dead_code)]
    const SHUFFLE_1: [i8; 16] =
      [8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_2: [i8; 16] =
      [4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_3: [i8; 16] =
      [2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    #[allow(dead_code)]
    const SHUFFLE_4: [i8; 16] =
      [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    pick! {
      if #[cfg(target_feature="ssse3")] {
        let rhs = shuffle_av_i8z_all_m128i(self.sse, m128i::from(SHUFFLE_1));
        let min = min_u8_m128i(self.sse, rhs);
        let rhs = shuffle_av_i8z_all_m128i(min, m128i::from(SHUFFLE_2));
        let min = min_u8_m128i(min, rhs);
        let rhs = shuffle_av_i8z_all_m128i(min, m128i::from(SHUFFLE_3));
        let min = min_u8_m128i(min, rhs);
        let rhs = shuffle_av_i8z_all_m128i(min, m128i::from(SHUFFLE_4));
        let min = min_u8_m128i(min, rhs);
        get_i32_from_m128i_s(min) as u8
      } else if #[cfg(target_feature="simd128")] {
        let rhs = u8x16_shuffle::<8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7>(self.simd, self.simd);
        let min = u8x16_min(self.simd, rhs);
        let rhs = u8x16_shuffle::<4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0>(min, min);
        let min = u8x16_min(min, rhs);
        let rhs = u8x16_shuffle::<2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(min, min);
        let min = u8x16_min(min, rhs);
        let rhs = u8x16_shuffle::<1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(min, min);
        let min = u8x16_min(min, rhs);
        u8x16_extract_lane::<0>(min)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Use `transmute` instead of `cast` because `uint8x16_t` does not
          // implement `bytemuck::Pod`.
          let rhs = vqtbl1q_u8(self.neon, core::mem::transmute(SHUFFLE_1));
          let min = vminq_u8(self.neon, rhs);
          let rhs = vqtbl1q_u8(min, core::mem::transmute(SHUFFLE_2));
          let min = vminq_u8(min, rhs);
          let rhs = vqtbl1q_u8(min, core::mem::transmute(SHUFFLE_3));
          let min = vminq_u8(min, rhs);
          let rhs = vqtbl1q_u8(min, core::mem::transmute(SHUFFLE_4));
          let min = vminq_u8(min, rhs);
          vgetq_lane_u8(min, 0)
        }
      } else {
        let array: [u8; 16] = cast(self);
        array.into_iter().reduce(u8::min).unwrap()
      }
    }
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    // For x86, this technically can be done explicitly by converting to `u16`
    // or `u32` then converting back after multiplication, but that may not
    // actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          Self { neon: vshlq_u8(self.neon, vreinterpretq_s8_u8(rhs.neon)) } & rhs.simd_lt(8)
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          self_array[0].unbounded_shl(rhs_array[0] as u32),
          self_array[1].unbounded_shl(rhs_array[1] as u32),
          self_array[2].unbounded_shl(rhs_array[2] as u32),
          self_array[3].unbounded_shl(rhs_array[3] as u32),
          self_array[4].unbounded_shl(rhs_array[4] as u32),
          self_array[5].unbounded_shl(rhs_array[5] as u32),
          self_array[6].unbounded_shl(rhs_array[6] as u32),
          self_array[7].unbounded_shl(rhs_array[7] as u32),
          self_array[8].unbounded_shl(rhs_array[8] as u32),
          self_array[9].unbounded_shl(rhs_array[9] as u32),
          self_array[10].unbounded_shl(rhs_array[10] as u32),
          self_array[11].unbounded_shl(rhs_array[11] as u32),
          self_array[12].unbounded_shl(rhs_array[12] as u32),
          self_array[13].unbounded_shl(rhs_array[13] as u32),
          self_array[14].unbounded_shl(rhs_array[14] as u32),
          self_array[15].unbounded_shl(rhs_array[15] as u32),
        ])
      }
    }
  }

  #[inline]
  pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(target_feature="simd128")] {
        // The intrinsic performs wrapping shift so we need to mask the result.
        if rhs >= 8 { Self::ZERO } else { Self { simd: u8x16_shl(self.simd, rhs) } }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // The intrinsic has different semantics so we need to saturate `rhs`.
        unsafe { Self { neon: vshlq_u8(self.neon, vmovq_n_s8(rhs.min(i8::MAX as u32) as i8)) } }
      } else {
        let values: u16x8 = cast(self);
        let shifted = values.unbounded_shl_scalar(rhs);
        let crossed = u16x8::splat(!(0x00FFu16.wrapping_shl(rhs) & 0xFF00));
        cast(shifted & crossed)
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: Self) -> Self {
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that may
    // not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // mask to hide `rhs` overflow.
          Self { neon: vshlq_u8(self.neon, vnegq_s8(vreinterpretq_s8_u8(rhs.neon))) } & rhs.simd_lt(8)
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          self_array[0].unbounded_shr(rhs_array[0] as u32),
          self_array[1].unbounded_shr(rhs_array[1] as u32),
          self_array[2].unbounded_shr(rhs_array[2] as u32),
          self_array[3].unbounded_shr(rhs_array[3] as u32),
          self_array[4].unbounded_shr(rhs_array[4] as u32),
          self_array[5].unbounded_shr(rhs_array[5] as u32),
          self_array[6].unbounded_shr(rhs_array[6] as u32),
          self_array[7].unbounded_shr(rhs_array[7] as u32),
          self_array[8].unbounded_shr(rhs_array[8] as u32),
          self_array[9].unbounded_shr(rhs_array[9] as u32),
          self_array[10].unbounded_shr(rhs_array[10] as u32),
          self_array[11].unbounded_shr(rhs_array[11] as u32),
          self_array[12].unbounded_shr(rhs_array[12] as u32),
          self_array[13].unbounded_shr(rhs_array[13] as u32),
          self_array[14].unbounded_shr(rhs_array[14] as u32),
          self_array[15].unbounded_shr(rhs_array[15] as u32),
        ])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    // For x86, this technically can be done explicitly by converting
    // to `u16` or `u32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(target_feature="simd128")] {
        if rhs < 8 { Self { simd: u8x16_shr(self.simd, rhs) } } else { Self::ZERO }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_u8(self.neon, vmovq_n_s8(-rhs.min(8).cast_signed() as i8)) }
        }
      } else {
        let values: u16x8 = cast(self);
        let shifted = values.unbounded_shr_scalar(rhs);
        let crossed = u16x8::splat(!(0xFF00u16.wrapping_shr(rhs) & 0x00FF));
        cast(shifted & crossed)
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: add_saturating_u8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_add_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vqaddq_u8(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].saturating_add(rhs.arr[0]),
          self.arr[1].saturating_add(rhs.arr[1]),
          self.arr[2].saturating_add(rhs.arr[2]),
          self.arr[3].saturating_add(rhs.arr[3]),
          self.arr[4].saturating_add(rhs.arr[4]),
          self.arr[5].saturating_add(rhs.arr[5]),
          self.arr[6].saturating_add(rhs.arr[6]),
          self.arr[7].saturating_add(rhs.arr[7]),
          self.arr[8].saturating_add(rhs.arr[8]),
          self.arr[9].saturating_add(rhs.arr[9]),
          self.arr[10].saturating_add(rhs.arr[10]),
          self.arr[11].saturating_add(rhs.arr[11]),
          self.arr[12].saturating_add(rhs.arr[12]),
          self.arr[13].saturating_add(rhs.arr[13]),
          self.arr[14].saturating_add(rhs.arr[14]),
          self.arr[15].saturating_add(rhs.arr[15]),
        ]}
      }
    }
  }

  #[inline]
  pub fn saturating_sub(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: sub_saturating_u8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u8x16_sub_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_u8(self.neon, rhs.neon) } }
      } else {
        Self { arr: [
          self.arr[0].saturating_sub(rhs.arr[0]),
          self.arr[1].saturating_sub(rhs.arr[1]),
          self.arr[2].saturating_sub(rhs.arr[2]),
          self.arr[3].saturating_sub(rhs.arr[3]),
          self.arr[4].saturating_sub(rhs.arr[4]),
          self.arr[5].saturating_sub(rhs.arr[5]),
          self.arr[6].saturating_sub(rhs.arr[6]),
          self.arr[7].saturating_sub(rhs.arr[7]),
          self.arr[8].saturating_sub(rhs.arr[8]),
          self.arr[9].saturating_sub(rhs.arr[9]),
          self.arr[10].saturating_sub(rhs.arr[10]),
          self.arr[11].saturating_sub(rhs.arr[11]),
          self.arr[12].saturating_sub(rhs.arr[12]),
          self.arr[13].saturating_sub(rhs.arr[13]),
          self.arr[14].saturating_sub(rhs.arr[14]),
          self.arr[15].saturating_sub(rhs.arr[15]),
        ]}
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
    pub fn widening_mul(self, rhs: Self) -> u16x16 {
      pick! {
        if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
          unsafe {
            let low_wide_mul = vmull_u8(vget_low_u8(self.neon), vget_low_u8(rhs.neon));
            let high_wide_mul = vmull_u8(vget_high_u8(self.neon), vget_high_u8(rhs.neon));

            u16x16 {
              a: u16x8 { neon: low_wide_mul },
              b: u16x8 { neon: high_wide_mul },
            }
          }
        } else {
          let self_array = self.to_array();
          let rhs_array = rhs.to_array();

          u16x16::new([
            (self_array[0] as u16).wrapping_mul(rhs_array[0] as u16),
            (self_array[1] as u16).wrapping_mul(rhs_array[1] as u16),
            (self_array[2] as u16).wrapping_mul(rhs_array[2] as u16),
            (self_array[3] as u16).wrapping_mul(rhs_array[3] as u16),
            (self_array[4] as u16).wrapping_mul(rhs_array[4] as u16),
            (self_array[5] as u16).wrapping_mul(rhs_array[5] as u16),
            (self_array[6] as u16).wrapping_mul(rhs_array[6] as u16),
            (self_array[7] as u16).wrapping_mul(rhs_array[7] as u16),
            (self_array[8] as u16).wrapping_mul(rhs_array[8] as u16),
            (self_array[9] as u16).wrapping_mul(rhs_array[9] as u16),
            (self_array[10] as u16).wrapping_mul(rhs_array[10] as u16),
            (self_array[11] as u16).wrapping_mul(rhs_array[11] as u16),
            (self_array[12] as u16).wrapping_mul(rhs_array[12] as u16),
            (self_array[13] as u16).wrapping_mul(rhs_array[13] as u16),
            (self_array[14] as u16).wrapping_mul(rhs_array[14] as u16),
            (self_array[15] as u16).wrapping_mul(rhs_array[15] as u16),
          ])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_u8_u16(
            vmull_u8(vget_low_u8(self.neon), vget_low_u8(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_u8_u16(
            vmull_u8(vget_high_u8(self.neon), vget_high_u8(rhs.neon)),
          );
          let low_high = vuzpq_u8(low_wide_mul, high_wide_mul);
          (
            Self { neon: low_high.0 },
            Self { neon: low_high.1 },
          )
        }
      } else {
        // TODO(perf): This implementation looks quite bad. Is there a better
        // one?

        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        let widening_mul = [
          (self_array[0] as u16).wrapping_mul(rhs_array[0] as u16),
          (self_array[1] as u16).wrapping_mul(rhs_array[1] as u16),
          (self_array[2] as u16).wrapping_mul(rhs_array[2] as u16),
          (self_array[3] as u16).wrapping_mul(rhs_array[3] as u16),
          (self_array[4] as u16).wrapping_mul(rhs_array[4] as u16),
          (self_array[5] as u16).wrapping_mul(rhs_array[5] as u16),
          (self_array[6] as u16).wrapping_mul(rhs_array[6] as u16),
          (self_array[7] as u16).wrapping_mul(rhs_array[7] as u16),
          (self_array[8] as u16).wrapping_mul(rhs_array[8] as u16),
          (self_array[9] as u16).wrapping_mul(rhs_array[9] as u16),
          (self_array[10] as u16).wrapping_mul(rhs_array[10] as u16),
          (self_array[11] as u16).wrapping_mul(rhs_array[11] as u16),
          (self_array[12] as u16).wrapping_mul(rhs_array[12] as u16),
          (self_array[13] as u16).wrapping_mul(rhs_array[13] as u16),
          (self_array[14] as u16).wrapping_mul(rhs_array[14] as u16),
          (self_array[15] as u16).wrapping_mul(rhs_array[15] as u16),
        ];

        (
          Self::new([
            widening_mul[0] as u8,
            widening_mul[1] as u8,
            widening_mul[2] as u8,
            widening_mul[3] as u8,
            widening_mul[4] as u8,
            widening_mul[5] as u8,
            widening_mul[6] as u8,
            widening_mul[7] as u8,
            widening_mul[8] as u8,
            widening_mul[9] as u8,
            widening_mul[10] as u8,
            widening_mul[11] as u8,
            widening_mul[12] as u8,
            widening_mul[13] as u8,
            widening_mul[14] as u8,
            widening_mul[15] as u8,
          ]),
          Self::new([
            (widening_mul[0] >> 8) as u8,
            (widening_mul[1] >> 8) as u8,
            (widening_mul[2] >> 8) as u8,
            (widening_mul[3] >> 8) as u8,
            (widening_mul[4] >> 8) as u8,
            (widening_mul[5] >> 8) as u8,
            (widening_mul[6] >> 8) as u8,
            (widening_mul[7] >> 8) as u8,
            (widening_mul[8] >> 8) as u8,
            (widening_mul[9] >> 8) as u8,
            (widening_mul[10] >> 8) as u8,
            (widening_mul[11] >> 8) as u8,
            (widening_mul[12] >> 8) as u8,
            (widening_mul[13] >> 8) as u8,
            (widening_mul[14] >> 8) as u8,
            (widening_mul[15] >> 8) as u8,
          ]),
        )
      }
    }
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_u8_u16(
            vmull_u8(vget_low_u8(self.neon), vget_low_u8(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_u8_u16(
            vmull_u8(vget_high_u8(self.neon), vget_high_u8(rhs.neon)),
          );
          Self { neon: vuzpq_u8(low_wide_mul, high_wide_mul).1 }
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          ((self_array[0] as u16).wrapping_mul(rhs_array[0] as u16) >> 8) as u8,
          ((self_array[1] as u16).wrapping_mul(rhs_array[1] as u16) >> 8) as u8,
          ((self_array[2] as u16).wrapping_mul(rhs_array[2] as u16) >> 8) as u8,
          ((self_array[3] as u16).wrapping_mul(rhs_array[3] as u16) >> 8) as u8,
          ((self_array[4] as u16).wrapping_mul(rhs_array[4] as u16) >> 8) as u8,
          ((self_array[5] as u16).wrapping_mul(rhs_array[5] as u16) >> 8) as u8,
          ((self_array[6] as u16).wrapping_mul(rhs_array[6] as u16) >> 8) as u8,
          ((self_array[7] as u16).wrapping_mul(rhs_array[7] as u16) >> 8) as u8,
          ((self_array[8] as u16).wrapping_mul(rhs_array[8] as u16) >> 8) as u8,
          ((self_array[9] as u16).wrapping_mul(rhs_array[9] as u16) >> 8) as u8,
          ((self_array[10] as u16).wrapping_mul(rhs_array[10] as u16) >> 8) as u8,
          ((self_array[11] as u16).wrapping_mul(rhs_array[11] as u16) >> 8) as u8,
          ((self_array[12] as u16).wrapping_mul(rhs_array[12] as u16) >> 8) as u8,
          ((self_array[13] as u16).wrapping_mul(rhs_array[13] as u16) >> 8) as u8,
          ((self_array[14] as u16).wrapping_mul(rhs_array[14] as u16) >> 8) as u8,
          ((self_array[15] as u16).wrapping_mul(rhs_array[15] as u16) >> 8) as u8,
        ])
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`u8x16`], or only for
/// particular types inconsistently.
impl u8x16 {
  /// Returns `[lhs[0], rhs[0], lhs[1], rhs[1], ...]`, taking the first 8
  /// elements of each input and dropping their last 8 elements.
  #[inline]
  #[must_use]
  pub fn unpack_low(lhs: u8x16, rhs: u8x16) -> u8x16 {
    pick! {
        if #[cfg(target_feature = "sse2")] {
            u8x16 { sse: unpack_low_i8_m128i(lhs.sse, rhs.sse) }
        } else if #[cfg(target_feature = "simd128")] {
          u8x16 { simd: u8x16_shuffle::<0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23>(lhs.simd, rhs.simd) }
        } else if #[cfg(all(target_feature = "neon", target_arch = "aarch64"))] {
            let lhs = unsafe { vget_low_u8(lhs.neon) };
            let rhs = unsafe { vget_low_u8(rhs.neon) };

            let zipped = unsafe { vzip_u8(lhs, rhs) };
            u8x16 { neon: unsafe { vcombine_u8(zipped.0, zipped.1) } }
        } else {
            u8x16::new([
                lhs.as_array()[0], rhs.as_array()[0],
                lhs.as_array()[1], rhs.as_array()[1],
                lhs.as_array()[2], rhs.as_array()[2],
                lhs.as_array()[3], rhs.as_array()[3],
                lhs.as_array()[4], rhs.as_array()[4],
                lhs.as_array()[5], rhs.as_array()[5],
                lhs.as_array()[6], rhs.as_array()[6],
                lhs.as_array()[7], rhs.as_array()[7],
            ])
        }
    }
  }

  /// Returns `[lhs[8], rhs[8], lhs[9], rhs[9], ...]`, taking the last 8
  /// elements of each input and dropping their first 8 elements.
  #[inline]
  #[must_use]
  pub fn unpack_high(lhs: u8x16, rhs: u8x16) -> u8x16 {
    pick! {
        if #[cfg(target_feature = "sse2")] {
            u8x16 { sse: unpack_high_i8_m128i(lhs.sse, rhs.sse) }
        } else if #[cfg(target_feature = "simd128")] {
            u8x16 { simd: u8x16_shuffle::<8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31>(lhs.simd, rhs.simd) }
        } else if #[cfg(all(target_feature = "neon", target_arch = "aarch64"))] {
            let lhs = unsafe { vget_high_u8(lhs.neon) };
            let rhs = unsafe { vget_high_u8(rhs.neon) };

            let zipped = unsafe { vzip_u8(lhs, rhs) };
            u8x16 { neon: unsafe { vcombine_u8(zipped.0, zipped.1) } }
        } else {
            u8x16::new([
                lhs.as_array()[8], rhs.as_array()[8],
                lhs.as_array()[9], rhs.as_array()[9],
                lhs.as_array()[10], rhs.as_array()[10],
                lhs.as_array()[11], rhs.as_array()[11],
                lhs.as_array()[12], rhs.as_array()[12],
                lhs.as_array()[13], rhs.as_array()[13],
                lhs.as_array()[14], rhs.as_array()[14],
                lhs.as_array()[15], rhs.as_array()[15],
            ])
        }
    }
  }

  /// Treats two [`i16x8`] values as a single [`i16x16`] value, then converts
  /// each element from [`i16`] to [`u8`], saturating out of range values.
  #[inline]
  #[must_use]
  pub fn narrow_i16x8(lhs: i16x8, rhs: i16x8) -> Self {
    pick! {
        if #[cfg(target_feature = "sse2")] {
            u8x16 { sse: pack_i16_to_u8_m128i(lhs.sse, rhs.sse) }
        } else if #[cfg(target_feature = "simd128")] {
            u8x16 { simd: u8x16_narrow_i16x8(lhs.simd, rhs.simd) }
        } else if #[cfg(all(target_feature = "neon", target_arch = "aarch64"))] {
            let lhs = unsafe { vqmovun_s16(lhs.neon) };
            let rhs = unsafe { vqmovun_s16(rhs.neon) };
            u8x16 { neon: unsafe { vcombine_u8(lhs, rhs) } }
        } else {
            fn clamp(a: i16) -> u8 {
                  if a < u8::MIN as i16 {
                      u8::MIN
                  } else if a > u8::MAX as i16 {
                      u8::MAX
                  } else {
                      a as u8
                  }
            }

            Self { arr: [
                clamp(lhs.as_array()[0]),
                clamp(lhs.as_array()[1]),
                clamp(lhs.as_array()[2]),
                clamp(lhs.as_array()[3]),
                clamp(lhs.as_array()[4]),
                clamp(lhs.as_array()[5]),
                clamp(lhs.as_array()[6]),
                clamp(lhs.as_array()[7]),
                clamp(rhs.as_array()[0]),
                clamp(rhs.as_array()[1]),
                clamp(rhs.as_array()[2]),
                clamp(rhs.as_array()[3]),
                clamp(rhs.as_array()[4]),
                clamp(rhs.as_array()[5]),
                clamp(rhs.as_array()[6]),
                clamp(rhs.as_array()[7]),
            ]}
        }
    }
  }

  /// Returns a new vector where each element is based on the index values in
  /// `rhs`.
  ///
  /// * Index values in the range `[0, 15]` select the i-th element of `self`.
  /// * Index values that are out of range will cause that output lane to be
  ///   `0`.
  ///
  /// This function has been deprecated and replaced with [`shuffle_zeroing`].
  ///
  /// [`shuffle_zeroing`]: Self::shuffle_zeroing
  #[inline]
  #[deprecated(since = "1.7.0", note = "replaced with `shuffle_zeroing`")]
  pub fn swizzle(self, rhs: i8x16) -> i8x16 {
    self.shuffle_zeroing(rhs.cast_unsigned()).cast_signed()
  }

  /// Works like [`swizzle`](Self::swizzle) with the following additional
  /// details
  ///
  /// * Indices in the range `[0, 15]` will select the i-th element of `self`.
  /// * If the high bit of any index is set (meaning that the index is
  ///   negative), then the corresponding output lane is guaranteed to be zero.
  /// * Otherwise the output lane is either `0` or `self[rhs[i] % 16]`,
  ///   depending on the implementation.
  ///
  /// This function has been deprecated and replaced with [`shuffle`].
  ///
  /// [`shuffle`]: Self::shuffle
  #[inline]
  #[deprecated(since = "1.7.0", note = "replaced with `shuffle`")]
  pub fn swizzle_relaxed(self, rhs: u8x16) -> u8x16 {
    self.shuffle(rhs)
  }
}
