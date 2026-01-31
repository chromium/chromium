// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{U32SimdVec, impl_f32_array_interface};

use super::super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};
use std::{
    arch::x86_64::*,
    mem::MaybeUninit,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Sub, SubAssign,
    },
};

// Safety invariant: this type is only ever constructed if sse4.2 is available.
#[derive(Clone, Copy, Debug)]
pub struct Sse42Descriptor(());

impl Sse42Descriptor {
    /// # Safety
    /// The caller must guarantee that the sse4.2 target feature is available.
    pub unsafe fn new_unchecked() -> Self {
        Self(())
    }
}

impl SimdDescriptor for Sse42Descriptor {
    type F32Vec = F32VecSse42;
    type I32Vec = I32VecSse42;
    type U32Vec = U32VecSse42;
    type Mask = MaskSse42;
    type Bf16Table8 = Bf16Table8Sse42;

    type Descriptor256 = Self;
    type Descriptor128 = Self;

    fn maybe_downgrade_256bit(self) -> Self::Descriptor256 {
        self
    }

    fn maybe_downgrade_128bit(self) -> Self::Descriptor128 {
        self
    }

    fn new() -> Option<Self> {
        if is_x86_feature_detected!("sse4.2") {
            // SAFETY: we just checked sse4.2.
            Some(unsafe { Self::new_unchecked() })
        } else {
            None
        }
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        #[target_feature(enable = "sse4.2")]
        #[inline(never)]
        unsafe fn inner<R>(d: Sse42Descriptor, f: impl FnOnce(Sse42Descriptor) -> R) -> R {
            f(d)
        }
        // SAFETY: the safety invariant on `self` guarantees sse4.2.
        unsafe { inner(self, f) }
    }
}

// TODO(veluca): retire this macro once we have #[unsafe(target_feature)].
macro_rules! fn_sse42 {
    (
        $this:ident: $self_ty:ty,
        fn $name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty )? $body: block) => {
        #[inline(always)]
        fn $name(self: $self_ty, $($arg: $ty),*) $(-> $ret)? {
            #[target_feature(enable = "sse4.2")]
            #[inline]
            fn inner($this: $self_ty, $($arg: $ty),*) $(-> $ret)? {
                $body
            }
            // SAFETY: `self.1` is constructed iff sse42 are available.
            unsafe { inner(self, $($arg),*) }
        }
    };
}

/// Prepared 8-entry BF16 lookup table for SSE4.2.
#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct Bf16Table8Sse42(__m128i);

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct F32VecSse42(__m128, Sse42Descriptor);

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct MaskSse42(__m128, Sse42Descriptor);

