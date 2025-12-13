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
pub(super) fn reinterpreting_dct_16<D: SimdDescriptor>(
    d: D,
    v0: D::F32Vec,
    v1: D::F32Vec,
    v2: D::F32Vec,
    v3: D::F32Vec,
    v4: D::F32Vec,
    v5: D::F32Vec,
    v6: D::F32Vec,
    v7: D::F32Vec,
    v8: D::F32Vec,
    v9: D::F32Vec,
    v10: D::F32Vec,
    v11: D::F32Vec,
    v12: D::F32Vec,
    v13: D::F32Vec,
    v14: D::F32Vec,
    v15: D::F32Vec,
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
    let v16 = v0 + v15;
    let v17 = v1 + v14;
    let v18 = v2 + v13;
    let v19 = v3 + v12;
    let v20 = v4 + v11;
    let v21 = v5 + v10;
    let v22 = v6 + v9;
    let v23 = v7 + v8;
    let v24 = v16 + v23;
    let v25 = v17 + v22;
    let v26 = v18 + v21;
    let v27 = v19 + v20;
    let v28 = v24 + v27;
    let v29 = v25 + v26;
    let v30 = v28 + v29;
    let v31 = v28 - v29;
    let v32 = v24 - v27;
    let v33 = v25 - v26;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v34 = v32 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v35 = v33 * mul;
    let v36 = v34 + v35;
    let v37 = v34 - v35;
    let v38 = v36.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v37);
    let v39 = v16 - v23;
    let v40 = v17 - v22;
    let v41 = v18 - v21;
    let v42 = v19 - v20;
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let v43 = v39 * mul;
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let v44 = v40 * mul;
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let v45 = v41 * mul;
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let v46 = v42 * mul;
    let v47 = v43 + v46;
    let v48 = v44 + v45;
    let v49 = v47 + v48;
    let v50 = v47 - v48;
    let v51 = v43 - v46;
    let v52 = v44 - v45;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v53 = v51 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v54 = v52 * mul;
    let v55 = v53 + v54;
    let v56 = v53 - v54;
    let v57 = v55.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v56);
    let v58 = v49.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v57);
    let v59 = v57 + v50;
    let v60 = v50 + v56;
    let v61 = v0 - v15;
    let v62 = v1 - v14;
    let v63 = v2 - v13;
    let v64 = v3 - v12;
    let v65 = v4 - v11;
    let v66 = v5 - v10;
    let v67 = v6 - v9;
    let v68 = v7 - v8;
    let mul = D::F32Vec::splat(d, 0.5024192861881557);
    let v69 = v61 * mul;
    let mul = D::F32Vec::splat(d, 0.5224986149396889);
    let v70 = v62 * mul;
    let mul = D::F32Vec::splat(d, 0.5669440348163577);
    let v71 = v63 * mul;
    let mul = D::F32Vec::splat(d, 0.6468217833599901);
    let v72 = v64 * mul;
    let mul = D::F32Vec::splat(d, 0.7881546234512502);
    let v73 = v65 * mul;
    let mul = D::F32Vec::splat(d, 1.0606776859903471);
    let v74 = v66 * mul;
    let mul = D::F32Vec::splat(d, 1.7224470982383342);
    let v75 = v67 * mul;
    let mul = D::F32Vec::splat(d, 5.1011486186891553);
    let v76 = v68 * mul;
    let v77 = v69 + v76;
    let v78 = v70 + v75;
    let v79 = v71 + v74;
    let v80 = v72 + v73;
    let v81 = v77 + v80;
    let v82 = v78 + v79;
    let v83 = v81 + v82;
    let v84 = v81 - v82;
    let v85 = v77 - v80;
    let v86 = v78 - v79;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v87 = v85 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v88 = v86 * mul;
    let v89 = v87 + v88;
    let v90 = v87 - v88;
    let v91 = v89.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v90);
    let v92 = v69 - v76;
    let v93 = v70 - v75;
    let v94 = v71 - v74;
    let v95 = v72 - v73;
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let v96 = v92 * mul;
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let v97 = v93 * mul;
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let v98 = v94 * mul;
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let v99 = v95 * mul;
    let v100 = v96 + v99;
    let v101 = v97 + v98;
    let v102 = v100 + v101;
    let v103 = v100 - v101;
    let v104 = v96 - v99;
    let v105 = v97 - v98;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v106 = v104 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v107 = v105 * mul;
    let v108 = v106 + v107;
    let v109 = v106 - v107;
    let v110 = v108.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v109);
    let v111 = v102.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v110);
    let v112 = v110 + v103;
    let v113 = v103 + v109;
    let v114 = v83.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v111);
    let v115 = v111 + v91;
    let v116 = v91 + v112;
    let v117 = v112 + v84;
    let v118 = v84 + v113;
    let v119 = v113 + v90;
    let v120 = v90 + v109;
    (
        v30 * D::F32Vec::splat(d, 0.062500),
        v114 * D::F32Vec::splat(d, 0.062599),
        v58 * D::F32Vec::splat(d, 0.062897),
        v115 * D::F32Vec::splat(d, 0.063398),
        v38 * D::F32Vec::splat(d, 0.064110),
        v116 * D::F32Vec::splat(d, 0.065042),
        v59 * D::F32Vec::splat(d, 0.066206),
        v117 * D::F32Vec::splat(d, 0.067622),
        v31 * D::F32Vec::splat(d, 0.069309),
        v118 * D::F32Vec::splat(d, 0.071294),
        v60 * D::F32Vec::splat(d, 0.073611),
        v119 * D::F32Vec::splat(d, 0.076300),
        v37 * D::F32Vec::splat(d, 0.079410),
        v120 * D::F32Vec::splat(d, 0.083003),
        v56 * D::F32Vec::splat(d, 0.087156),
        v109 * D::F32Vec::splat(d, 0.091963),
    )
}

