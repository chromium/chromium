// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    arch::wasm32::*,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Sub, SubAssign,
    },
};

use crate::U32SimdVec;

use super::super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask, U8SimdVec, U16SimdVec};

#[derive(Clone, Copy, Debug)]
pub struct Simd128Descriptor(());

/// Prepared 8-entry BF16 lookup table for WASM SIMD128.
/// Contains 8 BF16 values packed into 16 bytes (v128).
#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct Bf16Table8Simd128(v128);

impl SimdDescriptor for Simd128Descriptor {
    type F32Vec = F32VecSimd128;

    type I32Vec = I32VecSimd128;

    type U32Vec = U32VecSimd128;

    type U16Vec = U16VecSimd128;

    type U8Vec = U8VecSimd128;

    type Mask = MaskSimd128;
    type Bf16Table8 = Bf16Table8Simd128;

    type Descriptor256 = Self;
    type Descriptor128 = Self;

    fn new() -> Option<Self> {
        if cfg!(target_feature = "simd128") {
            Some(Self(()))
        } else {
            None
        }
    }

    fn maybe_downgrade_256bit(self) -> Self {
        self
    }

    fn maybe_downgrade_128bit(self) -> Self {
        self
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        f(self)
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct F32VecSimd128(v128, Simd128Descriptor);

impl F32SimdVec for F32VecSimd128 {
    type Descriptor = Simd128Descriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: f32) -> Self {
        Self(f32x4_splat(v), d)
    }

    #[inline(always)]
    fn zero(d: Self::Descriptor) -> Self {
        Self(f32x4_splat(0.0), d)
    }

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[f32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        Self(unsafe { v128_load(mem.as_ptr().cast()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [f32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        unsafe { v128_store(mem.as_mut_ptr().cast(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2(a: Self, b: Self, dest: &mut [f32]) {
        assert!(dest.len() >= 2 * Self::LEN);
        // a = [a0, a1, a2, a3], b = [b0, b1, b2, b3]
        // out0 = [a0, b0, a1, b1], out1 = [a2, b2, a3, b3]
        let lo = i32x4_shuffle::<0, 4, 1, 5>(a.0, b.0);
        let hi = i32x4_shuffle::<2, 6, 3, 7>(a.0, b.0);
        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, lo);
            v128_store(ptr.add(1), hi);
        }
    }

    #[inline(always)]
    fn store_interleaved_3(a: Self, b: Self, c: Self, dest: &mut [f32]) {
        assert!(dest.len() >= 3 * Self::LEN);
        // a = [a0, a1, a2, a3], b = [b0, b1, b2, b3], c = [c0, c1, c2, c3]
        // out0 = [a0, b0, c0, a1], out1 = [b1, c1, a2, b2], out2 = [c2, a3, b3, c3]
        let ab_lo = i32x4_shuffle::<0, 4, 1, 5>(a.0, b.0); // [a0, b0, a1, b1]
        let ab_hi = i32x4_shuffle::<2, 6, 3, 7>(a.0, b.0); // [a2, b2, a3, b3]
        let out0 = i32x4_shuffle::<0, 1, 4, 2>(ab_lo, c.0); // [a0, b0, c0, a1]
        let tmp1 = i32x4_shuffle::<3, 5, 0, 0>(ab_lo, c.0); // [b1, c1, ?, ?]
        let out1 = i32x4_shuffle::<0, 1, 4, 5>(tmp1, ab_hi); // [b1, c1, a2, b2]
        let out2 = i32x4_shuffle::<6, 2, 3, 7>(ab_hi, c.0); // [c2, a3, b3, c3]

        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, out0);
            v128_store(ptr.add(1), out1);
            v128_store(ptr.add(2), out2);
        }
    }

    #[inline(always)]
    fn store_interleaved_4(a: Self, b: Self, c: Self, d: Self, dest: &mut [f32]) {
        assert!(dest.len() >= 4 * Self::LEN);
        // a = [a0,a1,a2,a3], b = [b0,b1,b2,b3], c = [c0,c1,c2,c3], d = [d0,d1,d2,d3]
        // out = [a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3]
        // Stage 1: interleave pairs
        let ab_lo = i32x4_shuffle::<0, 4, 1, 5>(a.0, b.0); // [a0, b0, a1, b1]
        let ab_hi = i32x4_shuffle::<2, 6, 3, 7>(a.0, b.0); // [a2, b2, a3, b3]
        let cd_lo = i32x4_shuffle::<0, 4, 1, 5>(c.0, d.0); // [c0, d0, c1, d1]
        let cd_hi = i32x4_shuffle::<2, 6, 3, 7>(c.0, d.0); // [c2, d2, c3, d3]

        // Stage 2: interleave pairs of pairs (using 64-bit granularity)
        let out0 = i64x2_shuffle::<0, 2>(ab_lo, cd_lo); // [a0, b0, c0, d0]
        let out1 = i64x2_shuffle::<1, 3>(ab_lo, cd_lo); // [a1, b1, c1, d1]
        let out2 = i64x2_shuffle::<0, 2>(ab_hi, cd_hi); // [a2, b2, c2, d2]
        let out3 = i64x2_shuffle::<1, 3>(ab_hi, cd_hi); // [a3, b3, c3, d3]

        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, out0);
            v128_store(ptr.add(1), out1);
            v128_store(ptr.add(2), out2);
            v128_store(ptr.add(3), out3);
        }
    }

    #[inline(always)]
    fn store_interleaved_8(
        a: Self,
        b: Self,
        c: Self,
        d: Self,
        e: Self,
        f: Self,
        g: Self,
        h: Self,
        dest: &mut [f32],
    ) {
        assert!(dest.len() >= 8 * Self::LEN);

        // Interleave adjacent pairs
        let ab_lo = i32x4_shuffle::<0, 4, 1, 5>(a.0, b.0);
        let ab_hi = i32x4_shuffle::<2, 6, 3, 7>(a.0, b.0);
        let cd_lo = i32x4_shuffle::<0, 4, 1, 5>(c.0, d.0);
        let cd_hi = i32x4_shuffle::<2, 6, 3, 7>(c.0, d.0);
        let ef_lo = i32x4_shuffle::<0, 4, 1, 5>(e.0, f.0);
        let ef_hi = i32x4_shuffle::<2, 6, 3, 7>(e.0, f.0);
        let gh_lo = i32x4_shuffle::<0, 4, 1, 5>(g.0, h.0);
        let gh_hi = i32x4_shuffle::<2, 6, 3, 7>(g.0, h.0);

        // Combine into 4-wide groups
        let abcd_0 = i64x2_shuffle::<0, 2>(ab_lo, cd_lo);
        let abcd_1 = i64x2_shuffle::<1, 3>(ab_lo, cd_lo);
        let abcd_2 = i64x2_shuffle::<0, 2>(ab_hi, cd_hi);
        let abcd_3 = i64x2_shuffle::<1, 3>(ab_hi, cd_hi);
        let efgh_0 = i64x2_shuffle::<0, 2>(ef_lo, gh_lo);
        let efgh_1 = i64x2_shuffle::<1, 3>(ef_lo, gh_lo);
        let efgh_2 = i64x2_shuffle::<0, 2>(ef_hi, gh_hi);
        let efgh_3 = i64x2_shuffle::<1, 3>(ef_hi, gh_hi);

        // SAFETY: we just checked that dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, abcd_0);
            v128_store(ptr.add(1), efgh_0);
            v128_store(ptr.add(2), abcd_1);
            v128_store(ptr.add(3), efgh_1);
            v128_store(ptr.add(4), abcd_2);
            v128_store(ptr.add(5), efgh_2);
            v128_store(ptr.add(6), abcd_3);
            v128_store(ptr.add(7), efgh_3);
        }
    }

    #[inline(always)]
    fn load_deinterleaved_2(d: Self::Descriptor, src: &[f32]) -> (Self, Self) {
        assert!(src.len() >= 2 * Self::LEN);
        // src = [a0, b0, a1, b1, a2, b2, a3, b3]
        // SAFETY: we just checked that `src` has enough space.
        let (lo, hi) = unsafe {
            (
                v128_load(src.as_ptr().cast()),        // [a0, b0, a1, b1]
                v128_load(src.as_ptr().add(4).cast()), // [a2, b2, a3, b3]
            )
        };
        let a = i32x4_shuffle::<0, 2, 4, 6>(lo, hi); // [a0, a1, a2, a3]
        let b = i32x4_shuffle::<1, 3, 5, 7>(lo, hi); // [b0, b1, b2, b3]
        (Self(a, d), Self(b, d))
    }

    #[inline(always)]
    fn load_deinterleaved_3(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self) {
        assert!(src.len() >= 3 * Self::LEN);
        // src = [a0,b0,c0, a1,b1,c1, a2,b2,c2, a3,b3,c3]
        // v0 = [a0, b0, c0, a1], v1 = [b1, c1, a2, b2], v2 = [c2, a3, b3, c3]
        // SAFETY: we just checked that `src` has enough space.
        let (v0, v1, v2) = unsafe {
            (
                v128_load(src.as_ptr().cast()),
                v128_load(src.as_ptr().add(4).cast()),
                v128_load(src.as_ptr().add(8).cast()),
            )
        };

        // a = [a0, a1, a2, a3] = v0[0], v0[3], v1[2], v2[1]
        let a01 = i32x4_shuffle::<0, 3, 0, 0>(v0, v0); // [a0, a1, ?, ?]
        let a23 = i32x4_shuffle::<2, 5, 0, 0>(v1, v2); // [a2, a3, ?, ?]
        let a = i64x2_shuffle::<0, 2>(a01, a23);

        // b = [b0, b1, b2, b3] = v0[1], v1[0], v1[3], v2[2]
        let b01 = i32x4_shuffle::<1, 4, 0, 0>(v0, v1); // [b0, b1, ?, ?]
        let b23 = i32x4_shuffle::<3, 6, 0, 0>(v1, v2); // [b2, b3, ?, ?]
        let b = i64x2_shuffle::<0, 2>(b01, b23);

        // c = [c0, c1, c2, c3] = v0[2], v1[1], v2[0], v2[3]
        let c01 = i32x4_shuffle::<2, 5, 0, 0>(v0, v1); // [c0, c1, ?, ?]
        let c23 = i32x4_shuffle::<0, 3, 0, 0>(v2, v2); // [c2, c3, ?, ?]
        let c = i64x2_shuffle::<0, 2>(c01, c23);

        (Self(a, d), Self(b, d), Self(c, d))
    }

    #[inline(always)]
    fn load_deinterleaved_4(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self, Self) {
        assert!(src.len() >= 4 * Self::LEN);
        // src = [a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3]
        // SAFETY: we just checked that `src` has enough space.
        let (v0, v1, v2, v3) = unsafe {
            (
                v128_load(src.as_ptr().cast()),         // [a0, b0, c0, d0]
                v128_load(src.as_ptr().add(4).cast()),  // [a1, b1, c1, d1]
                v128_load(src.as_ptr().add(8).cast()),  // [a2, b2, c2, d2]
                v128_load(src.as_ptr().add(12).cast()), // [a3, b3, c3, d3]
            )
        };

        // Stage 1: interleave pairs
        let ac02 = i32x4_shuffle::<0, 4, 2, 6>(v0, v1); // [a0, a1, c0, c1]
        let bd02 = i32x4_shuffle::<1, 5, 3, 7>(v0, v1); // [b0, b1, d0, d1]
        let ac13 = i32x4_shuffle::<0, 4, 2, 6>(v2, v3); // [a2, a3, c2, c3]
        let bd13 = i32x4_shuffle::<1, 5, 3, 7>(v2, v3); // [b2, b3, d2, d3]

        // Stage 2: combine
        let a = i64x2_shuffle::<0, 2>(ac02, ac13); // [a0, a1, a2, a3]
        let b = i64x2_shuffle::<0, 2>(bd02, bd13); // [b0, b1, b2, b3]
        let c = i64x2_shuffle::<1, 3>(ac02, ac13); // [c0, c1, c2, c3]
        let dd = i64x2_shuffle::<1, 3>(bd02, bd13); // [d0, d1, d2, d3]
        (Self(a, d), Self(b, d), Self(c, d), Self(dd, d))
    }

    #[inline(always)]
    fn transpose_square(d: Simd128Descriptor, data: &mut [[f32; 4]], stride: usize) {
        assert!(data.len() > 3 * stride);

        let p0 = F32VecSimd128::load_array(d, &data[0]).0;
        let p1 = F32VecSimd128::load_array(d, &data[stride]).0;
        let p2 = F32VecSimd128::load_array(d, &data[2 * stride]).0;
        let p3 = F32VecSimd128::load_array(d, &data[3 * stride]).0;

        let tr0 = i32x4_shuffle::<0, 4, 1, 5>(p0, p1);
        let tr1 = i32x4_shuffle::<2, 6, 3, 7>(p0, p1);
        let tr2 = i32x4_shuffle::<0, 4, 1, 5>(p2, p3);
        let tr3 = i32x4_shuffle::<2, 6, 3, 7>(p2, p3);

        F32VecSimd128(i64x2_shuffle::<0, 2>(tr0, tr2), d).store_array(&mut data[0]);
        F32VecSimd128(i64x2_shuffle::<1, 3>(tr0, tr2), d).store_array(&mut data[stride]);
        F32VecSimd128(i64x2_shuffle::<0, 2>(tr1, tr3), d).store_array(&mut data[2 * stride]);
        F32VecSimd128(i64x2_shuffle::<1, 3>(tr1, tr3), d).store_array(&mut data[3 * stride]);
    }

    crate::impl_f32_array_interface!();

    #[inline(always)]
    fn mul_add(self, mul: F32VecSimd128, add: F32VecSimd128) -> F32VecSimd128 {
        #[cfg(target_feature = "relaxed-simd")]
        {
            F32VecSimd128(f32x4_relaxed_madd(self.0, mul.0, add.0), self.1)
        }
        #[cfg(not(target_feature = "relaxed-simd"))]
        {
            F32VecSimd128(f32x4_add(f32x4_mul(self.0, mul.0), add.0), self.1)
        }
    }

    #[inline(always)]
    fn neg_mul_add(self, mul: F32VecSimd128, add: F32VecSimd128) -> F32VecSimd128 {
        #[cfg(target_feature = "relaxed-simd")]
        {
            F32VecSimd128(f32x4_relaxed_nmadd(self.0, mul.0, add.0), self.1)
        }
        #[cfg(not(target_feature = "relaxed-simd"))]
        {
            F32VecSimd128(f32x4_sub(add.0, f32x4_mul(self.0, mul.0)), self.1)
        }
    }

    #[inline(always)]
    fn abs(self) -> F32VecSimd128 {
        F32VecSimd128(f32x4_abs(self.0), self.1)
    }

    #[inline(always)]
    fn floor(self) -> F32VecSimd128 {
        F32VecSimd128(f32x4_floor(self.0), self.1)
    }

    #[inline(always)]
    fn sqrt(self) -> F32VecSimd128 {
        F32VecSimd128(f32x4_sqrt(self.0), self.1)
    }

    #[inline(always)]
    fn neg(self) -> F32VecSimd128 {
        F32VecSimd128(f32x4_neg(self.0), self.1)
    }

    #[inline(always)]
    fn copysign(self, sign: F32VecSimd128) -> F32VecSimd128 {
        // Select sign bit from `sign`, magnitude from `self`
        let sign_mask = u32x4_splat(0x8000_0000);
        F32VecSimd128(v128_bitselect(sign.0, self.0, sign_mask), self.1)
    }

    #[inline(always)]
    fn max(self, other: F32VecSimd128) -> F32VecSimd128 {
        F32VecSimd128(f32x4_max(self.0, other.0), self.1)
    }

    #[inline(always)]
    fn min(self, other: F32VecSimd128) -> F32VecSimd128 {
        F32VecSimd128(f32x4_min(self.0, other.0), self.1)
    }

    #[inline(always)]
    fn gt(self, other: F32VecSimd128) -> MaskSimd128 {
        MaskSimd128(f32x4_gt(self.0, other.0), self.1)
    }

    #[inline(always)]
    fn as_i32(self) -> I32VecSimd128 {
        I32VecSimd128(i32x4_trunc_sat_f32x4(self.0), self.1)
    }

    #[inline(always)]
    fn bitcast_to_i32(self) -> I32VecSimd128 {
        // v128 is untyped; no conversion needed, just reinterpret.
        I32VecSimd128(self.0, self.1)
    }

    #[inline(always)]
    fn round_store_u8(self, dest: &mut [u8]) {
        assert!(dest.len() >= F32VecSimd128::LEN);
        let rounded = f32x4_nearest(self.0);
        let i32s = i32x4_trunc_sat_f32x4(rounded);
        // Saturate i32 -> i16 (signed narrow, preserving sign for next stage)
        let i16s = i16x8_narrow_i32x4(i32s, i32s);
        // Saturate i16 -> u8 (signed-to-unsigned narrow, clamping to [0, 255])
        let u8s = u8x16_narrow_i16x8(i16s, i16s);
        // SAFETY: we checked dest has enough space.
        unsafe { v128_store32_lane::<0>(u8s, dest.as_mut_ptr().cast()) }
    }

    #[inline(always)]
    fn round_store_u16(self, dest: &mut [u16]) {
        assert!(dest.len() >= F32VecSimd128::LEN);
        let rounded = f32x4_nearest(self.0);
        let i32s = i32x4_trunc_sat_f32x4(rounded);
        // Saturate i32 -> u16 (narrow to 16-bit)
        let u16s = u16x8_narrow_i32x4(i32s, i32s);
        // Store lower 8 bytes (4 u16s)
        let lo = i64x2_extract_lane::<0>(u16s);
        // SAFETY: we checked dest has enough space.
        unsafe {
            std::ptr::copy_nonoverlapping(
                (&raw const lo).cast::<u8>(),
                dest.as_mut_ptr().cast::<u8>(),
                8,
            );
        }
    }

    #[inline(always)]
    fn store_f16_bits(self, dest: &mut [u16]) {
        assert!(dest.len() >= Self::LEN);
        let abs = f32x4_abs(self.0);
        let sign = v128_and(u32x4_shr(self.0, 16), u32x4_splat(0x8000));

        // Round to nearest even
        let abs_shifted = u32x4_shr(abs, 13);
        let lsb = v128_and(abs_shifted, u32x4_splat(1));
        let rounded = i32x4_add(abs, i32x4_add(lsb, u32x4_splat(0x0FFF)));

        // Normal: shift exp+mant into f16 position, rebias, clamp to inf (0x7C00)
        let normal = i32x4_min(
            i32x4_sub(u32x4_shr(rounded, 13), u32x4_splat(112 << 10)),
            i32x4_splat(0x7C00),
        );

        // Subnormal (|x| < 2^-14): magic number trick to denormalize
        // Adding 2^-14 normalizes the mantissa via float rounding,
        // then integer-subtract the bias and shift to get the 10-bit mantissa.
        let magic = u32x4_splat(113 << 23);
        let subnorm = u32x4_shr(i32x4_sub(f32x4_add(abs, magic), magic), 13);

        // Inf/NaN (exp==255): set f16 exp=31, keep top 10 mantissa bits
        let infnan = v128_or(
            u32x4_splat(0x7C00),
            v128_and(abs_shifted, u32x4_splat(0x03FF)),
        );

        let is_subnorm = i32x4_lt(abs, u32x4_splat(0x38800000));
        let is_infnan = i32x4_gt(abs, u32x4_splat(0x7F7FFFFF));
        let result = v128_bitselect(subnorm, normal, is_subnorm);
        let result = v128_bitselect(infnan, result, is_infnan);
        let h = v128_or(sign, result);

        // Narrow u32->u16 and store
        let packed = i8x16_shuffle::<0, 1, 4, 5, 8, 9, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0>(h, h);
        // SAFETY: we checked dest has enough space.
        unsafe {
            v128_store64_lane::<0>(packed, dest.as_mut_ptr().cast::<u64>());
        }
    }

    #[inline(always)]
    fn load_f16_bits(d: Self::Descriptor, mem: &[u16]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        let lo = unsafe { v128_load64_splat(mem.as_ptr().cast()) };
        let zero = i32x4_splat(0);
        let wide =
            i8x16_shuffle::<0, 1, 16, 16, 2, 3, 16, 16, 4, 5, 16, 16, 6, 7, 16, 16>(lo, zero);
        let sign = i32x4_shl(v128_and(wide, u32x4_splat(0x8000)), 16);
        let mag = v128_and(wide, u32x4_splat(0x7FFF));
        let mag_shifted = i32x4_shl(mag, 13);

        // Normal: shift exp+mant into f32 position, rebias exponent (+112)
        let normal = i32x4_add(mag_shifted, u32x4_splat(112 << 23));

        // Subnormal (exp==0): magic number trick to normalize
        let magic = u32x4_splat(113 << 23);
        let subnorm = f32x4_sub(v128_or(magic, mag_shifted), magic);

        // Inf/NaN (exp==31): normal path gives f32 exp=143, need 255; add 112<<23
        let infnan = i32x4_add(normal, u32x4_splat(112 << 23));

        let is_subnorm = i32x4_lt(mag, u32x4_splat(0x0400));
        let is_infnan = i32x4_gt(mag, u32x4_splat(0x7BFF));
        let result = v128_bitselect(subnorm, normal, is_subnorm);
        let result = v128_bitselect(infnan, result, is_infnan);

        Self(v128_or(sign, result), d)
    }

    #[inline(always)]
    fn prepare_table_bf16_8(_d: Simd128Descriptor, table: &[f32; 8]) -> Bf16Table8Simd128 {
        // Convert f32 table to BF16 packed in 128 bits (16 bytes for 8 entries)
        // BF16 is the high 16 bits of f32
        // SAFETY: `table` has 8 elements, so both loads are in bounds.
        let (table_lo, table_hi) = unsafe {
            (
                v128_load(table.as_ptr().cast()),
                v128_load(table.as_ptr().add(4).cast()),
            )
        };

        // Shift right by 16 to get BF16 values, then narrow to 16-bit
        let lo_shifted = u32x4_shr(table_lo, 16);
        let hi_shifted = u32x4_shr(table_hi, 16);

        // Narrow u32 -> u16, packing both halves
        Bf16Table8Simd128(u16x8_narrow_i32x4(lo_shifted, hi_shifted))
    }

    #[inline(always)]
    fn table_lookup_bf16_8(
        d: Simd128Descriptor,
        table: Bf16Table8Simd128,
        indices: I32VecSimd128,
    ) -> Self {
        // Build shuffle mask efficiently using arithmetic on 32-bit indices.
        // For each index i (0-7), we need to select bytes [2*i, 2*i+1] from bf16_table
        // and place them in the high 16 bits of each 32-bit f32 lane (bytes 2,3),
        // with bytes 0,1 set to zero (using out-of-range index which gives 0 in swizzle).
        //
        // Output byte pattern per lane (little-endian): [0x80, 0x80, 2*i, 2*i+1]
        // As a 32-bit value: 0x80 | (0x80 << 8) | (2*i << 16) | ((2*i+1) << 24)
        //                  = (i << 17) | (i << 25) | 0x01008080
        let shl17 = i32x4_shl(indices.0, 17);
        let shl25 = i32x4_shl(indices.0, 25);
        let base = u32x4_splat(0x01008080);
        let shuffle_mask = v128_or(v128_or(shl17, shl25), base);

        // Perform the table lookup (out of range indices give 0)
        F32VecSimd128(i8x16_swizzle(table.0, shuffle_mask), d)
    }
}

impl Add<F32VecSimd128> for F32VecSimd128 {
    type Output = Self;
    #[inline(always)]
    fn add(self, rhs: F32VecSimd128) -> F32VecSimd128 {
        F32VecSimd128(f32x4_add(self.0, rhs.0), self.1)
    }
}

impl Sub<F32VecSimd128> for F32VecSimd128 {
    type Output = Self;
    #[inline(always)]
    fn sub(self, rhs: F32VecSimd128) -> F32VecSimd128 {
        F32VecSimd128(f32x4_sub(self.0, rhs.0), self.1)
    }
}

impl Mul<F32VecSimd128> for F32VecSimd128 {
    type Output = Self;
    #[inline(always)]
    fn mul(self, rhs: F32VecSimd128) -> F32VecSimd128 {
        F32VecSimd128(f32x4_mul(self.0, rhs.0), self.1)
    }
}

impl Div<F32VecSimd128> for F32VecSimd128 {
    type Output = Self;
    #[inline(always)]
    fn div(self, rhs: F32VecSimd128) -> F32VecSimd128 {
        F32VecSimd128(f32x4_div(self.0, rhs.0), self.1)
    }
}

impl AddAssign<F32VecSimd128> for F32VecSimd128 {
    #[inline(always)]
    fn add_assign(&mut self, rhs: F32VecSimd128) {
        self.0 = f32x4_add(self.0, rhs.0);
    }
}

impl SubAssign<F32VecSimd128> for F32VecSimd128 {
    #[inline(always)]
    fn sub_assign(&mut self, rhs: F32VecSimd128) {
        self.0 = f32x4_sub(self.0, rhs.0);
    }
}

impl MulAssign<F32VecSimd128> for F32VecSimd128 {
    #[inline(always)]
    fn mul_assign(&mut self, rhs: F32VecSimd128) {
        self.0 = f32x4_mul(self.0, rhs.0);
    }
}

impl DivAssign<F32VecSimd128> for F32VecSimd128 {
    #[inline(always)]
    fn div_assign(&mut self, rhs: F32VecSimd128) {
        self.0 = f32x4_div(self.0, rhs.0);
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct I32VecSimd128(v128, Simd128Descriptor);

impl I32SimdVec for I32VecSimd128 {
    type Descriptor = Simd128Descriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: i32) -> Self {
        Self(i32x4_splat(v), d)
    }

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[i32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        Self(unsafe { v128_load(mem.as_ptr().cast()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [i32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        unsafe { v128_store(mem.as_mut_ptr().cast(), self.0) }
    }

    #[inline(always)]
    fn abs(self) -> I32VecSimd128 {
        I32VecSimd128(i32x4_abs(self.0), self.1)
    }

    #[inline(always)]
    fn as_f32(self) -> F32VecSimd128 {
        F32VecSimd128(f32x4_convert_i32x4(self.0), self.1)
    }

    #[inline(always)]
    fn bitcast_to_f32(self) -> F32VecSimd128 {
        // v128 is untyped; no conversion needed.
        F32VecSimd128(self.0, self.1)
    }

    #[inline(always)]
    fn bitcast_to_u32(self) -> U32VecSimd128 {
        // v128 is untyped; no conversion needed.
        U32VecSimd128(self.0, self.1)
    }

    #[inline(always)]
    fn gt(self, other: I32VecSimd128) -> MaskSimd128 {
        MaskSimd128(i32x4_gt(self.0, other.0), self.1)
    }

    #[inline(always)]
    fn lt_zero(self) -> MaskSimd128 {
        MaskSimd128(i32x4_lt(self.0, i32x4_splat(0)), self.1)
    }

    #[inline(always)]
    fn eq(self, other: I32VecSimd128) -> MaskSimd128 {
        MaskSimd128(i32x4_eq(self.0, other.0), self.1)
    }

    #[inline(always)]
    fn eq_zero(self) -> MaskSimd128 {
        MaskSimd128(i32x4_eq(self.0, i32x4_splat(0)), self.1)
    }

    #[inline(always)]
    fn mul_wide_take_high(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        // Multiply pairs and take the high 32 bits of each 64-bit result
        let lo = i64x2_extmul_low_i32x4(self.0, rhs.0); // [a0*b0, a1*b1] as i64
        let hi = i64x2_extmul_high_i32x4(self.0, rhs.0); // [a2*b2, a3*b3] as i64
        // Extract high 32 bits: shift right by 32, then shuffle to pack
        // After shift: lo = [hi(a0*b0), 0, hi(a1*b1), 0] as i32 view
        // We want: [hi(a0*b0), hi(a1*b1), hi(a2*b2), hi(a3*b3)]
        let lo_shifted = i64x2_shr(lo, 32);
        let hi_shifted = i64x2_shr(hi, 32);
        // Pack: take lanes 0,2 from lo_shifted and 0,2 from hi_shifted
        // lo_shifted as i32x4: [hi0, 0, hi1, 0]
        // hi_shifted as i32x4: [hi2, 0, hi3, 0]
        I32VecSimd128(i32x4_shuffle::<0, 2, 4, 6>(lo_shifted, hi_shifted), self.1)
    }

    #[inline(always)]
    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        Self(i32x4_shl(self.0, AMOUNT_U), self.1)
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        Self(i32x4_shr(self.0, AMOUNT_U), self.1)
    }

    #[inline(always)]
    fn store_u16(self, dest: &mut [u16]) {
        assert!(dest.len() >= Self::LEN);
        // Truncating narrow: take the lower 16 bits of each 32-bit lane via shuffle.
        // Input bytes: [l0,l1,h0,h1, l2,l3,h2,h3, l4,l5,h4,h5, l6,l7,h6,h7]
        // Output bytes (lower 8): [l0,l1,l2,l3,l4,l5,l6,l7]
        let packed =
            i8x16_shuffle::<0, 1, 4, 5, 8, 9, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0>(self.0, self.0);
        let lo = i64x2_extract_lane::<0>(packed);
        // SAFETY: we checked that `dest` has enough space.
        unsafe {
            std::ptr::copy_nonoverlapping(
                (&raw const lo).cast::<u8>(),
                dest.as_mut_ptr().cast::<u8>(),
                8,
            );
        }
    }

    #[inline(always)]
    fn store_u8(self, dest: &mut [u8]) {
        assert!(dest.len() >= Self::LEN);
        // Truncating narrow i32 -> u8: take the lowest byte of each 32-bit lane.
        // Input bytes: [b0,_,_,_, b1,_,_,_, b2,_,_,_, b3,_,_,_]
        // Output bytes (lower 4): [b0, b1, b2, b3]
        let packed =
            i8x16_shuffle::<0, 4, 8, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>(self.0, self.0);
        let val = u32x4_extract_lane::<0>(packed);
        // SAFETY: we checked that `dest` has enough space.
        unsafe {
            std::ptr::copy_nonoverlapping((&raw const val).cast::<u8>(), dest.as_mut_ptr(), 4);
        }
    }
}

impl Add<I32VecSimd128> for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn add(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        I32VecSimd128(i32x4_add(self.0, rhs.0), self.1)
    }
}

impl Sub<I32VecSimd128> for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn sub(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        I32VecSimd128(i32x4_sub(self.0, rhs.0), self.1)
    }
}

impl Mul<I32VecSimd128> for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn mul(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        I32VecSimd128(i32x4_mul(self.0, rhs.0), self.1)
    }
}

impl Neg for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn neg(self) -> I32VecSimd128 {
        I32VecSimd128(i32x4_neg(self.0), self.1)
    }
}

impl BitAnd<I32VecSimd128> for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn bitand(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        I32VecSimd128(v128_and(self.0, rhs.0), self.1)
    }
}

impl BitOr<I32VecSimd128> for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn bitor(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        I32VecSimd128(v128_or(self.0, rhs.0), self.1)
    }
}

