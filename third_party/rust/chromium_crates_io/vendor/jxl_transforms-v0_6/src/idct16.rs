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
pub(super) fn idct_16<D: SimdDescriptor>(
    d: D,
    mut v0: D::F32Vec,
    mut v1: D::F32Vec,
    mut v2: D::F32Vec,
    mut v3: D::F32Vec,
    mut v4: D::F32Vec,
    mut v5: D::F32Vec,
    mut v6: D::F32Vec,
    mut v7: D::F32Vec,
    mut v8: D::F32Vec,
    mut v9: D::F32Vec,
    mut v10: D::F32Vec,
    mut v11: D::F32Vec,
    mut v12: D::F32Vec,
    mut v13: D::F32Vec,
    mut v14: D::F32Vec,
    mut v15: D::F32Vec,
) -> (
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
) {
    let mut v16 = v0 + v8;
    let mut v17 = v0 - v8;
    let mut v18 = v4 + v12;
    let mut v19 = v4 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v20 = v19 + v18;
    let mut v21 = v19 - v18;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v22 = v20.mul_add(mul, v16);
    let mut v23 = v20.neg_mul_add(mul, v16);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v24 = v21.mul_add(mul, v17);
    let mut v25 = v21.neg_mul_add(mul, v17);
    let mut v26 = v2 + v6;
    let mut v27 = v6 + v10;
    let mut v28 = v10 + v14;
    let mut v29 = v2 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v30 = v29 + v27;
    let mut v31 = v29 - v27;
    let mut v32 = v26 + v28;
    let mut v33 = v26 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v34 = v33 + v32;
    let mut v35 = v33 - v32;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v36 = v34.mul_add(mul, v30);
    let mut v37 = v34.neg_mul_add(mul, v30);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v38 = v35.mul_add(mul, v31);
    let mut v39 = v35.neg_mul_add(mul, v31);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v40 = v36.mul_add(mul, v22);
    let mut v41 = v36.neg_mul_add(mul, v22);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v42 = v38.mul_add(mul, v24);
    let mut v43 = v38.neg_mul_add(mul, v24);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v44 = v39.mul_add(mul, v25);
    let mut v45 = v39.neg_mul_add(mul, v25);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v46 = v37.mul_add(mul, v23);
    let mut v47 = v37.neg_mul_add(mul, v23);
    let mut v48 = v1 + v3;
    let mut v49 = v3 + v5;
    let mut v50 = v5 + v7;
    let mut v51 = v7 + v9;
    let mut v52 = v9 + v11;
    let mut v53 = v11 + v13;
    let mut v54 = v13 + v15;
    let mut v55 = v1 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v56 = v55 + v51;
    let mut v57 = v55 - v51;
    let mut v58 = v49 + v53;
    let mut v59 = v49 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v60 = v59 + v58;
    let mut v61 = v59 - v58;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v62 = v60.mul_add(mul, v56);
    let mut v63 = v60.neg_mul_add(mul, v56);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v64 = v61.mul_add(mul, v57);
    let mut v65 = v61.neg_mul_add(mul, v57);
    let mut v66 = v48 + v50;
    let mut v67 = v50 + v52;
    let mut v68 = v52 + v54;
    let mut v69 = v48 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v70 = v69 + v67;
    let mut v71 = v69 - v67;
    let mut v72 = v66 + v68;
    let mut v73 = v66 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v74 = v73 + v72;
    let mut v75 = v73 - v72;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v76 = v74.mul_add(mul, v70);
    let mut v77 = v74.neg_mul_add(mul, v70);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v78 = v75.mul_add(mul, v71);
    let mut v79 = v75.neg_mul_add(mul, v71);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v80 = v76.mul_add(mul, v62);
    let mut v81 = v76.neg_mul_add(mul, v62);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v82 = v78.mul_add(mul, v64);
    let mut v83 = v78.neg_mul_add(mul, v64);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v84 = v79.mul_add(mul, v65);
    let mut v85 = v79.neg_mul_add(mul, v65);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v86 = v77.mul_add(mul, v63);
    let mut v87 = v77.neg_mul_add(mul, v63);
    let mul = D::F32Vec::splat(d, 0.5024192861881557);
    let mut v88 = v80.mul_add(mul, v40);
    let mut v89 = v80.neg_mul_add(mul, v40);
    let mul = D::F32Vec::splat(d, 0.5224986149396889);
    let mut v90 = v82.mul_add(mul, v42);
    let mut v91 = v82.neg_mul_add(mul, v42);
    let mul = D::F32Vec::splat(d, 0.5669440348163577);
    let mut v92 = v84.mul_add(mul, v44);
    let mut v93 = v84.neg_mul_add(mul, v44);
    let mul = D::F32Vec::splat(d, 0.6468217833599901);
    let mut v94 = v86.mul_add(mul, v46);
    let mut v95 = v86.neg_mul_add(mul, v46);
    let mul = D::F32Vec::splat(d, 0.7881546234512502);
    let mut v96 = v87.mul_add(mul, v47);
    let mut v97 = v87.neg_mul_add(mul, v47);
    let mul = D::F32Vec::splat(d, 1.0606776859903471);
    let mut v98 = v85.mul_add(mul, v45);
    let mut v99 = v85.neg_mul_add(mul, v45);
    let mul = D::F32Vec::splat(d, 1.7224470982383342);
    let mut v100 = v83.mul_add(mul, v43);
    let mut v101 = v83.neg_mul_add(mul, v43);
    let mul = D::F32Vec::splat(d, 5.1011486186891553);
    let mut v102 = v81.mul_add(mul, v41);
    let mut v103 = v81.neg_mul_add(mul, v41);
    (
        v88, v90, v92, v94, v96, v98, v100, v102, v103, v101, v99, v97, v95, v93, v91, v89,
    )
}

