// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::super::{AvxDescriptor, F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};
use crate::{Sse42Descriptor, U32SimdVec, impl_f32_array_interface};
use std::{
    arch::x86_64::*,
    mem::MaybeUninit,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Shl, ShlAssign, Shr, ShrAssign, Sub, SubAssign,
    },
};

// Safety invariant: this type is only ever constructed if avx512f and avx512bw are available.
#[derive(Clone, Copy, Debug)]
pub struct Avx512Descriptor(());

#[allow(unused)]
impl Avx512Descriptor {
    /// # Safety
    /// The caller must guarantee that "avx512f" and "avx512bw" target features are available.
    pub unsafe fn new_unchecked() -> Self {
        Self(())
    }
    pub fn as_avx(&self) -> AvxDescriptor {
        // SAFETY: the safety invariant on `self` guarantees avx512f is available, which implies
        // avx2 and fma.
        unsafe { AvxDescriptor::new_unchecked() }
    }
}

/// Prepared 8-entry lookup table for AVX512.
/// For AVX512, vpermutexvar_ps is both fast and exact, so we store f32 values
/// duplicated to fill a 512-bit register.
#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct Bf16Table8Avx512(__m512);

impl SimdDescriptor for Avx512Descriptor {
    type F32Vec = F32VecAvx512;
    type I32Vec = I32VecAvx512;
    type U32Vec = U32VecAvx512;
    type Mask = MaskAvx512;
    type Bf16Table8 = Bf16Table8Avx512;

    type Descriptor256 = AvxDescriptor;
    type Descriptor128 = Sse42Descriptor;

    fn maybe_downgrade_256bit(self) -> Self::Descriptor256 {
        self.as_avx()
    }

    fn maybe_downgrade_128bit(self) -> Self::Descriptor128 {
        self.as_avx().as_sse42()
    }

    fn new() -> Option<Self> {
        if is_x86_feature_detected!("avx512f") && is_x86_feature_detected!("avx512bw") {
            // SAFETY: we just checked avx512f and avx512bw.
            Some(Self(()))
        } else {
            None
        }
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        #[target_feature(enable = "avx512f")]
        #[inline(never)]
        unsafe fn inner<R>(d: Avx512Descriptor, f: impl FnOnce(Avx512Descriptor) -> R) -> R {
            f(d)
        }
        // SAFETY: the safety invariant on `self` guarantees avx512f.
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
            #[target_feature(enable = "avx512f")]
            #[inline]
            fn inner($this: $self_ty, $($arg: $ty),*) $(-> $ret)? {
                $body
            }
            // SAFETY: `self.1` is constructed iff avx512f is available.
            unsafe { inner(self, $($arg),*) }
        }
    };
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct F32VecAvx512(__m512, Avx512Descriptor);

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct MaskAvx512(__mmask16, Avx512Descriptor);

// SAFETY: The methods in this implementation that write to `MaybeUninit` (store_interleaved_*)
// ensure that they write valid data to the output slice without reading uninitialized memory.
unsafe impl F32SimdVec for F32VecAvx512 {
    type Descriptor = Avx512Descriptor;