impl BitXor<I32VecSimd128> for I32VecSimd128 {
    type Output = I32VecSimd128;
    #[inline(always)]
    fn bitxor(self, rhs: I32VecSimd128) -> I32VecSimd128 {
        I32VecSimd128(v128_xor(self.0, rhs.0), self.1)
    }
}

impl AddAssign<I32VecSimd128> for I32VecSimd128 {
    #[inline(always)]
    fn add_assign(&mut self, rhs: I32VecSimd128) {
        self.0 = i32x4_add(self.0, rhs.0)
    }
}

impl SubAssign<I32VecSimd128> for I32VecSimd128 {
    #[inline(always)]
    fn sub_assign(&mut self, rhs: I32VecSimd128) {
        self.0 = i32x4_sub(self.0, rhs.0)
    }
}

impl MulAssign<I32VecSimd128> for I32VecSimd128 {
    #[inline(always)]
    fn mul_assign(&mut self, rhs: I32VecSimd128) {
        self.0 = i32x4_mul(self.0, rhs.0)
    }
}

impl BitAndAssign<I32VecSimd128> for I32VecSimd128 {
    #[inline(always)]
    fn bitand_assign(&mut self, rhs: I32VecSimd128) {
        self.0 = v128_and(self.0, rhs.0);
    }
}

