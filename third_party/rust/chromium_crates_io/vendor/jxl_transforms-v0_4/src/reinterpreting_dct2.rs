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
pub(super) fn reinterpreting_dct_2<D: SimdDescriptor>(
    d: D,
    v0: D::F32Vec,
    v1: D::F32Vec,
) -> (D::F32Vec, D::F32Vec) {
    let v2 = v0 + v1;
    let v3 = v0 - v1;
    (
        v2 * D::F32Vec::splat(d, 0.500000),
        v3 * D::F32Vec::splat(d, 0.554469),
    )
}

#[inline(always)]
pub(super) fn do_reinterpreting_dct_2<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    assert!(data.len() > 1 * stride);
    let mut v0 = D::F32Vec::load_array(d, &data[0 * stride]);
    let mut v1 = D::F32Vec::load_array(d, &data[1 * stride]);
    (v0, v1) = reinterpreting_dct_2(d, v0, v1);
    v0.store_array(&mut data[0 * stride]);
    v1.store_array(&mut data[1 * stride]);
}
