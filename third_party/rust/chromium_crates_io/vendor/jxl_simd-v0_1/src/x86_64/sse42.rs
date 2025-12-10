// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{U32SimdVec, impl_f32_array_interface};

use super::super::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};
use std::{
    arch::x86_64::*,
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

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct F32VecSse42(__m128, Sse42Descriptor);

#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct MaskSse42(__m128, Sse42Descriptor);

impl F32SimdVec for F32VecSse42 {
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

    fn_sse42!(this: F32VecSse42, fn gt(other: F32VecSse42) -> MaskSse42 {
        MaskSse42(_mm_cmpgt_ps(this.0, other.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn as_i32() -> I32VecSse42 {
        I32VecSse42(_mm_cvtps_epi32(this.0), this.1)
    });

    fn_sse42!(this: F32VecSse42, fn bitcast_to_i32() -> I32VecSse42 {
        I32VecSse42(_mm_castps_si128(this.0), this.1)
    });

    impl_f32_array_interface!();

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
        I32VecSse42(_mm_mul_epi32(this.0, rhs.0), this.1)
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
        this.0 = _mm_mul_epi32(this.0, rhs.0)
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