    const LEN: usize = 16;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[f32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx512f is available
        // from the safety invariant on `d`.
        Self(unsafe { _mm512_loadu_ps(mem.as_ptr()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [f32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx512f is available
        // from the safety invariant on `self.1`.
        unsafe { _mm512_storeu_ps(mem.as_mut_ptr(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2_uninit(a: Self, b: Self, dest: &mut [MaybeUninit<f32>]) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn store_interleaved_2_impl(a: __m512, b: __m512, dest: &mut [MaybeUninit<f32>]) {
            assert!(dest.len() >= 2 * F32VecAvx512::LEN);
            // a = [a0..a15], b = [b0..b15]
            // Output: [a0, b0, a1, b1, ..., a15, b15]
            // unpacklo within each 128-bit lane: lane0=[a0,b0,a1,b1], lane1=[a4,b4,a5,b5], etc.
            let lo = _mm512_unpacklo_ps(a, b);
            // unpackhi within each 128-bit lane: lane0=[a2,b2,a3,b3], lane1=[a6,b6,a7,b7], etc.
            let hi = _mm512_unpackhi_ps(a, b);

            // Permute to interleave 128-bit chunks from lo and hi
            // out0 needs: lo lanes 0,1 interleaved with hi lanes 0,1
            let idx_lo = _mm512_setr_epi32(0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23);
            // out1 needs: lo lanes 2,3 interleaved with hi lanes 2,3
            let idx_hi =
                _mm512_setr_epi32(8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31);

            let out0 = _mm512_permutex2var_ps(lo, idx_lo, hi);
            let out1 = _mm512_permutex2var_ps(lo, idx_hi, hi);

            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm512_storeu_ps(dest_ptr, out0);
                _mm512_storeu_ps(dest_ptr.add(16), out1);
            }
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_2_impl(a.0, b.0, dest) }
    }

    #[inline(always)]
    fn store_interleaved_3_uninit(a: Self, b: Self, c: Self, dest: &mut [MaybeUninit<f32>]) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn store_interleaved_3_impl(
            a: __m512,
            b: __m512,
            c: __m512,
            dest: &mut [MaybeUninit<f32>],
        ) {
            assert!(dest.len() >= 3 * F32VecAvx512::LEN);

            let idx_ab0 = _mm512_setr_epi32(0, 16, 0, 1, 17, 0, 2, 18, 0, 3, 19, 0, 4, 20, 0, 5);
            let idx_c0 = _mm512_setr_epi32(0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0);

            let idx_ab1 = _mm512_setr_epi32(21, 0, 6, 22, 0, 7, 23, 0, 8, 24, 0, 9, 25, 0, 10, 26);
            let idx_c1 = _mm512_setr_epi32(0, 5, 0, 0, 6, 0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0);

            let idx_ab2 =
                _mm512_setr_epi32(0, 11, 27, 0, 12, 28, 0, 13, 29, 0, 14, 30, 0, 15, 31, 0);
            let idx_c2 = _mm512_setr_epi32(10, 0, 0, 11, 0, 0, 12, 0, 0, 13, 0, 0, 14, 0, 0, 15);

            let out0 = _mm512_permutex2var_ps(a, idx_ab0, b);
            let out0 = _mm512_mask_permutexvar_ps(out0, 0b0100100100100100, idx_c0, c);

            let out1 = _mm512_permutex2var_ps(a, idx_ab1, b);
            let out1 = _mm512_mask_permutexvar_ps(out1, 0b0010010010010010, idx_c1, c);

            let out2 = _mm512_permutex2var_ps(a, idx_ab2, b);
            let out2 = _mm512_mask_permutexvar_ps(out2, 0b1001001001001001, idx_c2, c);

            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm512_storeu_ps(dest_ptr, out0);
                _mm512_storeu_ps(dest_ptr.add(16), out1);
                _mm512_storeu_ps(dest_ptr.add(32), out2);
            }
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
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
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn store_interleaved_4_impl(
            a: __m512,
            b: __m512,
            c: __m512,
            d: __m512,
            dest: &mut [MaybeUninit<f32>],
        ) {
            assert!(dest.len() >= 4 * F32VecAvx512::LEN);
            // a = [a0..a15], b = [b0..b15], c = [c0..c15], d = [d0..d15]
            // Output: [a0,b0,c0,d0, a1,b1,c1,d1, ..., a15,b15,c15,d15]

            // Stage 1: Interleave pairs within 128-bit lanes
            // ab_lo lane k: [a[4k], b[4k], a[4k+1], b[4k+1]]
            let ab_lo = _mm512_unpacklo_ps(a, b);
            // ab_hi lane k: [a[4k+2], b[4k+2], a[4k+3], b[4k+3]]
            let ab_hi = _mm512_unpackhi_ps(a, b);
            let cd_lo = _mm512_unpacklo_ps(c, d);
            let cd_hi = _mm512_unpackhi_ps(c, d);

            // Stage 2: 64-bit interleave to get 4 elements together
            // abcd_0 lane k: [a[4k], b[4k], c[4k], d[4k]]
            let abcd_0 = _mm512_castpd_ps(_mm512_unpacklo_pd(
                _mm512_castps_pd(ab_lo),
                _mm512_castps_pd(cd_lo),
            ));
            // abcd_1 lane k: [a[4k+1], b[4k+1], c[4k+1], d[4k+1]]
            let abcd_1 = _mm512_castpd_ps(_mm512_unpackhi_pd(
                _mm512_castps_pd(ab_lo),
                _mm512_castps_pd(cd_lo),
            ));
            // abcd_2 lane k: [a[4k+2], b[4k+2], c[4k+2], d[4k+2]]
            let abcd_2 = _mm512_castpd_ps(_mm512_unpacklo_pd(
                _mm512_castps_pd(ab_hi),
                _mm512_castps_pd(cd_hi),
            ));
            // abcd_3 lane k: [a[4k+3], b[4k+3], c[4k+3], d[4k+3]]
            let abcd_3 = _mm512_castpd_ps(_mm512_unpackhi_pd(
                _mm512_castps_pd(ab_hi),
                _mm512_castps_pd(cd_hi),
            ));

            // Stage 3: We need to output where each output vector collects same-index
            // elements from all 4 lanes. This is essentially a 4x4 transpose of 128-bit blocks.
            // out0 = [abcd_0 lane 0, abcd_1 lane 0, abcd_2 lane 0, abcd_3 lane 0]
            // out1 = [abcd_0 lane 1, abcd_1 lane 1, abcd_2 lane 1, abcd_3 lane 1]
            // etc.

            // Step 3a: First combine pairs (0,1) and (2,3) selecting same lane from each
            // pair01_lane0 = [abcd_0 lane 0, abcd_1 lane 0, abcd_0 lane 2, abcd_1 lane 2]
            let idx_even =
                _mm512_setr_epi32(0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27);
            // pair01_lane1 = [abcd_0 lane 1, abcd_1 lane 1, abcd_0 lane 3, abcd_1 lane 3]
            let idx_odd =
                _mm512_setr_epi32(4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 28, 29, 30, 31);

            let pair01_02 = _mm512_permutex2var_ps(abcd_0, idx_even, abcd_1);
            let pair01_13 = _mm512_permutex2var_ps(abcd_0, idx_odd, abcd_1);
            let pair23_02 = _mm512_permutex2var_ps(abcd_2, idx_even, abcd_3);
            let pair23_13 = _mm512_permutex2var_ps(abcd_2, idx_odd, abcd_3);

            // Step 3b: Now combine pairs to get final output
            // out0 needs lanes 0 from pair01_02 and pair23_02
            // out1 needs lanes 1 from pair01_13 and pair23_13
            // But pair01_02 has: [abcd_0 lane 0, abcd_1 lane 0, abcd_0 lane 2, abcd_1 lane 2]
            // And pair23_02 has: [abcd_2 lane 0, abcd_3 lane 0, abcd_2 lane 2, abcd_3 lane 2]
            // out0 = [abcd_0 lane 0, abcd_1 lane 0, abcd_2 lane 0, abcd_3 lane 0]
            //      = [pair01_02 lane 0, pair01_02 lane 1, pair23_02 lane 0, pair23_02 lane 1]
            let idx_0 = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23);
            let idx_1 =
                _mm512_setr_epi32(8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31);

            let out0 = _mm512_permutex2var_ps(pair01_02, idx_0, pair23_02);
            let out2 = _mm512_permutex2var_ps(pair01_02, idx_1, pair23_02);
            let out1 = _mm512_permutex2var_ps(pair01_13, idx_0, pair23_13);
            let out3 = _mm512_permutex2var_ps(pair01_13, idx_1, pair23_13);

            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm512_storeu_ps(dest_ptr, out0);
                _mm512_storeu_ps(dest_ptr.add(16), out1);
                _mm512_storeu_ps(dest_ptr.add(32), out2);
                _mm512_storeu_ps(dest_ptr.add(48), out3);
            }
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
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
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn store_interleaved_8_impl(
            a: __m512,
            b: __m512,
            c: __m512,
            d: __m512,
            e: __m512,
            f: __m512,
            g: __m512,
            h: __m512,
            dest: &mut [f32],
        ) {
            assert!(dest.len() >= 8 * F32VecAvx512::LEN);
            // a..h each have 16 elements. Output is 128 elements interleaved:
            // [a0,b0,c0,d0,e0,f0,g0,h0, a1,b1,c1,d1,e1,f1,g1,h1, ..., a15,b15,...,h15]
            // Each output vector is 16 floats = 2 groups of 8.

            // Stage 1: Unpack pairs within 128-bit lanes
            // ab_lo lane k: [a[4k], b[4k], a[4k+1], b[4k+1]]
            let ab_lo = _mm512_unpacklo_ps(a, b);
            let ab_hi = _mm512_unpackhi_ps(a, b);
            let cd_lo = _mm512_unpacklo_ps(c, d);
            let cd_hi = _mm512_unpackhi_ps(c, d);
            let ef_lo = _mm512_unpacklo_ps(e, f);
            let ef_hi = _mm512_unpackhi_ps(e, f);
            let gh_lo = _mm512_unpacklo_ps(g, h);
            let gh_hi = _mm512_unpackhi_ps(g, h);

            // Stage 2: 64-bit interleave to get 4-element groups
            // abcd_0 lane k: [a[4k], b[4k], c[4k], d[4k]]
            let abcd_0 = _mm512_castpd_ps(_mm512_unpacklo_pd(
                _mm512_castps_pd(ab_lo),
                _mm512_castps_pd(cd_lo),
            ));
            let abcd_1 = _mm512_castpd_ps(_mm512_unpackhi_pd(
                _mm512_castps_pd(ab_lo),
                _mm512_castps_pd(cd_lo),
            ));
            let abcd_2 = _mm512_castpd_ps(_mm512_unpacklo_pd(
                _mm512_castps_pd(ab_hi),
                _mm512_castps_pd(cd_hi),
            ));
            let abcd_3 = _mm512_castpd_ps(_mm512_unpackhi_pd(
                _mm512_castps_pd(ab_hi),
                _mm512_castps_pd(cd_hi),
            ));
            let efgh_0 = _mm512_castpd_ps(_mm512_unpacklo_pd(
                _mm512_castps_pd(ef_lo),
                _mm512_castps_pd(gh_lo),
            ));
            let efgh_1 = _mm512_castpd_ps(_mm512_unpackhi_pd(
                _mm512_castps_pd(ef_lo),
                _mm512_castps_pd(gh_lo),
            ));
            let efgh_2 = _mm512_castpd_ps(_mm512_unpacklo_pd(
                _mm512_castps_pd(ef_hi),
                _mm512_castps_pd(gh_hi),
            ));
            let efgh_3 = _mm512_castpd_ps(_mm512_unpackhi_pd(
                _mm512_castps_pd(ef_hi),
                _mm512_castps_pd(gh_hi),
            ));

            // Stage 3: Combine abcd_i with efgh_i to get 8-element groups per lane
            // full_0 = [abcd_0 lane 0, efgh_0 lane 0, abcd_0 lane 1, efgh_0 lane 1,
            //           abcd_0 lane 2, efgh_0 lane 2, abcd_0 lane 3, efgh_0 lane 3]
            // But we need output like:
            // out0 = [all channels at index 0, all channels at index 1]
            //      = [abcd_0 lane 0 ++ efgh_0 lane 0, abcd_1 lane 0 ++ efgh_1 lane 0]

            // Interleave 128-bit blocks from abcd and efgh within each vector
            let idx_02 =
                _mm512_setr_epi32(0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27);
            let idx_13 =
                _mm512_setr_epi32(4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 28, 29, 30, 31);

            // full_0_02 = [abcd_0 lane 0, efgh_0 lane 0, abcd_0 lane 2, efgh_0 lane 2]
            let full_0_02 = _mm512_permutex2var_ps(abcd_0, idx_02, efgh_0);
            let full_0_13 = _mm512_permutex2var_ps(abcd_0, idx_13, efgh_0);
            let full_1_02 = _mm512_permutex2var_ps(abcd_1, idx_02, efgh_1);
            let full_1_13 = _mm512_permutex2var_ps(abcd_1, idx_13, efgh_1);
            let full_2_02 = _mm512_permutex2var_ps(abcd_2, idx_02, efgh_2);
            let full_2_13 = _mm512_permutex2var_ps(abcd_2, idx_13, efgh_2);
            let full_3_02 = _mm512_permutex2var_ps(abcd_3, idx_02, efgh_3);
            let full_3_13 = _mm512_permutex2var_ps(abcd_3, idx_13, efgh_3);

            // Stage 4: Now we need to combine across the _0/_1/_2/_3 indices
            // full_i_02 has: [idx 4*lane, idx 4*lane+1 at (abcd,efgh) for lanes 0,2]
            // We need output vectors that have consecutive indices from all channels

            // out0 = [idx 0 all 8 ch, idx 1 all 8 ch] = [full_0_02 lanes 0,1, full_1_02 lanes 0,1]
            // out1 = [idx 2 all 8 ch, idx 3 all 8 ch] = [full_2_02 lanes 0,1, full_3_02 lanes 0,1]
            // out2 = [idx 4 all 8 ch, idx 5 all 8 ch] = [full_0_13 lanes 0,1, full_1_13 lanes 0,1]
            // out3 = [idx 6 all 8 ch, idx 7 all 8 ch] = [full_2_13 lanes 0,1, full_3_13 lanes 0,1]
            // out4 = [idx 8 all 8 ch, idx 9 all 8 ch] = [full_0_02 lanes 2,3, full_1_02 lanes 2,3]
            // etc.

            let idx_lo = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23);
            let idx_hi =
                _mm512_setr_epi32(8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31);

            let out0 = _mm512_permutex2var_ps(full_0_02, idx_lo, full_1_02);
            let out1 = _mm512_permutex2var_ps(full_2_02, idx_lo, full_3_02);
            let out2 = _mm512_permutex2var_ps(full_0_13, idx_lo, full_1_13);
            let out3 = _mm512_permutex2var_ps(full_2_13, idx_lo, full_3_13);
            let out4 = _mm512_permutex2var_ps(full_0_02, idx_hi, full_1_02);
            let out5 = _mm512_permutex2var_ps(full_2_02, idx_hi, full_3_02);
            let out6 = _mm512_permutex2var_ps(full_0_13, idx_hi, full_1_13);
            let out7 = _mm512_permutex2var_ps(full_2_13, idx_hi, full_3_13);

            // SAFETY: we just checked that dest has enough space.
            unsafe {
                let ptr = dest.as_mut_ptr();
                _mm512_storeu_ps(ptr, out0);
                _mm512_storeu_ps(ptr.add(16), out1);
                _mm512_storeu_ps(ptr.add(32), out2);
                _mm512_storeu_ps(ptr.add(48), out3);
                _mm512_storeu_ps(ptr.add(64), out4);
                _mm512_storeu_ps(ptr.add(80), out5);
                _mm512_storeu_ps(ptr.add(96), out6);
                _mm512_storeu_ps(ptr.add(112), out7);
            }
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_8_impl(a.0, b.0, c.0, d.0, e.0, f.0, g.0, h.0, dest) }
    }

    #[inline(always)]
    fn load_deinterleaved_2(d: Self::Descriptor, src: &[f32]) -> (Self, Self) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn load_deinterleaved_2_impl(src: &[f32]) -> (__m512, __m512) {
            assert!(src.len() >= 2 * F32VecAvx512::LEN);
            // Input: [a0,b0,a1,b1,...,a15,b15]
            // Output: a = [a0..a15], b = [b0..b15]
            // SAFETY: we just checked that src has enough space.
            let (in0, in1) = unsafe {
                (
                    _mm512_loadu_ps(src.as_ptr()),
                    _mm512_loadu_ps(src.as_ptr().add(16)),
                )
            };

            // Use permutex2var to gather even/odd indices
            let idx_a =
                _mm512_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30);
            let idx_b =
                _mm512_setr_epi32(1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31);

            let a = _mm512_permutex2var_ps(in0, idx_a, in1);
            let b = _mm512_permutex2var_ps(in0, idx_b, in1);

            (a, b)
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
        let (a, b) = unsafe { load_deinterleaved_2_impl(src) };
        (Self(a, d), Self(b, d))
    }

    #[inline(always)]
    fn load_deinterleaved_3(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn load_deinterleaved_3_impl(src: &[f32]) -> (__m512, __m512, __m512) {
            assert!(src.len() >= 3 * F32VecAvx512::LEN);
            // Input layout (48 floats in 3x16-float vectors):
            // in0: [a0,b0,c0,a1,b1,c1,a2,b2,c2,a3,b3,c3,a4,b4,c4,a5]
            // in1: [b5,c5,a6,b6,c6,a7,b7,c7,a8,b8,c8,a9,b9,c9,a10,b10]
            // in2: [c10,a11,b11,c11,a12,b12,c12,a13,b13,c13,a14,b14,c14,a15,b15,c15]
            // Output: a = [a0..a15], b = [b0..b15], c = [c0..c15]

            // SAFETY: we just checked that src has enough space.
            let (in0, in1, in2) = unsafe {
                (
                    _mm512_loadu_ps(src.as_ptr()),
                    _mm512_loadu_ps(src.as_ptr().add(16)),
                    _mm512_loadu_ps(src.as_ptr().add(32)),
                )
            };

            // Use permutex2var to gather elements from pairs of vectors, then blend.
            // For 'a': positions 0,3,6,9,12,15 from in0; 2,5,8,11,14 from in1; 1,4,7,10,13 from in2
            // a[0..5] from in0, a[6..10] from in1, a[11..15] from in2

            // Gather indices for each channel from in0+in1 (first 32 elements)
            let idx_a_01 = _mm512_setr_epi32(0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 0, 0, 0, 0, 0);
            let idx_b_01 =
                _mm512_setr_epi32(1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 0, 0, 0, 0, 0);
            let idx_c_01 = _mm512_setr_epi32(2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 0, 0, 0, 0, 0, 0);

            // Gather indices for remaining elements from in1+in2 (last 32 elements)
            let idx_a_12 = _mm512_setr_epi32(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 20, 23, 26, 29);
            let idx_b_12 = _mm512_setr_epi32(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18, 21, 24, 27, 30);
            let idx_c_12 = _mm512_setr_epi32(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 19, 22, 25, 28, 31);

            // Gather from in0+in1 for first ~11 elements, in1+in2 for last ~5
            let a_01 = _mm512_permutex2var_ps(in0, idx_a_01, in1);
            let a_12 = _mm512_permutex2var_ps(in1, idx_a_12, in2);
            let a = _mm512_mask_blend_ps(0xF800, a_01, a_12); // positions 11-15 from a_12

            let b_01 = _mm512_permutex2var_ps(in0, idx_b_01, in1);
            let b_12 = _mm512_permutex2var_ps(in1, idx_b_12, in2);
            let b = _mm512_mask_blend_ps(0xF800, b_01, b_12); // positions 11-15 from b_12

            let c_01 = _mm512_permutex2var_ps(in0, idx_c_01, in1);
            let c_12 = _mm512_permutex2var_ps(in1, idx_c_12, in2);
            let c = _mm512_mask_blend_ps(0xFC00, c_01, c_12); // positions 10-15 from c_12

            (a, b, c)
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
        let (a, b, c) = unsafe { load_deinterleaved_3_impl(src) };
        (Self(a, d), Self(b, d), Self(c, d))
    }

    #[inline(always)]
    fn load_deinterleaved_4(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self, Self) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn load_deinterleaved_4_impl(src: &[f32]) -> (__m512, __m512, __m512, __m512) {
            assert!(src.len() >= 4 * F32VecAvx512::LEN);
            // Input: [a0,b0,c0,d0,a1,b1,c1,d1,...] (64 floats)
            // Output: a = [a0..a15], b = [b0..b15], c = [c0..c15], d = [d0..d15]
            // SAFETY: we just checked that src has enough space.
            let (in0, in1, in2, in3) = unsafe {
                (
                    _mm512_loadu_ps(src.as_ptr()),
                    _mm512_loadu_ps(src.as_ptr().add(16)),
                    _mm512_loadu_ps(src.as_ptr().add(32)),
                    _mm512_loadu_ps(src.as_ptr().add(48)),
                )
            };

            // Use permutex2var to gather every 4th element
            let idx_a = _mm512_setr_epi32(0, 4, 8, 12, 16, 20, 24, 28, 0, 4, 8, 12, 16, 20, 24, 28);
            let idx_b = _mm512_setr_epi32(1, 5, 9, 13, 17, 21, 25, 29, 1, 5, 9, 13, 17, 21, 25, 29);
            let idx_c =
                _mm512_setr_epi32(2, 6, 10, 14, 18, 22, 26, 30, 2, 6, 10, 14, 18, 22, 26, 30);
            let idx_d =
                _mm512_setr_epi32(3, 7, 11, 15, 19, 23, 27, 31, 3, 7, 11, 15, 19, 23, 27, 31);

            // Gather from in0+in1 for first 8 elements, in2+in3 for last 8
            let a01 = _mm512_permutex2var_ps(in0, idx_a, in1);
            let a23 = _mm512_permutex2var_ps(in2, idx_a, in3);
            let a = _mm512_mask_blend_ps(0xFF00, a01, a23);

            let b01 = _mm512_permutex2var_ps(in0, idx_b, in1);
            let b23 = _mm512_permutex2var_ps(in2, idx_b, in3);
            let b = _mm512_mask_blend_ps(0xFF00, b01, b23);

            let c01 = _mm512_permutex2var_ps(in0, idx_c, in1);
            let c23 = _mm512_permutex2var_ps(in2, idx_c, in3);
            let c = _mm512_mask_blend_ps(0xFF00, c01, c23);

            let d01 = _mm512_permutex2var_ps(in0, idx_d, in1);
            let d23 = _mm512_permutex2var_ps(in2, idx_d, in3);
            let dv = _mm512_mask_blend_ps(0xFF00, d01, d23);

            (a, b, c, dv)
        }

        // SAFETY: avx512f is available from the safety invariant on the descriptor.
        let (a, b, c, dv) = unsafe { load_deinterleaved_4_impl(src) };
        (Self(a, d), Self(b, d), Self(c, d), Self(dv, d))
    }

    fn_avx!(this: F32VecAvx512, fn mul_add(mul: F32VecAvx512, add: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_fmadd_ps(this.0, mul.0, add.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn neg_mul_add(mul: F32VecAvx512, add: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_fnmadd_ps(this.0, mul.0, add.0), this.1)
    });

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: f32) -> Self {
        // SAFETY: We know avx512f is available from the safety invariant on `d`.
        unsafe { Self(_mm512_set1_ps(v), d) }
    }

    #[inline(always)]
    fn zero(d: Self::Descriptor) -> Self {
        // SAFETY: We know avx512f is available from the safety invariant on `d`.
        unsafe { Self(_mm512_setzero_ps(), d) }
    }

    fn_avx!(this: F32VecAvx512, fn abs() -> F32VecAvx512 {
        F32VecAvx512(_mm512_abs_ps(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn floor() -> F32VecAvx512 {
        F32VecAvx512(_mm512_roundscale_ps::<{ _MM_FROUND_FLOOR }>(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn sqrt() -> F32VecAvx512 {
        F32VecAvx512(_mm512_sqrt_ps(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn neg() -> F32VecAvx512 {
        F32VecAvx512(
            _mm512_castsi512_ps(_mm512_xor_si512(
                _mm512_set1_epi32(i32::MIN),
                _mm512_castps_si512(this.0),
            )),
            this.1,
        )
    });

    fn_avx!(this: F32VecAvx512, fn copysign(sign: F32VecAvx512) -> F32VecAvx512 {
        let sign_mask = _mm512_set1_epi32(i32::MIN);
        F32VecAvx512(
            _mm512_castsi512_ps(_mm512_or_si512(
                _mm512_andnot_si512(sign_mask, _mm512_castps_si512(this.0)),
                _mm512_and_si512(sign_mask, _mm512_castps_si512(sign.0)),
            )),
            this.1,
        )
    });

    fn_avx!(this: F32VecAvx512, fn max(other: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_max_ps(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn min(other: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_min_ps(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn gt(other: F32VecAvx512) -> MaskAvx512 {
        MaskAvx512(_mm512_cmp_ps_mask::<{_CMP_GT_OQ}>(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn as_i32() -> I32VecAvx512 {
        I32VecAvx512(_mm512_cvtps_epi32(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn bitcast_to_i32() -> I32VecAvx512 {
        I32VecAvx512(_mm512_castps_si512(this.0), this.1)
    });

    #[inline(always)]
    fn prepare_table_bf16_8(_d: Avx512Descriptor, table: &[f32; 8]) -> Bf16Table8Avx512 {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn prepare_impl(table: &[f32; 8]) -> __m512 {
            // SAFETY: avx512f is available from target_feature, and we load 8 elements,
            // exactly as many as are present in `table`.
            let table_256 = unsafe { _mm256_loadu_ps(table.as_ptr()) };
            // Zero-extend to 512-bit; vpermutexvar with indices 0-7 only reads first 256 bits
            _mm512_castps256_ps512(table_256)
        }
        // SAFETY: avx512f is available from the safety invariant on the descriptor
        Bf16Table8Avx512(unsafe { prepare_impl(table) })
    }

    #[inline(always)]
    fn table_lookup_bf16_8(
        d: Avx512Descriptor,
        table: Bf16Table8Avx512,
        indices: I32VecAvx512,
    ) -> Self {
        // SAFETY: avx512f is available from the safety invariant on the descriptor
        F32VecAvx512(unsafe { _mm512_permutexvar_ps(indices.0, table.0) }, d)
    }

    #[inline(always)]
    fn round_store_u8(self, dest: &mut [u8]) {
        #[target_feature(enable = "avx512f", enable = "avx512bw")]
        #[inline]
        fn round_store_u8_impl(v: __m512, dest: &mut [u8]) {
            assert!(dest.len() >= F32VecAvx512::LEN);
            // Round to nearest integer
            let rounded = _mm512_roundscale_ps::<{ _MM_FROUND_TO_NEAREST_INT }>(v);
            // Convert to i32
            let i32s = _mm512_cvtps_epi32(rounded);
            // Use pmovusdb: saturating conversion from 32-bit to 8-bit unsigned
            let u8s = _mm512_cvtusepi32_epi8(i32s);
            // Store 16 bytes
            // SAFETY: we checked dest has enough space
            unsafe {
                _mm_storeu_si128(dest.as_mut_ptr() as *mut __m128i, u8s);
            }
        }
        // SAFETY: avx512f and avx512bw are available from the safety invariant on the descriptor.
        unsafe { round_store_u8_impl(self.0, dest) }
    }

    #[inline(always)]
    fn round_store_u16(self, dest: &mut [u16]) {
        #[target_feature(enable = "avx512f", enable = "avx512bw")]
        #[inline]
        fn round_store_u16_impl(v: __m512, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecAvx512::LEN);
            // Round to nearest integer
            let rounded = _mm512_roundscale_ps::<{ _MM_FROUND_TO_NEAREST_INT }>(v);
            // Convert to i32
            let i32s = _mm512_cvtps_epi32(rounded);
            // Use pmovusdw: saturating conversion from 32-bit to 16-bit unsigned
            let u16s = _mm512_cvtusepi32_epi16(i32s);
            // Store 16 u16s (32 bytes)
            // SAFETY: we checked dest has enough space
            unsafe {
                _mm256_storeu_si256(dest.as_mut_ptr() as *mut __m256i, u16s);
            }
        }
        // SAFETY: avx512f and avx512bw are available from the safety invariant on the descriptor.
        unsafe { round_store_u16_impl(self.0, dest) }
    }

    impl_f32_array_interface!();

    #[inline(always)]
    fn load_f16_bits(d: Self::Descriptor, mem: &[u16]) -> Self {
        // AVX512 implies F16C, so we can always use hardware conversion
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn load_f16_impl(d: Avx512Descriptor, mem: &[u16]) -> F32VecAvx512 {
            assert!(mem.len() >= F32VecAvx512::LEN);
            // SAFETY: mem.len() >= 16 is checked above
            let bits = unsafe { _mm256_loadu_si256(mem.as_ptr() as *const __m256i) };
            F32VecAvx512(_mm512_cvtph_ps(bits), d)
        }
        // SAFETY: avx512f is available from the safety invariant on the descriptor
        unsafe { load_f16_impl(d, mem) }
    }

    #[inline(always)]
    fn store_f16_bits(self, dest: &mut [u16]) {
        // AVX512 implies F16C, so we can always use hardware conversion
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn store_f16_bits_impl(v: __m512, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecAvx512::LEN);
            let bits = _mm512_cvtps_ph::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(v);
            // SAFETY: dest.len() >= 16 is checked above
            unsafe { _mm256_storeu_si256(dest.as_mut_ptr() as *mut __m256i, bits) };
        }
        // SAFETY: avx512f is available from the safety invariant on the descriptor
        unsafe { store_f16_bits_impl(self.0, dest) }
    }

    #[inline(always)]
    fn transpose_square(d: Self::Descriptor, data: &mut [Self::UnderlyingArray], stride: usize) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn transpose16x16f32(d: Avx512Descriptor, data: &mut [[f32; 16]], stride: usize) {
            assert!(data.len() > stride * 15);

            let r0 = F32VecAvx512::load_array(d, &data[0]).0;
            let r1 = F32VecAvx512::load_array(d, &data[1 * stride]).0;
            let r2 = F32VecAvx512::load_array(d, &data[2 * stride]).0;
            let r3 = F32VecAvx512::load_array(d, &data[3 * stride]).0;
            let r4 = F32VecAvx512::load_array(d, &data[4 * stride]).0;
            let r5 = F32VecAvx512::load_array(d, &data[5 * stride]).0;
            let r6 = F32VecAvx512::load_array(d, &data[6 * stride]).0;
            let r7 = F32VecAvx512::load_array(d, &data[7 * stride]).0;
            let r8 = F32VecAvx512::load_array(d, &data[8 * stride]).0;
            let r9 = F32VecAvx512::load_array(d, &data[9 * stride]).0;
            let r10 = F32VecAvx512::load_array(d, &data[10 * stride]).0;
            let r11 = F32VecAvx512::load_array(d, &data[11 * stride]).0;
            let r12 = F32VecAvx512::load_array(d, &data[12 * stride]).0;
            let r13 = F32VecAvx512::load_array(d, &data[13 * stride]).0;
            let r14 = F32VecAvx512::load_array(d, &data[14 * stride]).0;
            let r15 = F32VecAvx512::load_array(d, &data[15 * stride]).0;

            // Stage 1: Unpack low/high pairs
            let t0 = _mm512_unpacklo_ps(r0, r1);
            let t1 = _mm512_unpackhi_ps(r0, r1);
            let t2 = _mm512_unpacklo_ps(r2, r3);
            let t3 = _mm512_unpackhi_ps(r2, r3);
            let t4 = _mm512_unpacklo_ps(r4, r5);
            let t5 = _mm512_unpackhi_ps(r4, r5);
            let t6 = _mm512_unpacklo_ps(r6, r7);
            let t7 = _mm512_unpackhi_ps(r6, r7);
            let t8 = _mm512_unpacklo_ps(r8, r9);
            let t9 = _mm512_unpackhi_ps(r8, r9);
            let t10 = _mm512_unpacklo_ps(r10, r11);
            let t11 = _mm512_unpackhi_ps(r10, r11);
            let t12 = _mm512_unpacklo_ps(r12, r13);
            let t13 = _mm512_unpackhi_ps(r12, r13);
            let t14 = _mm512_unpacklo_ps(r14, r15);
            let t15 = _mm512_unpackhi_ps(r14, r15);

            // Cast to 64 bits.
            let t0 = _mm512_castps_pd(t0);
            let t1 = _mm512_castps_pd(t1);
            let t2 = _mm512_castps_pd(t2);
            let t3 = _mm512_castps_pd(t3);
            let t4 = _mm512_castps_pd(t4);
            let t5 = _mm512_castps_pd(t5);
            let t6 = _mm512_castps_pd(t6);
            let t7 = _mm512_castps_pd(t7);
            let t8 = _mm512_castps_pd(t8);
            let t9 = _mm512_castps_pd(t9);
            let t10 = _mm512_castps_pd(t10);
            let t11 = _mm512_castps_pd(t11);
            let t12 = _mm512_castps_pd(t12);
            let t13 = _mm512_castps_pd(t13);
            let t14 = _mm512_castps_pd(t14);
            let t15 = _mm512_castps_pd(t15);

            // Stage 2: Shuffle to group 32-bit elements
            let s0 = _mm512_unpacklo_pd(t0, t2);
            let s1 = _mm512_unpackhi_pd(t0, t2);
            let s2 = _mm512_unpacklo_pd(t1, t3);
            let s3 = _mm512_unpackhi_pd(t1, t3);
            let s4 = _mm512_unpacklo_pd(t4, t6);
            let s5 = _mm512_unpackhi_pd(t4, t6);
            let s6 = _mm512_unpacklo_pd(t5, t7);
            let s7 = _mm512_unpackhi_pd(t5, t7);
            let s8 = _mm512_unpacklo_pd(t8, t10);
            let s9 = _mm512_unpackhi_pd(t8, t10);
            let s10 = _mm512_unpacklo_pd(t9, t11);
            let s11 = _mm512_unpackhi_pd(t9, t11);
            let s12 = _mm512_unpacklo_pd(t12, t14);
            let s13 = _mm512_unpackhi_pd(t12, t14);
            let s14 = _mm512_unpacklo_pd(t13, t15);
            let s15 = _mm512_unpackhi_pd(t13, t15);

            // Stage 3: 128-bit permute
            let idx_hi = _mm512_setr_epi64(0, 1, 8, 9, 4, 5, 12, 13);
            let idx_lo = _mm512_add_epi64(idx_hi, _mm512_set1_epi64(2));

            let c0 = _mm512_permutex2var_pd(s0, idx_hi, s4);
            let c1 = _mm512_permutex2var_pd(s1, idx_hi, s5);
            let c2 = _mm512_permutex2var_pd(s2, idx_hi, s6);
            let c3 = _mm512_permutex2var_pd(s3, idx_hi, s7);
            let c4 = _mm512_permutex2var_pd(s0, idx_lo, s4);
            let c5 = _mm512_permutex2var_pd(s1, idx_lo, s5);
            let c6 = _mm512_permutex2var_pd(s2, idx_lo, s6);
            let c7 = _mm512_permutex2var_pd(s3, idx_lo, s7);
            let c8 = _mm512_permutex2var_pd(s8, idx_hi, s12);
            let c9 = _mm512_permutex2var_pd(s9, idx_hi, s13);
            let c10 = _mm512_permutex2var_pd(s10, idx_hi, s14);
            let c11 = _mm512_permutex2var_pd(s11, idx_hi, s15);
            let c12 = _mm512_permutex2var_pd(s8, idx_lo, s12);
            let c13 = _mm512_permutex2var_pd(s9, idx_lo, s13);
            let c14 = _mm512_permutex2var_pd(s10, idx_lo, s14);
            let c15 = _mm512_permutex2var_pd(s11, idx_lo, s15);

            // Stage 4: 256-bit permute
            let idx_hi = _mm512_setr_epi64(0, 1, 2, 3, 8, 9, 10, 11);
            let idx_lo = _mm512_add_epi64(idx_hi, _mm512_set1_epi64(4));

            let o0 = _mm512_permutex2var_pd(c0, idx_hi, c8);
            let o1 = _mm512_permutex2var_pd(c1, idx_hi, c9);
            let o2 = _mm512_permutex2var_pd(c2, idx_hi, c10);
            let o3 = _mm512_permutex2var_pd(c3, idx_hi, c11);
            let o4 = _mm512_permutex2var_pd(c4, idx_hi, c12);
            let o5 = _mm512_permutex2var_pd(c5, idx_hi, c13);
            let o6 = _mm512_permutex2var_pd(c6, idx_hi, c14);
            let o7 = _mm512_permutex2var_pd(c7, idx_hi, c15);
            let o8 = _mm512_permutex2var_pd(c0, idx_lo, c8);
            let o9 = _mm512_permutex2var_pd(c1, idx_lo, c9);
            let o10 = _mm512_permutex2var_pd(c2, idx_lo, c10);
            let o11 = _mm512_permutex2var_pd(c3, idx_lo, c11);
            let o12 = _mm512_permutex2var_pd(c4, idx_lo, c12);
            let o13 = _mm512_permutex2var_pd(c5, idx_lo, c13);
            let o14 = _mm512_permutex2var_pd(c6, idx_lo, c14);
            let o15 = _mm512_permutex2var_pd(c7, idx_lo, c15);

            let o0 = _mm512_castpd_ps(o0);
            let o1 = _mm512_castpd_ps(o1);
            let o2 = _mm512_castpd_ps(o2);
            let o3 = _mm512_castpd_ps(o3);
            let o4 = _mm512_castpd_ps(o4);
            let o5 = _mm512_castpd_ps(o5);
            let o6 = _mm512_castpd_ps(o6);
            let o7 = _mm512_castpd_ps(o7);
            let o8 = _mm512_castpd_ps(o8);
            let o9 = _mm512_castpd_ps(o9);
            let o10 = _mm512_castpd_ps(o10);
            let o11 = _mm512_castpd_ps(o11);
            let o12 = _mm512_castpd_ps(o12);
            let o13 = _mm512_castpd_ps(o13);
            let o14 = _mm512_castpd_ps(o14);
            let o15 = _mm512_castpd_ps(o15);

            F32VecAvx512(o0, d).store_array(&mut data[0]);
            F32VecAvx512(o1, d).store_array(&mut data[1 * stride]);
            F32VecAvx512(o2, d).store_array(&mut data[2 * stride]);
            F32VecAvx512(o3, d).store_array(&mut data[3 * stride]);
            F32VecAvx512(o4, d).store_array(&mut data[4 * stride]);
            F32VecAvx512(o5, d).store_array(&mut data[5 * stride]);
            F32VecAvx512(o6, d).store_array(&mut data[6 * stride]);
            F32VecAvx512(o7, d).store_array(&mut data[7 * stride]);
            F32VecAvx512(o8, d).store_array(&mut data[8 * stride]);
            F32VecAvx512(o9, d).store_array(&mut data[9 * stride]);
            F32VecAvx512(o10, d).store_array(&mut data[10 * stride]);
            F32VecAvx512(o11, d).store_array(&mut data[11 * stride]);
            F32VecAvx512(o12, d).store_array(&mut data[12 * stride]);
            F32VecAvx512(o13, d).store_array(&mut data[13 * stride]);
            F32VecAvx512(o14, d).store_array(&mut data[14 * stride]);
            F32VecAvx512(o15, d).store_array(&mut data[15 * stride]);
        }
        // SAFETY: the safety invariant on `d` guarantees avx512f
        unsafe {
            transpose16x16f32(d, data, stride);
        }
    }
}

impl Add<F32VecAvx512> for F32VecAvx512 {
    type Output = F32VecAvx512;
    fn_avx!(this: F32VecAvx512, fn add(rhs: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_add_ps(this.0, rhs.0), this.1)
    });
}

impl Sub<F32VecAvx512> for F32VecAvx512 {
    type Output = F32VecAvx512;
    fn_avx!(this: F32VecAvx512, fn sub(rhs: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_sub_ps(this.0, rhs.0), this.1)
    });
}

impl Mul<F32VecAvx512> for F32VecAvx512 {
    type Output = F32VecAvx512;
    fn_avx!(this: F32VecAvx512, fn mul(rhs: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_mul_ps(this.0, rhs.0), this.1)
    });
}

impl Div<F32VecAvx512> for F32VecAvx512 {
    type Output = F32VecAvx512;
    fn_avx!(this: F32VecAvx512, fn div(rhs: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_div_ps(this.0, rhs.0), this.1)
    });
}

impl AddAssign<F32VecAvx512> for F32VecAvx512 {
    fn_avx!(this: &mut F32VecAvx512, fn add_assign(rhs: F32VecAvx512) {
        this.0 = _mm512_add_ps(this.0, rhs.0)
    });
}

impl SubAssign<F32VecAvx512> for F32VecAvx512 {
    fn_avx!(this: &mut F32VecAvx512, fn sub_assign(rhs: F32VecAvx512) {
        this.0 = _mm512_sub_ps(this.0, rhs.0)
    });
}

impl MulAssign<F32VecAvx512> for F32VecAvx512 {
    fn_avx!(this: &mut F32VecAvx512, fn mul_assign(rhs: F32VecAvx512) {
        this.0 = _mm512_mul_ps(this.0, rhs.0)
    });
}

impl DivAssign<F32VecAvx512> for F32VecAvx512 {
    fn_avx!(this: &mut F32VecAvx512, fn div_assign(rhs: F32VecAvx512) {
        this.0 = _mm512_div_ps(this.0, rhs.0)
    });
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct I32VecAvx512(__m512i, Avx512Descriptor);

impl I32SimdVec for I32VecAvx512 {
    type Descriptor = Avx512Descriptor;

    const LEN: usize = 16;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[i32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx512f is available
        // from the safety invariant on `d`.
        Self(unsafe { _mm512_loadu_epi32(mem.as_ptr()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [i32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know avx512f is available
        // from the safety invariant on `self.1`.
        unsafe { _mm512_storeu_epi32(mem.as_mut_ptr(), self.0) }
    }

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: i32) -> Self {
        // SAFETY: We know avx512f is available from the safety invariant on `d`.
        unsafe { Self(_mm512_set1_epi32(v), d) }
    }

    fn_avx!(this: I32VecAvx512, fn as_f32() -> F32VecAvx512 {
         F32VecAvx512(_mm512_cvtepi32_ps(this.0), this.1)
    });

    fn_avx!(this: I32VecAvx512, fn bitcast_to_f32() -> F32VecAvx512 {
         F32VecAvx512(_mm512_castsi512_ps(this.0), this.1)
    });

    #[inline(always)]
    fn bitcast_to_u32(self) -> U32VecAvx512 {
        U32VecAvx512(self.0, self.1)
    }

    fn_avx!(this: I32VecAvx512, fn abs() -> I32VecAvx512 {
        I32VecAvx512(_mm512_abs_epi32(this.0), this.1)
    });

    fn_avx!(this: I32VecAvx512, fn gt(rhs: I32VecAvx512) -> MaskAvx512 {
        MaskAvx512(_mm512_cmpgt_epi32_mask(this.0, rhs.0), this.1)
    });

    fn_avx!(this: I32VecAvx512, fn lt_zero() -> MaskAvx512 {
        I32VecAvx512(_mm512_setzero_epi32(), this.1).gt(this)
    });

    fn_avx!(this: I32VecAvx512, fn eq(rhs: I32VecAvx512) -> MaskAvx512 {
        MaskAvx512(_mm512_cmpeq_epi32_mask(this.0, rhs.0), this.1)
    });

    fn_avx!(this: I32VecAvx512, fn eq_zero() -> MaskAvx512 {
        I32VecAvx512(_mm512_setzero_epi32(), this.1).eq(this)
    });

    #[inline(always)]
    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know avx512f is available from the safety invariant on `d`.
        unsafe { I32VecAvx512(_mm512_slli_epi32::<AMOUNT_U>(self.0), self.1) }
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know avx512f is available from the safety invariant on `d`.
        unsafe { I32VecAvx512(_mm512_srai_epi32::<AMOUNT_U>(self.0), self.1) }
    }

    fn_avx!(this: I32VecAvx512, fn mul_wide_take_high(rhs: I32VecAvx512) -> I32VecAvx512 {
        let l = _mm512_mul_epi32(this.0, rhs.0);
        let h = _mm512_mul_epi32(_mm512_srli_epi64::<32>(this.0), _mm512_srli_epi64::<32>(rhs.0));
        let idx = _mm512_setr_epi32(1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31);
        I32VecAvx512(_mm512_permutex2var_epi32(l, idx, h), this.1)
    });

    #[inline(always)]
    fn store_u16(self, dest: &mut [u16]) {
        #[target_feature(enable = "avx512f")]
        #[inline]
        fn store_u16_impl(v: __m512i, dest: &mut [u16]) {
            assert!(dest.len() >= I32VecAvx512::LEN);
            let tmp = _mm512_cvtepi32_epi16(v);
            // SAFETY: We just checked `dst` has enough space.
            unsafe { _mm256_storeu_epi32(dest.as_mut_ptr().cast(), tmp) };
        }
        // SAFETY: avx512f is available from the safety invariant on the descriptor.
        unsafe { store_u16_impl(self.0, dest) }
    }
}

impl Add<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn add(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_add_epi32(this.0, rhs.0), this.1)
    });
}

impl Sub<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn sub(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_sub_epi32(this.0, rhs.0), this.1)
    });
}

impl Mul<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn mul(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_mullo_epi32(this.0, rhs.0), this.1)
    });
}

impl Neg for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn neg() -> I32VecAvx512 {
        I32VecAvx512(_mm512_setzero_epi32(), this.1) - this
    });
}

impl Shl<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn shl(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_sllv_epi32(this.0, rhs.0), this.1)
    });
}

impl Shr<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn shr(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_srav_epi32(this.0, rhs.0), this.1)
    });
}

