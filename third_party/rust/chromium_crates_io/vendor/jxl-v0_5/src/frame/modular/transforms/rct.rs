// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{I32SimdVec, ScalarDescriptor, SimdDescriptor, shr, simd_function};

use crate::{
    frame::modular::{
        ModularChannel,
        transforms::{RctOp, RctPermutation},
    },
    image::Image,
    util::tracing_wrappers::*,
};

#[inline(always)]
fn rct_impl<D: SimdDescriptor, const OP: u32>(
    _: D,
    v0: D::I32Vec,
    v1: D::I32Vec,
    v2: D::I32Vec,
) -> (D::I32Vec, D::I32Vec, D::I32Vec) {
    const { assert!(OP <= 6) };

    match OP {
        0 => (v0, v1, v2),
        1 => (v0, v1, v2 + v0),
        2 => (v0, v1 + v0, v2),
        3 => (v0, v1 + v0, v2 + v0),
        4 => {
            let avg = shr!(v0 + v2, 1);
            (v0, v1 + avg, v2)
        }
        5 => {
            let v2 = v0 + v2;
            let avg = shr!(v0 + v2, 1);
            (v0, v1 + avg, v2)
        }
        6 => {
            let (y, co, cg) = (v0, v1, v2);
            let y = y - shr!(cg, 1);
            let g = cg + y;
            let y = y - shr!(co, 1);
            let r = y + co;
            (r, g, y)
        }
        _ => unreachable!(),
    }
}

#[inline(always)]
fn rct_row_impl<D: SimdDescriptor, const OP: u32>(d: D, rgb: [&mut [i32]; 3]) -> [&mut [i32]; 3] {
    const { assert!(OP <= 6) };

    let [mut it_row_r, mut it_row_g, mut it_row_b] =
        rgb.map(|x| x.chunks_exact_mut(D::I32Vec::LEN));
    let it = (&mut it_row_r).zip(&mut it_row_g).zip(&mut it_row_b);

    for ((r, g), b) in it {
        let v0 = D::I32Vec::load(d, r);
        let v1 = D::I32Vec::load(d, g);
        let v2 = D::I32Vec::load(d, b);
        let (w0, w1, w2) = rct_impl::<D, OP>(d, v0, v1, v2);
        w0.store(r);
        w1.store(g);
        w2.store(b);
    }

    [
        it_row_r.into_remainder(),
        it_row_g.into_remainder(),
        it_row_b.into_remainder(),
    ]
}

#[inline(always)]
fn rct_loop_impl<D: SimdDescriptor, const OP: u32>(
    d: D,
    r: &mut Image<i32>,
    g: &mut Image<i32>,
    b: &mut Image<i32>,
) {
    const { assert!(OP <= 6) };

    let h = r.size().1;

    for pos_y in 0..h {
        let mut rgb = [&mut *r, &mut *g, &mut *b].map(|x| x.row_mut(pos_y));

        rgb = rct_row_impl::<D, OP>(d, rgb);
        if D::I32Vec::LEN > 8 {
            rgb = rct_row_impl::<_, OP>(d.maybe_downgrade_256bit(), rgb);
        }
        if D::I32Vec::LEN > 4 {
            rgb = rct_row_impl::<_, OP>(d.maybe_downgrade_128bit(), rgb);
        }
        if D::I32Vec::LEN > 1 {
            rct_row_impl::<_, OP>(ScalarDescriptor::new().unwrap(), rgb);
        }
    }
}

simd_function!(
    rct_loop,
    d: D,
    fn rct_loop_fwd(r: &mut Image<i32>, g: &mut Image<i32>, b: &mut Image<i32>, op: RctOp) {
        match op {
            RctOp::Noop => {},
            RctOp::AddFirstToThird => rct_loop_impl::<D, 1>(d, r, g, b),
            RctOp::AddFirstToSecond => rct_loop_impl::<D, 2>(d, r, g, b),
            RctOp::AddFirstToSecondAndThird => rct_loop_impl::<D, 3>(d, r, g, b),
            RctOp::AddAvgToSecond => rct_loop_impl::<D, 4>(d, r, g, b),
            RctOp::AddFirstToThirdAndAvgToSecond => rct_loop_impl::<D, 5>(d, r, g, b),
            RctOp::YCoCg => rct_loop_impl::<D, 6>(d, r, g, b),
        }
    }
);

// Applies a RCT in-place to the given buffers.
#[instrument(level = "debug", skip(buffers), ret)]
pub fn do_rct_step(buffers: &mut [&mut ModularChannel], op: RctOp, perm: RctPermutation) {
    let [r, g, b] = buffers else {
        unreachable!("incorrect buffer count for RCT");
    };

    if op != RctOp::Noop {
        rct_loop(&mut r.data, &mut g.data, &mut b.data, op);
    }

    // Note: Gbr and Brg use the *inverse* permutation compared to libjxl, because we *first* write
    // to the buffers and then permute them, while in libjxl the buffers to be written to are
    // permuted first.
    // The same is true for Rbg/Grb/Bgr, but since those are involutions it doesn't change
    // anything.
    match perm {
        RctPermutation::Rgb => {}
        RctPermutation::Gbr => {
            // out[1, 2, 0] = in[0, 1, 2]
            std::mem::swap(&mut g.data, &mut b.data); // [1, 0, 2]
            std::mem::swap(&mut r.data, &mut g.data);
        }
        RctPermutation::Brg => {
            // out[2, 0, 1] = in[0, 1, 2]
            std::mem::swap(&mut r.data, &mut b.data); // [1, 0, 2]
            std::mem::swap(&mut r.data, &mut g.data);
        }
        RctPermutation::Rbg => {
            // out[0, 2, 1] = in[0, 1, 2]
            std::mem::swap(&mut b.data, &mut g.data);
        }
        RctPermutation::Grb => {
            // out[1, 0, 2] = in[0, 1, 2]
            std::mem::swap(&mut r.data, &mut g.data);
        }
        RctPermutation::Bgr => {
            // out[2, 1, 0] = in[0, 1, 2]
            std::mem::swap(&mut r.data, &mut b.data);
        }
    }
}