#[inline(always)]
pub(super) fn do_idct_16<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    assert!(data.len() > 15 * stride);
    let mut v0 = D::F32Vec::load_array(d, &data[0 * stride]);
    let mut v1 = D::F32Vec::load_array(d, &data[1 * stride]);
    let mut v2 = D::F32Vec::load_array(d, &data[2 * stride]);
    let mut v3 = D::F32Vec::load_array(d, &data[3 * stride]);
    let mut v4 = D::F32Vec::load_array(d, &data[4 * stride]);
    let mut v5 = D::F32Vec::load_array(d, &data[5 * stride]);
    let mut v6 = D::F32Vec::load_array(d, &data[6 * stride]);
    let mut v7 = D::F32Vec::load_array(d, &data[7 * stride]);
    let mut v8 = D::F32Vec::load_array(d, &data[8 * stride]);
    let mut v9 = D::F32Vec::load_array(d, &data[9 * stride]);
    let mut v10 = D::F32Vec::load_array(d, &data[10 * stride]);
    let mut v11 = D::F32Vec::load_array(d, &data[11 * stride]);
    let mut v12 = D::F32Vec::load_array(d, &data[12 * stride]);
    let mut v13 = D::F32Vec::load_array(d, &data[13 * stride]);
    let mut v14 = D::F32Vec::load_array(d, &data[14 * stride]);
    let mut v15 = D::F32Vec::load_array(d, &data[15 * stride]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    ) = idct_16(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    );
    v0.store_array(&mut data[0 * stride]);
    v1.store_array(&mut data[1 * stride]);
    v2.store_array(&mut data[2 * stride]);
    v3.store_array(&mut data[3 * stride]);
    v4.store_array(&mut data[4 * stride]);
    v5.store_array(&mut data[5 * stride]);
    v6.store_array(&mut data[6 * stride]);
    v7.store_array(&mut data[7 * stride]);
    v8.store_array(&mut data[8 * stride]);
    v9.store_array(&mut data[9 * stride]);
    v10.store_array(&mut data[10 * stride]);
    v11.store_array(&mut data[11 * stride]);
    v12.store_array(&mut data[12 * stride]);
    v13.store_array(&mut data[13 * stride]);
    v14.store_array(&mut data[14 * stride]);
    v15.store_array(&mut data[15 * stride]);
}

