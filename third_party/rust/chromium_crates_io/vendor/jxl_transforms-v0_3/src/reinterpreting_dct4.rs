// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::type_complexity)]
#![allow(clippy::erasing_op)]
#![allow(clippy::identity_op)]
use jxl_simd::{F32SimdVec, SimdDescriptor};

#[allow(clippy::too_many_arguments)]
#[allow(clippy::excessive_precision)]
#[inline(always)]
pub(super) fn reinterpreting_dct_4<D: SimdDescriptor>(
    d: D,
    v0: D::F32Vec,
    v1: D::F32Vec,
    v2: D::F32Vec,
    v3: D::F32Vec,
) -> (D::F32Vec, D::F32Vec, D::F32Vec, D::F32Vec) {
    let v4 = v0 + v3;
    let v5 = v1 + v2;
    let v6 = v4 + v5;
    let v7 = v4 - v5;
    let v8 = v0 - v3;
    let v9 = v1 - v2;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v10 = v8 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v11 = v9 * mul;
    let v12 = v10 + v11;
    let v13 = v10 - v11;
    let v14 = v12.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v13);
    (
        v6 * D::F32Vec::splat(d, 0.250000),
        v14 * D::F32Vec::splat(d, 0.256440),
        v7 * D::F32Vec::splat(d, 0.277234),
        v13 * D::F32Vec::splat(d, 0.317640),
    )
}

#[inline(always)]
pub(super) fn do_reinterpreting_dct_4<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    assert!(data.len() > 3 * stride);
    let mut v0 = D::F32Vec::load_array(d, &data[0 * stride]);
    let mut v1 = D::F32Vec::load_array(d, &data[1 * stride]);
    let mut v2 = D::F32Vec::load_array(d, &data[2 * stride]);
    let mut v3 = D::F32Vec::load_array(d, &data[3 * stride]);
    (v0, v1, v2, v3) = reinterpreting_dct_4(d, v0, v1, v2, v3);
    v0.store_array(&mut data[0 * stride]);
    v1.store_array(&mut data[1 * stride]);
    v2.store_array(&mut data[2 * stride]);
    v3.store_array(&mut data[3 * stride]);
}

#[inline(always)]
pub(super) fn do_reinterpreting_dct_4_rowblock<D: SimdDescriptor>(
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
    (v0, v1, v2, v3) = reinterpreting_dct_4(d, v0, v1, v2, v3);
    v0.store_array(&mut data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)]);
    v1.store_array(&mut data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)]);
    v2.store_array(&mut data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)]);
    v3.store_array(&mut data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)]);
}

#[inline(always)]
pub(super) fn do_reinterpreting_dct_4_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 2 / D::F32Vec::LEN;
    assert!(data.len() > 3 * row_stride);
    const { assert!(2usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    (v0, v1, v2, v3) = reinterpreting_dct_4(d, v0, v1, v2, v3);
    v0.store_array(&mut data[row_stride * 0]);
    v2.store_array(&mut data[row_stride * 1]);
    v1.store_array(&mut data[row_stride * 2]);
    v3.store_array(&mut data[row_stride * 3]);
}
