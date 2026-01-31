// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{U32SimdVec, impl_f32_array_interface, x86_64::sse42::Sse42Descriptor};

use super::super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};
use std::{
    arch::x86_64::*,
    mem::MaybeUninit,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Shl, ShlAssign, Shr, ShrAssign, Sub, SubAssign,
    },
};

/// Core 8x8 transpose algorithm for AVX2.
/// Takes 8 __m256 vectors representing rows and returns 8 transposed vectors.
/// Used by both store_interleaved_8 and transpose_square.
#[target_feature(enable = "avx2")]
#[inline]
fn transpose_8x8_core(
    r0: __m256,
    r1: __m256,
    r2: __m256,
    r3: __m256,
    r4: __m256,
    r5: __m256,
    r6: __m256,
    r7: __m256,
) -> (
    __m256,
    __m256,
    __m256,
    __m256,
    __m256,
    __m256,
    __m256,
    __m256,
) {
    // Stage 1: Unpack low/high pairs
    let t0 = _mm256_unpacklo_ps(r0, r1);
    let t1 = _mm256_unpackhi_ps(r0, r1);
    let t2 = _mm256_unpacklo_ps(r2, r3);
    let t3 = _mm256_unpackhi_ps(r2, r3);
    let t4 = _mm256_unpacklo_ps(r4, r5);
    let t5 = _mm256_unpackhi_ps(r4, r5);
    let t6 = _mm256_unpacklo_ps(r6, r7);
    let t7 = _mm256_unpackhi_ps(r6, r7);

    // Stage 2: Shuffle to group 32-bit elements using 64-bit unpacks
    let s0 = _mm256_castpd_ps(_mm256_unpacklo_pd(
        _mm256_castps_pd(t0),
        _mm256_castps_pd(t2),
    ));
    let s1 = _mm256_castpd_ps(_mm256_unpackhi_pd(
        _mm256_castps_pd(t0),
        _mm256_castps_pd(t2),
    ));
    let s2 = _mm256_castpd_ps(_mm256_unpacklo_pd(
        _mm256_castps_pd(t1),
        _mm256_castps_pd(t3),
    ));
    let s3 = _mm256_castpd_ps(_mm256_unpackhi_pd(
        _mm256_castps_pd(t1),
        _mm256_castps_pd(t3),
    ));
    let s4 = _mm256_castpd_ps(_mm256_unpacklo_pd(
        _mm256_castps_pd(t4),
        _mm256_castps_pd(t6),
    ));
    let s5 = _mm256_castpd_ps(_mm256_unpackhi_pd(
        _mm256_castps_pd(t4),
        _mm256_castps_pd(t6),
    ));
    let s6 = _mm256_castpd_ps(_mm256_unpacklo_pd(
        _mm256_castps_pd(t5),
        _mm256_castps_pd(t7),
    ));
    let s7 = _mm256_castpd_ps(_mm256_unpackhi_pd(
        _mm256_castps_pd(t5),
        _mm256_castps_pd(t7),
    ));

    // Stage 3: 128-bit permute to finalize transpose
    let c0 = _mm256_permute2f128_ps::<0x20>(s0, s4);
    let c1 = _mm256_permute2f128_ps::<0x20>(s1, s5);
    let c2 = _mm256_permute2f128_ps::<0x20>(s2, s6);
    let c3 = _mm256_permute2f128_ps::<0x20>(s3, s7);
    let c4 = _mm256_permute2f128_ps::<0x31>(s0, s4);
    let c5 = _mm256_permute2f128_ps::<0x31>(s1, s5);
    let c6 = _mm256_permute2f128_ps::<0x31>(s2, s6);
    let c7 = _mm256_permute2f128_ps::<0x31>(s3, s7);

    (c0, c1, c2, c3, c4, c5, c6, c7)
}

// Safety invariant: this type is only ever constructed if avx2, fma, and f16c are available.
#[derive(Clone, Copy, Debug)]
pub struct AvxDescriptor(());

impl AvxDescriptor {
    /// # Safety
    /// The caller must guarantee that the "avx2", "fma", and "f16c" target features are available.
    pub unsafe fn new_unchecked() -> Self {
        Self(())
    }

    pub fn as_sse42(&self) -> Sse42Descriptor {
        // SAFETY: the safety invariant on `self` guarantees avx is available, which implies
        // sse42.
        unsafe { Sse42Descriptor::new_unchecked() }
    }
}

/// Prepared 8-entry lookup table for AVX2.
/// For AVX2, vpermps is both fast and exact, so we just store f32 values directly.
#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct Bf16Table8Avx(__m256);

impl SimdDescriptor for AvxDescriptor {
    type F32Vec = F32VecAvx;
    type I32Vec = I32VecAvx;
    type U32Vec = U32VecAvx;
    type Mask = MaskAvx;
    type Bf16Table8 = Bf16Table8Avx;

    type Descriptor256 = Self;
    type Descriptor128 = Sse42Descriptor;

    fn maybe_downgrade_256bit(self) -> Self::Descriptor256 {
        self
    }

    fn maybe_downgrade_128bit(self) -> Self::Descriptor128 {
        self.as_sse42()
    }