impl BitOrAssign<I32VecSimd128> for I32VecSimd128 {
    #[inline(always)]
    fn bitor_assign(&mut self, rhs: I32VecSimd128) {
        self.0 = v128_or(self.0, rhs.0);
    }
}

impl BitXorAssign<I32VecSimd128> for I32VecSimd128 {
    #[inline(always)]
    fn bitxor_assign(&mut self, rhs: I32VecSimd128) {
        self.0 = v128_xor(self.0, rhs.0);
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U32VecSimd128(v128, Simd128Descriptor);

impl U32SimdVec for U32VecSimd128 {
    type Descriptor = Simd128Descriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn bitcast_to_i32(self) -> I32VecSimd128 {
        // v128 is untyped; no conversion needed.
        I32VecSimd128(self.0, self.1)
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        Self(u32x4_shr(self.0, AMOUNT_U), self.1)
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U8VecSimd128(v128, Simd128Descriptor);

impl U8SimdVec for U8VecSimd128 {
    type Descriptor = Simd128Descriptor;
    const LEN: usize = 16;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[u8]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        Self(unsafe { v128_load(mem.as_ptr().cast()) }, d)
    }

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: u8) -> Self {
        Self(u8x16_splat(v), d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [u8]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        unsafe { v128_store(mem.as_mut_ptr().cast(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2(a: Self, b: Self, dest: &mut [u8]) {
        assert!(dest.len() >= 2 * Self::LEN);
        let lo = i8x16_shuffle::<0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23>(a.0, b.0);
        let hi =
            i8x16_shuffle::<8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31>(a.0, b.0);
        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, lo);
            v128_store(ptr.add(1), hi);
        }
    }

    #[inline(always)]
    fn store_interleaved_3(a: Self, b: Self, c: Self, dest: &mut [u8]) {
        assert!(dest.len() >= 3 * Self::LEN);
        // 3-way byte interleave using 2 shuffles per output vector.
        let ab0 = i8x16_shuffle::<0, 16, 0, 1, 17, 0, 2, 18, 0, 3, 19, 0, 4, 20, 0, 5>(a.0, b.0);
        let out0 =
            i8x16_shuffle::<0, 1, 16, 3, 4, 17, 6, 7, 18, 9, 10, 19, 12, 13, 20, 15>(ab0, c.0);

        let ab1 = i8x16_shuffle::<21, 0, 6, 22, 0, 7, 23, 0, 8, 24, 0, 9, 25, 0, 10, 26>(a.0, b.0);
        let out1 =
            i8x16_shuffle::<0, 21, 2, 3, 22, 5, 6, 23, 8, 9, 24, 11, 12, 25, 14, 15>(ab1, c.0);

        let ab2 =
            i8x16_shuffle::<0, 11, 27, 0, 12, 28, 0, 13, 29, 0, 14, 30, 0, 15, 31, 0>(a.0, b.0);
        let out2 =
            i8x16_shuffle::<26, 1, 2, 27, 4, 5, 28, 7, 8, 29, 10, 11, 30, 13, 14, 31>(ab2, c.0);

        // SAFETY: dest has 3*16 = 48 bytes.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, out0);
            v128_store(ptr.add(1), out1);
            v128_store(ptr.add(2), out2);
        }
    }

    #[inline(always)]
    fn store_interleaved_4(a: Self, b: Self, c: Self, d: Self, dest: &mut [u8]) {
        assert!(dest.len() >= 4 * Self::LEN);
        // Stage 1: interleave a,b and c,d
        let ab_lo =
            i8x16_shuffle::<0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23>(a.0, b.0);
        let ab_hi =
            i8x16_shuffle::<8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31>(a.0, b.0);
        let cd_lo =
            i8x16_shuffle::<0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23>(c.0, d.0);
        let cd_hi =
            i8x16_shuffle::<8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31>(c.0, d.0);

        // Stage 2: interleave ab and cd at 16-bit granularity
        let out0 =
            i8x16_shuffle::<0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23>(ab_lo, cd_lo);
        let out1 = i8x16_shuffle::<8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31>(
            ab_lo, cd_lo,
        );
        let out2 =
            i8x16_shuffle::<0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23>(ab_hi, cd_hi);
        let out3 = i8x16_shuffle::<8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31>(
            ab_hi, cd_hi,
        );

        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, out0);
            v128_store(ptr.add(1), out1);
            v128_store(ptr.add(2), out2);
            v128_store(ptr.add(3), out3);
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U16VecSimd128(v128, Simd128Descriptor);

impl U16SimdVec for U16VecSimd128 {
    type Descriptor = Simd128Descriptor;
    const LEN: usize = 8;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[u16]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        Self(unsafe { v128_load(mem.as_ptr().cast()) }, d)
    }

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: u16) -> Self {
        Self(u16x8_splat(v), d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [u16]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space.
        unsafe { v128_store(mem.as_mut_ptr().cast(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2(a: Self, b: Self, dest: &mut [u16]) {
        assert!(dest.len() >= 2 * Self::LEN);
        let lo = i16x8_shuffle::<0, 8, 1, 9, 2, 10, 3, 11>(a.0, b.0);
        let hi = i16x8_shuffle::<4, 12, 5, 13, 6, 14, 7, 15>(a.0, b.0);
        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, lo);
            v128_store(ptr.add(1), hi);
        }
    }

    #[inline(always)]
    fn store_interleaved_3(a: Self, b: Self, c: Self, dest: &mut [u16]) {
        assert!(dest.len() >= 3 * Self::LEN);
        // 3-way u16 interleave using 2 shuffles per output vector.
        let ab0 = i16x8_shuffle::<0, 8, 0, 1, 9, 0, 2, 10>(a.0, b.0);
        let out0 = i16x8_shuffle::<0, 1, 8, 3, 4, 9, 6, 7>(ab0, c.0);

        let ab1 = i16x8_shuffle::<0, 3, 11, 0, 4, 12, 0, 5>(a.0, b.0);
        let out1 = i16x8_shuffle::<10, 1, 2, 11, 4, 5, 12, 7>(ab1, c.0);

        let ab2 = i16x8_shuffle::<13, 0, 6, 14, 0, 7, 15, 0>(a.0, b.0);
        let out2 = i16x8_shuffle::<0, 13, 2, 3, 14, 5, 6, 15>(ab2, c.0);

        // SAFETY: dest has 3*8 = 24 u16 elements.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, out0);
            v128_store(ptr.add(1), out1);
            v128_store(ptr.add(2), out2);
        }
    }

    #[inline(always)]
    fn store_interleaved_4(a: Self, b: Self, c: Self, d: Self, dest: &mut [u16]) {
        assert!(dest.len() >= 4 * Self::LEN);
        // Stage 1: interleave a,b and c,d at 16-bit level
        let ab_lo = i16x8_shuffle::<0, 8, 1, 9, 2, 10, 3, 11>(a.0, b.0);
        let ab_hi = i16x8_shuffle::<4, 12, 5, 13, 6, 14, 7, 15>(a.0, b.0);
        let cd_lo = i16x8_shuffle::<0, 8, 1, 9, 2, 10, 3, 11>(c.0, d.0);
        let cd_hi = i16x8_shuffle::<4, 12, 5, 13, 6, 14, 7, 15>(c.0, d.0);

        // Stage 2: interleave at 32-bit level
        let out0 = i32x4_shuffle::<0, 4, 1, 5>(ab_lo, cd_lo);
        let out1 = i32x4_shuffle::<2, 6, 3, 7>(ab_lo, cd_lo);
        let out2 = i32x4_shuffle::<0, 4, 1, 5>(ab_hi, cd_hi);
        let out3 = i32x4_shuffle::<2, 6, 3, 7>(ab_hi, cd_hi);

        // SAFETY: dest has enough space.
        unsafe {
            let ptr = dest.as_mut_ptr().cast::<v128>();
            v128_store(ptr, out0);
            v128_store(ptr.add(1), out1);
            v128_store(ptr.add(2), out2);
            v128_store(ptr.add(3), out3);
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct MaskSimd128(v128, Simd128Descriptor);

impl SimdMask for MaskSimd128 {
    type Descriptor = Simd128Descriptor;

    #[inline(always)]
    fn if_then_else_f32(self, if_true: F32VecSimd128, if_false: F32VecSimd128) -> F32VecSimd128 {
        #[cfg(target_feature = "relaxed-simd")]
        {
            F32VecSimd128(
                i32x4_relaxed_laneselect(if_true.0, if_false.0, self.0),
                self.1,
            )
        }
        #[cfg(not(target_feature = "relaxed-simd"))]
        {
            F32VecSimd128(v128_bitselect(if_true.0, if_false.0, self.0), self.1)
        }
    }

    #[inline(always)]
    fn if_then_else_i32(self, if_true: I32VecSimd128, if_false: I32VecSimd128) -> I32VecSimd128 {
        #[cfg(target_feature = "relaxed-simd")]
        {
            I32VecSimd128(
                i32x4_relaxed_laneselect(if_true.0, if_false.0, self.0),
                self.1,
            )
        }
        #[cfg(not(target_feature = "relaxed-simd"))]
        {
            I32VecSimd128(v128_bitselect(if_true.0, if_false.0, self.0), self.1)
        }
    }

    #[inline(always)]
    fn maskz_i32(self, v: I32VecSimd128) -> I32VecSimd128 {
        // Zero out lanes where mask is true: v AND NOT mask
        I32VecSimd128(v128_andnot(v.0, self.0), self.1)
    }

    #[inline(always)]
    fn andnot(self, rhs: MaskSimd128) -> MaskSimd128 {
        // !self & rhs
        MaskSimd128(v128_andnot(rhs.0, self.0), self.1)
    }

    #[inline(always)]
    fn all(self) -> bool {
        u32x4_all_true(self.0)
    }
}

impl BitAnd<MaskSimd128> for MaskSimd128 {
    type Output = MaskSimd128;
    #[inline(always)]
    fn bitand(self, rhs: MaskSimd128) -> MaskSimd128 {
        MaskSimd128(v128_and(self.0, rhs.0), self.1)
    }
}

impl BitOr<MaskSimd128> for MaskSimd128 {
    type Output = MaskSimd128;
    #[inline(always)]
    fn bitor(self, rhs: MaskSimd128) -> MaskSimd128 {
        MaskSimd128(v128_or(self.0, rhs.0), self.1)
    }
}