impl BitAnd<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn bitand(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_and_si512(this.0, rhs.0), this.1)
    });
}

impl BitOr<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn bitor(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_or_si512(this.0, rhs.0), this.1)
    });
}

impl BitXor<I32VecAvx512> for I32VecAvx512 {
    type Output = I32VecAvx512;
    fn_avx!(this: I32VecAvx512, fn bitxor(rhs: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_xor_si512(this.0, rhs.0), this.1)
    });
}

impl AddAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn add_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_add_epi32(this.0, rhs.0)
    });
}

impl SubAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn sub_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_sub_epi32(this.0, rhs.0)
    });
}

impl MulAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn mul_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_mullo_epi32(this.0, rhs.0)
    });
}

impl ShlAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn shl_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_sllv_epi32(this.0, rhs.0)
    });
}

impl ShrAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn shr_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_srav_epi32(this.0, rhs.0)
    });
}

impl BitAndAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn bitand_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_and_si512(this.0, rhs.0)
    });
}

impl BitOrAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn bitor_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_or_si512(this.0, rhs.0)
    });
}

impl BitXorAssign<I32VecAvx512> for I32VecAvx512 {
    fn_avx!(this: &mut I32VecAvx512, fn bitxor_assign(rhs: I32VecAvx512) {
        this.0 = _mm512_xor_si512(this.0, rhs.0)
    });
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U32VecAvx512(__m512i, Avx512Descriptor);

impl U32SimdVec for U32VecAvx512 {
    type Descriptor = Avx512Descriptor;

