use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with eight elements of type [`i16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i16x8 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with eight elements of type [`i16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct i16x8 { pub(crate) simd: v128 }

    impl Default for i16x8 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i16x8 {
      fn eq(&self, other: &Self) -> bool {
        u16x8_all_true(i16x8_eq(self.simd, other.simd))
      }
    }

    impl Eq for i16x8 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with eight elements of type [`i16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct i16x8 { pub(crate) neon : int16x8_t }

    impl Default for i16x8 {
      #[inline]
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i16x8 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe { vminvq_u16(vceqq_s16(self.neon, other.neon))==u16::MAX }
      }
    }

    impl Eq for i16x8 { }
  } else {
    /// A SIMD vector with eight elements of type [`i16`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i16x8 { pub(crate) arr: [i16;8] }
  }
}

impl_simd_int! {
  unsafe {
    T = i16,
    N = 8,
    Simd = i16x8,
    UintSimd = u16x8,
    T_BITS = 16,
    T_BITS_MUL_2 = 32,
    BitmaskType = u32,
    [0, 1, 2, 3, 4, 5, 6, 7],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = int16x8_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_lt_mask_i16_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_lt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s16_u16(vcltq_s16(self.neon, rhs.neon)) }}
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
        ]}
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_gt_mask_i16_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_gt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s16_u16(vcgtq_s16(self.neon, rhs.neon)) }}
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
        Self { simd: i16x8_le(self.simd, rhs.simd) }
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
        Self { simd: i16x8_ge(self.simd, rhs.simd) }
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
        ]}
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u16x8) -> Self::Output {
    pick! {
      if #[cfg(all(target_feature="avx512bw", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm_srav_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm_srav_epi16;

        // Mask `rhs` to 15 to match `wrapping_shr`.
        let rhs = bitand_m128i(rhs.sse, set_splat_i16_m128i(15));
        // TODO(safe_arch): Add `_mm_srav_epi16`.
        cast(unsafe { _mm_srav_epi16(self.sse.0, rhs.0) })
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {
          // Mask `rhs` to 15 to match `wrapping_shr`, and negate it because
          // there is no shift-right intrinsic.
          let neg_rhs = vnegq_s16(vreinterpretq_s16_u16(vandq_u16(rhs.neon, vmovq_n_u16(15))));
          Self { neon: vshlq_s16(self.neon, neg_rhs) }
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          self_array[0].wrapping_shr(rhs_array[0] as u32),
          self_array[1].wrapping_shr(rhs_array[1] as u32),
          self_array[2].wrapping_shr(rhs_array[2] as u32),
          self_array[3].wrapping_shr(rhs_array[3] as u32),
          self_array[4].wrapping_shr(rhs_array[4] as u32),
          self_array[5].wrapping_shr(rhs_array[5] as u32),
          self_array[6].wrapping_shr(rhs_array[6] as u32),
          self_array[7].wrapping_shr(rhs_array[7] as u32),
        ])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Use `rhs % 16` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 15, 0]);
        Self { sse: shr_all_i16_m128i(self.sse, shift) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_shr(self.simd, rhs) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Use `rhs % 16` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        unsafe {Self { neon: vshlq_s16(self.neon, vmovq_n_s16( -(rhs as i16 & 15))) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_shr(rhs),
          self.arr[1].wrapping_shr(rhs),
          self.arr[2].wrapping_shr(rhs),
          self.arr[3].wrapping_shr(rhs),
          self.arr[4].wrapping_shr(rhs),
          self.arr[5].wrapping_shr(rhs),
          self.arr[6].wrapping_shr(rhs),
          self.arr[7].wrapping_shr(rhs),
        ]}
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: max_i16_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_max(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmaxq_s16(self.neon, rhs.neon) }}
      } else {
        self.simd_lt(rhs).select(rhs, self)
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: min_i16_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_min(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vminq_s16(self.neon, rhs.neon) }}
      } else {
        self.simd_lt(rhs).select(self, rhs)
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i16 {
    pick! {
        if #[cfg(target_feature="sse2")] {
          let hi64 = shuffle_ai_f32_all_m128i::<0b01_00_11_10>(self.sse);
          let sum64 = max_i16_m128i(self.sse, hi64);
          let hi32 = shuffle_ai_f32_all_m128i::<0b11_10_00_01>(sum64);
          let sum32 = max_i16_m128i(sum64, hi32);
          let lo16 = shr_imm_u32_m128i::<16>(sum32);
          let sum16 = max_i16_m128i(sum32, lo16);
          extract_i16_as_i32_m128i::<0>(sum16) as i16
        } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
          unsafe { vmaxvq_s16(self.neon) }
        } else {
        let arr: [i16; 8] = cast(self);

        // most boring implementation possible so optimizer doesn't overthink this
        let mut r = arr[0];
        r = r.max(arr[1]);
        r = r.max(arr[2]);
        r = r.max(arr[3]);
        r = r.max(arr[4]);
        r = r.max(arr[5]);
        r = r.max(arr[6]);
        r.max(arr[7])
      }
    }
  }

  #[inline]
  pub fn reduce_min(self) -> i16 {
    pick! {
        if #[cfg(target_feature="sse2")] {
          let hi64 = shuffle_ai_f32_all_m128i::<0b01_00_11_10>(self.sse);
          let sum64 = min_i16_m128i(self.sse, hi64);
          let hi32 = shuffle_ai_f32_all_m128i::<0b11_10_00_01>(sum64);
          let sum32 = min_i16_m128i(sum64, hi32);
          let lo16 = shr_imm_u32_m128i::<16>(sum32);
          let sum16 = min_i16_m128i(sum32, lo16);
          extract_i16_as_i32_m128i::<0>(sum16) as i16
        } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
          unsafe { vminvq_s16(self.neon) }
        } else {
        let arr: [i16; 8] = cast(self);

        // most boring implementation possible so optimizer doesn't overthink this
        let mut r = arr[0];
        r = r.min(arr[1]);
        r = r.min(arr[2]);
        r = r.min(arr[3]);
        r = r.min(arr[4]);
        r = r.min(arr[5]);
        r = r.min(arr[6]);
        r.min(arr[7])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u16x8) -> Self {
    pick! {
      if #[cfg(all(target_feature="avx512bw", target_feature="avx512vl"))] {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::_mm_srav_epi16;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::_mm_srav_epi16;

        // TODO(safe_arch): Add `_mm_srav_epi16`.
        cast(unsafe { _mm_srav_epi16(self.sse.0, rhs.sse.0) })
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_s16(self.neon, vnegq_s16(vreinterpretq_s16_u16(rhs.min(u16x8::splat(16)).neon))) }
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
        ])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: shr_all_i16_m128i(self.sse, cast([rhs as u64, 0])) }
      } else if #[cfg(target_feature="simd128")] {
        if rhs < 16 { Self { simd: i16x8_shr(self.simd, rhs) } } else { self.is_negative() }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_s16(self.neon, vmovq_n_s16(-rhs.min(16).cast_signed() as i16)) }
        }
      } else {
        Self {
          arr: [
            self.arr[0].unbounded_shr(rhs),
            self.arr[1].unbounded_shr(rhs),
            self.arr[2].unbounded_shr(rhs),
            self.arr[3].unbounded_shr(rhs),
            self.arr[4].unbounded_shr(rhs),
            self.arr[5].unbounded_shr(rhs),
            self.arr[6].unbounded_shr(rhs),
            self.arr[7].unbounded_shr(rhs),
          ]
        }
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: add_saturating_i16_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_add_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vqaddq_s16(self.neon, rhs.neon) }}
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
        ]}
      }
    }
  }

  #[inline]
  pub fn saturating_sub(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: sub_saturating_i16_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_sub_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_s16(self.neon, rhs.neon) } }
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
        ]}
      }
    }
  }

  #[inline]
  pub fn overflowing_mul(self, rhs: Self) -> (Self, Self) {
    let (low, high) = self.mul_keep_low_high(rhs);
    let low = cast::<u16x8, i16x8>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> i32x8 {
      pick! {
        if #[cfg(target_feature="avx2")] {
          let a = convert_to_i32_m256i_from_i16_m128i(self.sse);
          let b = convert_to_i32_m256i_from_i16_m128i(rhs.sse);
          i32x8 { avx2: mul_i32_keep_low_m256i(a,b) }
        } else if #[cfg(target_feature="sse2")] {
          let low = mul_i16_keep_low_m128i(self.sse, rhs.sse);
          let high = mul_i16_keep_high_m128i(self.sse, rhs.sse);
          i32x8 {
            a: i32x4 { sse:unpack_low_i16_m128i(low, high) },
            b: i32x4 { sse:unpack_high_i16_m128i(low, high) }
          }
        } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
          let lhs_low = unsafe { vget_low_s16(self.neon) };
          let rhs_low = unsafe { vget_low_s16(rhs.neon) };

          let lhs_high = unsafe { vget_high_s16(self.neon) };
          let rhs_high = unsafe { vget_high_s16(rhs.neon) };

          let low = unsafe { vmull_s16(lhs_low, rhs_low) };
          let high = unsafe { vmull_s16(lhs_high, rhs_high) };

          i32x8 { a: i32x4 { neon: low }, b: i32x4 {neon: high } }
        } else {
          let a = self.as_array();
          let b = rhs.as_array();

          i32x8::new([
            i32::from(a[0]) * i32::from(b[0]),
            i32::from(a[1]) * i32::from(b[1]),
            i32::from(a[2]) * i32::from(b[2]),
            i32::from(a[3]) * i32::from(b[3]),
            i32::from(a[4]) * i32::from(b[4]),
            i32::from(a[5]) * i32::from(b[5]),
            i32::from(a[6]) * i32::from(b[6]),
            i32::from(a[7]) * i32::from(b[7]),
          ])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u16x8, i16x8) {
    pick! {
      if #[cfg(target_feature="simd128")] {
        let low_wide_mul = i32x4_extmul_low_i16x8(self.simd, rhs.simd);
        let high_wide_mul = i32x4_extmul_high_i16x8(self.simd, rhs.simd);
        (
          u16x8 { simd: i16x8_shuffle::<0, 2, 4, 6, 8, 10, 12, 14>(low_wide_mul, high_wide_mul) },
          i16x8 { simd: i16x8_shuffle::<1, 3, 5, 7, 9, 11, 13, 15>(low_wide_mul, high_wide_mul) },
        )
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_s16_s32(
            vmull_s16(vget_low_s16(self.neon), vget_low_s16(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_s16_s32(
            vmull_s16(vget_high_s16(self.neon), vget_high_s16(rhs.neon)),
          );
          let low_high = vuzpq_s16(low_wide_mul, high_wide_mul);
          (
            u16x8 { neon: vreinterpretq_u16_s16(low_high.0) },
            i16x8 { neon: low_high.1 },
          )
        }
      } else {
        // TODO(perf): This implementation looks quite bad. Is there a better
        // one?

        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        let widening_mul = [
          (self_array[0] as i32).wrapping_mul(rhs_array[0] as i32),
          (self_array[1] as i32).wrapping_mul(rhs_array[1] as i32),
          (self_array[2] as i32).wrapping_mul(rhs_array[2] as i32),
          (self_array[3] as i32).wrapping_mul(rhs_array[3] as i32),
          (self_array[4] as i32).wrapping_mul(rhs_array[4] as i32),
          (self_array[5] as i32).wrapping_mul(rhs_array[5] as i32),
          (self_array[6] as i32).wrapping_mul(rhs_array[6] as i32),
          (self_array[7] as i32).wrapping_mul(rhs_array[7] as i32),
        ];

        (
          u16x8::new([
            widening_mul[0] as u16,
            widening_mul[1] as u16,
            widening_mul[2] as u16,
            widening_mul[3] as u16,
            widening_mul[4] as u16,
            widening_mul[5] as u16,
            widening_mul[6] as u16,
            widening_mul[7] as u16,
          ]),
          i16x8::new([
            (widening_mul[0] >> 16) as i16,
            (widening_mul[1] >> 16) as i16,
            (widening_mul[2] >> 16) as i16,
            (widening_mul[3] >> 16) as i16,
            (widening_mul[4] >> 16) as i16,
            (widening_mul[5] >> 16) as i16,
            (widening_mul[6] >> 16) as i16,
            (widening_mul[7] >> 16) as i16,
          ]),
        )
      }
    }
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: mul_i16_keep_high_m128i(self.sse, rhs.sse) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        let lhs_low = unsafe { vget_low_s16(self.neon) };
        let rhs_low = unsafe { vget_low_s16(rhs.neon) };

        let lhs_high = unsafe { vget_high_s16(self.neon) };
        let rhs_high = unsafe { vget_high_s16(rhs.neon) };

        let low = unsafe { vmull_s16(lhs_low, rhs_low) };
        let high = unsafe { vmull_s16(lhs_high, rhs_high) };

        Self {
          neon: unsafe {
            vreinterpretq_s16_u16(
              vuzpq_u16(vreinterpretq_u16_s32(low),
              vreinterpretq_u16_s32(high)).1
            )
          }
        }
      } else if #[cfg(target_feature="simd128")] {
        let low =  i32x4_extmul_low_i16x8(self.simd, rhs.simd);
        let high = i32x4_extmul_high_i16x8(self.simd, rhs.simd);

        Self { simd: i16x8_shuffle::<1, 3, 5, 7, 9, 11, 13, 15>(low, high) }
      } else {
        i16x8::new([
          ((i32::from(rhs.as_array()[0]) * i32::from(self.as_array()[0])) >> 16) as i16,
          ((i32::from(rhs.as_array()[1]) * i32::from(self.as_array()[1])) >> 16) as i16,
          ((i32::from(rhs.as_array()[2]) * i32::from(self.as_array()[2])) >> 16) as i16,
          ((i32::from(rhs.as_array()[3]) * i32::from(self.as_array()[3])) >> 16) as i16,
          ((i32::from(rhs.as_array()[4]) * i32::from(self.as_array()[4])) >> 16) as i16,
          ((i32::from(rhs.as_array()[5]) * i32::from(self.as_array()[5])) >> 16) as i16,
          ((i32::from(rhs.as_array()[6]) * i32::from(self.as_array()[6])) >> 16) as i16,
          ((i32::from(rhs.as_array()[7]) * i32::from(self.as_array()[7])) >> 16) as i16,
        ])
      }
    }
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        let mask = shr_imm_i16_m128i::<15>(self.sse);
        Self { sse: bitxor_m128i(add_i16_m128i(self.sse, mask), mask) }
      } else if #[cfg(target_feature="ssse3")] {
        Self { sse: abs_i16_m128i(self.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_abs(self.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vabsq_s16(self.neon) }}
      } else {
        let arr: [i16; 8] = cast(self);
        cast(
          [
            arr[0].wrapping_abs(),
            arr[1].wrapping_abs(),
            arr[2].wrapping_abs(),
            arr[3].wrapping_abs(),
            arr[4].wrapping_abs(),
            arr[5].wrapping_abs(),
            arr[6].wrapping_abs(),
            arr[7].wrapping_abs(),
          ])
      }
    }
  }

  #[inline]
  pub fn is_positive(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s16_u16(vcgtzq_s16(self.neon)) } }
      } else {
        self.simd_gt(Self::ZERO)
      }
    }
  }

  #[inline]
  pub fn is_negative(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s16_u16(vcltzq_s16(self.neon)) } }
      } else {
        self.simd_lt(Self::ZERO)
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`i16x8`], or only for
/// particular types inconsistently.
impl i16x8 {
  /// Converts the lower eight elements of `u` from [`u8`] to [`i16`], dropping
  /// the higher eight elements.
  #[inline]
  #[must_use]
  pub fn from_u8x16_low(u: u8x16) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self{ sse: unpack_low_i8_m128i(u.sse, m128i::zeroed()) }
      } else {
        let u_arr: [u8; 16] = cast(u);
        cast([
          u_arr[0] as u16 as i16,
          u_arr[1] as u16 as i16,
          u_arr[2] as u16 as i16,
          u_arr[3] as u16 as i16,
          u_arr[4] as u16 as i16,
          u_arr[5] as u16 as i16,
          u_arr[6] as u16 as i16,
          u_arr[7] as u16 as i16,
        ])
      }
    }
  }

  /// Converts the higher eight elements of `u` from [`u8`] to [`i16`], dropping
  /// the lower eight elements.
  #[inline]
  #[must_use]
  pub fn from_u8x16_high(u: u8x16) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self{ sse: unpack_high_i8_m128i(u.sse, m128i::zeroed()) }
      } else {
        let u_arr: [u8; 16] = cast(u);
        cast([
          u_arr[8] as u16 as i16,
          u_arr[9] as u16 as i16,
          u_arr[10] as u16 as i16,
          u_arr[11] as u16 as i16,
          u_arr[12] as u16 as i16,
          u_arr[13] as u16 as i16,
          u_arr[14] as u16 as i16,
          u_arr[15] as u16 as i16,
        ])
      }
    }
  }

  /// Converts each element from [`i32`] to [`i16`], saturating out of range
  /// values.
  #[inline]
  #[must_use]
  pub fn from_i32x8_saturate(v: i32x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        i16x8 { sse: pack_i32_to_i16_m128i( extract_m128i_from_m256i::<0>(v.avx2), extract_m128i_from_m256i::<1>(v.avx2))  }
      } else if #[cfg(target_feature="sse2")] {
        i16x8 { sse: pack_i32_to_i16_m128i( v.a.sse, v.b.sse ) }
      } else if #[cfg(target_feature="simd128")] {
        use core::arch::wasm32::*;

        i16x8 { simd: i16x8_narrow_i32x4(v.a.simd, v.b.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        use core::arch::aarch64::*;

        unsafe {
          i16x8 { neon: vcombine_s16(vqmovn_s32(v.a.neon), vqmovn_s32(v.b.neon)) }
        }
      } else {
        fn clamp(a : i32) -> i16 {
            if a < i16::MIN as i32 {
                i16::MIN
            }
            else if a > i16::MAX as i32 {
                i16::MAX
            } else {
                a as i16
            }
        }

        i16x8::new([
          clamp(v.as_array()[0]),
          clamp(v.as_array()[1]),
          clamp(v.as_array()[2]),
          clamp(v.as_array()[3]),
          clamp(v.as_array()[4]),
          clamp(v.as_array()[5]),
          clamp(v.as_array()[6]),
          clamp(v.as_array()[7]),
        ])
      }
    }
  }

  /// Converts each element from [`i32`] to [`i16`], truncating out of range
  /// values (behaves like [`as`] casting).
  ///
  /// [`as`]: https://doc.rust-lang.org/stable/reference/expressions/operator-expr.html#r-expr.as.numeric
  #[inline]
  #[must_use]
  pub fn from_i32x8_truncate(v: i32x8) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let a = v.avx2.bitand(set_splat_i32_m256i(0xffff));
        i16x8 { sse: pack_i32_to_u16_m128i( extract_m128i_from_m256i::<0>(a), extract_m128i_from_m256i::<1>(a) ) }
      } else if #[cfg(target_feature="sse2")] {
        let a = shr_imm_i32_m128i::<16>(shl_imm_u32_m128i::<16>(v.a.sse));
        let b = shr_imm_i32_m128i::<16>(shl_imm_u32_m128i::<16>(v.b.sse));

        i16x8 { sse: pack_i32_to_i16_m128i( a, b)  }
      } else {
      i16x8::new([
        v.as_array()[0] as i16,
        v.as_array()[1] as i16,
        v.as_array()[2] as i16,
        v.as_array()[3] as i16,
        v.as_array()[4] as i16,
        v.as_array()[5] as i16,
        v.as_array()[6] as i16,
        v.as_array()[7] as i16,
      ])
      }
    }
  }

  /// Converts a slice to a SIMD vector, ignoring elements beyond the first 8.
  ///
  /// # Panics
  ///
  /// Panics if `input` has less than 8 elements.
  #[inline]
  #[must_use]
  pub fn from_slice_unaligned(input: &[i16]) -> Self {
    assert!(input.len() >= 8);

    pick! {
      if #[cfg(target_feature="sse2")] {
        unsafe { Self { sse: load_unaligned_m128i( &*(input.as_ptr() as * const [u8;16]) ) } }
      } else if #[cfg(target_feature="simd128")] {
        unsafe { Self { simd: v128_load(input.as_ptr() as *const v128 ) } }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vld1q_s16( input.as_ptr() as *const i16 ) } }
      } else {
        // 2018 edition doesn't have try_into
        unsafe { Self::new( *(input.as_ptr() as * const [i16;8]) ) }
      }
    }
  }

  /// Partially computes the dot product.
  ///
  /// First this multiplies the input 16-bit integers, producing intermediate
  /// 32-bit integers. Then this horizontally adds adjacent pairs, resulting in
  /// four 32-bit integers.
  #[inline]
  #[must_use]
  pub fn dot(self, rhs: Self) -> i32x4 {
    pick! {
      if #[cfg(target_feature="sse2")] {
        i32x4 { sse:  mul_i16_horizontal_add_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        i32x4 { simd: i32x4_dot_i16x8(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          let pl = vmull_s16(vget_low_s16(self.neon),  vget_low_s16(rhs.neon));
          let ph = vmull_high_s16(self.neon, rhs.neon);
          i32x4 { neon: vpaddq_s32(pl, ph) }
        }
      } else {
        i32x4 { arr: [
          (i32::from(self.arr[0]) * i32::from(rhs.arr[0])) + (i32::from(self.arr[1]) * i32::from(rhs.arr[1])),
          (i32::from(self.arr[2]) * i32::from(rhs.arr[2])) + (i32::from(self.arr[3]) * i32::from(rhs.arr[3])),
          (i32::from(self.arr[4]) * i32::from(rhs.arr[4])) + (i32::from(self.arr[5]) * i32::from(rhs.arr[5])),
          (i32::from(self.arr[6]) * i32::from(rhs.arr[6])) + (i32::from(self.arr[7]) * i32::from(rhs.arr[7])),
        ] }
      }
    }
  }

  /// Multiply and scale equivalent to `((self * rhs) + 0x4000) >> 15` on each
  /// lane, effectively multiplying by a 16 bit fixed point number between `-1`
  /// and `1`. This corresponds to the following instructions:
  /// - `vqrdmulhq_s16` instruction on neon
  /// - `i16x8_q15mulr_sat` on simd128
  /// - `_mm_mulhrs_epi16` on ssse3
  /// - emulated via `mul_i16_*` on sse2
  #[inline]
  #[must_use]
  pub fn mul_scale_round(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="ssse3")] {
        Self { sse:  mul_i16_scale_round_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="sse2")] {
        // unfortunately mul_i16_scale_round_m128i only got added in sse3
        let hi = mul_i16_keep_high_m128i(self.sse, rhs.sse);
        let lo = mul_i16_keep_low_m128i(self.sse, rhs.sse);
        let mut v1 = unpack_low_i16_m128i(lo, hi);
        let mut v2 = unpack_high_i16_m128i(lo, hi);
        let a = set_splat_i32_m128i(0x4000);
        v1 = shr_imm_i32_m128i::<15>(add_i32_m128i(v1, a));
        v2 = shr_imm_i32_m128i::<15>(add_i32_m128i(v2, a));
        let s = pack_i32_to_i16_m128i(v1, v2);
        Self { sse: s }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_q15mulr_sat(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqrdmulhq_s16(self.neon, rhs.neon) } }
      } else {
        // compiler does a surprisingly good job of vectorizing this
        Self { arr: [
          ((i32::from(self.arr[0]) * i32::from(rhs.arr[0]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[1]) * i32::from(rhs.arr[1]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[2]) * i32::from(rhs.arr[2]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[3]) * i32::from(rhs.arr[3]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[4]) * i32::from(rhs.arr[4]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[5]) * i32::from(rhs.arr[5]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[6]) * i32::from(rhs.arr[6]) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[7]) * i32::from(rhs.arr[7]) + 0x4000) >> 15) as i16,
        ]}
      }
    }
  }

  #[inline]
  #[must_use]
  /// Multiply and scale, equivalent to `((self * rhs) + 0x4000) >> 15` on each
  /// lane, effectively multiplying by a 16 bit fixed point number between `-1`
  /// and `1`. This corresponds to the following instructions:
  /// - `vqrdmulhq_n_s16` instruction on neon
  /// - `i16x8_q15mulr_sat` on simd128
  /// - `_mm_mulhrs_epi16` on ssse3
  /// - emulated via `mul_i16_*` on sse2
  pub fn mul_scale_round_n(self, rhs: i16) -> Self {
    pick! {
      if #[cfg(target_feature="ssse3")] {
        Self { sse:  mul_i16_scale_round_m128i(self.sse, set_splat_i16_m128i(rhs)) }
      } else if #[cfg(target_feature="sse2")] {
        // unfortunately mul_i16_scale_round_m128i only got added in sse3
        let r = set_splat_i16_m128i(rhs);
        let hi = mul_i16_keep_high_m128i(self.sse, r);
        let lo = mul_i16_keep_low_m128i(self.sse, r);
        let mut v1 = unpack_low_i16_m128i(lo, hi);
        let mut v2 = unpack_high_i16_m128i(lo, hi);
        let a = set_splat_i32_m128i(0x4000);
        v1 = shr_imm_i32_m128i::<15>(add_i32_m128i(v1, a));
        v2 = shr_imm_i32_m128i::<15>(add_i32_m128i(v2, a));
        let s = pack_i32_to_i16_m128i(v1, v2);
        Self { sse: s }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i16x8_q15mulr_sat(self.simd, i16x8_splat(rhs)) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqrdmulhq_n_s16(self.neon, rhs) } }
      } else {
        // compiler does a surprisingly good job of vectorizing this
        Self { arr: [
          ((i32::from(self.arr[0]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[1]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[2]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[3]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[4]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[5]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[6]) * i32::from(rhs) + 0x4000) >> 15) as i16,
          ((i32::from(self.arr[7]) * i32::from(rhs) + 0x4000) >> 15) as i16,
        ]}
      }
    }
  }

  /// Widening multiplication. Computes `self * rhs`, widening to a SIMD
  /// vector of larger integers.
  ///
  /// The returned value is always exact and can never overflow.
  ///
  /// This function has been renamed to [`widening_mul`].
  ///
  /// [`widening_mul`]: Self::widening_mul
  #[inline]
  #[must_use]
  #[deprecated(since = "1.6.0", note = "renamed to `widening_mul`")]
  pub fn mul_widen(self, rhs: Self) -> i32x8 {
    self.widening_mul(rhs)
  }
}