// SAFETY: The methods in this implementation that write to `MaybeUninit` (store_interleaved_*)
// ensure that they write valid data to the output slice without reading uninitialized memory.
unsafe impl F32SimdVec for F32VecSse42 {
    type Descriptor = Sse42Descriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[f32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know sse4.2 is available
        // from the safety invariant on `d`.
        Self(unsafe { _mm_loadu_ps(mem.as_ptr()) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [f32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know sse4.2 is available
        // from the safety invariant on `self.1`.
        unsafe { _mm_storeu_ps(mem.as_mut_ptr(), self.0) }
    }

    #[inline(always)]
    fn store_interleaved_2_uninit(a: Self, b: Self, dest: &mut [MaybeUninit<f32>]) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn store_interleaved_2_impl(a: __m128, b: __m128, dest: &mut [MaybeUninit<f32>]) {
            assert!(dest.len() >= 2 * F32VecSse42::LEN);
            // a = [a0, a1, a2, a3], b = [b0, b1, b2, b3]
            // lo = [a0, b0, a1, b1], hi = [a2, b2, a3, b3]
            let lo = _mm_unpacklo_ps(a, b);
            let hi = _mm_unpackhi_ps(a, b);
            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm_storeu_ps(dest_ptr, lo);
                _mm_storeu_ps(dest_ptr.add(4), hi);
            }
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_2_impl(a.0, b.0, dest) }
    }

    #[inline(always)]
    fn store_interleaved_3_uninit(a: Self, b: Self, c: Self, dest: &mut [MaybeUninit<f32>]) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn store_interleaved_3_impl(
            a: __m128,
            b: __m128,
            c: __m128,
            dest: &mut [MaybeUninit<f32>],
        ) {
            assert!(dest.len() >= 3 * F32VecSse42::LEN);
            // Input vectors:
            // a = [a0, a1, a2, a3]
            // b = [b0, b1, b2, b3]
            // c = [c0, c1, c2, c3]

            // Desired interleaved output stored in 3 __m128 registers:
            // out0 = [a0, b0, c0, a1]
            // out1 = [b1, c1, a2, b2]
            // out2 = [c2, a3, b3, c3]

            // Intermediate interleavings of input pairs
            let p_ab_lo = _mm_unpacklo_ps(a, b); // [a0, b0, a1, b1]
            let p_ab_hi = _mm_unpackhi_ps(a, b); // [a2, b2, a3, b3]

            let p_ca_lo = _mm_unpacklo_ps(c, a); // [c0, a0, c1, a1]
            let p_ca_hi = _mm_unpackhi_ps(c, a); // [c2, a2, c3, a3]

            let p_bc_hi = _mm_unpackhi_ps(b, c); // [b2, c2, b3, c3]

            // Construct out0 = [a0, b0, c0, a1]
            let out0 = _mm_shuffle_ps::<0xC4>(p_ab_lo, p_ca_lo);

            // Construct out1 = [b1, c1, a2, b2]
            let out1_tmp1 = _mm_shuffle_ps::<0xAF>(p_ab_lo, p_ca_lo); // [b1, b1, c1, c1]
            let out1 = _mm_shuffle_ps::<0x48>(out1_tmp1, p_ab_hi);

            // Construct out2 = [c2, a3, b3, c3]
            let out2 = _mm_shuffle_ps::<0xEC>(p_ca_hi, p_bc_hi);

            // Store the results
            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm_storeu_ps(dest_ptr, out0);
                _mm_storeu_ps(dest_ptr.add(4), out1);
                _mm_storeu_ps(dest_ptr.add(8), out2);
            }
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
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
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn store_interleaved_4_impl(
            a: __m128,
            b: __m128,
            c: __m128,
            d: __m128,
            dest: &mut [MaybeUninit<f32>],
        ) {
            assert!(dest.len() >= 4 * F32VecSse42::LEN);
            // First interleave pairs: ab and cd
            let ab_lo = _mm_unpacklo_ps(a, b); // [a0, b0, a1, b1]
            let ab_hi = _mm_unpackhi_ps(a, b); // [a2, b2, a3, b3]
            let cd_lo = _mm_unpacklo_ps(c, d); // [c0, d0, c1, d1]
            let cd_hi = _mm_unpackhi_ps(c, d); // [c2, d2, c3, d3]

            // Then interleave the pairs to get final layout
            let out0 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(ab_lo), _mm_castps_pd(cd_lo))); // [a0, b0, c0, d0]
            let out1 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(ab_lo), _mm_castps_pd(cd_lo))); // [a1, b1, c1, d1]
            let out2 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(ab_hi), _mm_castps_pd(cd_hi))); // [a2, b2, c2, d2]
            let out3 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(ab_hi), _mm_castps_pd(cd_hi))); // [a3, b3, c3, d3]

            // SAFETY: `dest` has enough space and writing to `MaybeUninit<f32>` through `*mut f32` is valid.
            unsafe {
                let dest_ptr = dest.as_mut_ptr() as *mut f32;
                _mm_storeu_ps(dest_ptr, out0);
                _mm_storeu_ps(dest_ptr.add(4), out1);
                _mm_storeu_ps(dest_ptr.add(8), out2);
                _mm_storeu_ps(dest_ptr.add(12), out3);
            }
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
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
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn store_interleaved_8_impl(
            a: __m128,
            b: __m128,
            c: __m128,
            d: __m128,
            e: __m128,
            f: __m128,
            g: __m128,
            h: __m128,
            dest: &mut [f32],
        ) {
            assert!(dest.len() >= 8 * F32VecSse42::LEN);
            // For 4-wide vectors storing 8 interleaved, we need 32 elements output
            // Output: [a0,b0,c0,d0,e0,f0,g0,h0, a1,b1,c1,d1,e1,f1,g1,h1, ...]
            let ab_lo = _mm_unpacklo_ps(a, b);
            let ab_hi = _mm_unpackhi_ps(a, b);
            let cd_lo = _mm_unpacklo_ps(c, d);
            let cd_hi = _mm_unpackhi_ps(c, d);
            let ef_lo = _mm_unpacklo_ps(e, f);
            let ef_hi = _mm_unpackhi_ps(e, f);
            let gh_lo = _mm_unpacklo_ps(g, h);
            let gh_hi = _mm_unpackhi_ps(g, h);

            let abcd_0 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(ab_lo), _mm_castps_pd(cd_lo)));
            let abcd_1 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(ab_lo), _mm_castps_pd(cd_lo)));
            let abcd_2 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(ab_hi), _mm_castps_pd(cd_hi)));
            let abcd_3 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(ab_hi), _mm_castps_pd(cd_hi)));
            let efgh_0 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(ef_lo), _mm_castps_pd(gh_lo)));
            let efgh_1 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(ef_lo), _mm_castps_pd(gh_lo)));
            let efgh_2 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(ef_hi), _mm_castps_pd(gh_hi)));
            let efgh_3 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(ef_hi), _mm_castps_pd(gh_hi)));

            // SAFETY: we just checked that dest has enough space.
            unsafe {
                let ptr = dest.as_mut_ptr();
                _mm_storeu_ps(ptr, abcd_0);
                _mm_storeu_ps(ptr.add(4), efgh_0);
                _mm_storeu_ps(ptr.add(8), abcd_1);
                _mm_storeu_ps(ptr.add(12), efgh_1);
                _mm_storeu_ps(ptr.add(16), abcd_2);
                _mm_storeu_ps(ptr.add(20), efgh_2);
                _mm_storeu_ps(ptr.add(24), abcd_3);
                _mm_storeu_ps(ptr.add(28), efgh_3);
            }
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        unsafe { store_interleaved_8_impl(a.0, b.0, c.0, d.0, e.0, f.0, g.0, h.0, dest) }
    }

    #[inline(always)]
    fn load_deinterleaved_2(d: Self::Descriptor, src: &[f32]) -> (Self, Self) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn load_deinterleaved_2_impl(src: &[f32]) -> (__m128, __m128) {
            assert!(src.len() >= 2 * F32VecSse42::LEN);
            // Input: [a0, b0, a1, b1, a2, b2, a3, b3]
            // Output: a = [a0, a1, a2, a3], b = [b0, b1, b2, b3]
            // SAFETY: we just checked that src has enough space.
            let (in0, in1) = unsafe {
                (
                    _mm_loadu_ps(src.as_ptr()),        // [a0, b0, a1, b1]
                    _mm_loadu_ps(src.as_ptr().add(4)), // [a2, b2, a3, b3]
                )
            };

            // Shuffle to separate a and b components
            let a = _mm_shuffle_ps::<0x88>(in0, in1); // [a0, a1, a2, a3]
            let b = _mm_shuffle_ps::<0xDD>(in0, in1); // [b0, b1, b2, b3]

            (a, b)
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        let (a, b) = unsafe { load_deinterleaved_2_impl(src) };
        (Self(a, d), Self(b, d))
    }

    #[inline(always)]
    fn load_deinterleaved_3(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn load_deinterleaved_3_impl(src: &[f32]) -> (__m128, __m128, __m128) {
            assert!(src.len() >= 3 * F32VecSse42::LEN);
            // Input: [a0, b0, c0, a1, b1, c1, a2, b2, c2, a3, b3, c3]
            // Output: a = [a0, a1, a2, a3], b = [b0, b1, b2, b3], c = [c0, c1, c2, c3]

            // SAFETY: we just checked that src has enough space.
            let (in0, in1, in2) = unsafe {
                (
                    _mm_loadu_ps(src.as_ptr()),        // [a0, b0, c0, a1]
                    _mm_loadu_ps(src.as_ptr().add(4)), // [b1, c1, a2, b2]
                    _mm_loadu_ps(src.as_ptr().add(8)), // [c2, a3, b3, c3]
                )
            };

            // Extract using shuffles.
            // _mm_shuffle_ps(a, b, imm8): result[0:1] from a, result[2:3] from b
            // imm8 bits: [1:0]=A, [3:2]=B select from a; [5:4]=C, [7:6]=D select from b
            //
            // Element positions in input:
            // a: a0=in0[0], a1=in0[3], a2=in1[2], a3=in2[1]
            // b: b0=in0[1], b1=in1[0], b2=in1[3], b3=in2[2]
            // c: c0=in0[2], c1=in1[1], c2=in2[0], c3=in2[3]

            // Channel a: gather a0,a1 and a2,a3, then combine
            let a_lo = _mm_shuffle_ps::<0xC0>(in0, in0); // [a0, a0, a0, a1]
            let a_hi = _mm_shuffle_ps::<0x98>(in1, in2); // [b1, a2, a3, b3]
            let a = _mm_shuffle_ps::<0x9C>(a_lo, a_hi); // [a0, a1, a2, a3]

            // Channel b: gather b0,b1 and b2,b3, then combine
            let b_lo = _mm_shuffle_ps::<0x01>(in0, in1); // [b0, a0, b1, b1]
            let b_hi = _mm_shuffle_ps::<0x2C>(in1, in2); // [b1, b2, b3, c2]
            let b = _mm_shuffle_ps::<0x98>(b_lo, b_hi); // [b0, b1, b2, b3]

            // Channel c: gather c0,c1 and c2,c3, then combine
            let c_lo = _mm_shuffle_ps::<0x12>(in0, in1); // [c0, a0, c1, b1]
            let c_hi = _mm_shuffle_ps::<0x30>(in2, in2); // [c2, c2, c3, c2]
            let c = _mm_shuffle_ps::<0x98>(c_lo, c_hi); // [c0, c1, c2, c3]

            (a, b, c)
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        let (a, b, c) = unsafe { load_deinterleaved_3_impl(src) };
        (Self(a, d), Self(b, d), Self(c, d))
    }

    #[inline(always)]
    fn load_deinterleaved_4(d: Self::Descriptor, src: &[f32]) -> (Self, Self, Self, Self) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn load_deinterleaved_4_impl(src: &[f32]) -> (__m128, __m128, __m128, __m128) {
            assert!(src.len() >= 4 * F32VecSse42::LEN);
            // Input: [a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, a3, b3, c3, d3]
            // Output: a = [a0, a1, a2, a3], b = [b0, b1, b2, b3], c = [c0, c1, c2, c3], d = [d0, d1, d2, d3]
            // SAFETY: we just checked that src has enough space.
            let (in0, in1, in2, in3) = unsafe {
                (
                    _mm_loadu_ps(src.as_ptr()),         // [a0, b0, c0, d0]
                    _mm_loadu_ps(src.as_ptr().add(4)),  // [a1, b1, c1, d1]
                    _mm_loadu_ps(src.as_ptr().add(8)),  // [a2, b2, c2, d2]
                    _mm_loadu_ps(src.as_ptr().add(12)), // [a3, b3, c3, d3]
                )
            };

            // This is effectively a 4x4 matrix transpose
            // First interleave pairs
            let t0 = _mm_unpacklo_ps(in0, in1); // [a0, a1, b0, b1]
            let t1 = _mm_unpackhi_ps(in0, in1); // [c0, c1, d0, d1]
            let t2 = _mm_unpacklo_ps(in2, in3); // [a2, a3, b2, b3]
            let t3 = _mm_unpackhi_ps(in2, in3); // [c2, c3, d2, d3]

            // Then combine
            let a = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(t0), _mm_castps_pd(t2))); // [a0, a1, a2, a3]
            let b = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(t0), _mm_castps_pd(t2))); // [b0, b1, b2, b3]
            let c = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(t1), _mm_castps_pd(t3))); // [c0, c1, c2, c3]
            let dv = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(t1), _mm_castps_pd(t3))); // [d0, d1, d2, d3]

            (a, b, c, dv)
        }

        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        let (a, b, c, dv) = unsafe { load_deinterleaved_4_impl(src) };
        (Self(a, d), Self(b, d), Self(c, d), Self(dv, d))
    }

    fn_sse42!(this: F32VecSse42, fn mul_add(mul: F32VecSse42, add: F32VecSse42) -> F32VecSse42 {
        this * mul + add
    });

    fn_sse42!(this: F32VecSse42, fn neg_mul_add(mul: F32VecSse42, add: F32VecSse42) -> F32VecSse42 {
        add - this * mul
    });

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: f32) -> Self {
        // SAFETY: We know sse4.2 is available from the safety invariant on `d`.
        unsafe { Self(_mm_set1_ps(v), d) }
    }

    #[inline(always)]
    fn zero(d: Self::Descriptor) -> Self {
        // SAFETY: We know sse4.2 is available from the safety invariant on `d`.
        unsafe { Self(_mm_setzero_ps(), d) }
    }

    fn_sse42!(this: F32VecSse42, fn abs() -> F32VecSse42 {
        F32VecSse42(
            _mm_castsi128_ps(_mm_andnot_si128(
                _mm_set1_epi32(i32::MIN),
                _mm_castps_si128(this.0),
            )),
            this.1)
    });

    fn_sse42!(this: F32VecSse42, fn floor() -> F32VecSse42 {
        F32VecSse42(_mm_floor_ps(this.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn sqrt() -> F32VecSse42 {
        F32VecSse42(_mm_sqrt_ps(this.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn neg() -> F32VecSse42 {
        F32VecSse42(
            _mm_castsi128_ps(_mm_xor_si128(
                _mm_set1_epi32(i32::MIN),
                _mm_castps_si128(this.0),
            )),
            this.1)
    });

    fn_sse42!(this: F32VecSse42, fn copysign(sign: F32VecSse42) -> F32VecSse42 {
        let sign_mask = _mm_castsi128_ps(_mm_set1_epi32(i32::MIN));
        F32VecSse42(
            _mm_or_ps(
                _mm_andnot_ps(sign_mask, this.0),
                _mm_and_ps(sign_mask, sign.0),
            ),
            this.1,
        )
    });

    fn_sse42!(this: F32VecSse42, fn max(other: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_max_ps(this.0, other.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn min(other: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_min_ps(this.0, other.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn gt(other: F32VecSse42) -> MaskSse42 {
        MaskSse42(_mm_cmpgt_ps(this.0, other.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn as_i32() -> I32VecSse42 {
        I32VecSse42(_mm_cvtps_epi32(this.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn bitcast_to_i32() -> I32VecSse42 {
        I32VecSse42(_mm_castps_si128(this.0), this.1)
    });

    #[inline(always)]
    fn prepare_table_bf16_8(_d: Sse42Descriptor, table: &[f32; 8]) -> Bf16Table8Sse42 {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn prepare_impl(table: &[f32; 8]) -> __m128i {
            // Convert f32 table to BF16 packed in 128 bits (16 bytes for 8 entries)
            // BF16 is the high 16 bits of f32
            // SAFETY: table has exactly 8 elements and sse4.2 is available from target_feature
            let (table_lo, table_hi) = unsafe {
                (
                    _mm_loadu_ps(table.as_ptr()),
                    _mm_loadu_ps(table.as_ptr().add(4)),
                )
            };
            let table_lo_i32 = _mm_castps_si128(table_lo);
            let table_hi_i32 = _mm_castps_si128(table_hi);

            // Extract high 16 bits (bf16) from each f32 using shuffle
            // f32 bytes: [b0, b1, b2, b3] -> bf16 bytes: [b2, b3]
            let bf16_extract =
                _mm_setr_epi8(2, 3, 6, 7, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1);
            let bf16_lo = _mm_shuffle_epi8(table_lo_i32, bf16_extract);
            let bf16_hi = _mm_shuffle_epi8(table_hi_i32, bf16_extract);
            // Combine: bf16_lo has bytes 0-7, bf16_hi has bytes 0-7
            // Result: [bf16_0..bf16_3, bf16_4..bf16_7]
            _mm_unpacklo_epi64(bf16_lo, bf16_hi)
        }
        // SAFETY: sse4.2 is available from the safety invariant on the descriptor
        Bf16Table8Sse42(unsafe { prepare_impl(table) })
    }

    #[inline(always)]
    fn table_lookup_bf16_8(
        d: Sse42Descriptor,
        table: Bf16Table8Sse42,
        indices: I32VecSse42,
    ) -> Self {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn lookup_impl(bf16_table: __m128i, indices: __m128i) -> __m128 {
            // Build shuffle mask efficiently using arithmetic on 32-bit indices.
            // For each index i (0-7), we need to select bytes [2*i, 2*i+1] from bf16_table
            // and place them in the high 16 bits of each 32-bit f32 lane (bytes 2,3),
            // with bytes 0,1 set to zero (using 0x80 which gives 0 in pshufb).
            //
            // Output byte pattern per lane (little-endian): [0x80, 0x80, 2*i, 2*i+1]
            // As a 32-bit value: 0x80 | (0x80 << 8) | (2*i << 16) | ((2*i+1) << 24)
            //                  = 0x8080 | (i << 17) | (i << 25) | (1 << 24)
            //                  = (i << 17) | (i << 25) | 0x01008080
            let shl17 = _mm_slli_epi32::<17>(indices);
            let shl25 = _mm_slli_epi32::<25>(indices);
            let base = _mm_set1_epi32(0x01008080u32 as i32);
            let shuffle_mask = _mm_or_si128(_mm_or_si128(shl17, shl25), base);

            // Shuffle the bf16 table to get the values
            let result = _mm_shuffle_epi8(bf16_table, shuffle_mask);

            // Result has bf16 in high 16 bits of each 32-bit lane = valid f32
            _mm_castsi128_ps(result)
        }
        // SAFETY: sse4.2 is available from the safety invariant on the descriptor
        F32VecSse42(unsafe { lookup_impl(table.0, indices.0) }, d)
    }

    #[inline(always)]
    fn round_store_u8(self, dest: &mut [u8]) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn round_store_u8_impl(v: __m128, dest: &mut [u8]) {
            assert!(dest.len() >= F32VecSse42::LEN);
            // Round to nearest integer
            let rounded = _mm_round_ps::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(v);
            // Convert to i32
            let i32s = _mm_cvtps_epi32(rounded);
            // Pack i32 -> u16 -> u8 (use same vector twice, take lower half each time)
            let u16s = _mm_packus_epi32(i32s, i32s);
            let u8s = _mm_packus_epi16(u16s, u16s);
            // Store lower 4 bytes
            // SAFETY: we checked dest has enough space
            unsafe {
                let ptr = dest.as_mut_ptr() as *mut i32;
                *ptr = _mm_cvtsi128_si32(u8s);
            }
        }
        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        unsafe { round_store_u8_impl(self.0, dest) }
    }

    #[inline(always)]
    fn round_store_u16(self, dest: &mut [u16]) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn round_store_u16_impl(v: __m128, dest: &mut [u16]) {
            assert!(dest.len() >= F32VecSse42::LEN);
            // Round to nearest integer
            let rounded = _mm_round_ps::<{ _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC }>(v);
            // Convert to i32
            let i32s = _mm_cvtps_epi32(rounded);
            // Pack i32 -> u16 (use same vector twice, take lower half)
            let u16s = _mm_packus_epi32(i32s, i32s);
            // Store lower 8 bytes (4 u16s)
            // SAFETY: we checked dest has enough space
            unsafe {
                _mm_storel_epi64(dest.as_mut_ptr() as *mut __m128i, u16s);
            }
        }
        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        unsafe { round_store_u16_impl(self.0, dest) }
    }

    impl_f32_array_interface!();

    #[inline(always)]
    fn load_f16_bits(d: Self::Descriptor, mem: &[u16]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SSE4.2 doesn't have F16C, use scalar conversion
        let mut result = [0.0f32; 4];
        for i in 0..4 {
            result[i] = crate::f16::from_bits(mem[i]).to_f32();
        }
        Self::load(d, &result)
    }

    #[inline(always)]
    fn store_f16_bits(self, dest: &mut [u16]) {
        assert!(dest.len() >= Self::LEN);
        // SSE4.2 doesn't have F16C, use scalar conversion
        let mut tmp = [0.0f32; 4];
        self.store(&mut tmp);
        for i in 0..4 {
            dest[i] = crate::f16::from_f32(tmp[i]).to_bits();
        }
    }

    #[inline(always)]
    fn transpose_square(d: Self::Descriptor, data: &mut [Self::UnderlyingArray], stride: usize) {
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn transpose4x4f32(d: Sse42Descriptor, data: &mut [[f32; 4]], stride: usize) {
            assert!(data.len() > stride * 3);

            let p0 = F32VecSse42::load_array(d, &data[0]).0;
            let p1 = F32VecSse42::load_array(d, &data[1 * stride]).0;
            let p2 = F32VecSse42::load_array(d, &data[2 * stride]).0;
            let p3 = F32VecSse42::load_array(d, &data[3 * stride]).0;

            let q0 = _mm_unpacklo_ps(p0, p2);
            let q1 = _mm_unpacklo_ps(p1, p3);
            let q2 = _mm_unpackhi_ps(p0, p2);
            let q3 = _mm_unpackhi_ps(p1, p3);

            let r0 = _mm_unpacklo_ps(q0, q1);
            let r1 = _mm_unpackhi_ps(q0, q1);
            let r2 = _mm_unpacklo_ps(q2, q3);
            let r3 = _mm_unpackhi_ps(q2, q3);

            F32VecSse42(r0, d).store_array(&mut data[0]);
            F32VecSse42(r1, d).store_array(&mut data[1 * stride]);
            F32VecSse42(r2, d).store_array(&mut data[2 * stride]);
            F32VecSse42(r3, d).store_array(&mut data[3 * stride]);
        }

        // SAFETY: the safety invariant on `d` guarantees sse42
        unsafe {
            transpose4x4f32(d, data, stride);
        }
    }
}

impl Add<F32VecSse42> for F32VecSse42 {
    type Output = F32VecSse42;
    fn_sse42!(this: F32VecSse42, fn add(rhs: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_add_ps(this.0, rhs.0), this.1)
    });
}

impl Sub<F32VecSse42> for F32VecSse42 {
    type Output = F32VecSse42;
    fn_sse42!(this: F32VecSse42, fn sub(rhs: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_sub_ps(this.0, rhs.0), this.1)
    });
}

impl Mul<F32VecSse42> for F32VecSse42 {
    type Output = F32VecSse42;
    fn_sse42!(this: F32VecSse42, fn mul(rhs: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_mul_ps(this.0, rhs.0), this.1)
    });
}

impl Div<F32VecSse42> for F32VecSse42 {
    type Output = F32VecSse42;
    fn_sse42!(this: F32VecSse42, fn div(rhs: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_div_ps(this.0, rhs.0), this.1)
    });
}

impl AddAssign<F32VecSse42> for F32VecSse42 {
    fn_sse42!(this: &mut F32VecSse42, fn add_assign(rhs: F32VecSse42) {
        this.0 = _mm_add_ps(this.0, rhs.0)
    });
}

impl SubAssign<F32VecSse42> for F32VecSse42 {
    fn_sse42!(this: &mut F32VecSse42, fn sub_assign(rhs: F32VecSse42) {
        this.0 = _mm_sub_ps(this.0, rhs.0)
    });
}

impl MulAssign<F32VecSse42> for F32VecSse42 {
    fn_sse42!(this: &mut F32VecSse42, fn mul_assign(rhs: F32VecSse42) {
        this.0 = _mm_mul_ps(this.0, rhs.0)
    });
}

impl DivAssign<F32VecSse42> for F32VecSse42 {
    fn_sse42!(this: &mut F32VecSse42, fn div_assign(rhs: F32VecSse42) {
        this.0 = _mm_div_ps(this.0, rhs.0)
    });
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct I32VecSse42(__m128i, Sse42Descriptor);

impl I32SimdVec for I32VecSse42 {
    type Descriptor = Sse42Descriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn load(d: Self::Descriptor, mem: &[i32]) -> Self {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know sse4.2 is available
        // from the safety invariant on `d`.
        Self(unsafe { _mm_loadu_si128(mem.as_ptr() as *const _) }, d)
    }

    #[inline(always)]
    fn store(&self, mem: &mut [i32]) {
        assert!(mem.len() >= Self::LEN);
        // SAFETY: we just checked that `mem` has enough space. Moreover, we know sse4.2 is available
        // from the safety invariant on `self.1`.
        unsafe { _mm_storeu_si128(mem.as_mut_ptr().cast(), self.0) }
    }

    #[inline(always)]
    fn splat(d: Self::Descriptor, v: i32) -> Self {
        // SAFETY: We know sse4.2 is available from the safety invariant on `d`.
        unsafe { Self(_mm_set1_epi32(v), d) }
    }

    fn_sse42!(this: I32VecSse42, fn as_f32() -> F32VecSse42 {
        F32VecSse42(_mm_cvtepi32_ps(this.0), this.1)
    });

    fn_sse42!(this: I32VecSse42, fn bitcast_to_f32() -> F32VecSse42 {
        F32VecSse42(_mm_castsi128_ps(this.0), this.1)
    });

    #[inline(always)]
    fn bitcast_to_u32(self) -> U32VecSse42 {
        U32VecSse42(self.0, self.1)
    }

    fn_sse42!(this: I32VecSse42, fn abs() -> I32VecSse42 {
        I32VecSse42(
            _mm_abs_epi32(
                this.0,
            ),
            this.1)
    });

    fn_sse42!(this: I32VecSse42, fn gt(rhs: I32VecSse42) -> MaskSse42 {
        MaskSse42(
            _mm_castsi128_ps(_mm_cmpgt_epi32(this.0, rhs.0)),
            this.1,
        )
    });

    fn_sse42!(this: I32VecSse42, fn lt_zero() -> MaskSse42 {
        I32VecSse42(_mm_setzero_si128(), this.1).gt(this)
    });

    fn_sse42!(this: I32VecSse42, fn eq(rhs: I32VecSse42) -> MaskSse42 {
        MaskSse42(
            _mm_castsi128_ps(_mm_cmpeq_epi32(this.0, rhs.0)),
            this.1,
        )
    });

    fn_sse42!(this: I32VecSse42, fn eq_zero() -> MaskSse42 {
        this.eq(I32VecSse42(_mm_setzero_si128(), this.1))
    });

    #[inline(always)]
    fn shl<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know sse2 is available from the safety invariant on `d`.
        unsafe { Self(_mm_slli_epi32::<AMOUNT_I>(self.0), self.1) }
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know sse2 is available from the safety invariant on `d`.
        unsafe { Self(_mm_srai_epi32::<AMOUNT_I>(self.0), self.1) }
    }

    fn_sse42!(this: I32VecSse42, fn mul_wide_take_high(rhs: I32VecSse42) -> I32VecSse42 {
        let l = _mm_mul_epi32(this.0, rhs.0);
        let h = _mm_mul_epi32(_mm_srli_epi64::<32>(this.0), _mm_srli_epi64::<32>(rhs.0));
        let p0 = _mm_unpacklo_epi32(l, h);
        let p1 = _mm_unpackhi_epi32(l, h);
        I32VecSse42(_mm_unpackhi_epi64(p0, p1), this.1)
    });

    #[inline(always)]
    fn store_u16(self, dest: &mut [u16]) {
        // Pack i32 to i16 with signed saturation, then store lower 64 bits
        // _mm_packs_epi32 saturates i32 to i16, which preserves low 16 bits for values in range
        #[target_feature(enable = "sse4.2")]
        #[inline]
        fn store_u16_impl(v: __m128i, dest: &mut [u16]) {
            assert!(dest.len() >= I32VecSse42::LEN);
            // Use scalar loop since _mm_packs_epi32 would saturate incorrectly for unsigned values
            let mut tmp = [0i32; 4];
            // SAFETY: tmp has 4 elements, matching LEN
            unsafe { _mm_storeu_si128(tmp.as_mut_ptr() as *mut __m128i, v) };
            for i in 0..4 {
                dest[i] = tmp[i] as u16;
            }
        }
        // SAFETY: sse4.2 is available from the safety invariant on the descriptor.
        unsafe { store_u16_impl(self.0, dest) }
    }
}

impl Add<I32VecSse42> for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn add(rhs: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_add_epi32(this.0, rhs.0), this.1)
    });
}

impl Sub<I32VecSse42> for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn sub(rhs: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_sub_epi32(this.0, rhs.0), this.1)
    });
}

impl Mul<I32VecSse42> for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn mul(rhs: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_mullo_epi32(this.0, rhs.0), this.1)
    });
}