    fn new() -> Option<Self> {
        if is_x86_feature_detected!("avx2")
            && is_x86_feature_detected!("fma")
            && is_x86_feature_detected!("f16c")
        {
            // SAFETY: we just checked avx2, fma, and f16c.
            Some(unsafe { Self::new_unchecked() })
        } else {
            None
        }
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        #[target_feature(enable = "avx2,fma,f16c")]
        #[inline(never)]
        unsafe fn inner<R>(d: AvxDescriptor, f: impl FnOnce(AvxDescriptor) -> R) -> R {
            f(d)
        }
        // SAFETY: the safety invariant on `self` guarantees avx2, fma, and f16c.
        unsafe { inner(self, f) }
    }
}

// TODO(veluca): retire this macro once we have #[unsafe(target_feature)].
macro_rules! fn_avx {
    (
        $this:ident: $self_ty:ty,
        fn $name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )? $body: block) => {
        #[inline(always)]
        fn $name(self: $self_ty, $($arg: $ty),*) $(-> $ret)? {
            #[target_feature(enable = "fma,avx2,f16c")]
            #[inline]
            fn inner($this: $self_ty, $($arg: $ty),*) $(-> $ret)? {
                $body
            }
            // SAFETY: `self.1` is constructed iff avx2, fma, and f16c are available.
            unsafe { inner(self, $($arg),*) }
        }
    };
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct F32VecAvx(__m256, AvxDescriptor);

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct MaskAvx(__m256, AvxDescriptor);

// SAFETY: The methods in this implementation that write to `MaybeUninit` (store_interleaved_*)
// ensure that they write valid data to the output slice without reading uninitialized memory.
unsafe impl F32SimdVec for F32VecAvx {
    type Descriptor = AvxDescriptor;

