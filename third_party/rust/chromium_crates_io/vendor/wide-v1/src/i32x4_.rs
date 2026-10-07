use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with four elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i32x4 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with four elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct i32x4 { pub(crate) simd: v128 }

    impl Default for i32x4 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i32x4 {
      fn eq(&self, other: &Self) -> bool {
        u32x4_all_true(i32x4_eq(self.simd, other.simd))
      }
    }

    impl Eq for i32x4 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with four elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct i32x4 { pub(crate) neon : int32x4_t }

    impl Default for i32x4 {
      #[inline]
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for i32x4 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe { vminvq_u32(vceqq_s32(self.neon, other.neon))==u32::MAX }
      }
    }

    impl Eq for i32x4 { }
  } else {
    /// A SIMD vector with four elements of type [`i32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct i32x4 { pub(crate) arr: [i32;4] }
  }
}

impl_simd_int! {
  unsafe {
    T = i32,
    N = 4,
    Simd = i32x4,
    UintSimd = u32x4,
    T_BITS = 32,
    T_BITS_MUL_2 = 64,
    BitmaskType = u32,
    [0, 1, 2, 3],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = int32x4_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn simd_lt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_lt_mask_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i32x4_lt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s32_u32(vcltq_s32(self.neon, rhs.neon)) }}
      } else {
        Self { arr: [
          if self.arr[0] < rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] < rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] < rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] < rhs.arr[3] { -1 } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn simd_gt(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_gt_mask_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i32x4_gt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vreinterpretq_s32_u32(vcgtq_s32(self.neon, rhs.neon)) }}
      } else {
        Self { arr: [
          if self.arr[0] > rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] > rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] > rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] > rhs.arr[3] { -1 } else { 0 },
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
        Self { simd: i32x4_le(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_gt(rhs)
      } else {
        Self { arr: [
          if self.arr[0] <= rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] <= rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] <= rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] <= rhs.arr[3] { -1 } else { 0 },
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
        Self { simd: i32x4_ge(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_lt(rhs)
      } else {
        Self { arr: [
          if self.arr[0] >= rhs.arr[0] { -1 } else { 0 },
          if self.arr[1] >= rhs.arr[1] { -1 } else { 0 },
          if self.arr[2] >= rhs.arr[2] { -1 } else { 0 },
          if self.arr[3] >= rhs.arr[3] { -1 } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32x4) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // mask the shift count to 31 to have same behavior on all platforms
        let shift_by = bitand_m128i(rhs.sse, set_splat_i32_m128i(31));
        Self { sse: shr_each_i32_m128i(self.sse, shift_by) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // mask the shift count to 31 to have same behavior on all platforms
          // no right shift, have to pass negative value to left shift on neon
          let shift_by = vnegq_s32(vreinterpretq_s32_u32(vandq_u32(rhs.neon, vmovq_n_u32(31))));
          Self { neon: vshlq_s32(self.neon, shift_by) }
        }
      } else {
        let arr: [i32; 4] = cast(self);
        let rhs: [u32; 4] = cast(rhs);
        cast([
          arr[0].wrapping_shr(rhs[0]),
          arr[1].wrapping_shr(rhs[1]),
          arr[2].wrapping_shr(rhs[2]),
          arr[3].wrapping_shr(rhs[3]),
        ])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 31, 0]);
        Self { sse: shr_all_i32_m128i(self.sse, shift) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i32x4_shr(self.simd, rhs) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        unsafe {Self { neon: vshlq_s32(self.neon, vmovq_n_s32( -(rhs as i32 & 31))) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_shr(rhs),
          self.arr[1].wrapping_shr(rhs),
          self.arr[2].wrapping_shr(rhs),
          self.arr[3].wrapping_shr(rhs),
        ]}
      }
    }
  }

  #[inline]
  pub fn max(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: max_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i32x4_max(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmaxq_s32(self.neon, rhs.neon) }}
      } else {
        self.simd_lt(rhs).select(rhs, self)
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: min_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i32x4_min(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vminq_s32(self.neon, rhs.neon) }}
      } else {
        self.simd_lt(rhs).select(self, rhs)
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> i32 {
    let arr: [i32; 4] = cast(self);
    arr[0].max(arr[1]).max(arr[2].max(arr[3]))
  }

  #[inline]
  pub fn reduce_min(self) -> i32 {
    let arr: [i32; 4] = cast(self);
    arr[0].min(arr[1]).min(arr[2].min(arr[3]))
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: u32x4) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { sse: shr_each_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_s32(self.neon, vnegq_s32(vreinterpretq_s32_u32(rhs.min(u32x4::splat(32)).neon))) }
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          self_array[0].unbounded_shr(rhs_array[0]),
          self_array[1].unbounded_shr(rhs_array[1]),
          self_array[2].unbounded_shr(rhs_array[2]),
          self_array[3].unbounded_shr(rhs_array[3]),
        ])
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: shr_all_i32_m128i(self.sse, cast([rhs as u64, 0])) }
      } else if #[cfg(target_feature="simd128")] {
        if rhs < 32 { Self { simd: i32x4_shr(self.simd, rhs) } } else { self.is_negative() }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_s32(self.neon, vmovq_n_s32(-rhs.min(32).cast_signed())) }
        }
      } else {
        Self {
          arr: [
            self.arr[0].unbounded_shr(rhs),
            self.arr[1].unbounded_shr(rhs),
            self.arr[2].unbounded_shr(rhs),
            self.arr[3].unbounded_shr(rhs),
          ]
        }
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let result = self + rhs;
        let overflow = (!(self ^ rhs) & (self ^ result)).is_negative();
        let negative = self.is_negative();

        // If overflow occurs return `MAX` if positive or `MIN` if negative.
        overflow.select(Self::MAX ^ negative, result)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqaddq_s32(self.neon, rhs.neon) } }
      } else {
        Self {
          arr: [
            self.arr[0].saturating_add(rhs.arr[0]),
            self.arr[1].saturating_add(rhs.arr[1]),
            self.arr[2].saturating_add(rhs.arr[2]),
            self.arr[3].saturating_add(rhs.arr[3]),
          ],
        }
      }
    }
  }

  #[inline]
  pub fn saturating_sub(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let result = self - rhs;
        let overflow = ((self ^ rhs) & (self ^ result)).is_negative();
        let negative = self.is_negative();

        // If overflow occurs return `MAX` if positive or `MIN` if negative.
        overflow.select(Self::MAX ^ negative, result)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_s32(self.neon, rhs.neon) } }
      } else {
        Self {
          arr: [
            self.arr[0].saturating_sub(rhs.arr[0]),
            self.arr[1].saturating_sub(rhs.arr[1]),
            self.arr[2].saturating_sub(rhs.arr[2]),
            self.arr[3].saturating_sub(rhs.arr[3]),
          ],
        }
      }
    }
  }

  #[inline]
  pub fn overflowing_mul(self, rhs: Self) -> (Self, Self) {
    let (low, high) = self.mul_keep_low_high(rhs);
    let low = cast::<u32x4, i32x4>(low);

    let overflow = high.simd_ne(low.is_negative());
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> i64x4 {
      pick! {
        if #[cfg(target_feature="avx2")] {
          let a = convert_to_i64_m256i_from_i32_m128i(self.sse);
          let b = convert_to_i64_m256i_from_i32_m128i(rhs.sse);
          cast(mul_i64_low_bits_m256i(a, b))
        } else if #[cfg(target_feature="sse4.1")] {
            let evenp = mul_widen_i32_odd_m128i(self.sse, rhs.sse);

            let oddp = mul_widen_i32_odd_m128i(
              shr_imm_u64_m128i::<32>(self.sse),
              shr_imm_u64_m128i::<32>(rhs.sse));

            i64x4 {
              a: i64x2 { sse: unpack_low_i64_m128i(evenp, oddp)},
              b: i64x2 { sse: unpack_high_i64_m128i(evenp, oddp)}
            }
        } else if #[cfg(target_feature="simd128")] {
            i64x4 {
              a: i64x2 { simd: i64x2_extmul_low_i32x4(self.simd, rhs.simd) },
              b: i64x2 { simd: i64x2_extmul_high_i32x4(self.simd, rhs.simd) },
            }
        } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
          unsafe {
            i64x4 { a: i64x2 { neon: vmull_s32(vget_low_s32(self.neon), vget_low_s32(rhs.neon)) },
                    b: i64x2 { neon: vmull_s32(vget_high_s32(self.neon), vget_high_s32(rhs.neon)) } }
          }
        } else {
          let a = self.as_array();
          let b = rhs.as_array();

          cast([
            i64::from(a[0]) * i64::from(b[0]),
            i64::from(a[1]) * i64::from(b[1]),
            i64::from(a[2]) * i64::from(b[2]),
            i64::from(a[3]) * i64::from(b[3]),
          ])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (u32x4, i32x4) {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        let even_wide_mul = mul_widen_i32_odd_m128i(self.sse, rhs.sse);
        let odd_wide_mul = mul_widen_i32_odd_m128i(
          shuffle_ai_f32_all_m128i::<0b_00_11_00_01>(self.sse),
          shuffle_ai_f32_all_m128i::<0b_00_11_00_01>(rhs.sse),
        );
        let ll_hh_1 = unpack_low_i32_m128i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m128i(even_wide_mul, odd_wide_mul);

        (
          u32x4 { sse: unpack_low_i64_m128i(ll_hh_1, ll_hh_2) },
          i32x4 { sse: unpack_high_i64_m128i(ll_hh_1, ll_hh_2) },
        )
      } else if #[cfg(target_feature="simd128")] {
        let low_wide_mul = i64x2_extmul_low_i32x4(self.simd, rhs.simd);
        let high_wide_mul = i64x2_extmul_high_i32x4(self.simd, rhs.simd);

        (
          u32x4 { simd: i32x4_shuffle::<0, 2, 4, 6>(low_wide_mul, high_wide_mul) },
          i32x4 { simd: i32x4_shuffle::<1, 3, 5, 7>(low_wide_mul, high_wide_mul) },
        )
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_s32_s64(
            vmull_s32(vget_low_s32(self.neon), vget_low_s32(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_s32_s64(
            vmull_s32(vget_high_s32(self.neon), vget_high_s32(rhs.neon)),
          );
          let low_high = vuzpq_s32(low_wide_mul, high_wide_mul);

          (
            u32x4 { neon: vreinterpretq_u32_s32(low_high.0) },
            i32x4 { neon: low_high.1 },
          )
        }
      } else {
        // TODO(perf): This implementation looks quite bad. Is there a better
        // one?

        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        let widening_mul = [
          (self_array[0] as i64).wrapping_mul(rhs_array[0] as i64),
          (self_array[1] as i64).wrapping_mul(rhs_array[1] as i64),
          (self_array[2] as i64).wrapping_mul(rhs_array[2] as i64),
          (self_array[3] as i64).wrapping_mul(rhs_array[3] as i64),
        ];

        (
          u32x4::new([
            widening_mul[0] as u32,
            widening_mul[1] as u32,
            widening_mul[2] as u32,
            widening_mul[3] as u32,
          ]),
          i32x4::new([
            (widening_mul[0] >> 32) as i32,
            (widening_mul[1] >> 32) as i32,
            (widening_mul[2] >> 32) as i32,
            (widening_mul[3] >> 32) as i32,
          ]),
        )
      }
    }
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        let even_wide_mul = mul_widen_i32_odd_m128i(self.sse, rhs.sse);
        let odd_wide_mul = mul_widen_i32_odd_m128i(
          shuffle_ai_f32_all_m128i::<0b_00_11_00_01>(self.sse),
          shuffle_ai_f32_all_m128i::<0b_00_11_00_01>(rhs.sse),
        );
        let ll_hh_1 = unpack_low_i32_m128i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m128i(even_wide_mul, odd_wide_mul);

        Self { sse: unpack_high_i64_m128i(ll_hh_1, ll_hh_2) }
      } else if #[cfg(target_feature="simd128")] {
        let low_wide_mul = i64x2_extmul_low_i32x4(self.simd, rhs.simd);
        let high_wide_mul = i64x2_extmul_high_i32x4(self.simd, rhs.simd);

        Self { simd: i32x4_shuffle::<1, 3, 5, 7>(low_wide_mul, high_wide_mul) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_s32_s64(
            vmull_s32(vget_low_s32(self.neon), vget_low_s32(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_s32_s64(
            vmull_s32(vget_high_s32(self.neon), vget_high_s32(rhs.neon)),
          );

          Self { neon: vuzpq_s32(low_wide_mul, high_wide_mul).1 }
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        Self::new([
          ((self_array[0] as i64).wrapping_mul(rhs_array[0] as i64) >> 32) as i32,
          ((self_array[1] as i64).wrapping_mul(rhs_array[1] as i64) >> 32) as i32,
          ((self_array[2] as i64).wrapping_mul(rhs_array[2] as i64) >> 32) as i32,
          ((self_array[3] as i64).wrapping_mul(rhs_array[3] as i64) >> 32) as i32,
        ])
      }
    }
  }

  #[inline]
  pub fn abs(self) -> Self {
    pick! {
      if #[cfg(target_feature="ssse3")] {
        Self { sse: abs_i32_m128i(self.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i32x4_abs(self.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vabsq_s32(self.neon) }}
      } else {
        let arr: [i32; 4] = cast(self);
        cast([
          arr[0].wrapping_abs(),
          arr[1].wrapping_abs(),
          arr[2].wrapping_abs(),
          arr[3].wrapping_abs(),
        ])
      }
    }
  }

  #[inline]
  pub fn is_positive(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s32_u32(vcgtzq_s32(self.neon)) } }
      } else {
        self.simd_gt(Self::ZERO)
      }
    }
  }

  #[inline]
  pub fn is_negative(self) -> Self {
    pick! {
      if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vreinterpretq_s32_u32(vcltzq_s32(self.neon)) } }
      } else {
        self.simd_lt(Self::ZERO)
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`i32x4`], or only for
/// particular types inconsistently.
impl i32x4 {
  /// Converts each element from [`i32`] to [`f32`].
  #[inline]
  #[must_use]
  pub fn round_float(self) -> f32x4 {
    pick! {
      if #[cfg(target_feature="sse2")] {
        cast(convert_to_m128_from_i32_m128i(self.sse))
      } else if #[cfg(target_feature="simd128")] {
        cast(Self { simd: f32x4_convert_i32x4(self.simd) })
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        cast(unsafe {Self { neon: vreinterpretq_s32_f32(vcvtq_f32_s32(self.neon)) }})
      } else {
        let arr: [i32; 4] = cast(self);
        cast([
          arr[0] as f32,
          arr[1] as f32,
          arr[2] as f32,
          arr[3] as f32,
        ])
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
  pub fn mul_widen(self, rhs: Self) -> i64x4 {
    self.widening_mul(rhs)
  }
}