#[inline(always)]
pub(super) fn do_idct_16_rowblock<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    assert!(data.len() >= 16);
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    let row_stride = 16 / D::F32Vec::LEN;
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
    let mut v8 = D::F32Vec::load_array(
        d,
        &data[row_stride * (8 % D::F32Vec::LEN) + (8 / D::F32Vec::LEN)],
    );
    let mut v9 = D::F32Vec::load_array(
        d,
        &data[row_stride * (9 % D::F32Vec::LEN) + (9 / D::F32Vec::LEN)],
    );
    let mut v10 = D::F32Vec::load_array(
        d,
        &data[row_stride * (10 % D::F32Vec::LEN) + (10 / D::F32Vec::LEN)],
    );
    let mut v11 = D::F32Vec::load_array(
        d,
        &data[row_stride * (11 % D::F32Vec::LEN) + (11 / D::F32Vec::LEN)],
    );
    let mut v12 = D::F32Vec::load_array(
        d,
        &data[row_stride * (12 % D::F32Vec::LEN) + (12 / D::F32Vec::LEN)],
    );
    let mut v13 = D::F32Vec::load_array(
        d,
        &data[row_stride * (13 % D::F32Vec::LEN) + (13 / D::F32Vec::LEN)],
    );
    let mut v14 = D::F32Vec::load_array(
        d,
        &data[row_stride * (14 % D::F32Vec::LEN) + (14 / D::F32Vec::LEN)],
    );
    let mut v15 = D::F32Vec::load_array(
        d,
        &data[row_stride * (15 % D::F32Vec::LEN) + (15 / D::F32Vec::LEN)],
    );
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    ) = idct_16(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    );
    v0.store_array(&mut data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)]);
    v1.store_array(&mut data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)]);
    v2.store_array(&mut data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)]);
    v3.store_array(&mut data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)]);
    v4.store_array(&mut data[row_stride * (4 % D::F32Vec::LEN) + (4 / D::F32Vec::LEN)]);
    v5.store_array(&mut data[row_stride * (5 % D::F32Vec::LEN) + (5 / D::F32Vec::LEN)]);
    v6.store_array(&mut data[row_stride * (6 % D::F32Vec::LEN) + (6 / D::F32Vec::LEN)]);
    v7.store_array(&mut data[row_stride * (7 % D::F32Vec::LEN) + (7 / D::F32Vec::LEN)]);
    v8.store_array(&mut data[row_stride * (8 % D::F32Vec::LEN) + (8 / D::F32Vec::LEN)]);
    v9.store_array(&mut data[row_stride * (9 % D::F32Vec::LEN) + (9 / D::F32Vec::LEN)]);
    v10.store_array(&mut data[row_stride * (10 % D::F32Vec::LEN) + (10 / D::F32Vec::LEN)]);
    v11.store_array(&mut data[row_stride * (11 % D::F32Vec::LEN) + (11 / D::F32Vec::LEN)]);
    v12.store_array(&mut data[row_stride * (12 % D::F32Vec::LEN) + (12 / D::F32Vec::LEN)]);
    v13.store_array(&mut data[row_stride * (13 % D::F32Vec::LEN) + (13 / D::F32Vec::LEN)]);
    v14.store_array(&mut data[row_stride * (14 % D::F32Vec::LEN) + (14 / D::F32Vec::LEN)]);
    v15.store_array(&mut data[row_stride * (15 % D::F32Vec::LEN) + (15 / D::F32Vec::LEN)]);
}

#[inline(always)]
pub(super) fn do_idct_16_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 8 / D::F32Vec::LEN;
    assert!(data.len() > 15 * row_stride);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 4]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 6]);
    let mut v4 = D::F32Vec::load_array(d, &data[row_stride * 8]);
    let mut v5 = D::F32Vec::load_array(d, &data[row_stride * 10]);
    let mut v6 = D::F32Vec::load_array(d, &data[row_stride * 12]);
    let mut v7 = D::F32Vec::load_array(d, &data[row_stride * 14]);
    let mut v8 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v9 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    let mut v10 = D::F32Vec::load_array(d, &data[row_stride * 5]);
    let mut v11 = D::F32Vec::load_array(d, &data[row_stride * 7]);
    let mut v12 = D::F32Vec::load_array(d, &data[row_stride * 9]);
    let mut v13 = D::F32Vec::load_array(d, &data[row_stride * 11]);
    let mut v14 = D::F32Vec::load_array(d, &data[row_stride * 13]);
    let mut v15 = D::F32Vec::load_array(d, &data[row_stride * 15]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    ) = idct_16(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    );
    v0.store_array(&mut data[row_stride * 0]);
    v1.store_array(&mut data[row_stride * 1]);
    v2.store_array(&mut data[row_stride * 2]);
    v3.store_array(&mut data[row_stride * 3]);
    v4.store_array(&mut data[row_stride * 4]);
    v5.store_array(&mut data[row_stride * 5]);
    v6.store_array(&mut data[row_stride * 6]);
    v7.store_array(&mut data[row_stride * 7]);
    v8.store_array(&mut data[row_stride * 8]);
    v9.store_array(&mut data[row_stride * 9]);
    v10.store_array(&mut data[row_stride * 10]);
    v11.store_array(&mut data[row_stride * 11]);
    v12.store_array(&mut data[row_stride * 12]);
    v13.store_array(&mut data[row_stride * 13]);
    v14.store_array(&mut data[row_stride * 14]);
    v15.store_array(&mut data[row_stride * 15]);
}