    const LEN: usize = 8;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[f32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx is available
        // from the safety invariant on `d`.
        Self(unsafe { _mm256_loadu_ps(mem.as_ptr()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [f32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx is available
        // from the safety invariant on `self.1`.
        unsafe { _mm256_storeu_ps(mem.as_mut_ptr(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2_uninit(a: Self, b: Self, dest: &mut [MaybeUninit<f32>]) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn store_interleaved_2_impl(a: __m256, b: __m256, dest: &mut [MaybeUninit<f32>]) {
            assert!(dest.len() >= 2 * F32VecAvx::LEN);
            // a = [a0, a1, a2, a3, a4, a5, a6, a7], b = [b0, b1, b2, b3, b4, b5, b6, b7]
            // Output: [a0, b0, a1, b1, a2, b2, a3, b3, a4, b4, a5, b5, a6, b6, a7, b7]
            let lo = _mm256_unpacklo_ps(a, b); // [a0, b0, a1, b1, a4, b4, a5, b5]
            let hi = _mm256_unpackhi_ps(a, b); // [a2, b2, a3, b3, a6, b6, a7, b7]
            // Need to permute to get correct order
            let out0 = _mm256_permute2f128_ps::<0x20>(lo, hi); // lower halves: [a0,b0,a1,b1, a2,b2,a3,b3]
            let out1 = _mm256_permute2f128_ps::<0x31>(lo, hi); // upper halves: [a4,b4,a5,b5, a6,b6,a7,b7]
            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm256_storeu_ps(dest_ptr, out0);
                _mm256_storeu_ps(dest_ptr.add(8), out1);
            }
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_2_impl(a.0, b.0, dest) }
    }

    #[inline(always)]
    fn store_interleaved_3_uninit(a: Self, b: Self, c: Self, dest: &mut [MaybeUninit<f32>]) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn store_interleaved_3_impl(
            a: __m256,
            b: __m256,
            c: __m256,
            dest: &mut [MaybeUninit<f32>],
        ) {
            assert!(dest.len() >= 3 * F32VecAvx::LEN);

            let idx_a0 = _mm256_setr_epi32(0, 0, 0, 1, 0, 0, 2, 0);
            let idx_b0 = _mm256_setr_epi32(0, 0, 0, 0, 1, 0, 0, 2);
            let idx_c0 = _mm256_setr_epi32(0, 0, 0, 0, 0, 1, 0, 0);

            let two = _mm256_set1_epi32(2);
            let three = _mm256_set1_epi32(3);
            let five = _mm256_set1_epi32(5);
            let six = _mm256_set1_epi32(6);

            let a0 = _mm256_permutevar8x32_ps(a, idx_a0);
            let b0 = _mm256_permutevar8x32_ps(b, idx_b0);
            let c0 = _mm256_permutevar8x32_ps(c, idx_c0);
            let out0 = _mm256_blend_ps::<0b10010010>(a0, b0);
            let out0 = _mm256_blend_ps::<0b00100100>(out0, c0);

            let a1 = _mm256_permutevar8x32_ps(a, _mm256_add_epi32(idx_b0, three));
            let b1 = _mm256_permutevar8x32_ps(b, _mm256_add_epi32(idx_c0, three));
            let c1 = _mm256_permutevar8x32_ps(c, _mm256_add_epi32(idx_a0, two));
            let out1 = _mm256_blend_ps::<0b00100100>(a1, b1);
            let out1 = _mm256_blend_ps::<0b01001001>(out1, c1);

            let a2 = _mm256_permutevar8x32_ps(a, _mm256_add_epi32(idx_c0, six));
            let b2 = _mm256_permutevar8x32_ps(b, _mm256_add_epi32(idx_a0, five));
            let c2 = _mm256_permutevar8x32_ps(c, _mm256_add_epi32(idx_b0, five));
            let out2 = _mm256_blend_ps::<0b01001001>(a2, b2);
            let out2 = _mm256_blend_ps::<0b10010010>(out2, c2);

            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm256_storeu_ps(dest_ptr, out0);
                _mm256_storeu_ps(dest_ptr.add(8), out1);
                _mm256_storeu_ps(dest_ptr.add(16), out2);
            }
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_3_impl(a.0, b.0, c.0, dest) }
    }

    #[inline(always)]
    fn store_interleaved_4_uninit(
        a: Self,
        b: Self,
        c: Self,
        d: Self,
        dest: &mut [MaybeUninit<f32>],
    ) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn store_interleaved_4_impl(
            a: __m256,
            b: __m256,
            c: __m256,
            d: __m256,
            dest: &mut [MaybeUninit<f32>],
        ) {
            assert!(dest.len() >= 4 * F32VecAvx::LEN);
            // First interleave pairs
            let ab_lo = _mm256_unpacklo_ps(a, b);
            let ab_hi = _mm256_unpackhi_ps(a, b);
            let cd_lo = _mm256_unpacklo_ps(c, d);
            let cd_hi = _mm256_unpackhi_ps(c, d);

            // Cast to pd for 64-bit interleave
            let abcd_0 = _mm256_castpd_ps(_mm256_unpacklo_pd(
                _mm256_castps_pd(ab_lo),
                _mm256_castps_pd(cd_lo),
            ));
            let abcd_1 = _mm256_castpd_ps(_mm256_unpackhi_pd(
                _mm256_castps_pd(ab_lo),
                _mm256_castps_pd(cd_lo),
            ));
            let abcd_2 = _mm256_castpd_ps(_mm256_unpacklo_pd(
                _mm256_castps_pd(ab_hi),
                _mm256_castps_pd(cd_hi),
            ));
            let abcd_3 = _mm256_castpd_ps(_mm256_unpackhi_pd(
                _mm256_castps_pd(ab_hi),
                _mm256_castps_pd(cd_hi),
            ));

            // Permute to get correct order across lanes
            let out0 = _mm256_permute2f128_ps::<0x20>(abcd_0, abcd_1);
            let out1 = _mm256_permute2f128_ps::<0x20>(abcd_2, abcd_3);
            let out2 = _mm256_permute2f128_ps::<0x31>(abcd_0, abcd_1);
            let out3 = _mm256_permute2f128_ps::<0x31>(abcd_2, abcd_3);

            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm256_storeu_ps(dest_ptr, out0);
                _mm256_storeu_ps(dest_ptr.add(8), out1);
                _mm256_storeu_ps(dest_ptr.add(16), out2);
                _mm256_storeu_ps(dest_ptr.add(24), out3);
            }
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_4_impl(a.0, b.0, c.0, d.0, dest) }
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
        #[target_feature(enable = "avx2")]
        #[inline]
        fn store_interleaved_8_impl(
            r0: __m256,
            r1: __m256,
            r2: __m256,
            r3: __m256,
            r4: __m256,
            r5: __m256,
            r6: __m256,
            r7: __m256,
            dest: &mut [f32],
        ) {
            assert!(dest.len() >= 8 * F32VecAvx::LEN);
            // This is essentially an 8x8 transpose, same algorithm as transpose_square
            let (c0, c1, c2, c3, c4, c5, c6, c7) =
                transpose_8x8_core(r0, r1, r2, r3, r4, r5, r6, r7);

            // SAFETY: we just checked that dest has enough space.
            unsafe {
                _mm256_storeu_ps(dest.as_mut_ptr(), c0);
                _mm256_storeu_ps(dest.as_mut_ptr().add(8), c1);
                _mm256_storeu_ps(dest.as_mut_ptr().add(16), c2);
                _mm256_storeu_ps(dest.as_mut_ptr().add(24), c3);
                _mm256_storeu_ps(dest.as_mut_ptr().add(32), c4);
                _mm256_storeu_ps(dest.as_mut_ptr().add(40), c5);
                _mm256_storeu_ps(dest.as_mut_ptr().add(48), c6);
                _mm256_storeu_ps(dest.as_mut_ptr().add(56), c7);
            }
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_8_impl(a.0, b.0, c.0, d.0, e.0, f.0, g.0, h.0, dest) }
    }

    #[inline(always)]
    fn load_deinterleaved_2(d: Self::Descriptor, src: &[f32]) -> (Self, Self) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn load_deinterleaved_2_impl(src: &[f32]) -> (__m256, __m256) {
            assert!(src.len() >= 2 * F32VecAvx::LEN);
            // Input: [a0, b0, a1, b1, a2, b2, a3, b3, a4, b4, a5, b5, a6, b6, a7, b7]
            // Output: a = [a0, a1, a2, a3, a4, a5, a6, a7], b = [b0, b1, b2, b3, b4, b5, b6, b7]
            // SAFETY: we just checked that src has enough space.
            let (in0, in1) = unsafe {
                (
                    _mm256_loadu_ps(src.as_ptr()),        // [a0,b0,a1,b1, a2,b2,a3,b3]
                    _mm256_loadu_ps(src.as_ptr().add(8)), // [a4,b4,a5,b5, a6,b6,a7,b7]
                )
            };

            // Reverse the store_interleaved_2 operation
            // First, undo the permute2f128
            let lo = _mm256_permute2f128_ps::<0x20>(in0, in1); // [a0,b0,a1,b1, a4,b4,a5,b5]
            let hi = _mm256_permute2f128_ps::<0x31>(in0, in1); // [a2,b2,a3,b3, a6,b6,a7,b7]

            // Then undo the unpack - use shuffle to separate a and b
            let a_lo = _mm256_shuffle_ps::<0x88>(lo, hi); // [a0, a1, a2, a3, a4, a5, a6, a7]
            let b_lo = _mm256_shuffle_ps::<0xDD>(lo, hi); // [b0, b1, b2, b3, b4, b5, b6, b7]

            (a_lo, b_lo)
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        let (a, b) = unsafe { load_deinterleaved_2_impl(src) };
        (Self(a, d), Self(b, d))
    }

    #[inline(always)]
    fn load_deinterleaved_3(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn load_deinterleaved_3_impl(src: &[f32]) -> (__m256, __m256, __m256) {
            assert!(src.len() >= 3 * F32VecAvx::LEN);
            // Input layout (24 floats):
            // in0: [a0, b0, c0, a1, b1, c1, a2, b2]
            // in1: [c2, a3, b3, c3, a4, b4, c4, a5]
            // in2: [b5, c5, a6, b6, c6, a7, b7, c7]
            // Output: a = [a0..a7], b = [b0..b7], c = [c0..c7]

            // SAFETY: we just checked that src has enough space.
            let (in0, in1, in2) = unsafe {
                (
                    _mm256_loadu_ps(src.as_ptr()),
                    _mm256_loadu_ps(src.as_ptr().add(8)),
                    _mm256_loadu_ps(src.as_ptr().add(16)),
                )
            };

            // Use permutevar8x32 to gather elements into correct positions, then blend.
            // permutevar8x32(src, idx): output[i] = src[idx[i]]

            // a: a0=in0[0], a1=in0[3], a2=in0[6], a3=in1[1], a4=in1[4], a5=in1[7], a6=in2[2], a7=in2[5]
            let perm_a0 = _mm256_setr_epi32(0, 3, 6, 0, 0, 0, 0, 0);
            let perm_a1 = _mm256_setr_epi32(0, 0, 0, 1, 4, 7, 0, 0);
            let perm_a2 = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, 2, 5);
            let a0 = _mm256_permutevar8x32_ps(in0, perm_a0);
            let a1 = _mm256_permutevar8x32_ps(in1, perm_a1);
            let a2 = _mm256_permutevar8x32_ps(in2, perm_a2);
            let a_out = _mm256_blend_ps::<0b00111000>(a0, a1); // positions 3,4,5 from a1
            let a_out = _mm256_blend_ps::<0b11000000>(a_out, a2); // positions 6,7 from a2

            // b: b0=in0[1], b1=in0[4], b2=in0[7], b3=in1[2], b4=in1[5], b5=in2[0], b6=in2[3], b7=in2[6]
            let perm_b0 = _mm256_setr_epi32(1, 4, 7, 0, 0, 0, 0, 0);
            let perm_b1 = _mm256_setr_epi32(0, 0, 0, 2, 5, 0, 0, 0);
            let perm_b2 = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, 3, 6);
            let b0 = _mm256_permutevar8x32_ps(in0, perm_b0);
            let b1 = _mm256_permutevar8x32_ps(in1, perm_b1);
            let b2 = _mm256_permutevar8x32_ps(in2, perm_b2);
            let b_out = _mm256_blend_ps::<0b00011000>(b0, b1); // positions 3,4 from b1
            let b_out = _mm256_blend_ps::<0b11100000>(b_out, b2); // positions 5,6,7 from b2

            // c: c0=in0[2], c1=in0[5], c2=in1[0], c3=in1[3], c4=in1[6], c5=in2[1], c6=in2[4], c7=in2[7]
            let perm_c0 = _mm256_setr_epi32(2, 5, 0, 0, 0, 0, 0, 0);
            let perm_c1 = _mm256_setr_epi32(0, 0, 0, 3, 6, 0, 0, 0);
            let perm_c2 = _mm256_setr_epi32(0, 0, 0, 0, 0, 1, 4, 7);
            let c0 = _mm256_permutevar8x32_ps(in0, perm_c0);
            let c1 = _mm256_permutevar8x32_ps(in1, perm_c1);
            let c2 = _mm256_permutevar8x32_ps(in2, perm_c2);
            let c_out = _mm256_blend_ps::<0b00011100>(c0, c1); // positions 2,3,4 from c1
            let c_out = _mm256_blend_ps::<0b11100000>(c_out, c2); // positions 5,6,7 from c2

            (a_out, b_out, c_out)
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        let (a, b, c) = unsafe { load_deinterleaved_3_impl(src) };
        (Self(a, d), Self(b, d), Self(c, d))
    }

    #[inline(always)]
    fn load_deinterleaved_4(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self, Self) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn load_deinterleaved_4_impl(src: &[f32]) -> (__m256, __m256, __m256, __m256) {
            assert!(src.len() >= 4 * F32VecAvx::LEN);
            // Input: [a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3, ...]
            // Output: a = [a0..a7], b = [b0..b7], c = [c0..c7], d = [d0..d7]
            // SAFETY: we just checked that src has enough space.
            let (in0, in1, in2, in3) = unsafe {
                (
                    _mm256_loadu_ps(src.as_ptr()),         // [a0,b0,c0,d0, a1,b1,c1,d1]
                    _mm256_loadu_ps(src.as_ptr().add(8)),  // [a2,b2,c2,d2, a3,b3,c3,d3]
                    _mm256_loadu_ps(src.as_ptr().add(16)), // [a4,b4,c4,d4, a5,b5,c5,d5]
                    _mm256_loadu_ps(src.as_ptr().add(24)), // [a6,b6,c6,d6, a7,b7,c7,d7]
                )
            };

            // This is essentially an 8x4 to 4x8 transpose
            // First, unpack pairs
            let t0 = _mm256_unpacklo_ps(in0, in1); // [a0,a2,b0,b2, a1,a3,b1,b3]
            let t1 = _mm256_unpackhi_ps(in0, in1); // [c0,c2,d0,d2, c1,c3,d1,d3]
            let t2 = _mm256_unpacklo_ps(in2, in3); // [a4,a6,b4,b6, a5,a7,b5,b7]
            let t3 = _mm256_unpackhi_ps(in2, in3); // [c4,c6,d4,d6, c5,c7,d5,d7]

            // Second level unpack
            let u0 = _mm256_unpacklo_ps(t0, t2); // [a0,a4,a2,a6, a1,a5,a3,a7]
            let u1 = _mm256_unpackhi_ps(t0, t2); // [b0,b4,b2,b6, b1,b5,b3,b7]
            let u2 = _mm256_unpacklo_ps(t1, t3); // [c0,c4,c2,c6, c1,c5,c3,c7]
            let u3 = _mm256_unpackhi_ps(t1, t3); // [d0,d4,d2,d6, d1,d5,d3,d7]

            // Permute to get correct order
            let perm = _mm256_setr_epi32(0, 4, 2, 6, 1, 5, 3, 7);
            let a = _mm256_permutevar8x32_ps(u0, perm);
            let b = _mm256_permutevar8x32_ps(u1, perm);
            let c = _mm256_permutevar8x32_ps(u2, perm);
            let dv = _mm256_permutevar8x32_ps(u3, perm);

            (a, b, c, dv)
        }

        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        let (a, b, c, dv) = unsafe { load_deinterleaved_4_impl(src) };
        (Self(a, d), Self(b, d), Self(c, d), Self(dv, d))
    }

