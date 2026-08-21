// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unused)]
#![allow(clippy::type_complexity)]
#![allow(clippy::erasing_op)]
#![allow(clippy::identity_op)]
use crate::*;
use jxl_simd::{F32SimdVec, SimdDescriptor};

#[allow(clippy::too_many_arguments)]
#[allow(clippy::excessive_precision)]
#[inline(always)]
pub(super) fn idct_4<D: SimdDescriptor>(
    d: D,
    mut v0: D::F32Vec,
    mut v1: D::F32Vec,
    mut v2: D::F32Vec,
    mut v3: D::F32Vec,
) -> (D::F32Vec, D::F32Vec, D::F32Vec, D::F32Vec) {
    let mut v4 = v0 + v2;
    let mut v5 = v0 - v2;
    let mut v6 = v1 + v3;
    let mut v7 = v1 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v8 = v7 + v6;
    let mut v9 = v7 - v6;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v10 = v8.mul_add(mul, v4);
    let mut v11 = v8.neg_mul_add(mul, v4);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v12 = v9.mul_add(mul, v5);
    let mut v13 = v9.neg_mul_add(mul, v5);
    (v10, v12, v13, v11)
}

#[inline(always)]
pub(super) fn do_idct_4<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    assert!(data.len() > 3 * stride);
    let mut v0 = D::F32Vec::load_array(d, &data[0 * stride]);
    let mut v1 = D::F32Vec::load_array(d, &data[1 * stride]);
    let mut v2 = D::F32Vec::load_array(d, &data[2 * stride]);
    let mut v3 = D::F32Vec::load_array(d, &data[3 * stride]);
    (v0, v1, v2, v3) = idct_4(d, v0, v1, v2, v3);
    v0.store_array(&mut data[0 * stride]);
    v1.store_array(&mut data[1 * stride]);
    v2.store_array(&mut data[2 * stride]);
    v3.store_array(&mut data[3 * stride]);
}

#[inline(always)]
pub(super) fn do_idct_4_rowblock<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    assert!(data.len() >= 4);
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    let row_stride = 4 / D::F32Vec::LEN;
    let mut v0 = D::F32Vec::load_array(
        d,
        &data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)],
    );
    let mut v1 = D::F32Vec::load_array(
        d,
        &data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)],
    );
    let mut v2 = D::F32Vec::load_array(
        d,
        &data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)],
    );
    let mut v3 = D::F32Vec::load_array(
        d,
        &data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)],
    );
    (v0, v1, v2, v3) = idct_4(d, v0, v1, v2, v3);
    v0.store_array(&mut data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)]);
    v1.store_array(&mut data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)]);
    v2.store_array(&mut data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)]);
    v3.store_array(&mut data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)]);
}

#[inline(always)]
pub(super) fn do_idct_4_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 2 / D::F32Vec::LEN;
    assert!(data.len() > 3 * row_stride);
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    (v0, v1, v2, v3) = idct_4(d, v0, v1, v2, v3);
    v0.store_array(&mut data[row_stride * 0]);
    v1.store_array(&mut data[row_stride * 1]);
    v2.store_array(&mut data[row_stride * 2]);
    v3.store_array(&mut data[row_stride * 3]);
}
