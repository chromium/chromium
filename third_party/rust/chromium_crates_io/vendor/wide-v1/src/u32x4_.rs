use super::*;

pick! {
  if #[cfg(target_feature="sse2")] {
    /// A SIMD vector with four elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct u32x4 { pub(crate) sse: m128i }
  } else if #[cfg(target_feature="simd128")] {
    use core::arch::wasm32::*;

    /// A SIMD vector with four elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Clone, Copy)]
    #[repr(transparent)]
    pub struct u32x4 { pub(crate) simd: v128 }

    impl Default for u32x4 {
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for u32x4 {
      fn eq(&self, other: &Self) -> bool {
        u32x4_all_true(u32x4_eq(self.simd, other.simd))
      }
    }

    impl Eq for u32x4 { }
  } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
    use core::arch::aarch64::*;

    /// A SIMD vector with four elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct u32x4 { pub(crate) neon : uint32x4_t }

    impl Default for u32x4 {
      #[inline]
      fn default() -> Self {
        Self::splat(0)
      }
    }

    impl PartialEq for u32x4 {
      #[inline]
      fn eq(&self, other: &Self) -> bool {
        unsafe { vminvq_u32(vceqq_u32(self.neon, other.neon))==u32::MAX }
      }
    }

    impl Eq for u32x4 { }
} else {
    /// A SIMD vector with four elements of type [`u32`].
    ///
    /// See the [crate level documentation] for more information about SIMD
    /// vectors.
    ///
    /// [crate level documentation]: crate
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    #[repr(C, align(16))]
    pub struct u32x4 { arr: [u32;4] }
  }
}

