use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with two elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct u64x2 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with two elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct u64x2 { pub(crate) simd: v128 }

    impl Default for u64x2 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for u64x2 {
      fn eq(&self, other: &Self) -> bool {
        u64x2_all_true(u64x2_eq(self.simd, other.simd))
      }
    }

    impl Eq for u64x2 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with two elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct u64x2 { pub(crate) neon : uint64x2_t }

    impl Default for u64x2 {
      #[inline]
      fn default() -> Self {
        unsafe { Self { neon: vdupq_n_u64(0)} }
      }
    }

    impl PartialEq for u64x2 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe {
          vgetq_lane_u64(self.neon,0) == vgetq_lane_u64(other.neon,0) &&
          vgetq_lane_u64(self.neon,1) == vgetq_lane_u64(other.neon,1)
        }
      }
    }

    impl Eq for u64x2 { }
  } else {
    /// A SIMD vector with two elements of type [`u64`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct u64x2 { arr: [u64;2] }
  }
}

impl_simd_uint! {
  unsafe {
    T = u64,
    N = 2,
    Simd = u64x2,
    IntSimd = i64x2,
    T_BITS = 64,
    T_BITS_MUL_2 = 128,
    [0, 1],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = uint64x2_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn not(self) -> Self::Output {
    self ^ cast::<u128, u64x2>(u128::MAX)
  }

  #[inline]
  fn add(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: add_i64_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_add(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vaddq_u64(self.neon, rhs.neon) } }
      } else {
        Self { arr: [
          self.arr[0].wrapping_add(rhs.arr[0]),
          self.arr[1].wrapping_add(rhs.arr[1]),
        ]}
      }
    }
  }

  #[inline]
  fn sub(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: sub_i64_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_sub(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vsubq_u64(self.neon, rhs.neon) } }
      } else {
        Self { arr: [
          self.arr[0].wrapping_sub(rhs.arr[0]),
          self.arr[1].wrapping_sub(rhs.arr[1]),
        ]}
      }
    }
  }

  #[inline]
  fn mul(self, rhs: Self) -> Self::Output {
    //we should try to implement this on sse2
    pick! {
      if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_mul(self.simd, rhs.simd) }
      } else {
        let arr1: [u64; 2] = cast(self);
        let arr2: [u64; 2] = cast(rhs);
        cast([
          arr1[0].wrapping_mul(arr2[0]),
          arr1[1].wrapping_mul(arr2[1]),
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
        unsafe {Self { neon: vandq_u64(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitand(rhs.arr[0]),
          self.arr[1].bitand(rhs.arr[1]),
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
        unsafe {Self { neon: vorrq_u64(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitor(rhs.arr[0]),
          self.arr[1].bitor(rhs.arr[1]),
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
        unsafe {Self { neon: veorq_u64(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitxor(rhs.arr[0]),
          self.arr[1].bitxor(rhs.arr[1]),
        ]}
      }
    }
  }

  #[inline]
  fn simd_eq(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: cmp_eq_mask_i64_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_eq(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vceqq_u64(self.neon, rhs.neon) } }
      } else {
        let s: [u64;2] = cast(self);
        let r: [u64;2] = cast(rhs);
        cast([
          if s[0] == r[0] { -1_i64 } else { 0 },
          if s[1] == r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn simd_ne(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        !self.simd_eq(rhs)
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_ne(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_eq(rhs)
      } else {
        let s: [u64;2] = cast(self);
        let r: [u64;2] = cast(rhs);
        cast([
          if s[0] != r[0] { -1_i64 } else { 0 },
          if s[1] != r[1] { -1_i64 } else { 0 },
        ])
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
      if #[cfg(target_feature="sse4.2")] {
        // no unsigned gt so inverting the high bit will get the correct result
        let highbit = u64x2::splat(1 << 63);
        Self { sse: cmp_gt_mask_i64_m128i((self ^ highbit).sse, (rhs ^ highbit).sse) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vcgtq_u64(self.neon, rhs.neon) }}
      } else {
        // u64x2_gt on WASM is not a thing. https://github.com/WebAssembly/simd/pull/414
        let s: [u64;2] = cast(self);
        let r: [u64;2] = cast(rhs);
        cast([
          if s[0] > r[0] { u64::MAX } else { 0 },
          if s[1] > r[1] { u64::MAX } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn simd_le(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        !self.simd_gt(rhs)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_gt(rhs)
      } else {
        let s: [u64;2] = cast(self);
        let r: [u64;2] = cast(rhs);
        cast([
          if s[0] <= r[0] { -1_i64 } else { 0 },
          if s[1] <= r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  fn simd_ge(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        !self.simd_lt(rhs)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_lt(rhs)
      } else {
        let s: [u64;2] = cast(self);
        let r: [u64;2] = cast(rhs);
        cast([
          if s[0] >= r[0] { -1_i64 } else { 0 },
          if s[1] >= r[1] { -1_i64 } else { 0 },
        ])
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> u64 {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let array: [u64; 2] = cast(self);
        array[0].wrapping_add(array[1])
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { vgetq_lane_u64(self.neon, 0).wrapping_add(vgetq_lane_u64(self.neon, 1)) }
      } else {
        self.arr[0].wrapping_add(self.arr[1])
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> u64 {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let array: [u64; 2] = cast(self);
        array[0].wrapping_mul(array[1])
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { vgetq_lane_u64(self.neon, 0).wrapping_mul(vgetq_lane_u64(self.neon, 1)) }
      } else {
        self.arr[0].wrapping_mul(self.arr[1])
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
        unsafe {Self { neon: vbslq_u64(self.neon, if_one.neon, if_zero.neon) }}
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
        unsafe {Self { neon: vbslq_u64(self.neon, if_true.neon, if_false.neon) }}
      } else {
        generic_bit_blend(self, if_true, if_false)
      }
    }
  }

  #[inline]
  pub fn to_bitmask(self) -> u32 {
    pick! {
      if #[cfg(target_feature="sse")] {
        // use f64 move_mask since it is the same size as i64
        move_mask_m128d(cast(self.sse)) as u32
      } else if #[cfg(target_feature="simd128")] {
        i64x2_bitmask(self.simd) as u32
      } else {
        // nothing amazingly efficient for neon
        let arr: [u64; 2] = cast(self);
        (arr[0] >> 63 | ((arr[1] >> 62) & 2)) as u32
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="sse")] {
        // use f64 move_mask since it is the same size as i64
        move_mask_m128d(cast(self.sse)) != 0
      } else if #[cfg(target_feature="simd128")] {
        i64x2_bitmask(self.simd) != 0
      } else {
        let v : [u64;2] = cast(self);
        ((v[0] | v[1]) & 0x8000000000000000) != 0
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // use f64 move_mask since it is the same size as i64
        move_mask_m128d(cast(self.sse)) == 0b11
      }  else if #[cfg(target_feature="simd128")] {
        i64x2_bitmask(self.simd) == 0b11
      } else {
        let v : [u64;2] = cast(self);
        ((v[0] & v[1]) & 0x8000000000000000) == 0x8000000000000000
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u64x2) -> Self {
    pick! {
      if #[cfg(target_feature = "sse2")] {
        let e0 = Self { sse: shuffle_ai_f32_all_m128i::<0b01_00_01_00>(self.sse) };
        let e1 = Self { sse: shuffle_ai_f32_all_m128i::<0b11_10_11_10>(self.sse) };

        // We can assume that each index is either `0` or `1`, and negating that
        // always gives us either all bits zero or all bits one.
        (-indices).select(e1, e0)
      } else if #[cfg(target_feature = "simd128")] {
        let e0 = Self { simd: u64x2_shuffle::<0, 0>(self.simd, self.simd) };
        let e1 = Self { simd: u64x2_shuffle::<1, 1>(self.simd, self.simd) };

        // We can assume that each index is either `0` or `1`, and negating that
        // always gives us either all bits zero or all bits one.
        (-indices).select(e1, e0)
      } else if #[cfg(all(target_feature = "neon", target_arch = "aarch64"))]{
        let e0 = unsafe { Self { neon: vdupq_n_u64(vget_lane_u64::<0>(vget_low_u64(self.neon))) } };
        let e1 = unsafe { Self { neon: vdupq_n_u64(vget_lane_u64::<0>(vget_high_u64(self.neon))) } };

        // We can assume that each index is either `0` or `1`, and negating that
        // always gives us either all bits zero or all bits one.
        (-indices).select(e1, e0)
      } else {
        let e0 = Self::splat(self.as_array()[0]);
        let e1 = Self::splat(self.as_array()[1]);

        // We can assume that each index is either `0` or `1`, and negating that
        // always gives us either all bits zero or all bits one.
        (-indices).select(e1, e0)
      }
    }
  }

  #[inline]
  pub fn shuffle_zeroing(self, indices: u64x2) -> Self {
    // Since all implementations use the `select` trick, we always need to mask
    self.shuffle(indices) & indices.simd_lt(2)
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u64x2) -> Self {
    // Since all implementations use the `select` trick, we always need to mask
    self.shuffle(indices & 1)
  }

  #[inline]
  fn shuffle(self: [u64x2; 2], indices: u64x2) -> u64x2 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        u64x2 { sse: shuffle_abv_i64_all_m128i(self[0].sse, indices.sse, self[1].sse) }
      } else {
        let self_bytes = cast::<[u64x2; 2], [u8x16; 2]>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x16, u64x2>(self_bytes.shuffle(byte_indices))
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x2; 2], indices: u64x2) -> u64x2 {
    // Even if the `u8x16::shuffle` implementation is zeroing, our 64-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(4)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x2; 2], indices: u64x2) -> u64x2 {
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
  fn shuffle(self: [u64x2; 3], indices: u64x2) -> u64x2 {
    let self_bytes = cast::<[u64x2; 3], [u8x16; 3]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x16, u64x2>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x2; 3], indices: u64x2) -> u64x2 {
    // Even if the `u8x16::shuffle` implementation is zeroing, our 64-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(6)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x2; 3], indices: u64x2) -> u64x2 {
    self.shuffle(indices % 6)
  }

  #[inline]
  fn shuffle(self: [u64x2; 4], indices: u64x2) -> u64x2 {
    let self_bytes = cast::<[u64x2; 4], [u8x16; 4]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x16, u64x2>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u64x2; 4], indices: u64x2) -> u64x2 {
    // Even if the `u8x16::shuffle` implementation is zeroing, our 64-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(8)
  }

  #[inline]
  fn shuffle_wrapping(self: [u64x2; 4], indices: u64x2) -> u64x2 {
    self.shuffle(indices & 7)
  }

  ///
  /// This function is accelerated on multiple target architectures.
  #[inline]
  pub fn transpose(data: [Self; 2]) -> [Self; 2] {
    pick! {
      if #[cfg(any(
        target_feature="sse2",
        all(target_feature="neon",target_arch="aarch64"),
        target_feature="simd128",
      ))] {
        // TODO: Remove the casts once unpack functions exist for `Self`.
        // [data[0].unpack_lo(data[1]), data[0].unpack_hi(data[1])]
        [
          data[0].cast_signed().unpack_lo(data[1].cast_signed()).cast_unsigned(),
          data[0].cast_signed().unpack_hi(data[1].cast_signed()).cast_unsigned(),
        ]
      } else {
        let [x, y, z, w]: [u64; 4] = cast(data);
        cast([x, z, y, w])
      }
    }
  }

  #[inline]
  fn shl(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // mask the shift count to 63 to have same behavior on all platforms
        let shift_by = rhs & Self::splat(63);
        Self { sse: shl_each_u64_m128i(self.sse, shift_by.sse) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // mask the shift count to 63 to have same behavior on all platforms
          let shift_by = vreinterpretq_s64_u64(vandq_u64(rhs.neon, vmovq_n_u64(63)));
          Self { neon: vshlq_u64(self.neon, shift_by) }
        }
      } else {
        let arr: [u64; 2] = cast(self);
        let rhs: [u64; 2] = cast(rhs);
        cast([
          arr[0].wrapping_shl(rhs[0] as u32),
          arr[1].wrapping_shl(rhs[1] as u32),
        ])
      }
    }
  }

  #[inline]
  fn shl(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 63, 0]);
        Self { sse: shl_all_u64_m128i(self.sse, shift) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_shl(self.simd, rhs) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        unsafe {Self { neon: vshlq_u64(self.neon, vmovq_n_s64(rhs as i64 & 63)) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_shl(rhs),
          self.arr[1].wrapping_shl(rhs),
        ]}
      }
    }
  }

  #[inline]
  fn shr(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // mask the shift count to 63 to have same behavior on all platforms
        let shift_by = rhs & Self::splat(63);
        Self { sse: shr_each_u64_m128i(self.sse, shift_by.sse) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // mask the shift count to 63 to have same behavior on all platforms
          // no right shift, have to pass negative value to left shift on neon
          let shift_by = vnegq_s64(vreinterpretq_s64_u64(vandq_u64(rhs.neon, vmovq_n_u64(63))));
          Self { neon: vshlq_u64(self.neon, shift_by) }
        }
      } else {
        let arr: [u64; 2] = cast(self);
        let rhs: [u64; 2] = cast(rhs);
        cast([
          arr[0].wrapping_shr(rhs[0] as u32),
          arr[1].wrapping_shr(rhs[1] as u32),
        ])
      }
    }
  }

  #[inline]
  fn shr(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 63, 0]);
        Self { sse: shr_all_u64_m128i(self.sse, shift) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u64x2_shr(self.simd, rhs) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Use `rhs % 64` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        unsafe {Self { neon: vshlq_u64(self.neon, vmovq_n_s64(-(rhs as i64 & 63))) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_shr(rhs),
          self.arr[1].wrapping_shr(rhs),
        ]}
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
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let array: [u64; 2] = cast(self);
        array[0].max(array[1])
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { vgetq_lane_u64(self.neon, 0).max(vgetq_lane_u64(self.neon, 1)) }
      } else {
        self.arr[0].max(self.arr[1])
      }
    }
  }

  #[inline]
  pub fn reduce_min(self) -> u64 {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let array: [u64; 2] = cast(self);
        array[0].min(array[1])
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { vgetq_lane_u64(self.neon, 0).min(vgetq_lane_u64(self.neon, 1)) }
      } else {
        self.arr[0].min(self.arr[1])
      }
    }
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { sse: shl_each_u64_m128i(self.sse, rhs.sse) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // The intrinsic has different semantics so we need to mask ourselves.
          Self { neon: vshlq_u64(self.neon, vreinterpretq_s64_u64(rhs.neon)) } & rhs.simd_lt(64)
        }
      } else {
        // Cannot use scalar `unbounded_shl` because it takes `u32`, which is
        // smaller than `u64`.
        (self << rhs) & rhs.simd_lt(64)
      }
    }
  }

  #[inline]
  pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: shl_all_u64_m128i(self.sse, cast([rhs as u64, 0])) }
      } else if #[cfg(target_feature="simd128")] {
        // The intrinsic performs wrapping shift so we need to mask the result.
        Self { simd: u64x2_shl(self.simd, rhs) } & Self::splat(rhs as u64).simd_lt(64)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vshlq_u64(self.neon, vmovq_n_s64(rhs.min(64) as i64)) } }
      } else {
        Self { arr: [
          self.arr[0].unbounded_shl(rhs),
          self.arr[1].unbounded_shl(rhs),
        ]}
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { sse: shr_each_u64_m128i(self.sse, rhs.sse) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // mask to hide `rhs` overflow.
          Self { neon: vshlq_u64(self.neon, vnegq_s64(vreinterpretq_s64_u64(rhs.neon))) } & rhs.simd_lt(64)
        }
      } else {
        // Cannot use scalar `unbounded_shr` because it takes `u32`, which is
        // smaller than `u64`.
        (self >> rhs) & rhs.simd_lt(64)
      }
    }
  }

  #[inline]
  pub fn unbounded_shr_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: shr_all_u64_m128i(self.sse, cast([rhs as u64, 0])) }
      } else if #[cfg(target_feature="simd128")] {
        if rhs < 64 { Self { simd: u64x2_shr(self.simd, rhs) } } else { Self::ZERO }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_u64(self.neon, vmovq_n_s64(-rhs.min(64).cast_signed() as i64)) }
        }
      } else {
        Self {
          arr: [
            self.arr[0].unbounded_shr(rhs),
            self.arr[1].unbounded_shr(rhs),
          ],
        }
      }
    }
  }

  #[inline]
  pub fn saturating_add(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(any(target_feature="sse2", target_feature="simd128"))] {
        let result = self + rhs;
        let overflow = result.simd_lt(self);
        // Return `MAX` (all bits set) if overflow occurs.
        result | overflow
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqaddq_u64(self.neon, rhs.neon) } }
      } else {
        Self {
          arr: [
            self.arr[0].saturating_add(rhs.arr[0]),
            self.arr[1].saturating_add(rhs.arr[1]),
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
        let no_overflow = result.simd_le(self);
        // Return `0` (no bits set) if overflow occurs.
        result & no_overflow
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_u64(self.neon, rhs.neon) } }
      } else {
        Self {
          arr: [
            self.arr[0].saturating_sub(rhs.arr[0]),
            self.arr[1].saturating_sub(rhs.arr[1]),
          ],
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
    ];
    (
      Self::new([result[0].0, result[1].0]),
      Self::new([-(result[0].1 as i64) as u64, -(result[1].1 as i64) as u64]),
    )
  }

  optional_fn_widening_mul {
    // Cannot have `widening_mul` because there is no `u128x2` type.
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
    ];

    (
      Self::new([
        widening_mul[0] as u64,
        widening_mul[1] as u64,
      ]),
      Self::new([
        (widening_mul[0] >> 64) as u64,
        (widening_mul[1] >> 64) as u64,
      ]),
    )
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    let arr1: [u64; 2] = cast(self);
    let arr2: [u64; 2] = cast(rhs);
    cast([
      ((arr1[0] as u128 * arr2[0] as u128) >> 64) as u64,
      ((arr1[1] as u128 * arr2[1] as u128) >> 64) as u64,
    ])
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`u64x2`], or only for
/// particular types inconsistently.
impl u64x2 {
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
  fn to_byte_indices(self) -> u8x16 {
    // The byte offset of the lane, broadcast to every byte of the lane.
    let base = self.unbounded_shl_scalar(3);
    let base = base | base.unbounded_shl_scalar(8);
    let base = base | base.unbounded_shl_scalar(16);
    let base = base | base.unbounded_shl_scalar(32);

    // Then the offset of each byte within its lane. These bits are free because
    // every byte of `base` is a multiple of eight. `from_ne_bytes` keeps this
    // correct on big endian, where the bytes of a lane are the other way around.
    const WITHIN_LANE: u64x2 =
      u64x2::splat(u64::from_ne_bytes([0, 1, 2, 3, 4, 5, 6, 7]));

    cast::<u64x2, u8x16>(base | WITHIN_LANE)
  }

