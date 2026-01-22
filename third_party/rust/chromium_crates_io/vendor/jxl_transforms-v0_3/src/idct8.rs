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
pub(super) fn idct_8<D: SimdDescriptor>(
    d: D,
    mut v0: D::F32Vec,
    mut v1: D::F32Vec,
    mut v2: D::F32Vec,
    mut v3: D::F32Vec,
    mut v4: D::F32Vec,
    mut v5: D::F32Vec,
    mut v6: D::F32Vec,
    mut v7: D::F32Vec,
) -> (
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
) {
    let mut v8 = v0 + v4;
    let mut v9 = v0 - v4;
    let mut v10 = v2 + v6;
    let mut v11 = v2 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v12 = v11 + v10;
    let mut v13 = v11 - v10;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v14 = v12.mul_add(mul, v8);
    let mut v15 = v12.neg_mul_add(mul, v8);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v16 = v13.mul_add(mul, v9);
    let mut v17 = v13.neg_mul_add(mul, v9);
    let mut v18 = v1 + v3;
    let mut v19 = v3 + v5;
    let mut v20 = v5 + v7;
    let mut v21 = v1 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v22 = v21 + v19;
    let mut v23 = v21 - v19;
    let mut v24 = v18 + v20;
    let mut v25 = v18 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v26 = v25 + v24;
    let mut v27 = v25 - v24;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v28 = v26.mul_add(mul, v22);
    let mut v29 = v26.neg_mul_add(mul, v22);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v30 = v27.mul_add(mul, v23);
    let mut v31 = v27.neg_mul_add(mul, v23);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v32 = v28.mul_add(mul, v14);
    let mut v33 = v28.neg_mul_add(mul, v14);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v34 = v30.mul_add(mul, v16);
    let mut v35 = v30.neg_mul_add(mul, v16);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v36 = v31.mul_add(mul, v17);
    let mut v37 = v31.neg_mul_add(mul, v17);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v38 = v29.mul_add(mul, v15);
    let mut v39 = v29.neg_mul_add(mul, v15);
    (v32, v34, v36, v38, v39, v37, v35, v33)
}

#[inline(always)]
pub(super) fn do_idct_8<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    assert!(data.len() > 7 * stride);
    let mut v0 = D::F32Vec::load_array(d, &data[0 * stride]);
    let mut v1 = D::F32Vec::load_array(d, &data[1 * stride]);
    let mut v2 = D::F32Vec::load_array(d, &data[2 * stride]);
    let mut v3 = D::F32Vec::load_array(d, &data[3 * stride]);
    let mut v4 = D::F32Vec::load_array(d, &data[4 * stride]);
    let mut v5 = D::F32Vec::load_array(d, &data[5 * stride]);
    let mut v6 = D::F32Vec::load_array(d, &data[6 * stride]);
    let mut v7 = D::F32Vec::load_array(d, &data[7 * stride]);
    (v0, v1, v2, v3, v4, v5, v6, v7) = idct_8(d, v0, v1, v2, v3, v4, v5, v6, v7);
    v0.store_array(&mut data[0 * stride]);
    v1.store_array(&mut data[1 * stride]);
    v2.store_array(&mut data[2 * stride]);
    v3.store_array(&mut data[3 * stride]);
    v4.store_array(&mut data[4 * stride]);
    v5.store_array(&mut data[5 * stride]);
    v6.store_array(&mut data[6 * stride]);
    v7.store_array(&mut data[7 * stride]);
}

#[inline(always)]
pub(super) fn do_idct_8_rowblock<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    assert!(data.len() >= 8);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    let row_stride = 8 / D::F32Vec::LEN;
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
    let mut v4 = D::F32Vec::load_array(
        d,
        &data[row_stride * (4 % D::F32Vec::LEN) + (4 / D::F32Vec::LEN)],
    );
    let mut v5 = D::F32Vec::load_array(
        d,
        &data[row_stride * (5 % D::F32Vec::LEN) + (5 / D::F32Vec::LEN)],
    );
    let mut v6 = D::F32Vec::load_array(
        d,
        &data[row_stride * (6 % D::F32Vec::LEN) + (6 / D::F32Vec::LEN)],
    );
    let mut v7 = D::F32Vec::load_array(
        d,
        &data[row_stride * (7 % D::F32Vec::LEN) + (7 / D::F32Vec::LEN)],
    );
    (v0, v1, v2, v3, v4, v5, v6, v7) = idct_8(d, v0, v1, v2, v3, v4, v5, v6, v7);
    v0.store_array(&mut data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)]);
    v1.store_array(&mut data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)]);
    v2.store_array(&mut data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)]);
    v3.store_array(&mut data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)]);
    v4.store_array(&mut data[row_stride * (4 % D::F32Vec::LEN) + (4 / D::F32Vec::LEN)]);
    v5.store_array(&mut data[row_stride * (5 % D::F32Vec::LEN) + (5 / D::F32Vec::LEN)]);
    v6.store_array(&mut data[row_stride * (6 % D::F32Vec::LEN) + (6 / D::F32Vec::LEN)]);
    v7.store_array(&mut data[row_stride * (7 % D::F32Vec::LEN) + (7 / D::F32Vec::LEN)]);
}

#[inline(always)]
pub(super) fn do_idct_8_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 4 / D::F32Vec::LEN;
    assert!(data.len() > 7 * row_stride);
    const { assert!(4usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 4]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 6]);
    let mut v4 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v5 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    let mut v6 = D::F32Vec::load_array(d, &data[row_stride * 5]);
    let mut v7 = D::F32Vec::load_array(d, &data[row_stride * 7]);
    (v0, v1, v2, v3, v4, v5, v6, v7) = idct_8(d, v0, v1, v2, v3, v4, v5, v6, v7);
    v0.store_array(&mut data[row_stride * 0]);
    v1.store_array(&mut data[row_stride * 1]);
    v2.store_array(&mut data[row_stride * 2]);
    v3.store_array(&mut data[row_stride * 3]);
    v4.store_array(&mut data[row_stride * 4]);
    v5.store_array(&mut data[row_stride * 5]);
    v6.store_array(&mut data[row_stride * 6]);
    v7.store_array(&mut data[row_stride * 7]);
}