impl_simd_uint! {
  unsafe {
    T = u32,
    N = 4,
    Simd = u32x4,
    IntSimd = i32x4,
    T_BITS = 32,
    T_BITS_MUL_2 = 64,
    [0, 1, 2, 3],
    optional_type_x86_inner { X86Inner = __m128i },
    optional_type_arm_inner { ArmInner = uint32x4_t },
    optional_type_wasm_inner { WasmInner = v128 },
  }

  #[inline]
  fn not(self) -> Self::Output {
    self ^ cast::<u128, u32x4>(u128::MAX)
  }

  #[inline]
  fn add(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: add_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_add(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vaddq_u32(self.neon, rhs.neon) } }
      } else {
        Self { arr: [
          self.arr[0].wrapping_add(rhs.arr[0]),
          self.arr[1].wrapping_add(rhs.arr[1]),
          self.arr[2].wrapping_add(rhs.arr[2]),
          self.arr[3].wrapping_add(rhs.arr[3]),
        ]}
      }
    }
  }

  #[inline]
  fn sub(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: sub_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_sub(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vsubq_u32(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_sub(rhs.arr[0]),
          self.arr[1].wrapping_sub(rhs.arr[1]),
          self.arr[2].wrapping_sub(rhs.arr[2]),
          self.arr[3].wrapping_sub(rhs.arr[3]),
        ]}
      }
    }
  }

  #[inline]
  fn mul(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: mul_32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_mul(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmulq_u32(self.neon, rhs.neon) }}
      } else {
        let arr1: [u32; 4] = cast(self);
        let arr2: [u32; 4] = cast(rhs);
        cast([
          arr1[0].wrapping_mul(arr2[0]),
          arr1[1].wrapping_mul(arr2[1]),
          arr1[2].wrapping_mul(arr2[2]),
          arr1[3].wrapping_mul(arr2[3]),
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
        unsafe {Self { neon: vandq_u32(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitand(rhs.arr[0]),
          self.arr[1].bitand(rhs.arr[1]),
          self.arr[2].bitand(rhs.arr[2]),
          self.arr[3].bitand(rhs.arr[3]),
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
        unsafe {Self { neon: vorrq_u32(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitor(rhs.arr[0]),
          self.arr[1].bitor(rhs.arr[1]),
          self.arr[2].bitor(rhs.arr[2]),
          self.arr[3].bitor(rhs.arr[3]),
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
        unsafe {Self { neon: veorq_u32(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          self.arr[0].bitxor(rhs.arr[0]),
          self.arr[1].bitxor(rhs.arr[1]),
          self.arr[2].bitxor(rhs.arr[2]),
          self.arr[3].bitxor(rhs.arr[3]),
        ]}
      }
    }
  }

  #[inline]
  fn simd_eq(self, rhs: Self) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: cmp_eq_mask_i32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_eq(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vceqq_u32(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] == rhs.arr[0] { u32::MAX } else { 0 },
          if self.arr[1] == rhs.arr[1] { u32::MAX } else { 0 },
          if self.arr[2] == rhs.arr[2] { u32::MAX } else { 0 },
          if self.arr[3] == rhs.arr[3] { u32::MAX } else { 0 },
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
        Self { simd: u32x4_ne(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_eq(rhs)
      } else {
        Self { arr: [
          if self.arr[0] != rhs.arr[0] { u32::MAX } else { 0 },
          if self.arr[1] != rhs.arr[1] { u32::MAX } else { 0 },
          if self.arr[2] != rhs.arr[2] { u32::MAX } else { 0 },
          if self.arr[3] != rhs.arr[3] { u32::MAX } else { 0 },
        ]}
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
      if #[cfg(target_feature="sse2")] {
        // no unsigned less than so inverting the high bit will get the correct result
        let h = u32x4::splat(1 << 31);
        Self { sse: cmp_gt_mask_i32_m128i((self ^ h).sse, (rhs ^ h).sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_gt(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {Self { neon: vcgtq_u32(self.neon, rhs.neon) }}
      } else {
        Self { arr: [
          if self.arr[0] > rhs.arr[0] { u32::MAX } else { 0 },
          if self.arr[1] > rhs.arr[1] { u32::MAX } else { 0 },
          if self.arr[2] > rhs.arr[2] { u32::MAX } else { 0 },
          if self.arr[3] > rhs.arr[3] { u32::MAX } else { 0 },
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
        Self { simd: u32x4_le(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_gt(rhs)
      } else {
        Self { arr: [
          if self.arr[0] <= rhs.arr[0] { u32::MAX } else { 0 },
          if self.arr[1] <= rhs.arr[1] { u32::MAX } else { 0 },
          if self.arr[2] <= rhs.arr[2] { u32::MAX } else { 0 },
          if self.arr[3] <= rhs.arr[3] { u32::MAX } else { 0 },
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
        Self { simd: u32x4_ge(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        !self.simd_lt(rhs)
      } else {
        Self { arr: [
          if self.arr[0] >= rhs.arr[0] { u32::MAX } else { 0 },
          if self.arr[1] >= rhs.arr[1] { u32::MAX } else { 0 },
          if self.arr[2] >= rhs.arr[2] { u32::MAX } else { 0 },
          if self.arr[3] >= rhs.arr[3] { u32::MAX } else { 0 },
        ]}
      }
    }
  }

  #[inline]
  pub fn reduce_add(self) -> u32 {
    pick! {
      if #[cfg(target_feature="sse2")] {
        let hi64  = unpack_high_i64_m128i(self.sse, self.sse);
        let sum64 = add_i32_m128i(hi64, self.sse);
        let hi32  = shuffle_ai_f32_all_m128i::<0b10_11_00_01>(sum64);    // Swap the low two elements
        let sum32 = add_i32_m128i(sum64, hi32);
        get_i32_from_m128i_s(sum32).cast_unsigned()
      } else {
        let arr: [u32; 4] = cast(self);
        arr[0].wrapping_add(arr[1]).wrapping_add(
        arr[2].wrapping_add(arr[3]))
      }
    }
  }

  #[inline]
  pub fn reduce_mul(self) -> u32 {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        let high_64  = unpack_high_i64_m128i(self.sse, self.sse);
        let reduce_64 = mul_32_m128i(high_64, self.sse);
        let high_32  = shuffle_ai_f32_all_m128i::<0b10_11_00_01>(reduce_64);
        let reduce_32 = mul_32_m128i(reduce_64, high_32);
        get_i32_from_m128i_s(reduce_32).cast_unsigned()
      } else if #[cfg(target_feature="simd128")] {
        let high_64 = u64x2_shuffle::<1, 0>(self.simd, self.simd);
        let reduce_64 = u32x4_mul(self.simd, high_64);
        let high_32 = u32x4_shuffle::<1, 0, 0, 0>(reduce_64, reduce_64);
        let reduce_32 = u32x4_mul(reduce_64, high_32);
        u32x4_extract_lane::<0>(reduce_32)
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let high_64 = vextq_u32::<2>(self.neon, self.neon);
          let reduce_64 = vmulq_u32(self.neon, high_64);
          let high_32 = vrev64q_u32(reduce_64);
          let reduce_32 = vmulq_u32(reduce_64, high_32);
          vgetq_lane_u32::<0>(reduce_32)
        }
      } else {
        let array = self.to_array();
        array[0].wrapping_mul(array[1]).wrapping_mul(array[2].wrapping_mul(array[3]))
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
        unsafe {Self { neon: vbslq_u32(self.neon, if_one.neon, if_zero.neon) }}
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
        unsafe {Self { neon: vbslq_u32(self.neon, if_true.neon, if_false.neon) }}
      } else {
        generic_bit_blend(self, if_true, if_false)
      }
    }
  }

  #[inline]
  pub fn to_bitmask(self) -> u32 {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // use f32 move_mask since it is the same size as i32
        move_mask_m128(cast(self.sse)) as u32
      } else if #[cfg(target_feature="simd128")] {
        u32x4_bitmask(self.simd) as u32
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe
        {
          // set all to 1 if top bit is set, else 0
          let masked = vcltq_s32(self.cast_signed().neon, vdupq_n_s32(0));

          // select the right bit out of each lane
          let selectbit : uint32x4_t = core::mem::transmute([1u32, 2, 4, 8]);
          let r = vandq_u32(masked, selectbit);

          // horizontally add the 32-bit lanes
          vaddvq_u32(r) as u32
         }
      } else {
        self.arr[0] >> 31 |
        self.arr[1] >> 31 << 1 |
        self.arr[2] >> 31 << 2 |
        self.arr[3] >> 31 << 3
      }
    }
  }

  #[inline]
  pub fn any(self) -> bool {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // use f32 move_mask since it is the same size as i32
        move_mask_m128(cast(self.sse)) != 0
      } else if #[cfg(target_feature="simd128")] {
        u32x4_bitmask(self.simd) != 0
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        // some lanes are negative
        unsafe {
          vminvq_s32(self.cast_signed().neon) < 0
        }
      } else {
        let v : [u64;2] = cast(self);
        ((v[0] | v[1]) & 0x8000000080000000) != 0
      }
    }
  }

  #[inline]
  pub fn all(self) -> bool {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // use f32 move_mask since it is the same size as i32
        move_mask_m128(cast(self.sse)) == 0b1111
      } else if #[cfg(target_feature="simd128")] {
        u32x4_bitmask(self.simd) == 0b1111
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // all lanes are negative
        unsafe {
          vmaxvq_s32(self.cast_signed().neon) < 0
        }
      } else {
        let v : [u64;2] = cast(self);
        (v[0] & v[1] & 0x8000000080000000) == 0x8000000080000000
      }
    }
  }

  #[inline]
  pub fn shuffle(self, indices: u32x4) -> Self {
    pick! {
      if #[cfg(target_feature = "avx")] {
        cast(shuffle_av_f32_all_m128(cast(self), indices.sse))
      } else if #[cfg(any(
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        let self_bytes = cast::<u32x4, u8x16>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x16, u32x4>(self_bytes.shuffle(byte_indices))
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
  pub fn shuffle_zeroing(self, indices: u32x4) -> Self {
    pick! {
      if #[cfg(any(
        target_feature = "avx",
        target_feature = "ssse3",
        all(target_arch = "aarch64", target_feature = "neon"),
        target_feature = "simd128",
      ))] {
        // Even if the `u8x16::shuffle` implementation is zeroing, our 32-bit to
        // 8-bit can trigger an overflow causing incorrect behavior
        self.shuffle(indices) & indices.simd_lt(4)
      } else {
        // The fallback branch of `shuffle` already has the behavior we want
        self.shuffle(indices)
      }
    }
  }

  #[inline]
  pub fn shuffle_wrapping(self, indices: u32x4) -> Self {
    self.shuffle(indices & 3)
  }

  #[inline]
  fn shuffle(self: [u32x4; 2], indices: u32x4) -> u32x4 {
    pick! {
      if #[cfg(all(target_feature = "avx512f", target_feature = "avx512vl"))] {
        u32x4 { sse: shuffle_abv_i32_all_m128i(self[0].sse, indices.sse, self[1].sse) }
      } else {
        let self_bytes = cast::<[u32x4; 2], [u8x16; 2]>(self);
        let byte_indices = indices.to_byte_indices();

        cast::<u8x16, u32x4>(self_bytes.shuffle(byte_indices))
      }
    }
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x4; 2], indices: u32x4) -> u32x4 {
    // Even if the `u8x16::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(8)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x4; 2], indices: u32x4) -> u32x4 {
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
  fn shuffle(self: [u32x4; 3], indices: u32x4) -> u32x4 {
    let self_bytes = cast::<[u32x4; 3], [u8x16; 3]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x16, u32x4>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x4; 3], indices: u32x4) -> u32x4 {
    // Even if the `u8x16::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(12)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x4; 3], indices: u32x4) -> u32x4 {
    self.shuffle(indices % 12)
  }

  #[inline]
  fn shuffle(self: [u32x4; 4], indices: u32x4) -> u32x4 {
    let self_bytes = cast::<[u32x4; 4], [u8x16; 4]>(self);
    let byte_indices = indices.to_byte_indices();

    cast::<u8x16, u32x4>(self_bytes.shuffle(byte_indices))
  }

  #[inline]
  fn shuffle_zeroing(self: [u32x4; 4], indices: u32x4) -> u32x4 {
    // Even if the `u8x16::shuffle` implementation is zeroing, our 32-bit to
    // 8-bit can trigger an overflow causing incorrect behavior
    self.shuffle(indices) & indices.simd_lt(16)
  }

  #[inline]
  fn shuffle_wrapping(self: [u32x4; 4], indices: u32x4) -> u32x4 {
    self.shuffle(indices & 15)
  }

  ///
  /// Currently this function is only accelerated on `sse`.
  #[inline]
  pub fn transpose(data: [Self; 4]) -> [Self; 4] {
    pick! {
      if #[cfg(target_feature="sse")] {
        let mut e0 = data[0];
        let mut e1 = data[1];
        let mut e2 = data[2];
        let mut e3 = data[3];

        transpose_four_m128(
          cast_mut(&mut e0.sse),
          cast_mut(&mut e1.sse),
          cast_mut(&mut e2.sse),
          cast_mut(&mut e3.sse),
        );

        [e0, e1, e2, e3]
      } else {
        #[inline(always)]
        fn transpose_column(data: &[u32x4; 4], index: usize) -> u32x4 {
          u32x4::new([
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
  fn shl(self, rhs: u32x4) -> Self::Output {
    pick! {
      if #[cfg(target_feature="avx2")] {
        // mask the shift count to 31 to have same behavior on all platforms
        let shift_by = bitand_m128i(rhs.sse, set_splat_i32_m128i(31));
        Self { sse: shl_each_u32_m128i(self.sse, shift_by) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // mask the shift count to 31 to have same behavior on all platforms
          let shift_by = vreinterpretq_s32_u32(vandq_u32(rhs.neon, vmovq_n_u32(31)));
          Self { neon: vshlq_u32(self.neon, shift_by) }
        }
      } else {
        let arr: [u32; 4] = cast(self);
        let rhs: [u32; 4] = cast(rhs);
        cast([
          arr[0].wrapping_shl(rhs[0]),
          arr[1].wrapping_shl(rhs[1]),
          arr[2].wrapping_shl(rhs[2]),
          arr[3].wrapping_shl(rhs[3]),
        ])
      }
    }
  }

  #[inline]
  fn shl(self, rhs: u32) -> Self::Output {
    pick! {
      if #[cfg(target_feature="sse2")] {
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        let shift = cast([rhs as u64 & 31, 0]);
        Self { sse: shl_all_u32_m128i(self.sse, shift) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_shl(self.simd, rhs) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        unsafe {Self { neon: vshlq_u32(self.neon, vmovq_n_s32(rhs as i32 & 31)) }}
      } else {
        Self { arr: [
          self.arr[0].wrapping_shl(rhs),
          self.arr[1].wrapping_shl(rhs),
          self.arr[2].wrapping_shl(rhs),
          self.arr[3].wrapping_shl(rhs),
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
        Self { sse: shr_each_u32_m128i(self.sse, shift_by) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // mask the shift count to 31 to have same behavior on all platforms
          // no right shift, have to pass negative value to left shift on neon
          let shift_by = vnegq_s32(vreinterpretq_s32_u32(vandq_u32(rhs.neon, vmovq_n_u32(31))));
          Self { neon: vshlq_u32(self.neon, shift_by) }
        }
      } else {
        let arr: [u32; 4] = cast(self);
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
        Self { sse: shr_all_u32_m128i(self.sse, shift) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_shr(self.simd, rhs) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // Use `rhs % 32` to perform wrapping shift and not unbounded shift.
        #[expect(clippy::suspicious_arithmetic_impl)]
        unsafe {Self { neon: vshlq_u32(self.neon, vmovq_n_s32( -(rhs as i32 & 31))) }}
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
        Self { sse: max_u32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_max(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmaxq_u32(self.neon, rhs.neon) }}
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vmaxq_u16(self.neon, rhs.neon) }}
      } else {
        let arr: [u32; 4] = cast(self);
        let rhs: [u32; 4] = cast(rhs);
        cast([
          arr[0].max(rhs[0]),
          arr[1].max(rhs[1]),
          arr[2].max(rhs[2]),
          arr[3].max(rhs[3]),
        ])
      }
    }
  }

  #[inline]
  pub fn min(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        Self { sse: min_u32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_min(self.simd, rhs.simd) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {Self { neon: vminq_u32(self.neon, rhs.neon) }}
      } else {
        let arr: [u32; 4] = cast(self);
        let rhs: [u32; 4] = cast(rhs);
        cast([
          arr[0].min(rhs[0]),
          arr[1].min(rhs[1]),
          arr[2].min(rhs[2]),
          arr[3].min(rhs[3]),
        ])
      }
    }
  }

  #[inline]
  pub fn reduce_max(self) -> u32 {
    let arr: [u32; 4] = cast(self);
    arr[0].max(arr[1]).max(arr[2].max(arr[3]))
  }

  #[inline]
  pub fn reduce_min(self) -> u32 {
    let arr: [u32; 4] = cast(self);
    arr[0].min(arr[1]).min(arr[2].min(arr[3]))
  }

  #[inline]
  pub fn unbounded_shl(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { sse: shl_each_u32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // The intrinsic has different semantics so we need to mask ourselves.
          Self { neon: vshlq_u32(self.neon, vreinterpretq_s32_u32(rhs.neon)) } & rhs.simd_lt(32)
        }
      } else {
        let self_array = self.to_array();
        let rhs_array = rhs.to_array();

        cast([
          self_array[0].unbounded_shl(rhs_array[0]),
          self_array[1].unbounded_shl(rhs_array[1]),
          self_array[2].unbounded_shl(rhs_array[2]),
          self_array[3].unbounded_shl(rhs_array[3]),
        ])
      }
    }
  }

  #[inline]
  pub fn unbounded_shl_scalar(self, rhs: u32) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: shl_all_u32_m128i(self.sse, cast([rhs as u64, 0])) }
      } else if #[cfg(target_feature="simd128")] {
        // The intrinsic performs wrapping shift so we need to mask the result.
        Self { simd: u32x4_shl(self.simd, rhs) } & Self::splat(rhs).simd_lt(32)
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        // The intrinsic has different semantics so we need to saturate `rhs`.
        unsafe { Self { neon: vshlq_u32(self.neon, vmovq_n_s32(rhs.min(32) as i32)) } }
      } else {
        Self { arr: [
          self.arr[0].unbounded_shl(rhs),
          self.arr[1].unbounded_shl(rhs),
          self.arr[2].unbounded_shl(rhs),
          self.arr[3].unbounded_shl(rhs),
        ]}
      }
    }
  }

  #[inline]
  pub fn unbounded_shr(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        Self { sse: shr_each_u32_m128i(self.sse, rhs.sse) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // mask to hide `rhs` overflow.
          Self { neon: vshlq_u32(self.neon, vnegq_s32(vreinterpretq_s32_u32(rhs.neon))) } & rhs.simd_lt(32)
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
        Self { sse: shr_all_u32_m128i(self.sse, cast([rhs as u64, 0])) }
      } else if #[cfg(target_feature="simd128")] {
        if rhs < 32 { Self { simd: u32x4_shr(self.simd, rhs) } } else { Self::ZERO }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe {
          // Negate `rhs` because there is no direct shift-right intrinsic, and
          // restrict it to prevent overflow.
          Self { neon: vshlq_u32(self.neon, vmovq_n_s32(-rhs.min(32).cast_signed())) }
        }
      } else {
        Self {
          arr: [
            self.arr[0].unbounded_shr(rhs),
            self.arr[1].unbounded_shr(rhs),
            self.arr[2].unbounded_shr(rhs),
            self.arr[3].unbounded_shr(rhs),
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
        unsafe { Self { neon: vqaddq_u32(self.neon, rhs.neon) } }
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
        let no_overflow = result.simd_le(self);
        // Return `0` (no bits set) if overflow occurs.
        result & no_overflow
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))]{
        unsafe { Self { neon: vqsubq_u32(self.neon, rhs.neon) } }
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
    let overflow = high.simd_ne(Self::ZERO);
    (low, overflow)
  }

  optional_fn_widening_mul {
    #[inline]
    pub fn widening_mul(self, rhs: Self) -> u64x4 {
      pick! {
        if #[cfg(target_feature="avx2")] {
          // ok to sign extend since we are throwing away the high half of the result anyway
          let a = convert_to_i64_m256i_from_i32_m128i(self.sse);
          let b = convert_to_i64_m256i_from_i32_m128i(rhs.sse);
          cast(mul_u64_low_bits_m256i(a, b))
        } else if #[cfg(target_feature="sse2")] {
          let evenp = mul_widen_u32_odd_m128i(self.sse, rhs.sse);

          let oddp = mul_widen_u32_odd_m128i(
            shr_imm_u64_m128i::<32>(self.sse),
            shr_imm_u64_m128i::<32>(rhs.sse));

          u64x4 {
            a: u64x2 { sse: unpack_low_i64_m128i(evenp, oddp)},
            b: u64x2 { sse: unpack_high_i64_m128i(evenp, oddp)}
          }
        } else if #[cfg(target_feature="simd128")] {
          u64x4 {
            a: u64x2 { simd: u64x2_extmul_low_u32x4(self.simd, rhs.simd) },
            b: u64x2 { simd: u64x2_extmul_high_u32x4(self.simd, rhs.simd) },
          }
        } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {
          u64x4 { a: u64x2 { neon: vmull_u32(vget_low_u32(self.neon), vget_low_u32(rhs.neon)) },
                  b: u64x2 { neon: vmull_u32(vget_high_u32(self.neon), vget_high_u32(rhs.neon)) } }
          }
        } else {
          let a: [u32; 4] = cast(self);
          let b: [u32; 4] = cast(rhs);
          cast([
            u64::from(a[0]) * u64::from(b[0]),
            u64::from(a[1]) * u64::from(b[1]),
            u64::from(a[2]) * u64::from(b[2]),
            u64::from(a[3]) * u64::from(b[3]),
          ])
        }
      }
    }
  }

  #[inline]
  pub fn mul_keep_low_high(self, rhs: Self) -> (Self, Self) {
    pick! {
      if #[cfg(target_feature="sse4.1")] {
        let even_wide_mul = mul_widen_u32_odd_m128i(self.sse, rhs.sse);
        let odd_wide_mul = mul_widen_u32_odd_m128i(
          shuffle_ai_f32_all_m128i::<0b_00_11_00_01>(self.sse),
          shuffle_ai_f32_all_m128i::<0b_00_11_00_01>(rhs.sse),
        );

        let ll_hh_1 = unpack_low_i32_m128i(even_wide_mul, odd_wide_mul);
        let ll_hh_2 = unpack_high_i32_m128i(even_wide_mul, odd_wide_mul);
        (
          Self { sse: unpack_low_i64_m128i(ll_hh_1, ll_hh_2) },
          Self { sse: unpack_high_i64_m128i(ll_hh_1, ll_hh_2) },
        )
      } else if #[cfg(target_feature="simd128")] {
        let low_wide_mul = u64x2_extmul_low_u32x4(self.simd, rhs.simd);
        let high_wide_mul = u64x2_extmul_high_u32x4(self.simd, rhs.simd);
        (
          Self { simd: u32x4_shuffle::<0, 2, 4, 6>(low_wide_mul, high_wide_mul) },
          Self { simd: u32x4_shuffle::<1, 3, 5, 7>(low_wide_mul, high_wide_mul) },
        )
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        unsafe {
          let low_wide_mul = vreinterpretq_u32_u64(
            vmull_u32(vget_low_u32(self.neon), vget_low_u32(rhs.neon)),
          );
          let high_wide_mul = vreinterpretq_u32_u64(
            vmull_u32(vget_high_u32(self.neon), vget_high_u32(rhs.neon)),
          );
          let low_high = vuzpq_u32(low_wide_mul, high_wide_mul);
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
          (self_array[0] as u64).wrapping_mul(rhs_array[0] as u64),
          (self_array[1] as u64).wrapping_mul(rhs_array[1] as u64),
          (self_array[2] as u64).wrapping_mul(rhs_array[2] as u64),
          (self_array[3] as u64).wrapping_mul(rhs_array[3] as u64),
        ];

        (
          Self::new([
            widening_mul[0] as u32,
            widening_mul[1] as u32,
            widening_mul[2] as u32,
            widening_mul[3] as u32,
          ]),
          Self::new([
            (widening_mul[0] >> 32) as u32,
            (widening_mul[1] >> 32) as u32,
            (widening_mul[2] >> 32) as u32,
            (widening_mul[3] >> 32) as u32,
          ]),
        )
      }
    }
  }

  #[inline]
  pub fn mul_keep_high(self, rhs: Self) -> Self {
    pick! {
      if #[cfg(target_feature="avx2")] {
        let a = convert_to_i64_m256i_from_u32_m128i(self.sse);
        let b = convert_to_i64_m256i_from_u32_m128i(rhs.sse);
        let r = mul_u64_low_bits_m256i(a, b);

        // the compiler does a good job shuffling the lanes around
        let b : [u32;8] = cast(r);
        cast([b[1],b[3],b[5],b[7]])
      } else if #[cfg(target_feature="sse2")] {
        let evenp = mul_widen_u32_odd_m128i(self.sse, rhs.sse);

        let oddp = mul_widen_u32_odd_m128i(
          shr_imm_u64_m128i::<32>(self.sse),
          shr_imm_u64_m128i::<32>(rhs.sse));

        // the compiler does a good job shuffling the lanes around
        let a : [u32;4]= cast(evenp);
        let b : [u32;4]= cast(oddp);
        cast([a[1],b[1],a[3],b[3]])

      } else if #[cfg(target_feature="simd128")] {
        let low =  u64x2_extmul_low_u32x4(self.simd, rhs.simd);
        let high = u64x2_extmul_high_u32x4(self.simd, rhs.simd);

        Self { simd: u32x4_shuffle::<1, 3, 5, 7>(low, high) }
      } else if #[cfg(all(target_feature="neon",target_arch="aarch64"))] {
        unsafe {
          let l = vmull_u32(vget_low_u32(self.neon), vget_low_u32(rhs.neon));
          let h = vmull_u32(vget_high_u32(self.neon), vget_high_u32(rhs.neon));
          u32x4 { neon: vcombine_u32(vshrn_n_u64(l,32), vshrn_n_u64(h,32)) }
        }
      } else {
        let a: [u32; 4] = cast(self);
        let b: [u32; 4] = cast(rhs);
        cast([
          ((u64::from(a[0]) * u64::from(b[0])) >> 32) as u32,
          ((u64::from(a[1]) * u64::from(b[1])) >> 32) as u32,
          ((u64::from(a[2]) * u64::from(b[2])) >> 32) as u32,
          ((u64::from(a[3]) * u64::from(b[3])) >> 32) as u32,
        ])
      }
    }
  }

  optional_fn_deserialize {}
}