    const LEN: usize = 16;

    #[inline(always)]
    fn bitcast_to_i32(self) -> I32VecAvx512 {
        I32VecAvx512(self.0, self.1)
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know avx512f is available from the safety invariant on `self.1`.
        unsafe { Self(_mm512_srli_epi32::<AMOUNT_U>(self.0), self.1) }
    }
}

impl SimdMask for MaskAvx512 {
    type Descriptor = Avx512Descriptor;

    fn_avx!(this: MaskAvx512, fn if_then_else_f32(if_true: F32VecAvx512, if_false: F32VecAvx512) -> F32VecAvx512 {
        F32VecAvx512(_mm512_mask_blend_ps(this.0, if_false.0, if_true.0), this.1)
    });

    fn_avx!(this: MaskAvx512, fn if_then_else_i32(if_true: I32VecAvx512, if_false: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_mask_blend_epi32(this.0, if_false.0, if_true.0), this.1)
    });

    fn_avx!(this: MaskAvx512, fn maskz_i32(v: I32VecAvx512) -> I32VecAvx512 {
        I32VecAvx512(_mm512_mask_set1_epi32(v.0, this.0, 0), this.1)
    });

    fn_avx!(this: MaskAvx512, fn all() -> bool {
        this.0 == 0b1111111111111111
    });

    fn_avx!(this: MaskAvx512, fn andnot(rhs: MaskAvx512) -> MaskAvx512 {
        MaskAvx512((!this.0) & rhs.0, this.1)
    });
}

impl BitAnd<MaskAvx512> for MaskAvx512 {
    type Output = MaskAvx512;
    fn_avx!(this: MaskAvx512, fn bitand(rhs: MaskAvx512) -> MaskAvx512 {
        MaskAvx512(this.0 & rhs.0, this.1)
    });
}

impl BitOr<MaskAvx512> for MaskAvx512 {
    type Output = MaskAvx512;
    fn_avx!(this: MaskAvx512, fn bitor(rhs: MaskAvx512) -> MaskAvx512 {
        MaskAvx512(this.0 | rhs.0, this.1)
    });
}