impl Neg for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn neg() -> I32VecSse42 {
        I32VecSse42(_mm_setzero_si128(), this.1) - this
    });
}

impl BitAnd<I32VecSse42> for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn bitand(rhs: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_and_si128(this.0, rhs.0), this.1)
    });
}

impl BitOr<I32VecSse42> for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn bitor(rhs: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_or_si128(this.0, rhs.0), this.1)
    });
}

impl BitXor<I32VecSse42> for I32VecSse42 {
    type Output = I32VecSse42;
    fn_sse42!(this: I32VecSse42, fn bitxor(rhs: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_xor_si128(this.0, rhs.0), this.1)
    });
}

impl AddAssign<I32VecSse42> for I32VecSse42 {
    fn_sse42!(this: &mut I32VecSse42, fn add_assign(rhs: I32VecSse42) {
        this.0 = _mm_add_epi32(this.0, rhs.0)
    });
}

impl SubAssign<I32VecSse42> for I32VecSse42 {
    fn_sse42!(this: &mut I32VecSse42, fn sub_assign(rhs: I32VecSse42) {
        this.0 = _mm_sub_epi32(this.0, rhs.0)
    });
}

impl MulAssign<I32VecSse42> for I32VecSse42 {
    fn_sse42!(this: &mut I32VecSse42, fn mul_assign(rhs: I32VecSse42) {
        this.0 = _mm_mullo_epi32(this.0, rhs.0)
    });
}

