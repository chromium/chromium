use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with 16 elements of type [`i8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i8x16 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with 16 elements of type [`i8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct i8x16 { pub(crate) simd: v128 }

    impl Default for i8x16 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i8x16 {
      fn eq(&self, other: &Self) -> bool {
        u8x16_all_true(i8x16_eq(self.simd, other.simd))
      }
    }

    impl Eq for i8x16 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with 16 elements of type [`i8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct i8x16 { pub(crate) neon : int8x16_t }

    impl Default for i8x16 {
      #[inline]
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i8x16 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe { vminvq_u8(vceqq_s8(self.neon, other.neon))==u8::MAX }
      }
    }

    impl Eq for i8x16 { }
  } else {
    /// A SIMD vector with 16 elements of type [`i8`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i8x16 { arr: [i8;16] }
  }
}

impl_simd_int! {
  unsafe {
    T = i8,
    N = 16,
    Simd = i8x16,
    UintSimd = u8x16,
    T_BITS = 8,
    T_BITS_MUL_2 = 16,
    BitmaskType = u32,
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = int8x16_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_lt_mask_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_lt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s8_u8(vcltq_s8(self.neon, rhs.neon)) }}
      } else {
        Self { arr: [
          if self.arr[0] < rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] < rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] < rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] < rhs.arr[3] { -1 } else { 0 },
          if self.arr[4] < rhs.arr[4] { -1 } else { 0 },
          if self.arr[5] < rhs.arr[5] { -1 } else { 0 },
          if self.arr[6] < rhs.arr[6] { -1 } else { 0 },
          if self.arr[7] < rhs.arr[7] { -1 } else { 0 },
          if self.arr[8] < rhs.arr[8] { -1 } else { 0 },
          if self.arr[9] < rhs.arr[9] { -1 } else { 0 },
          if self.arr[10] < rhs.arr[10] { -1 } else { 0 },
          if self.arr[11] < rhs.arr[11] { -1 } else { 0 },
          if self.arr[12] < rhs.arr[12] { -1 } else { 0 },
          if self.arr[13] < rhs.arr[13] { -1 } else { 0 },
          if self.arr[14] < rhs.arr[14] { -1 } else { 0 },
          if self.arr[15] < rhs.arr[15] { -1 } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_gt_mask_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_gt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s8_u8(vcgtq_s8(self.neon, rhs.neon)) }}
      } else {
        Self { arr: [
          if self.arr[0] > rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] > rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] > rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] > rhs.arr[3] { -1 } else { 0 },
          if self.arr[4] > rhs.arr[4] { -1 } else { 0 },
          if self.arr[5] > rhs.arr[5] { -1 } else { 0 },
          if self.arr[6] > rhs.arr[6] { -1 } else { 0 },
          if self.arr[7] > rhs.arr[7] { -1 } else { 0 },
          if self.arr[8] > rhs.arr[8] { -1 } else { 0 },
          if self.arr[9] > rhs.arr[9] { -1 } else { 0 },
          if self.arr[10] > rhs.arr[10] { -1 } else { 0 },
          if self.arr[11] > rhs.arr[11] { -1 } else { 0 },
          if self.arr[12] > rhs.arr[12] { -1 } else { 0 },
          if self.arr[13] > rhs.arr[13] { -1 } else { 0 },
          if self.arr[14] > rhs.arr[14] { -1 } else { 0 },
          if self.arr[15] > rhs.arr[15] { -1 } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_le(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        !self.simd_gt(rhs)
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_le(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_gt(rhs)
      } else {
        Self { arr: [
          if self.arr[0] <= rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] <= rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] <= rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] <= rhs.arr[3] { -1 } else { 0 },
          if self.arr[4] <= rhs.arr[4] { -1 } else { 0 },
          if self.arr[5] <= rhs.arr[5] { -1 } else { 0 },
          if self.arr[6] <= rhs.arr[6] { -1 } else { 0 },
          if self.arr[7] <= rhs.arr[7] { -1 } else { 0 },
          if self.arr[8] <= rhs.arr[8] { -1 } else { 0 },
          if self.arr[9] <= rhs.arr[9] { -1 } else { 0 },
          if self.arr[10] <= rhs.arr[10] { -1 } else { 0 },
          if self.arr[11] <= rhs.arr[11] { -1 } else { 0 },
          if self.arr[12] <= rhs.arr[12] { -1 } else { 0 },
          if self.arr[13] <= rhs.arr[13] { -1 } else { 0 },
          if self.arr[14] <= rhs.arr[14] { -1 } else { 0 },
          if self.arr[15] <= rhs.arr[15] { -1 } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        !self.simd_lt(rhs)
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_ge(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_lt(rhs)
      } else {
        Self { arr: [
          if self.arr[0] >= rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] >= rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] >= rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] >= rhs.arr[3] { -1 } else { 0 },
          if self.arr[4] >= rhs.arr[4] { -1 } else { 0 },
          if self.arr[5] >= rhs.arr[5] { -1 } else { 0 },
          if self.arr[6] >= rhs.arr[6] { -1 } else { 0 },
          if self.arr[7] >= rhs.arr[7] { -1 } else { 0 },
          if self.arr[8] >= rhs.arr[8] { -1 } else { 0 },
          if self.arr[9] >= rhs.arr[9] { -1 } else { 0 },
          if self.arr[10] >= rhs.arr[10] { -1 } else { 0 },
          if self.arr[11] >= rhs.arr[11] { -1 } else { 0 },
          if self.arr[12] >= rhs.arr[12] { -1 } else { 0 },
          if self.arr[13] >= rhs.arr[13] { -1 } else { 0 },
          if self.arr[14] >= rhs.arr[14] { -1 } else { 0 },
          if self.arr[15] >= rhs.arr[15] { -1 } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u8x16) -> Self::Output {
    // For x86, this technically can be done explicitly by converting
    // to `i16` or `i32` then converting back after multiplication, but that may
    // not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Mask `rhs` to 7 to match `wrapping_shr`, and negate it because
          // there is no shift-right intrinsic.
          let neg_rhs = vnegq_s8(vreinterpretq_s8_u8(vandq_u8(rhs.neon, vmovq_n_u8(7))));
          Self { neon: vshlq_s8(self.neon, neg_rhs) }
        }
      } else {
        let self_array: [i8; 16] = cast(self);
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
    // to `i16` or `i32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(target_feature="simd128")] {
        // Mask `rhs` to 7 to match `wrapping_shr`.
        Self { simd: i8x16_shr(self.simd, rhs & 7) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Mask `rhs` to 7 to match `wrapping_shr`, and negate it because
        // there is no shift-right intrinsic.
        unsafe { Self { neon: vshlq_s8(self.neon, vmovq_n_s8(-(rhs as i8 & 7))) } }
      } else {
        let self_array = self.to_array();

        cast([
          self_array[0].wrapping_shr(rhs),
          self_array[1].wrapping_shr(rhs),
          self_array[2].wrapping_shr(rhs),
          self_array[3].wrapping_shr(rhs),
          self_array[4].wrapping_shr(rhs),
          self_array[5].wrapping_shr(rhs),
          self_array[6].wrapping_shr(rhs),
          self_array[7].wrapping_shr(rhs),
          self_array[8].wrapping_shr(rhs),
          self_array[9].wrapping_shr(rhs),
          self_array[10].wrapping_shr(rhs),
          self_array[11].wrapping_shr(rhs),
          self_array[12].wrapping_shr(rhs),
          self_array[13].wrapping_shr(rhs),
          self_array[14].wrapping_shr(rhs),
          self_array[15].wrapping_shr(rhs),
        ])
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: max_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_max(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmaxq_s8(self.neon, rhs.neon) }}
      } else {
        self.simd_lt(rhs).select(rhs, self)
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: min_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_min(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vminq_s8(self.neon, rhs.neon) }}
      } else {
        self.simd_lt(rhs).select(self, rhs)
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i8 {
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
      if #[cfg(all(target_feature="ssse3", target_feature="sse4.1"))] {
        let rhs = shuffle_av_i8z_all_m128i(self.sse, m128i::from(SHUFFLE_1));
        let max = max_i8_m128i(self.sse, rhs);
        let rhs = shuffle_av_i8z_all_m128i(max, m128i::from(SHUFFLE_2));
        let max = max_i8_m128i(max, rhs);
        let rhs = shuffle_av_i8z_all_m128i(max, m128i::from(SHUFFLE_3));
        let max = max_i8_m128i(max, rhs);
        let rhs = shuffle_av_i8z_all_m128i(max, m128i::from(SHUFFLE_4));
        let max = max_i8_m128i(max, rhs);
        get_i32_from_m128i_s(max) as i8
      } else if #[cfg(target_feature="simd128")] {
        let rhs = i8x16_shuffle::<8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7>(self.simd, self.simd);
        let max = i8x16_max(self.simd, rhs);
        let rhs = i8x16_shuffle::<4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0>(max, max);
        let max = i8x16_max(max, rhs);
        let rhs = i8x16_shuffle::<2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(max, max);
        let max = i8x16_max(max, rhs);
        let rhs = i8x16_shuffle::<1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(max, max);
        let max = i8x16_max(max, rhs);
        i8x16_extract_lane::<0>(max)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Use `transmute` instead of `cast` because `int8x16_t` does not
          // implement `bytemuck::Pod`.
          let rhs = vqtbl1q_s8(self.neon, core::mem::transmute(SHUFFLE_1));
          let max = vmaxq_s8(self.neon, rhs);
          let rhs = vqtbl1q_s8(max, core::mem::transmute(SHUFFLE_2));
          let max = vmaxq_s8(max, rhs);
          let rhs = vqtbl1q_s8(max, core::mem::transmute(SHUFFLE_3));
          let max = vmaxq_s8(max, rhs);
          let rhs = vqtbl1q_s8(max, core::mem::transmute(SHUFFLE_4));
          let max = vmaxq_s8(max, rhs);
          vgetq_lane_s8(max, 0)
        }
      } else {
        let array: [i8; 16] = cast(self);
        array.into_iter().reduce(i8::max).unwrap()
      }
    }
  }

  #[inline]
  pub fn reduce_min(self) -> i8 {
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
      if #[cfg(all(target_feature="ssse3", target_feature="sse4.1"))] {
        let rhs = shuffle_av_i8z_all_m128i(self.sse, m128i::from(SHUFFLE_1));
        let min = min_i8_m128i(self.sse, rhs);
        let rhs = shuffle_av_i8z_all_m128i(min, m128i::from(SHUFFLE_2));
        let min = min_i8_m128i(min, rhs);
        let rhs = shuffle_av_i8z_all_m128i(min, m128i::from(SHUFFLE_3));
        let min = min_i8_m128i(min, rhs);
        let rhs = shuffle_av_i8z_all_m128i(min, m128i::from(SHUFFLE_4));
        let min = min_i8_m128i(min, rhs);
        get_i32_from_m128i_s(min) as i8
      } else if #[cfg(target_feature="simd128")] {
        let rhs = i8x16_shuffle::<8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7>(self.simd, self.simd);
        let min = i8x16_min(self.simd, rhs);
        let rhs = i8x16_shuffle::<4, 5, 6, 7, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0>(min, min);
        let min = i8x16_min(min, rhs);
        let rhs = i8x16_shuffle::<2, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(min, min);
        let min = i8x16_min(min, rhs);
        let rhs = i8x16_shuffle::<1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(min, min);
        let min = i8x16_min(min, rhs);
        i8x16_extract_lane::<0>(min)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Use `transmute` instead of `cast` because `int8x16_t` does not
          // implement `bytemuck::Pod`.
          let rhs = vqtbl1q_s8(self.neon, core::mem::transmute(SHUFFLE_1));
          let min = vminq_s8(self.neon, rhs);
          let rhs = vqtbl1q_s8(min, core::mem::transmute(SHUFFLE_2));
          let min = vminq_s8(min, rhs);
          let rhs = vqtbl1q_s8(min, core::mem::transmute(SHUFFLE_3));
          let min = vminq_s8(min, rhs);
          let rhs = vqtbl1q_s8(min, core::mem::transmute(SHUFFLE_4));
          let min = vminq_s8(min, rhs);
          vgetq_lane_s8(min, 0)
        }
      } else {
        let array: [i8; 16] = cast(self);
        array.into_iter().reduce(i8::min).unwrap()
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u8x16) -> Self {
    // For x86, this technically can be done explicitly by converting
    // to `i16` or `i32` then converting back after multiplication, but that may
    // not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          let neg_rhs = vnegq_s8(vreinterpretq_s8_u8(rhs.min(u8x16::splat(8)).neon));
          Self { neon: vshlq_s8(self.neon, neg_rhs) }
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
    // to `i16` or `i32` then converting back after multiplication, but that
    // may not actually be faster than auto-vectorization.
    pick! {
      if #[cfg(target_feature="simd128")] {
        if rhs < 8 { Self { simd: i8x16_shr(self.simd, rhs) } } else { self.is_negative() }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_s8(self.neon, vmovq_n_s8(-rhs.min(8).cast_signed() as i8)) }
        }
      } else {
        let self_array = self.to_array();

        cast([
          self_array[0].unbounded_shr(rhs),
          self_array[1].unbounded_shr(rhs),
          self_array[2].unbounded_shr(rhs),
          self_array[3].unbounded_shr(rhs),
          self_array[4].unbounded_shr(rhs),
          self_array[5].unbounded_shr(rhs),
          self_array[6].unbounded_shr(rhs),
          self_array[7].unbounded_shr(rhs),
          self_array[8].unbounded_shr(rhs),
          self_array[9].unbounded_shr(rhs),
          self_array[10].unbounded_shr(rhs),
          self_array[11].unbounded_shr(rhs),
          self_array[12].unbounded_shr(rhs),
          self_array[13].unbounded_shr(rhs),
          self_array[14].unbounded_shr(rhs),
          self_array[15].unbounded_shr(rhs),
        ])
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: add_saturating_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_add_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vqaddq_s8(self.neon, rhs.neon) }}
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
        Self { sse: sub_saturating_i8_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_sub_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_s8(self.neon, rhs.neon) } }
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
    let low = cast::<u8x16, i8x16>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> i16x16 {
      pick! {
        if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
          unsafe {
            let low_wide_mul = vmull_s8(vget_low_s8(self.neon), vget_low_s8(rhs.neon));
            let high_wide_mul = vmull_s8(vget_high_s8(self.neon), vget_high_s8(rhs.neon));

            i16x16 {
              a: i16x8 { neon: low_wide_mul },
              b: i16x8 { neon: high_wide_mul },
            }
          }
        } else {
          let self_array = self.to_array();
          let rhs_array = rhs.to_array();

          i16x16::new([
            (self_array[0] as i16).wrapping_mul(rhs_array[0] as i16),
            (self_array[1] as i16).wrapping_mul(rhs_array[1] as i16),
            (self_array[2] as i16).wrapping_mul(rhs_array[2] as i16),
            (self_array[3] as i16).wrapping_mul(rhs_array[3] as i16),
            (self_array[4] as i16).wrapping_mul(rhs_array[4] as i16),
            (self_array[5] as i16).wrapping_mul(rhs_array[5] as i16),
            (self_array[6] as i16).wrapping_mul(rhs_array[6] as i16),
            (self_array[7] as i16).wrapping_mul(rhs_array[7] as i16),
            (self_array[8] as i16).wrapping_mul(rhs_array[8] as i16),
            (self_array[9] as i16).wrapping_mul(rhs_array[9] as i16),
            (self_array[10] as i16).wrapping_mul(rhs_array[10] as i16),
            (self_array[11] as i16).wrapping_mul(rhs_array[11] as i16),
            (self_array[12] as i16).wrapping_mul(rhs_array[12] as i16),
            (self_array[13] as i16).wrapping_mul(rhs_array[13] as i16),
            (self_array[14] as i16).wrapping_mul(rhs_array[14] as i16),
            (self_array[15] as i16).wrapping_mul(rhs_array[15] as i16),
          ])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u8x16, i8x16) {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_s8_s16(
            vmull_s8(vget_low_s8(self.neon), vget_low_s8(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_s8_s16(
            vmull_s8(vget_high_s8(self.neon), vget_high_s8(rhs.neon)),
          );
          let low_high = vuzpq_s8(low_wide_mul, high_wide_mul);

          (
            u8x16 { neon: vreinterpretq_u8_s8(low_high.0) },
            i8x16 { neon: low_high.1 },
          )
        }
      } else {
        // TODO(perf): This implementation looks quite bad. Is there a better
        // one?

        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        let widening_mul = [
          (self_array[0] as i16).wrapping_mul(rhs_array[0] as i16),
          (self_array[1] as i16).wrapping_mul(rhs_array[1] as i16),
          (self_array[2] as i16).wrapping_mul(rhs_array[2] as i16),
          (self_array[3] as i16).wrapping_mul(rhs_array[3] as i16),
          (self_array[4] as i16).wrapping_mul(rhs_array[4] as i16),
          (self_array[5] as i16).wrapping_mul(rhs_array[5] as i16),
          (self_array[6] as i16).wrapping_mul(rhs_array[6] as i16),
          (self_array[7] as i16).wrapping_mul(rhs_array[7] as i16),
          (self_array[8] as i16).wrapping_mul(rhs_array[8] as i16),
          (self_array[9] as i16).wrapping_mul(rhs_array[9] as i16),
          (self_array[10] as i16).wrapping_mul(rhs_array[10] as i16),
          (self_array[11] as i16).wrapping_mul(rhs_array[11] as i16),
          (self_array[12] as i16).wrapping_mul(rhs_array[12] as i16),
          (self_array[13] as i16).wrapping_mul(rhs_array[13] as i16),
          (self_array[14] as i16).wrapping_mul(rhs_array[14] as i16),
          (self_array[15] as i16).wrapping_mul(rhs_array[15] as i16),
        ];

        (
          u8x16::new([
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
          i8x16::new([
            (widening_mul[0] >> 8) as i8,
            (widening_mul[1] >> 8) as i8,
            (widening_mul[2] >> 8) as i8,
            (widening_mul[3] >> 8) as i8,
            (widening_mul[4] >> 8) as i8,
            (widening_mul[5] >> 8) as i8,
            (widening_mul[6] >> 8) as i8,
            (widening_mul[7] >> 8) as i8,
            (widening_mul[8] >> 8) as i8,
            (widening_mul[9] >> 8) as i8,
            (widening_mul[10] >> 8) as i8,
            (widening_mul[11] >> 8) as i8,
            (widening_mul[12] >> 8) as i8,
            (widening_mul[13] >> 8) as i8,
            (widening_mul[14] >> 8) as i8,
            (widening_mul[15] >> 8) as i8,
          ])
        )
      }
    }
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_s8_s16(
            vmull_s8(vget_low_s8(self.neon), vget_low_s8(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_s8_s16(
            vmull_s8(vget_high_s8(self.neon), vget_high_s8(rhs.neon)),
          );

          Self { neon: vuzpq_s8(low_wide_mul, high_wide_mul).1 }
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          ((self_array[0] as i16).wrapping_mul(rhs_array[0] as i16) >> 8) as i8,
          ((self_array[1] as i16).wrapping_mul(rhs_array[1] as i16) >> 8) as i8,
          ((self_array[2] as i16).wrapping_mul(rhs_array[2] as i16) >> 8) as i8,
          ((self_array[3] as i16).wrapping_mul(rhs_array[3] as i16) >> 8) as i8,
          ((self_array[4] as i16).wrapping_mul(rhs_array[4] as i16) >> 8) as i8,
          ((self_array[5] as i16).wrapping_mul(rhs_array[5] as i16) >> 8) as i8,
          ((self_array[6] as i16).wrapping_mul(rhs_array[6] as i16) >> 8) as i8,
          ((self_array[7] as i16).wrapping_mul(rhs_array[7] as i16) >> 8) as i8,
          ((self_array[8] as i16).wrapping_mul(rhs_array[8] as i16) >> 8) as i8,
          ((self_array[9] as i16).wrapping_mul(rhs_array[9] as i16) >> 8) as i8,
          ((self_array[10] as i16).wrapping_mul(rhs_array[10] as i16) >> 8) as i8,
          ((self_array[11] as i16).wrapping_mul(rhs_array[11] as i16) >> 8) as i8,
          ((self_array[12] as i16).wrapping_mul(rhs_array[12] as i16) >> 8) as i8,
          ((self_array[13] as i16).wrapping_mul(rhs_array[13] as i16) >> 8) as i8,
          ((self_array[14] as i16).wrapping_mul(rhs_array[14] as i16) >> 8) as i8,
          ((self_array[15] as i16).wrapping_mul(rhs_array[15] as i16) >> 8) as i8,
        ])
      }
    }
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="ssse3")] {
        Self { sse: abs_i8_m128i(self.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i8x16_abs(self.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vabsq_s8(self.neon) }}
      } else {
        let arr: [i8; 16] = cast(self);
        cast([
          arr[0].wrapping_abs(),
          arr[1].wrapping_abs(),
          arr[2].wrapping_abs(),
          arr[3].wrapping_abs(),
          arr[4].wrapping_abs(),
          arr[5].wrapping_abs(),
          arr[6].wrapping_abs(),
          arr[7].wrapping_abs(),
          arr[8].wrapping_abs(),
          arr[9].wrapping_abs(),
          arr[10].wrapping_abs(),
          arr[11].wrapping_abs(),
          arr[12].wrapping_abs(),
          arr[13].wrapping_abs(),
          arr[14].wrapping_abs(),
          arr[15].wrapping_abs(),
        ])
      }
    }
  }

  #[inline]
  pub fn is_positive(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s8_u8(vcgtzq_s8(self.neon)) } }
      } else {
        self.simd_gt(Self::ZERO)
      }
    }
  }

  #[inline]
  pub fn is_negative(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s8_u8(vcltzq_s8(self.neon)) } }
      } else {
        self.simd_lt(Self::ZERO)
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`i8x16`], or only for
/// particular types inconsistently.
impl i8x16 {
  /// Converts each element from [`i16`] to [`i8`], saturating out of range
  /// values.
  #[inline]
  #[must_use]
  pub fn from_i16x16_saturate(v: i16x16) -> i8x16 {
    pick! {
      if #[cfg(target_feature="avx2")] {
        i8x16 { sse: pack_i16_to_i8_m128i( extract_m128i_from_m256i::<0>(v.avx2), extract_m128i_from_m256i::<1>(v.avx2))  }
      } else if #[cfg(target_feature="sse2")] {
        i8x16 { sse: pack_i16_to_i8_m128i( v.a.sse, v.b.sse ) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        use core::arch::aarch64::*;

        unsafe {
          i8x16 { neon: vcombine_s8(vqmovn_s16(v.a.neon), vqmovn_s16(v.b.neon)) }
        }
      } else if #[cfg(target_feature="simd128")] {
        use core::arch::wasm32::*;

        i8x16 { simd: i8x16_narrow_i16x8(v.a.simd, v.b.simd) }
      } else {
        fn clamp(a : i16) -> i8 {
            if a < i8::MIN as i16 {
              i8::MIN
            }
            else if a > i8::MAX as i16 {
              i8::MAX
            } else {
                a as i8
            }
        }

        i8x16::new([
          clamp(v.as_array()[0]),
          clamp(v.as_array()[1]),
          clamp(v.as_array()[2]),
          clamp(v.as_array()[3]),
          clamp(v.as_array()[4]),
          clamp(v.as_array()[5]),
          clamp(v.as_array()[6]),
          clamp(v.as_array()[7]),
          clamp(v.as_array()[8]),
          clamp(v.as_array()[9]),
          clamp(v.as_array()[10]),
          clamp(v.as_array()[11]),
          clamp(v.as_array()[12]),
          clamp(v.as_array()[13]),
          clamp(v.as_array()[14]),
          clamp(v.as_array()[15]),
        ])
      }
    }
  }

  /// Converts each element from [`i16`] to [`i8`], truncating out of range
  /// values (behaves like [`as`] casting).
  ///
  /// [`as`]: https://doc.rust-lang.org/stable/reference/expressions/operator-expr.html#r-expr.as.numeric
  #[inline]
  #[must_use]
  pub fn from_i16x16_truncate(v: i16x16) -> i8x16 {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let a = v.avx2.bitand(set_splat_i16_m256i(0xff));
        i8x16 { sse: pack_i16_to_u8_m128i( extract_m128i_from_m256i::<0>(a), extract_m128i_from_m256i::<1>(a))  }
      } else if #[cfg(target_feature="sse2")] {
        let mask = set_splat_i16_m128i(0xff);
        i8x16 { sse: pack_i16_to_u8_m128i( v.a.sse.bitand(mask), v.b.sse.bitand(mask) ) }
      } else {
        // no super good intrinsics on other platforms... plain old codegen does a reasonable job
        i8x16::new([
          v.as_array()[0] as i8,
          v.as_array()[1] as i8,
          v.as_array()[2] as i8,
          v.as_array()[3] as i8,
          v.as_array()[4] as i8,
          v.as_array()[5] as i8,
          v.as_array()[6] as i8,
          v.as_array()[7] as i8,
          v.as_array()[8] as i8,
          v.as_array()[9] as i8,
          v.as_array()[10] as i8,
          v.as_array()[11] as i8,
          v.as_array()[12] as i8,
          v.as_array()[13] as i8,
          v.as_array()[14] as i8,
          v.as_array()[15] as i8,
        ])
      }
    }
  }

  /// Converts a slice to a SIMD vector, ignoring elements beyond the first 16.
  ///
  /// # Panics
  ///
  /// Panics if `input` has less than 16 elements.
  #[inline]
  #[must_use]
  pub fn from_slice_unaligned(input: &[i8]) -> Self {
    assert!(input.len() >= 16);

    pick! {
      if #[cfg(target_feature="sse2")] {
        unsafe { Self { sse: load_unaligned_m128i( &*(input.as_ptr() as * const [u8;16]) ) } }
      } else if #[cfg(target_feature="simd128")] {
        unsafe { Self { simd: v128_load(input.as_ptr() as *const v128 ) } }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vld1q_s8( input.as_ptr() as *const i8 ) } }
      } else {
        // 2018 edition doesn't have try_into
        unsafe { Self::new( *(input.as_ptr() as * const [i8;16]) ) }
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
    self.shuffle_zeroing(rhs.cast_unsigned())
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
  pub fn swizzle_relaxed(self, rhs: i8x16) -> i8x16 {
    self.shuffle(rhs.cast_unsigned())
  }
}
