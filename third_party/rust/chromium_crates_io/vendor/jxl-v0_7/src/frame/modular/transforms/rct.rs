// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{I16SimdVec, I32SimdVec, ScalarDescriptor, SimdDescriptor, shr, simd_function};

use crate::frame::modular::transforms::{RctOp, RctPermutation};
use crate::frame::modular::{ModularChannel, ModularStorage};
use crate::image::ImageRectMut;
use crate::util::tracing_wrappers::*;

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
fn avg_i16<D: SimdDescriptor>(d: D, a: D::I16Vec, b: D::I16Vec) -> D::I16Vec {
    shr!(a, 1) + shr!(b, 1) + (a & b & D::I16Vec::splat(d, 1))
}

#[inline(always)]
fn rct_impl_i16<D: SimdDescriptor, const OP: u32>(
    d: D,
    v0: D::I16Vec,
    v1: D::I16Vec,
    v2: D::I16Vec,
) -> (D::I16Vec, D::I16Vec, D::I16Vec) {
    const { assert!(OP <= 6) };

    match OP {
        0 => (v0, v1, v2),
        1 => (v0, v1, v2 + v0),
        2 => (v0, v1 + v0, v2),
        3 => (v0, v1 + v0, v2 + v0),
        4 => {
            let avg = avg_i16(d, v0, v2);
            (v0, v1 + avg, v2)
        }
        5 => {
            let v2 = v0 + v2;
            let avg = avg_i16(d, v0, v2);
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
fn rct_row_impl_i16<D: SimdDescriptor, const OP: u32>(
    d: D,
    rgb: [&mut [i16]; 3],
) -> [&mut [i16]; 3] {
    const { assert!(OP <= 6) };

    let [mut it_row_r, mut it_row_g, mut it_row_b] =
        rgb.map(|x| x.chunks_exact_mut(D::I16Vec::LEN));
    let it = (&mut it_row_r).zip(&mut it_row_g).zip(&mut it_row_b);

    for ((r, g), b) in it {
        let v0 = D::I16Vec::load(d, r);
        let v1 = D::I16Vec::load(d, g);
        let v2 = D::I16Vec::load(d, b);
        let (w0, w1, w2) = rct_impl_i16::<D, OP>(d, v0, v1, v2);
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
    r: &mut ImageRectMut<'_, i32>,
    g: &mut ImageRectMut<'_, i32>,
    b: &mut ImageRectMut<'_, i32>,
) {
    const { assert!(OP <= 6) };

    let h = r.size().1;

    for pos_y in 0..h {
        let rgb = [r.row(pos_y), g.row(pos_y), b.row(pos_y)];

        let mut rgb = rct_row_impl::<D, OP>(d, rgb);
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

#[inline(always)]
fn rct_loop_impl_i16<D: SimdDescriptor, const OP: u32>(
    d: D,
    r: &mut ImageRectMut<'_, i16>,
    g: &mut ImageRectMut<'_, i16>,
    b: &mut ImageRectMut<'_, i16>,
) {
    const { assert!(OP <= 6) };

    let h = r.size().1;

    for pos_y in 0..h {
        let mut rgb = [r.row(pos_y), g.row(pos_y), b.row(pos_y)];

        rgb = rct_row_impl_i16::<D, OP>(d, rgb);
        if D::I16Vec::LEN > 16 {
            rgb = rct_row_impl_i16::<_, OP>(d.maybe_downgrade_256bit(), rgb);
        }
        if D::I16Vec::LEN > 8 {
            rgb = rct_row_impl_i16::<_, OP>(d.maybe_downgrade_128bit(), rgb);
        }
        if D::I16Vec::LEN > 1 {
            rct_row_impl_i16::<_, OP>(ScalarDescriptor::new().unwrap(), rgb);
        }
    }
}

simd_function!(
    rct_loop,
    d: D,
    fn rct_loop_fwd(
        r: &mut ImageRectMut<'_, i32>,
        g: &mut ImageRectMut<'_, i32>,
        b: &mut ImageRectMut<'_, i32>,
        op: RctOp,
    ) {
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

simd_function!(
    rct_loop_i16,
    d: D,
    fn rct_loop_fwd_i16(
        r: &mut ImageRectMut<'_, i16>,
        g: &mut ImageRectMut<'_, i16>,
        b: &mut ImageRectMut<'_, i16>,
        op: RctOp,
    ) {
        match op {
            RctOp::Noop => {},
            RctOp::AddFirstToThird => rct_loop_impl_i16::<D, 1>(d, r, g, b),
            RctOp::AddFirstToSecond => rct_loop_impl_i16::<D, 2>(d, r, g, b),
            RctOp::AddFirstToSecondAndThird => rct_loop_impl_i16::<D, 3>(d, r, g, b),
            RctOp::AddAvgToSecond => rct_loop_impl_i16::<D, 4>(d, r, g, b),
            RctOp::AddFirstToThirdAndAvgToSecond => rct_loop_impl_i16::<D, 5>(d, r, g, b),
            RctOp::YCoCg => rct_loop_impl_i16::<D, 6>(d, r, g, b),
        }
    }
);

fn apply_rct_permutation(buffers: &mut [&mut ModularChannel], perm: RctPermutation) {
    let [r, g, b] = buffers else {
        unreachable!("incorrect buffer count for RCT");
    };

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

// Applies a RCT in-place to the given i16 buffers.
#[allow(dead_code)]
#[instrument(level = "debug", skip(buffers), ret)]
pub fn do_rct_step_i16(buffers: &mut [&mut ModularChannel], op: RctOp) {
    let [r, g, b] = buffers else {
        unreachable!("incorrect buffer count for RCT");
    };

    if op != RctOp::Noop {
        let mut r_rect = ImageRectMut::<i16>::from_raw(r.data.as_rect_mut());
        let mut g_rect = ImageRectMut::<i16>::from_raw(g.data.as_rect_mut());
        let mut b_rect = ImageRectMut::<i16>::from_raw(b.data.as_rect_mut());
        rct_loop_i16(&mut r_rect, &mut g_rect, &mut b_rect, op);
    }
}

// Applies a RCT in-place to the given i32 buffers.
#[instrument(level = "debug", skip(buffers), ret)]
pub fn do_rct_step_i32(buffers: &mut [&mut ModularChannel], op: RctOp) {
    let [r, g, b] = buffers else {
        unreachable!("incorrect buffer count for RCT");
    };

    if op != RctOp::Noop {
        let mut r_rect = ImageRectMut::<i32>::from_raw(r.data.as_rect_mut());
        let mut g_rect = ImageRectMut::<i32>::from_raw(g.data.as_rect_mut());
        let mut b_rect = ImageRectMut::<i32>::from_raw(b.data.as_rect_mut());
        rct_loop(&mut r_rect, &mut g_rect, &mut b_rect, op);
    }
}

// Applies a RCT in-place to the given buffers.
#[allow(dead_code)]
#[instrument(level = "debug", skip(buffers), ret)]
pub fn do_rct_step(
    buffers: &mut [&mut ModularChannel],
    storage: ModularStorage,
    op: RctOp,
    perm: RctPermutation,
) {
    match storage {
        ModularStorage::I16 => do_rct_step_i16(buffers, op),
        ModularStorage::I32 => do_rct_step_i32(buffers, op),
    }

    apply_rct_permutation(buffers, perm);
}