impl BitAndAssign<I32VecSse42> for I32VecSse42 {
    fn_sse42!(this: &mut I32VecSse42, fn bitand_assign(rhs: I32VecSse42) {
        this.0 = _mm_and_si128(this.0, rhs.0)
    });
}

impl BitOrAssign<I32VecSse42> for I32VecSse42 {
    fn_sse42!(this: &mut I32VecSse42, fn bitor_assign(rhs: I32VecSse42) {
        this.0 = _mm_or_si128(this.0, rhs.0)
    });
}

impl BitXorAssign<I32VecSse42> for I32VecSse42 {
    fn_sse42!(this: &mut I32VecSse42, fn bitxor_assign(rhs: I32VecSse42) {
        this.0 = _mm_xor_si128(this.0, rhs.0)
    });
}

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct U32VecSse42(__m128i, Sse42Descriptor);

impl U32SimdVec for U32VecSse42 {
    type Descriptor = Sse42Descriptor;

    const LEN: usize = 4;

    #[inline(always)]
    fn bitcast_to_i32(self) -> I32VecSse42 {
        I32VecSse42(self.0, self.1)
    }

    #[inline(always)]
    fn shr<const AMOUNT_U: u32, const AMOUNT_I: i32>(self) -> Self {
        // SAFETY: We know sse2 is available from the safety invariant on `self.1`.
        unsafe { Self(_mm_srli_epi32::<AMOUNT_I>(self.0), self.1) }
    }
}

