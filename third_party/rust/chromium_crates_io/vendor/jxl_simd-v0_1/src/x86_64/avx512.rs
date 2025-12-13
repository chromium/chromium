// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::super::{AvxDescriptor, F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};
use crate::{Sse42Descriptor, U32SimdVec, impl_f32_array_interface};
use std::{
    arch::x86_64::*,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Shl, ShlAssign, Shr, ShrAssign, Sub, SubAssign,
    },
};

// Safety invariant: this type is only ever constructed if avx512f is available.
#[derive(Clone, Copy, Debug)]
pub struct Avx512Descriptor(());

#[allow(unused)]
impl Avx512Descriptor {
    /// # Safety
    /// The caller must guarantee that the "avx512f" target feature is available.
    pub unsafe fn new_unchecked() -> Self {
        Self(())
    }
    pub fn as_avx(&self) -> AvxDescriptor {
        // SAFETY: the safety invariant on `self` guarantees avx512f is available, which implies
        // avx2 and fma.
        unsafe { AvxDescriptor::new_unchecked() }
    }
}

impl SimdDescriptor for Avx512Descriptor {
    type F32Vec = F32VecAvx512;
    type I32Vec = I32VecAvx512;
    type U32Vec = U32VecAvx512;
    type Mask = MaskAvx512;

    type Descriptor256 = AvxDescriptor;
    type Descriptor128 = Sse42Descriptor;

    fn maybe_downgrade_256bit(self) -> Self::Descriptor256 {
        self.as_avx()
    }

    fn maybe_downgrade_128bit(self) -> Self::Descriptor128 {
        self.as_avx().as_sse42()
    }

    fn new() -> Option<Self> {
        if is_x86_feature_detected!("avx512f") {
            // SAFETY: we just checked avx512f.
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

impl F32SimdVec for F32VecAvx512 {
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

    fn_avx!(this: F32VecAvx512, fn gt(other: F32VecAvx512) -> MaskAvx512 {
        MaskAvx512(_mm512_cmp_ps_mask::<{_CMP_GT_OQ}>(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn as_i32() -> I32VecAvx512 {
        I32VecAvx512(_mm512_cvtps_epi32(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx512, fn bitcast_to_i32() -> I32VecAvx512 {
        I32VecAvx512(_mm512_castps_si512(this.0), this.1)
    });

    impl_f32_array_interface!();

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
        I32VecAvx512(_mm512_mul_epi32(this.0, rhs.0), this.1)
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
        this.0 = _mm512_mul_epi32(this.0, rhs.0)
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

    const LEN: usize = 8;

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