#[inline(always)]
pub(super) fn do_reinterpreting_dct_16<D: SimdDescriptor>(
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
    ) = reinterpreting_dct_16(
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
pub(super) fn do_reinterpreting_dct_16_rowblock<D: SimdDescriptor>(
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
    ) = reinterpreting_dct_16(
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
pub(super) fn do_reinterpreting_dct_16_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 8 / D::F32Vec::LEN;
    assert!(data.len() > 15 * row_stride);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    let mut v4 = D::F32Vec::load_array(d, &data[row_stride * 4]);
    let mut v5 = D::F32Vec::load_array(d, &data[row_stride * 5]);
    let mut v6 = D::F32Vec::load_array(d, &data[row_stride * 6]);
    let mut v7 = D::F32Vec::load_array(d, &data[row_stride * 7]);
    let mut v8 = D::F32Vec::load_array(d, &data[row_stride * 8]);
    let mut v9 = D::F32Vec::load_array(d, &data[row_stride * 9]);
    let mut v10 = D::F32Vec::load_array(d, &data[row_stride * 10]);
    let mut v11 = D::F32Vec::load_array(d, &data[row_stride * 11]);
    let mut v12 = D::F32Vec::load_array(d, &data[row_stride * 12]);
    let mut v13 = D::F32Vec::load_array(d, &data[row_stride * 13]);
    let mut v14 = D::F32Vec::load_array(d, &data[row_stride * 14]);
    let mut v15 = D::F32Vec::load_array(d, &data[row_stride * 15]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    ) = reinterpreting_dct_16(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
    );
    v0.store_array(&mut data[row_stride * 0]);
    v8.store_array(&mut data[row_stride * 1]);
    v1.store_array(&mut data[row_stride * 2]);
    v9.store_array(&mut data[row_stride * 3]);
    v2.store_array(&mut data[row_stride * 4]);
    v10.store_array(&mut data[row_stride * 5]);
    v3.store_array(&mut data[row_stride * 6]);
    v11.store_array(&mut data[row_stride * 7]);
    v4.store_array(&mut data[row_stride * 8]);
    v12.store_array(&mut data[row_stride * 9]);
    v5.store_array(&mut data[row_stride * 10]);
    v13.store_array(&mut data[row_stride * 11]);
    v6.store_array(&mut data[row_stride * 12]);
    v14.store_array(&mut data[row_stride * 13]);
    v7.store_array(&mut data[row_stride * 14]);
    v15.store_array(&mut data[row_stride * 15]);
}