impl SimdMask for MaskSse42 {
    type Descriptor = Sse42Descriptor;

    fn_sse42!(this: MaskSse42, fn if_then_else_f32(if_true: F32VecSse42, if_false: F32VecSse42) -> F32VecSse42 {
        F32VecSse42(_mm_blendv_ps(if_false.0, if_true.0, this.0), this.1)
    });

    fn_sse42!(this: MaskSse42, fn if_then_else_i32(if_true: I32VecSse42, if_false: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_blendv_epi8(if_false.0, if_true.0, _mm_castps_si128(this.0)), this.1)
    });

    fn_sse42!(this: MaskSse42, fn maskz_i32(v: I32VecSse42) -> I32VecSse42 {
        I32VecSse42(_mm_andnot_si128(_mm_castps_si128(this.0), v.0), this.1)
    });

    fn_sse42!(this: MaskSse42, fn all() -> bool {
        _mm_movemask_ps(this.0) == 0b1111
    });

    fn_sse42!(this: MaskSse42, fn andnot(rhs: MaskSse42) -> MaskSse42 {
        MaskSse42(_mm_andnot_ps(this.0, rhs.0), this.1)
    });
}

impl BitAnd<MaskSse42> for MaskSse42 {
    type Output = MaskSse42;
    fn_sse42!(this: MaskSse42, fn bitand(rhs: MaskSse42) -> MaskSse42 {
        MaskSse42(_mm_and_ps(this.0, rhs.0), this.1)
    });
}

impl BitOr<MaskSse42> for MaskSse42 {
    type Output = MaskSse42;
    fn_sse42!(this: MaskSse42, fn bitor(rhs: MaskSse42) -> MaskSse42 {
        MaskSse42(_mm_or_ps(this.0, rhs.0), this.1)
    });
}