/// The following functionality exists only for [`u32x4`], or only for
/// particular types inconsistently.
impl u32x4 {
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
  pub fn mul_widen(self, rhs: Self) -> u64x4 {
    self.widening_mul(rhs)
  }

  /// Returns `[self[0], b[0], self[1], b[1]]`, interleaving the low two
  /// elements of the 128-bit lane.
  #[inline]
  #[must_use]
  pub fn unpack_lo(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: unpack_low_i32_m128i(self.sse, b.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_shuffle::<0, 4, 1, 5>(self.simd, b.simd) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vzip1q_u32(self.neon, b.neon) } }
      } else {
        let s = self.as_array();
        let b = b.as_array();
        Self::new([s[0], b[0], s[1], b[1]])
      }
    }
  }

  /// Returns `[self[2], b[2], self[3], b[3]]`, interleaving the high two
  /// elements of the 128-bit lane.
  #[inline]
  #[must_use]
  pub fn unpack_hi(self, b: Self) -> Self {
    pick! {
      if #[cfg(target_feature="sse2")] {
        Self { sse: unpack_high_i32_m128i(self.sse, b.sse) }
      } else if #[cfg(target_feature="simd128")] {
        Self { simd: u32x4_shuffle::<2, 6, 3, 7>(self.simd, b.simd) }
      } else if #[cfg(all(target_feature="neon", target_arch="aarch64"))] {
        Self { neon: unsafe { vzip2q_u32(self.neon, b.neon) } }
      } else {
        let s = self.as_array();
        let b = b.as_array();
        Self::new([s[2], b[2], s[3], b[3]])
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
  fn to_byte_indices(self) -> u8x16 {
    // The byte offset of the lane, broadcast to every byte of the lane.
    let base = self.unbounded_shl_scalar(2);
    let base = base | base.unbounded_shl_scalar(8);
    let base = base | base.unbounded_shl_scalar(16);

    // Then the offset of each byte within its lane. These bits are free because
    // every byte of `base` is a multiple of four. `from_ne_bytes` keeps this
    // correct on big endian, where the bytes of a lane are the other way around.
    const WITHIN_LANE: u32x4 = u32x4::splat(u32::from_ne_bytes([0, 1, 2, 3]));

    cast::<u32x4, u8x16>(base | WITHIN_LANE)
  }
}