    fn_avx!(this: F32VecAvx, fn mul_add(mul: F32VecAvx, add: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_fmadd_ps(this.0, mul.0, add.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn neg_mul_add(mul: F32VecAvx, add: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_fnmadd_ps(this.0, mul.0, add.0), this.1)
    });

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: f32) -> Self {
        // SAFETY: We know avx is available from the safety invariant on `d`.
        unsafe { Self(_mm256_set1_ps(v), d) }
    }

    #[inline(always)]
    fn zero(d: Self::Descriptor) -> Self {
        // SAFETY: We know avx is available from the safety invariant on `d`.
        unsafe { Self(_mm256_setzero_ps(), d) }
    }

    fn_avx!(this: F32VecAvx, fn abs() -> F32VecAvx {
        F32VecAvx(_mm256_andnot_ps(_mm256_set1_ps(-0.0), this.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn floor() -> F32VecAvx {
        F32VecAvx(_mm256_floor_ps(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn sqrt() -> F32VecAvx {
        F32VecAvx(_mm256_sqrt_ps(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn neg() -> F32VecAvx {
        F32VecAvx(_mm256_xor_ps(_mm256_set1_ps(-0.0), this.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn copysign(sign: F32VecAvx) -> F32VecAvx {
        let sign_mask = _mm256_castsi256_ps(_mm256_set1_epi32(i32::MIN));
        F32VecAvx(
            _mm256_or_ps(
                _mm256_andnot_ps(sign_mask, this.0),
                _mm256_and_ps(sign_mask, sign.0),
            ),
            this.1,
        )
    });

    fn_avx!(this: F32VecAvx, fn max(other: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_max_ps(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn min(other: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_min_ps(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn gt(other: F32VecAvx) -> MaskAvx {
        MaskAvx(_mm256_cmp_ps::<{_CMP_GT_OQ}>(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn as_i32() -> I32VecAvx {
        I32VecAvx(_mm256_cvtps_epi32(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn bitcast_to_i32() -> I32VecAvx {
        I32VecAvx(_mm256_castps_si256(this.0), this.1)
    });

    #[inline(always)]
    fn prepare_table_bf16_8(_d: AvxDescriptor, table: &[f32; 8]) -> Bf16Table8Avx {
        // For AVX2, vpermps is exact and fast, so we just load the table as-is
        // SAFETY: avx2 is available from the safety invariant on the descriptor,
        // and `table` has 8 elements, exactly as many as we load.
        Bf16Table8Avx(unsafe { _mm256_loadu_ps(table.as_ptr()) })
    }

    #[inline(always)]
    fn table_lookup_bf16_8(d: AvxDescriptor, table: Bf16Table8Avx, indices: I32VecAvx) -> Self {
        // SAFETY: avx2 is available from the safety invariant on the descriptor
        F32VecAvx(unsafe { _mm256_permutevar8x32_ps(table.0, indices.0) }, d)
    }

    #[inline(always)]
    fn round_store_u8(self, dest: &mut [u8]) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn round_store_u8_impl(v: __m256, dest: &mut [u8]) {
            assert!(dest.len() >= F32VecAvx::LEN);
            // Round to nearest integer
            let rounded = _mm256_round_ps::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(v);
            // Convert to i32
            let i32s = _mm256_cvtps_epi32(rounded);
            // Extract 128-bit halves and pack
            let lo = _mm256_castsi256_si128(i32s);
            let hi = _mm256_extracti128_si256::<1>(i32s);
            // Pack 4+4 i32s to 8 u16s
            let u16s = _mm_packus_epi32(lo, hi);
            // Pack 8 u16s to 8 u8s (use same vector twice, take lower half)
            let u8s = _mm_packus_epi16(u16s, u16s);
            // Store lower 8 bytes
            // SAFETY: we checked dest has enough space
            unsafe {
                _mm_storel_epi64(dest.as_mut_ptr() as *mut __m128i, u8s);
            }
        }
        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { round_store_u8_impl(self.0, dest) }
    }

    #[inline(always)]
    fn round_store_u16(self, dest: &mut [u16]) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn round_store_u16_impl(v: __m256, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecAvx::LEN);
            // Round to nearest integer
            let rounded = _mm256_round_ps::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(v);
            // Convert to i32
            let i32s = _mm256_cvtps_epi32(rounded);
            // Extract 128-bit halves and pack
            let lo = _mm256_castsi256_si128(i32s);
            let hi = _mm256_extracti128_si256::<1>(i32s);
            // Pack 4+4 i32s to 8 u16s
            let u16s = _mm_packus_epi32(lo, hi);
            // Store 8 u16s (16 bytes)
            // SAFETY: we checked dest has enough space
            unsafe {
                _mm_storeu_si128(dest.as_mut_ptr() as *mut __m128i, u16s);
            }
        }
        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { round_store_u16_impl(self.0, dest) }
    }

    impl_f32_array_interface!();

    #[inline(always)]
    fn load_f16_bits(d: Self::Descriptor, mem: &[u16]) -> Self {
        #[target_feature(enable = "avx2,f16c")]
        #[inline]
        fn load_f16_impl(d: AvxDescriptor, mem: &[u16]) -> F32VecAvx {
            assert!(mem.len() >= F32VecAvx::LEN);
            // SAFETY: mem.len() >= 8 is checked above
            let bits = unsafe { _mm_loadu_si128(mem.as_ptr() as *const __m128i) };
            F32VecAvx(_mm256_cvtph_ps(bits), d)
        }
        // SAFETY: avx2 and f16c are available from the safety invariant on the descriptor
        unsafe { load_f16_impl(d, mem) }
    }

    #[inline(always)]
    fn store_f16_bits(self, dest: &mut [u16]) {
        #[target_feature(enable = "avx2,f16c")]
        #[inline]
        fn store_f16_bits_impl(v: __m256, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecAvx::LEN);
            let bits = _mm256_cvtps_ph::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(v);
            // SAFETY: dest.len() >= 8 is checked above
            unsafe { _mm_storeu_si128(dest.as_mut_ptr() as *mut __m128i, bits) };
        }
        // SAFETY: avx2 and f16c are available from the safety invariant on the descriptor
        unsafe { store_f16_bits_impl(self.0, dest) }
    }

    #[inline(always)]
    fn transpose_square(d: Self::Descriptor, data: &mut [Self::UnderlyingArray], stride: usize) {
        #[target_feature(enable = "avx2")]
        #[inline]
        unsafe fn transpose8x8f32(d: AvxDescriptor, data: &mut [[f32; 8]], stride: usize) {
            assert!(data.len() > stride * 7);

            let r0 = F32VecAvx::load_array(d, &data[0]).0;
            let r1 = F32VecAvx::load_array(d, &data[1 * stride]).0;
            let r2 = F32VecAvx::load_array(d, &data[2 * stride]).0;
            let r3 = F32VecAvx::load_array(d, &data[3 * stride]).0;
            let r4 = F32VecAvx::load_array(d, &data[4 * stride]).0;
            let r5 = F32VecAvx::load_array(d, &data[5 * stride]).0;
            let r6 = F32VecAvx::load_array(d, &data[6 * stride]).0;
            let r7 = F32VecAvx::load_array(d, &data[7 * stride]).0;

            let (c0, c1, c2, c3, c4, c5, c6, c7) =
                transpose_8x8_core(r0, r1, r2, r3, r4, r5, r6, r7);

            F32VecAvx(c0, d).store_array(&mut data[0]);
            F32VecAvx(c1, d).store_array(&mut data[1 * stride]);
            F32VecAvx(c2, d).store_array(&mut data[2 * stride]);
            F32VecAvx(c3, d).store_array(&mut data[3 * stride]);
            F32VecAvx(c4, d).store_array(&mut data[4 * stride]);
            F32VecAvx(c5, d).store_array(&mut data[5 * stride]);
            F32VecAvx(c6, d).store_array(&mut data[6 * stride]);
            F32VecAvx(c7, d).store_array(&mut data[7 * stride]);
        }
        // SAFETY: the safety invariant on `d` guarantees avx2
        unsafe {
            transpose8x8f32(d, data, stride);
        }
    }
}

impl Add<F32VecAvx> for F32VecAvx {
    type Output = F32VecAvx;
    fn_avx!(this: F32VecAvx, fn add(rhs: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_add_ps(this.0, rhs.0), this.1)
    });
}

impl Sub<F32VecAvx> for F32VecAvx {
    type Output = F32VecAvx;
    fn_avx!(this: F32VecAvx, fn sub(rhs: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_sub_ps(this.0, rhs.0), this.1)
    });
}

impl Mul<F32VecAvx> for F32VecAvx {
    type Output = F32VecAvx;
    fn_avx!(this: F32VecAvx, fn mul(rhs: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_mul_ps(this.0, rhs.0), this.1)
    });
}

impl Div<F32VecAvx> for F32VecAvx {
    type Output = F32VecAvx;
    fn_avx!(this: F32VecAvx, fn div(rhs: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_div_ps(this.0, rhs.0), this.1)
    });
}

impl AddAssign<F32VecAvx> for F32VecAvx {
    fn_avx!(this: &mut F32VecAvx, fn add_assign(rhs: F32VecAvx) {
        this.0 = _mm256_add_ps(this.0, rhs.0)
    });
}

impl SubAssign<F32VecAvx> for F32VecAvx {
    fn_avx!(this: &mut F32VecAvx, fn sub_assign(rhs: F32VecAvx) {
        this.0 = _mm256_sub_ps(this.0, rhs.0)
    });
}

impl MulAssign<F32VecAvx> for F32VecAvx {
    fn_avx!(this: &mut F32VecAvx, fn mul_assign(rhs: F32VecAvx) {
        this.0 = _mm256_mul_ps(this.0, rhs.0)
    });
}

impl DivAssign<F32VecAvx> for F32VecAvx {
    fn_avx!(this: &mut F32VecAvx, fn div_assign(rhs: F32VecAvx) {
        this.0 = _mm256_div_ps(this.0, rhs.0)
    });
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct I32VecAvx(__m256i, AvxDescriptor);

impl I32SimdVec for I32VecAvx {
    type Descriptor = AvxDescriptor;

    const LEN: usize = 8;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[i32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx is available
        // from the safety invariant on `d`.
        Self(unsafe { _mm256_loadu_si256(mem.as_ptr() as *const _) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [i32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx is available
        // from the safety invariant on `self.1`.
        unsafe { _mm256_storeu_si256(mem.as_mut_ptr().cast(), self.0) }
    }

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: i32) -> Self {
        // SAFETY: We know avx is available from the safety invariant on `d`.
        unsafe { Self(_mm256_set1_epi32(v), d) }
    }

    fn_avx!(this: I32VecAvx, fn as_f32() -> F32VecAvx {
        F32VecAvx(_mm256_cvtepi32_ps(this.0), this.1)
    });

    fn_avx!(this: I32VecAvx, fn bitcast_to_f32() -> F32VecAvx {
        F32VecAvx(_mm256_castsi256_ps(this.0), this.1)
    });

    #[inline(always)]
    fn bitcast_to_u32(self) -> U32VecAvx {
        U32VecAvx(self.0, self.1)
    }

    fn_avx!(this: I32VecAvx, fn abs() -> I32VecAvx {
        I32VecAvx(
            _mm256_abs_epi32(this.0),
            this.1)
    });

    fn_avx!(this: I32VecAvx, fn gt(rhs: I32VecAvx) -> MaskAvx {
        MaskAvx(
            _mm256_castsi256_ps(_mm256_cmpgt_epi32(this.0, rhs.0)),
            this.1,
        )
    });

    fn_avx!(this: I32VecAvx, fn lt_zero() -> MaskAvx {
        I32VecAvx(_mm256_setzero_si256(), this.1).gt(this)
    });

    fn_avx!(this: I32VecAvx, fn eq(rhs: I32VecAvx) -> MaskAvx {
        MaskAvx(
            _mm256_castsi256_ps(_mm256_cmpeq_epi32(this.0, rhs.0)),
            this.1,
        )
    });

    fn_avx!(this: I32VecAvx, fn eq_zero() -> MaskAvx {
        this.eq(I32VecAvx(_mm256_setzero_si256(), this.1))
    });

    #[inline(always)]
    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know avx2 is available from the safety invariant on `d`.
        unsafe { I32VecAvx(_mm256_slli_epi32::<AMOUNT_I>(self.0), self.1) }
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know avx2 is available from the safety invariant on `d`.
        unsafe { I32VecAvx(_mm256_srai_epi32::<AMOUNT_I>(self.0), self.1) }
    }

    fn_avx!(this: I32VecAvx, fn mul_wide_take_high(rhs: I32VecAvx) -> I32VecAvx {
        let l = _mm256_mul_epi32(this.0, rhs.0);
        let h = _mm256_mul_epi32(_mm256_srli_epi64::<32>(this.0), _mm256_srli_epi64::<32>(rhs.0));
        let p0 = _mm256_unpacklo_epi32(l, h);
        let p1 = _mm256_unpackhi_epi32(l, h);
        I32VecAvx(_mm256_unpackhi_epi64(p0, p1), this.1)
    });

    #[inline(always)]
    fn store_u16(self, dest: &mut [u16]) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn store_u16_impl(v: __m256i, dest: &mut [u16]) {
            assert!(dest.len() >= I32VecAvx::LEN);
            let tmp = _mm256_shuffle_epi8(
                v,
                _mm256_setr_epi8(
                    0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15, //
                    0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15,
                ),
            );
            let tmp = _mm256_permute4x64_epi64(tmp, 0xD8);
            // SAFETY: we just checked that `dest` has enough space.
            unsafe {
                _mm_storeu_si128(dest.as_mut_ptr().cast(), _mm256_extracti128_si256::<0>(tmp))
            };
        }
        // SAFETY: avx2 is available from the safety invariant on the descriptor.
        unsafe { store_u16_impl(self.0, dest) }
    }
}

impl Add<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn add(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_add_epi32(this.0, rhs.0), this.1)
    });
}

impl Sub<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn sub(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_sub_epi32(this.0, rhs.0), this.1)
    });
}

impl Mul<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn mul(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_mullo_epi32(this.0, rhs.0), this.1)
    });
}

impl Shl<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn shl(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_sllv_epi32(this.0, rhs.0), this.1)
    });
}

impl Shr<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn shr(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_srav_epi32(this.0, rhs.0), this.1)
    });
}