  /// Returns `[self[0], b[0]]`, taking the low element of the 128-bit lane.
  #[inline]
  #[must_use]
  pub fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: unpack_low_i64_m128i(self.sse, b.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_shuffle::<0, 2>(self.simd, b.simd) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vzip1q_u64(self.neon, b.neon) } }
      } else {
        Self::new([self.as_array()[0], b.as_array()[0]])
      }
    }
  }

  /// Returns `[self[1], b[1]]`, taking the high element of the 128-bit lane.
  #[inline]
  #[must_use]
  pub fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: unpack_high_i64_m128i(self.sse, b.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: i64x2_shuffle::<1, 3>(self.simd, b.simd) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vzip2q_u64(self.neon, b.neon) } }
      } else {
        Self::new([self.as_array()[1], b.as_array()[1]])
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
      if #[cfg(target_feature="sse2")] {
        // `pmuludq` reads the low 32 bits of a lane anyway, so at `W == 32` the
        // operand masks are already implied and can be dropped. A scalar
        // multiply has no such truncation, so the other arm always masks.
        let (a, b) = if W == 32 {
          (a, b)
        } else {
          let mask = Self::splat(add_mul_operand_mask_u64::<W>());
          (a & mask, b & mask)
        };

        Self { sse: mul_widen_u32_odd_m128i(a.sse, b.sse) }
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
          return Self { sse: add_mul_low_u52_m128i(self.sse, a.sse, b.sse) };
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
          return Self { sse: add_mul_high_u52_m128i(self.sse, a.sse, b.sse) };
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
