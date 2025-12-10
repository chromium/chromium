// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, SimdDescriptor};

/// Computes `(p0 + p1 x + p2 x^2 + ...) / (q0 + q1 x + q2 x^2 + ...)`.
///
/// # Panics
/// Panics if either `P` or `Q` is zero.
#[inline]
pub fn eval_rational_poly<const P: usize, const Q: usize>(x: f32, p: [f32; P], q: [f32; Q]) -> f32 {
    let yp = p.into_iter().rev().reduce(|yp, p| yp * x + p).unwrap();
    let yq = q.into_iter().rev().reduce(|yq, q| yq * x + q).unwrap();
    yp / yq
}

#[inline(always)]
pub fn eval_rational_poly_simd<D: SimdDescriptor, const P: usize, const Q: usize>(
    d: D,
    x: D::F32Vec,
    p: [f32; P],
    q: [f32; Q],
) -> D::F32Vec {
    let mut yp = D::F32Vec::splat(d, p[P - 1]);
    for i in (0..P - 1).rev() {
        yp = yp.mul_add(x, D::F32Vec::splat(d, p[i]));
    }
    let mut yq = D::F32Vec::splat(d, q[Q - 1]);
    for i in (0..Q - 1).rev() {
        yq = yq.mul_add(x, D::F32Vec::splat(d, q[i]));
    }
    yp / yq
}