impl Neg for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn neg() -> I32VecAvx {
        I32VecAvx(_mm256_setzero_si256(), this.1) - this
    });
}

impl BitAnd<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn bitand(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_and_si256(this.0, rhs.0), this.1)
    });
}

impl BitOr<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn bitor(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_or_si256(this.0, rhs.0), this.1)
    });
}

impl BitXor<I32VecAvx> for I32VecAvx {
    type Output = I32VecAvx;
    fn_avx!(this: I32VecAvx, fn bitxor(rhs: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_xor_si256(this.0, rhs.0), this.1)
    });
}

impl AddAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn add_assign(rhs: I32VecAvx) {
        this.0 = _mm256_add_epi32(this.0, rhs.0)
    });
}

impl SubAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn sub_assign(rhs: I32VecAvx) {
        this.0 = _mm256_sub_epi32(this.0, rhs.0)
    });
}

impl MulAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn mul_assign(rhs: I32VecAvx) {
        this.0 = _mm256_mullo_epi32(this.0, rhs.0)
    });
}

impl ShlAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn shl_assign(rhs: I32VecAvx) {
        this.0 = _mm256_sllv_epi32(this.0, rhs.0)
    });
}

impl ShrAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn shr_assign(rhs: I32VecAvx) {
        this.0 = _mm256_srav_epi32(this.0, rhs.0)
    });
}

