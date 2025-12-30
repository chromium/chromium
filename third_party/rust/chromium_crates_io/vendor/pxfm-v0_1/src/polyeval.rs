/*
 * // Copyright (c) Radzivon Bartoshyk 7/2025. All rights reserved.
 * //
 * // Redistribution and use in source and binary forms, with or without modification,
 * // are permitted provided that the following conditions are met:
 * //
 * // 1.  Redistributions of source code must retain the above copyright notice, this
 * // list of conditions and the following disclaimer.
 * //
 * // 2.  Redistributions in binary form must reproduce the above copyright notice,
 * // this list of conditions and the following disclaimer in the documentation
 * // and/or other materials provided with the distribution.
 * //
 * // 3.  Neither the name of the copyright holder nor the names of its
 * // contributors may be used to endorse or promote products derived from
 * // this software without specific prior written permission.
 * //
 * // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * // AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * // IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * // DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * // FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * // DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * // SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * // CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * // OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * // OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
use crate::common::{f_fmla, f_fmlaf};
use crate::double_double::DoubleDouble;
use crate::dyadic_float::DyadicFloat128;
use std::ops::Mul;

pub(crate) trait PolyevalMla {
    fn polyeval_mla(a: Self, b: Self, c: Self) -> Self;
}

impl PolyevalMla for f64 {
    #[inline(always)]
    fn polyeval_mla(a: Self, b: Self, c: Self) -> Self {
        f_fmla(a, b, c)
    }
}

impl PolyevalMla for f32 {
    #[inline(always)]
    fn polyeval_mla(a: Self, b: Self, c: Self) -> Self {
        f_fmlaf(a, b, c)
    }
}

impl PolyevalMla for DoubleDouble {
    #[inline(always)]
    fn polyeval_mla(a: Self, b: Self, c: Self) -> Self {
        DoubleDouble::mul_add(a, b, c)
    }
}

impl PolyevalMla for DyadicFloat128 {
    #[inline(always)]
    fn polyeval_mla(a: Self, b: Self, c: Self) -> Self {
        c.quick_add(&a.quick_mul(&b))
    }
}

// impl PolyevalMla for DyadicFloat256 {
//     #[inline(always)]
//     fn polyeval_mla(a: Self, b: Self, c: Self) -> Self {
//         c.quick_add(&a.quick_mul(&b))
//     }
// }

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval6<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
) -> T {
    let x2 = x * x;

    let u0 = T::polyeval_mla(x, a5, a4);
    let u1 = T::polyeval_mla(x, a3, a2);
    let u2 = T::polyeval_mla(x, a1, a0);

    let v0 = T::polyeval_mla(x2, u0, u1);

    T::polyeval_mla(x2, v0, u2)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn dd_quick_polyeval6(
    x: DoubleDouble,
    a0: DoubleDouble,
    a1: DoubleDouble,
    a2: DoubleDouble,
    a3: DoubleDouble,
    a4: DoubleDouble,
    a5: DoubleDouble,
) -> DoubleDouble {
    let x2 = DoubleDouble::quick_mult(x, x);

    let u0 = DoubleDouble::quick_mul_add(x, a5, a4);
    let u1 = DoubleDouble::quick_mul_add(x, a3, a2);
    let u2 = DoubleDouble::quick_mul_add(x, a1, a0);

    let v0 = DoubleDouble::quick_mul_add(x2, u0, u1);

    DoubleDouble::quick_mul_add(x2, v0, u2)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn dd_quick_polyeval6_fma(
    x: DoubleDouble,
    a0: DoubleDouble,
    a1: DoubleDouble,
    a2: DoubleDouble,
    a3: DoubleDouble,
    a4: DoubleDouble,
    a5: DoubleDouble,
) -> DoubleDouble {
    let x2 = DoubleDouble::quick_mult_fma(x, x);

    let u0 = DoubleDouble::quick_mul_add_fma(x, a5, a4);
    let u1 = DoubleDouble::quick_mul_add_fma(x, a3, a2);
    let u2 = DoubleDouble::quick_mul_add_fma(x, a1, a0);

    let v0 = DoubleDouble::quick_mul_add_fma(x2, u0, u1);

    DoubleDouble::quick_mul_add_fma(x2, v0, u2)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn d_polyeval6(x: f64, a0: f64, a1: f64, a2: f64, a3: f64, a4: f64, a5: f64) -> f64 {
    let x2 = x * x;

    let u0 = f64::mul_add(x, a5, a4);
    let u1 = f64::mul_add(x, a3, a2);
    let u2 = f64::mul_add(x, a1, a0);

    let v0 = f64::mul_add(x2, u0, u1);

    f64::mul_add(x2, v0, u2)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval9<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
) -> T {
    let mut acc = a8;
    acc = T::polyeval_mla(x, acc, a7);
    acc = T::polyeval_mla(x, acc, a6);
    acc = T::polyeval_mla(x, acc, a5);
    acc = T::polyeval_mla(x, acc, a4);
    acc = T::polyeval_mla(x, acc, a3);
    acc = T::polyeval_mla(x, acc, a2);
    acc = T::polyeval_mla(x, acc, a1);
    T::polyeval_mla(x, acc, a0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_estrin_polyeval9<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;
    let p0 = T::polyeval_mla(x, a1, a0);
    let p1 = T::polyeval_mla(x, a3, a2);
    let p2 = T::polyeval_mla(x, a5, a4);
    let p3 = T::polyeval_mla(x, a7, a6);

    let q0 = T::polyeval_mla(x2, p1, p0);
    let q1 = T::polyeval_mla(x2, p3, p2);
    let r0 = T::polyeval_mla(x4, q1, q0);
    T::polyeval_mla(x8, a8, r0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval10<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let p0 = T::polyeval_mla(x, a1, a0);
    let p1 = T::polyeval_mla(x, a3, a2);
    let p2 = T::polyeval_mla(x, a5, a4);
    let p3 = T::polyeval_mla(x, a7, a6);
    let p4 = T::polyeval_mla(x, a9, a8);

    let q0 = T::polyeval_mla(x2, p1, p0);
    let q1 = T::polyeval_mla(x2, p3, p2);

    let r0 = T::polyeval_mla(x4, q1, q0);
    T::polyeval_mla(x8, p4, r0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn dd_quick_polyeval10(
    x: DoubleDouble,
    a0: DoubleDouble,
    a1: DoubleDouble,
    a2: DoubleDouble,
    a3: DoubleDouble,
    a4: DoubleDouble,
    a5: DoubleDouble,
    a6: DoubleDouble,
    a7: DoubleDouble,
    a8: DoubleDouble,
    a9: DoubleDouble,
) -> DoubleDouble {
    let x2 = DoubleDouble::quick_mult(x, x);
    let x4 = DoubleDouble::quick_mult(x2, x2);
    let x8 = DoubleDouble::quick_mult(x4, x4);

    let p0 = DoubleDouble::quick_mul_add(x, a1, a0);
    let p1 = DoubleDouble::quick_mul_add(x, a3, a2);
    let p2 = DoubleDouble::quick_mul_add(x, a5, a4);
    let p3 = DoubleDouble::quick_mul_add(x, a7, a6);
    let p4 = DoubleDouble::quick_mul_add(x, a9, a8);

    let q0 = DoubleDouble::quick_mul_add(x2, p1, p0);
    let q1 = DoubleDouble::quick_mul_add(x2, p3, p2);

    let r0 = DoubleDouble::quick_mul_add(x4, q1, q0);
    DoubleDouble::quick_mul_add(x8, p4, r0)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn dd_quick_polyeval10_fma(
    x: DoubleDouble,
    a0: DoubleDouble,
    a1: DoubleDouble,
    a2: DoubleDouble,
    a3: DoubleDouble,
    a4: DoubleDouble,
    a5: DoubleDouble,
    a6: DoubleDouble,
    a7: DoubleDouble,
    a8: DoubleDouble,
    a9: DoubleDouble,
) -> DoubleDouble {
    let x2 = DoubleDouble::quick_mult_fma(x, x);
    let x4 = DoubleDouble::quick_mult_fma(x2, x2);
    let x8 = DoubleDouble::quick_mult_fma(x4, x4);

    let p0 = DoubleDouble::quick_mul_add_fma(x, a1, a0);
    let p1 = DoubleDouble::quick_mul_add_fma(x, a3, a2);
    let p2 = DoubleDouble::quick_mul_add_fma(x, a5, a4);
    let p3 = DoubleDouble::quick_mul_add_fma(x, a7, a6);
    let p4 = DoubleDouble::quick_mul_add_fma(x, a9, a8);

    let q0 = DoubleDouble::quick_mul_add_fma(x2, p1, p0);
    let q1 = DoubleDouble::quick_mul_add_fma(x2, p3, p2);

    let r0 = DoubleDouble::quick_mul_add_fma(x4, q1, q0);
    DoubleDouble::quick_mul_add_fma(x8, p4, r0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval11<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let q0 = T::polyeval_mla(x, a1, a0);
    let q1 = T::polyeval_mla(x, a3, a2);
    let q2 = T::polyeval_mla(x, a5, a4);
    let q3 = T::polyeval_mla(x, a7, a6);
    let q4 = T::polyeval_mla(x, a9, a8);

    let r0 = T::polyeval_mla(x2, q1, q0);
    let r1 = T::polyeval_mla(x2, q3, q2);

    let s0 = T::polyeval_mla(x4, r1, r0);
    let s1 = T::polyeval_mla(x2, a10, q4);
    T::polyeval_mla(x8, s1, s0)
}

#[inline(always)]
pub(crate) fn f_polyeval3<T: PolyevalMla + Copy>(x: T, a0: T, a1: T, a2: T) -> T {
    T::polyeval_mla(x, T::polyeval_mla(x, a2, a1), a0)
}

#[inline(always)]
#[allow(unused)]
pub(crate) fn d_polyeval3(x: f64, a0: f64, a1: f64, a2: f64) -> f64 {
    f64::mul_add(x, f64::mul_add(x, a2, a1), a0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval4<T: PolyevalMla + Copy>(x: T, a0: T, a1: T, a2: T, a3: T) -> T {
    let t2 = T::polyeval_mla(x, a3, a2);
    let t5 = T::polyeval_mla(x, t2, a1);
    T::polyeval_mla(x, t5, a0)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn d_polyeval4(x: f64, a0: f64, a1: f64, a2: f64, a3: f64) -> f64 {
    let t2 = f64::mul_add(x, a3, a2);
    let t5 = f64::mul_add(x, t2, a1);
    f64::mul_add(x, t5, a0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_estrin_polyeval4<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
) -> T {
    let x2 = x * x;

    let p01 = T::polyeval_mla(x, a1, a0);
    let p23 = T::polyeval_mla(x, a3, a2);

    T::polyeval_mla(x2, p23, p01)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval13<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let t0 = T::polyeval_mla(x, a3, a2);
    let t1 = T::polyeval_mla(x, a1, a0);
    let t2 = T::polyeval_mla(x, a7, a6);
    let t3 = T::polyeval_mla(x, a5, a4);
    let t4 = T::polyeval_mla(x, a11, a10);
    let t5 = T::polyeval_mla(x, a9, a8);

    let q0 = T::polyeval_mla(x2, t0, t1);
    let q1 = T::polyeval_mla(x2, t2, t3);

    let q2 = T::polyeval_mla(x2, t4, t5);

    let q3 = a12;

    let r0 = T::polyeval_mla(x4, q1, q0);
    let r1 = T::polyeval_mla(x4, q3, q2);

    T::polyeval_mla(x8, r1, r0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval12<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let e0 = T::polyeval_mla(x, a1, a0);
    let e1 = T::polyeval_mla(x, a3, a2);
    let e2 = T::polyeval_mla(x, a5, a4);
    let e3 = T::polyeval_mla(x, a7, a6);
    let e4 = T::polyeval_mla(x, a9, a8);
    let e5 = T::polyeval_mla(x, a11, a10);

    let f0 = T::polyeval_mla(x2, e1, e0);
    let f1 = T::polyeval_mla(x2, e3, e2);
    let f2 = T::polyeval_mla(x2, e5, e4);

    let g0 = T::polyeval_mla(x4, f1, f0);

    T::polyeval_mla(x8, f2, g0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval14<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let g0 = T::polyeval_mla(x, a1, a0);
    let g1 = T::polyeval_mla(x, a3, a2);
    let g2 = T::polyeval_mla(x, a5, a4);
    let g3 = T::polyeval_mla(x, a7, a6);
    let g4 = T::polyeval_mla(x, a9, a8);
    let g5 = T::polyeval_mla(x, a11, a10);
    let g6 = T::polyeval_mla(x, a13, a12);

    let h0 = T::polyeval_mla(x2, g1, g0);
    let h1 = T::polyeval_mla(x2, g3, g2);
    let h2 = T::polyeval_mla(x2, g5, g4);

    let q0 = T::polyeval_mla(x4, h1, h0);
    let q1 = T::polyeval_mla(x4, g6, h2);

    T::polyeval_mla(x8, q1, q0)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn d_polyeval14(
    x: f64,
    a0: f64,
    a1: f64,
    a2: f64,
    a3: f64,
    a4: f64,
    a5: f64,
    a6: f64,
    a7: f64,
    a8: f64,
    a9: f64,
    a10: f64,
    a11: f64,
    a12: f64,
    a13: f64,
) -> f64 {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let g0 = f64::mul_add(x, a1, a0);
    let g1 = f64::mul_add(x, a3, a2);
    let g2 = f64::mul_add(x, a5, a4);
    let g3 = f64::mul_add(x, a7, a6);
    let g4 = f64::mul_add(x, a9, a8);
    let g5 = f64::mul_add(x, a11, a10);
    let g6 = f64::mul_add(x, a13, a12);

    let h0 = f64::mul_add(x2, g1, g0);
    let h1 = f64::mul_add(x2, g3, g2);
    let h2 = f64::mul_add(x2, g5, g4);

    let q0 = f64::mul_add(x4, h1, h0);
    let q1 = f64::mul_add(x4, g6, h2);

    f64::mul_add(x8, q1, q0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval7<T: PolyevalMla + Copy>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
) -> T {
    let t1 = T::polyeval_mla(x, a6, a5);
    let t2 = T::polyeval_mla(x, t1, a4);
    let t3 = T::polyeval_mla(x, t2, a3);
    let t4 = T::polyeval_mla(x, t3, a2);
    let t5 = T::polyeval_mla(x, t4, a1);
    T::polyeval_mla(x, t5, a0)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn d_polyeval7(
    x: f64,
    a0: f64,
    a1: f64,
    a2: f64,
    a3: f64,
    a4: f64,
    a5: f64,
    a6: f64,
) -> f64 {
    let t1 = f64::mul_add(x, a6, a5);
    let t2 = f64::mul_add(x, t1, a4);
    let t3 = f64::mul_add(x, t2, a3);
    let t4 = f64::mul_add(x, t3, a2);
    let t5 = f64::mul_add(x, t4, a1);
    f64::mul_add(x, t5, a0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_estrin_polyeval7<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;

    let b0 = T::polyeval_mla(x, a1, a0);
    let b1 = T::polyeval_mla(x, a3, a2);
    let b2 = T::polyeval_mla(x, a5, a4);

    let c0 = T::polyeval_mla(x2, b1, b0);
    let c1 = T::polyeval_mla(x2, a6, b2);

    T::polyeval_mla(x4, c1, c0)
}

#[inline(always)]
#[allow(unused)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn d_estrin_polyeval7(
    x: f64,
    a0: f64,
    a1: f64,
    a2: f64,
    a3: f64,
    a4: f64,
    a5: f64,
    a6: f64,
) -> f64 {
    let x2 = x * x;
    let x4 = x2 * x2;

    let b0 = f64::mul_add(x, a1, a0);
    let b1 = f64::mul_add(x, a3, a2);
    let b2 = f64::mul_add(x, a5, a4);

    let c0 = f64::mul_add(x2, b1, b0);
    let c1 = f64::mul_add(x2, a6, b2);

    f64::mul_add(x4, c1, c0)
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
pub(crate) fn f_polyeval5<T: PolyevalMla + Copy>(x: T, a0: T, a1: T, a2: T, a3: T, a4: T) -> T {
    let mut acc = a4;
    acc = T::polyeval_mla(x, acc, a3);
    acc = T::polyeval_mla(x, acc, a2);
    acc = T::polyeval_mla(x, acc, a1);
    T::polyeval_mla(x, acc, a0)
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
#[allow(unused)]
pub(crate) fn d_polyeval5(x: f64, a0: f64, a1: f64, a2: f64, a3: f64, a4: f64) -> f64 {
    let mut acc = a4;
    acc = f64::mul_add(x, acc, a3);
    acc = f64::mul_add(x, acc, a2);
    acc = f64::mul_add(x, acc, a1);
    f64::mul_add(x, acc, a0)
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
pub(crate) fn f_estrin_polyeval5<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
) -> T {
    let x2 = x * x;
    let p01 = T::polyeval_mla(x, a1, a0);
    let p23 = T::polyeval_mla(x, a3, a2);
    let t = T::polyeval_mla(x2, a4, p23);
    T::polyeval_mla(x2, t, p01)
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
#[allow(unused)]
pub(crate) fn d_estrin_polyeval5(x: f64, a0: f64, a1: f64, a2: f64, a3: f64, a4: f64) -> f64 {
    let x2 = x * x;
    let p01 = f64::mul_add(x, a1, a0);
    let p23 = f64::mul_add(x, a3, a2);
    let t = f64::mul_add(x2, a4, p23);
    f64::mul_add(x2, t, p01)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval8<T: PolyevalMla + Copy>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
) -> T {
    let z0 = T::polyeval_mla(x, a7, a6);
    let t1 = T::polyeval_mla(x, z0, a5);
    let t2 = T::polyeval_mla(x, t1, a4);
    let t3 = T::polyeval_mla(x, t2, a3);
    let t4 = T::polyeval_mla(x, t3, a2);
    let t5 = T::polyeval_mla(x, t4, a1);
    T::polyeval_mla(x, t5, a0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_estrin_polyeval8<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;

    let p0 = T::polyeval_mla(x, a1, a0);
    let p1 = T::polyeval_mla(x, a3, a2);
    let p2 = T::polyeval_mla(x, a5, a4);
    let p3 = T::polyeval_mla(x, a7, a6);

    let q0 = T::polyeval_mla(x2, p1, p0);
    let q1 = T::polyeval_mla(x2, p3, p2);

    T::polyeval_mla(x4, q1, q0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval16<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
    a14: T,
    a15: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let q0 = T::polyeval_mla(x, a1, a0);
    let q1 = T::polyeval_mla(x, a3, a2);
    let q2 = T::polyeval_mla(x, a5, a4);
    let q3 = T::polyeval_mla(x, a7, a6);
    let q4 = T::polyeval_mla(x, a9, a8);
    let q5 = T::polyeval_mla(x, a11, a10);
    let q6 = T::polyeval_mla(x, a13, a12);
    let q7 = T::polyeval_mla(x, a15, a14);

    let r0 = T::polyeval_mla(x2, q1, q0);
    let r1 = T::polyeval_mla(x2, q3, q2);
    let r2 = T::polyeval_mla(x2, q5, q4);
    let r3 = T::polyeval_mla(x2, q7, q6);

    let s0 = T::polyeval_mla(x4, r1, r0);
    let s1 = T::polyeval_mla(x4, r3, r2);

    T::polyeval_mla(x8, s1, s0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval15<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
    a14: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;

    let e0 = T::polyeval_mla(x, a1, a0);
    let e1 = T::polyeval_mla(x, a3, a2);
    let e2 = T::polyeval_mla(x, a5, a4);
    let e3 = T::polyeval_mla(x, a7, a6);
    let e4 = T::polyeval_mla(x, a9, a8);
    let e5 = T::polyeval_mla(x, a11, a10);
    let e6 = T::polyeval_mla(x, a13, a12);

    // Level 2
    let f0 = T::polyeval_mla(x2, e1, e0);
    let f1 = T::polyeval_mla(x2, e3, e2);
    let f2 = T::polyeval_mla(x2, e5, e4);
    let f3 = T::polyeval_mla(x2, a14, e6);

    // Level 3
    let g0 = T::polyeval_mla(x4, f1, f0);
    let g1 = T::polyeval_mla(x4, f3, f2);

    // Final
    T::polyeval_mla(x8, g1, g0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval18<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
    a14: T,
    a15: T,
    a16: T,
    a17: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;
    let x16 = x8 * x8;

    let q0 = T::polyeval_mla(x, a1, a0);
    let q1 = T::polyeval_mla(x, a3, a2);
    let q2 = T::polyeval_mla(x, a5, a4);
    let q3 = T::polyeval_mla(x, a7, a6);
    let q4 = T::polyeval_mla(x, a9, a8);
    let q5 = T::polyeval_mla(x, a11, a10);
    let q6 = T::polyeval_mla(x, a13, a12);
    let q7 = T::polyeval_mla(x, a15, a14);
    let q8 = T::polyeval_mla(x, a17, a16);

    let r0 = T::polyeval_mla(x2, q1, q0);
    let r1 = T::polyeval_mla(x2, q3, q2);
    let r2 = T::polyeval_mla(x2, q5, q4);
    let r3 = T::polyeval_mla(x2, q7, q6);

    let s0 = T::polyeval_mla(x4, r1, r0);
    let s1 = T::polyeval_mla(x4, r3, r2);

    let t0 = T::polyeval_mla(x8, s1, s0);

    T::polyeval_mla(x16, q8, t0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval19<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
    a14: T,
    a15: T,
    a16: T,
    a17: T,
    a18: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;
    let x16 = x8 * x8;

    // Level 0: pairs
    let e0 = T::polyeval_mla(x, a1, a0); // a0 + a1·x
    let e1 = T::polyeval_mla(x, a3, a2); // a2 + a3·x
    let e2 = T::polyeval_mla(x, a5, a4);
    let e3 = T::polyeval_mla(x, a7, a6);
    let e4 = T::polyeval_mla(x, a9, a8);
    let e5 = T::polyeval_mla(x, a11, a10);
    let e6 = T::polyeval_mla(x, a13, a12);
    let e7 = T::polyeval_mla(x, a15, a14);
    let e8 = T::polyeval_mla(x, a17, a16);

    // Level 1: combine with x²
    let f0 = T::polyeval_mla(x2, e1, e0);
    let f1 = T::polyeval_mla(x2, e3, e2);
    let f2 = T::polyeval_mla(x2, e5, e4);
    let f3 = T::polyeval_mla(x2, e7, e6);

    // Level 2: combine with x⁴
    let g0 = T::polyeval_mla(x4, f1, f0);
    let g1 = T::polyeval_mla(x4, f3, f2);

    // Level 3: combine with x⁸
    let h0 = T::polyeval_mla(x8, g1, g0);

    // Final: combine with x¹⁶
    let final_poly = T::polyeval_mla(x16, e8, h0);

    // Degree 18: Add a18·x¹⁸
    // This assumes `x18 = x16 * x2`, since x² already computed
    let x18 = x16 * x2;
    T::polyeval_mla(x18, a18, final_poly)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval22<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
    a14: T,
    a15: T,
    a16: T,
    a17: T,
    a18: T,
    a19: T,
    a20: T,
    a21: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;
    let x16 = x8 * x8;

    let p0 = T::polyeval_mla(x, a1, a0); // a1·x + a0
    let p1 = T::polyeval_mla(x, a3, a2); // a3·x + a2
    let p2 = T::polyeval_mla(x, a5, a4);
    let p3 = T::polyeval_mla(x, a7, a6);
    let p4 = T::polyeval_mla(x, a9, a8);
    let p5 = T::polyeval_mla(x, a11, a10);
    let p6 = T::polyeval_mla(x, a13, a12);
    let p7 = T::polyeval_mla(x, a15, a14);
    let p8 = T::polyeval_mla(x, a17, a16);
    let p9 = T::polyeval_mla(x, a19, a18);
    let p10 = T::polyeval_mla(x, a21, a20);

    let q0 = T::polyeval_mla(x2, p1, p0); // (a3·x + a2)·x² + (a1·x + a0)
    let q1 = T::polyeval_mla(x2, p3, p2);
    let q2 = T::polyeval_mla(x2, p5, p4);
    let q3 = T::polyeval_mla(x2, p7, p6);
    let q4 = T::polyeval_mla(x2, p9, p8);
    let r0 = T::polyeval_mla(x4, q1, q0); // q1·x⁴ + q0
    let r1 = T::polyeval_mla(x4, q3, q2);
    let s0 = T::polyeval_mla(x8, r1, r0); // r1·x⁸ + r0
    let r2 = T::polyeval_mla(x4, p10, q4); // p10·x⁴ + q4
    T::polyeval_mla(x16, r2, s0)
}

#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) fn f_polyeval24<T: PolyevalMla + Copy + Mul<T, Output = T>>(
    x: T,
    a0: T,
    a1: T,
    a2: T,
    a3: T,
    a4: T,
    a5: T,
    a6: T,
    a7: T,
    a8: T,
    a9: T,
    a10: T,
    a11: T,
    a12: T,
    a13: T,
    a14: T,
    a15: T,
    a16: T,
    a17: T,
    a18: T,
    a19: T,
    a20: T,
    a21: T,
    a22: T,
    a23: T,
) -> T {
    let x2 = x * x;
    let x4 = x2 * x2;
    let x8 = x4 * x4;
    let x16 = x8 * x8;

    // Group degree 0–1
    let e0 = T::polyeval_mla(x, a1, a0);
    // Group degree 2–3
    let e1 = T::polyeval_mla(x, a3, a2);
    // Group degree 4–5
    let e2 = T::polyeval_mla(x, a5, a4);
    // Group degree 6–7
    let e3 = T::polyeval_mla(x, a7, a6);
    // Group degree 8–9
    let e4 = T::polyeval_mla(x, a9, a8);
    // Group degree 10–11
    let e5 = T::polyeval_mla(x, a11, a10);
    // Group degree 12–13
    let e6 = T::polyeval_mla(x, a13, a12);
    // Group degree 14–15
    let e7 = T::polyeval_mla(x, a15, a14);
    // Group degree 16–17
    let e8 = T::polyeval_mla(x, a17, a16);
    // Group degree 18–19
    let e9 = T::polyeval_mla(x, a19, a18);
    // Group degree 20–21
    let e10 = T::polyeval_mla(x, a21, a20);
    // Group degree 22–23
    let e11 = T::polyeval_mla(x, a23, a22);

    // Now group into x2 terms
    let f0 = T::polyeval_mla(x2, e1, e0);
    let f1 = T::polyeval_mla(x2, e3, e2);
    let f2 = T::polyeval_mla(x2, e5, e4);
    let f3 = T::polyeval_mla(x2, e7, e6);
    let f4 = T::polyeval_mla(x2, e9, e8);
    let f5 = T::polyeval_mla(x2, e11, e10);

    // Now group into x4 terms
    let g0 = T::polyeval_mla(x4, f1, f0);
    let g1 = T::polyeval_mla(x4, f3, f2);
    let g2 = T::polyeval_mla(x4, f5, f4);

    // Now group into x8 terms
    let h0 = T::polyeval_mla(x8, g1, g0);
    let h1 = g2;

    // Final step (x16 term)
    T::polyeval_mla(x16, h1, h0)
}
