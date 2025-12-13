// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{U32SimdVec, impl_f32_array_interface, x86_64::sse42::Sse42Descriptor};

use super::super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};
use std::{
    arch::x86_64::*,
    ops::{
        Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div,
        DivAssign, Mul, MulAssign, Neg, Shl, ShlAssign, Shr, ShrAssign, Sub, SubAssign,
    },
};

// Safety invariant: this type is only ever constructed if avx2 and fma are available.
#[derive(Clone, Copy, Debug)]
pub struct AvxDescriptor(());

impl AvxDescriptor {
    /// # Safety
    /// The caller must guarantee that the "avx2" and "fma" target features are available.
    pub unsafe fn new_unchecked() -> Self {
        Self(())
    }

    pub fn as_sse42(&self) -> Sse42Descriptor {
        // SAFETY: the safety invariant on `self` guarantees avx is available, which implies
        // sse42.
        unsafe { Sse42Descriptor::new_unchecked() }
    }
}

impl SimdDescriptor for AvxDescriptor {
    type F32Vec = F32VecAvx;
    type I32Vec = I32VecAvx;
    type U32Vec = U32VecAvx;
    type Mask = MaskAvx;

    type Descriptor256 = Self;
    type Descriptor128 = Sse42Descriptor;

    fn maybe_downgrade_256bit(self) -> Self::Descriptor256 {
        self
    }

    fn maybe_downgrade_128bit(self) -> Self::Descriptor128 {
        self.as_sse42()
    }

    fn new() -> Option<Self> {
        if is_x86_feature_detected!("avx2") && is_x86_feature_detected!("fma") {
            // SAFETY: we just checked avx2 and fma.
            Some(unsafe { Self::new_unchecked() })
        } else {
            None
        }
    }

    fn call<R>(self, f: impl FnOnce(Self) -> R) -> R {
        #[target_feature(enable = "avx2,fma")]
        #[inline(never)]
        unsafe fn inner<R>(d: AvxDescriptor, f: impl FnOnce(AvxDescriptor) -> R) -> R {
            f(d)
        }
        // SAFETY: the safety invariant on `self` guarantees avx2 and fma.
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
            #[target_feature(enable = "fma,avx2")]
            #[inline]
            fn inner($this: $self_ty, $($arg: $ty),*) $(-> $ret)? {
                $body
            }
            // SAFETY: `self.1` is constructed iff avx2 and fma are available.
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

impl F32SimdVec for F32VecAvx {
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

    fn_avx!(this: F32VecAvx, fn gt(other: F32VecAvx) -> MaskAvx {
        MaskAvx(_mm256_cmp_ps::<{_CMP_GT_OQ}>(this.0, other.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn as_i32() -> I32VecAvx {
        I32VecAvx(_mm256_cvtps_epi32(this.0), this.1)
    });

    fn_avx!(this: F32VecAvx, fn bitcast_to_i32() -> I32VecAvx {
        I32VecAvx(_mm256_castps_si256(this.0), this.1)
    });

    impl_f32_array_interface!();

    #[inline(always)]
    fn transpose_square(d: Self::Descriptor, data: &mut [Self::UnderlyingArray], stride: usize) {
        #[target_feature(enable = "avx2")]
        #[inline]
        fn unpacklo_pd(a: __m256, b: __m256) -> __m256 {
            _mm256_castpd_ps(_mm256_unpacklo_pd(_mm256_castps_pd(a), _mm256_castps_pd(b)))
        }

        #[target_feature(enable = "avx2")]
        #[inline]
        fn unpackhi_pd(a: __m256, b: __m256) -> __m256 {
            _mm256_castpd_ps(_mm256_unpackhi_pd(_mm256_castps_pd(a), _mm256_castps_pd(b)))
        }

        #[target_feature(enable = "avx2")]
        #[inline]
        fn transpose8x8f32(d: AvxDescriptor, data: &mut [[f32; 8]], stride: usize) {
            assert!(data.len() > stride * 7);

            let r0 = F32VecAvx::load_array(d, &data[0]).0;
            let r1 = F32VecAvx::load_array(d, &data[1 * stride]).0;
            let r2 = F32VecAvx::load_array(d, &data[2 * stride]).0;
            let r3 = F32VecAvx::load_array(d, &data[3 * stride]).0;
            let r4 = F32VecAvx::load_array(d, &data[4 * stride]).0;
            let r5 = F32VecAvx::load_array(d, &data[5 * stride]).0;
            let r6 = F32VecAvx::load_array(d, &data[6 * stride]).0;
            let r7 = F32VecAvx::load_array(d, &data[7 * stride]).0;

            // Stage 1: Unpack low/high pairs
            let t0 = _mm256_unpacklo_ps(r0, r1);
            let t1 = _mm256_unpackhi_ps(r0, r1);
            let t2 = _mm256_unpacklo_ps(r2, r3);
            let t3 = _mm256_unpackhi_ps(r2, r3);
            let t4 = _mm256_unpacklo_ps(r4, r5);
            let t5 = _mm256_unpackhi_ps(r4, r5);
            let t6 = _mm256_unpacklo_ps(r6, r7);
            let t7 = _mm256_unpackhi_ps(r6, r7);

            // Stage 2: Shuffle to group 32-bit elements
            let s0 = unpacklo_pd(t0, t2);
            let s1 = unpackhi_pd(t0, t2);
            let s2 = unpacklo_pd(t1, t3);
            let s3 = unpackhi_pd(t1, t3);
            let s4 = unpacklo_pd(t4, t6);
            let s5 = unpackhi_pd(t4, t6);
            let s6 = unpacklo_pd(t5, t7);
            let s7 = unpackhi_pd(t5, t7);

            // Stage 3: 128-bit permute to finalize transpose
            let c0 = _mm256_permute2f128_ps::<0x20>(s0, s4);
            let c1 = _mm256_permute2f128_ps::<0x20>(s1, s5);
            let c2 = _mm256_permute2f128_ps::<0x20>(s2, s6);
            let c3 = _mm256_permute2f128_ps::<0x20>(s3, s7);
            let c4 = _mm256_permute2f128_ps::<0x31>(s0, s4);
            let c5 = _mm256_permute2f128_ps::<0x31>(s1, s5);
            let c6 = _mm256_permute2f128_ps::<0x31>(s2, s6);
            let c7 = _mm256_permute2f128_ps::<0x31>(s3, s7);

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
        I32VecAvx(_mm256_mul_epi32(this.0, rhs.0), this.1)
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
        this.0 = _mm256_mul_epi32(this.0, rhs.0)
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