impl BitAndAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn bitand_assign(rhs: I32VecAvx) {
        this.0 = _mm256_and_si256(this.0, rhs.0)
    });
}

impl BitOrAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn bitor_assign(rhs: I32VecAvx) {
        this.0 = _mm256_or_si256(this.0, rhs.0)
    });
}

impl BitXorAssign<I32VecAvx> for I32VecAvx {
    fn_avx!(this: &mut I32VecAvx, fn bitxor_assign(rhs: I32VecAvx) {
        this.0 = _mm256_xor_si256(this.0, rhs.0)
    });
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U32VecAvx(__m256i, AvxDescriptor);

impl U32SimdVec for U32VecAvx {
    type Descriptor = AvxDescriptor;

    const LEN: usize = 8;

    #[inline(always)]
    fn bitcast_to_i32(self) -> I32VecAvx {
        I32VecAvx(self.0, self.1)
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know avx2 is available from the safety invariant on `self.1`.
        unsafe { Self(_mm256_srli_epi32::<AMOUNT_I>(self.0), self.1) }
    }
}

impl SimdMask for MaskAvx {
    type Descriptor = AvxDescriptor;

    fn_avx!(this: MaskAvx, fn if_then_else_f32(if_true: F32VecAvx, if_false: F32VecAvx) -> F32VecAvx {
        F32VecAvx(_mm256_blendv_ps(if_false.0, if_true.0, this.0), this.1)
    });

    fn_avx!(this: MaskAvx, fn if_then_else_i32(if_true: I32VecAvx, if_false: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_blendv_epi8(if_false.0, if_true.0, _mm256_castps_si256(this.0)), this.1)
    });

    fn_avx!(this: MaskAvx, fn maskz_i32(v: I32VecAvx) -> I32VecAvx {
        I32VecAvx(_mm256_andnot_si256(_mm256_castps_si256(this.0), v.0), this.1)
    });

    fn_avx!(this: MaskAvx, fn all() -> bool {
        _mm256_movemask_ps(this.0) == 0b11111111
    });

    fn_avx!(this: MaskAvx, fn andnot(rhs: MaskAvx) -> MaskAvx {
        MaskAvx(_mm256_andnot_ps(this.0, rhs.0), this.1)
    });
}

impl BitAnd<MaskAvx> for MaskAvx {
    type Output = MaskAvx;
    fn_avx!(this: MaskAvx, fn bitand(rhs: MaskAvx) -> MaskAvx {
        MaskAvx(_mm256_and_ps(this.0, rhs.0), this.1)
    });
}

impl BitOr<MaskAvx> for MaskAvx {
    type Output = MaskAvx;
    fn_avx!(this: MaskAvx, fn bitor(rhs: MaskAvx) -> MaskAvx {
        MaskAvx(_mm256_or_ps(this.0, rhs.0), this.1)
    });
}
