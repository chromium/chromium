// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask};

use super::EpfStage;
use crate::features::epf::SigmaSource;
use crate::render::{Channels, ChannelsMut, ChannelsView, for_each_chunk};
use crate::{BLOCK_DIM, MIN_SIGMA};

/// Sigma row source for EPF processing.
/// Either a slice from the variable sigma image, or a constant value.
#[derive(Clone, Copy)]
pub(super) enum SigmaRow<'a> {
    Variable(&'a [f32]),
    Constant(f32),
}

impl SigmaSource {
    /// Get the sigma row for a given y position.
    #[inline(always)]
    pub(super) fn row(&self, y: usize) -> SigmaRow<'_> {
        match self {
            SigmaSource::Variable(image) => SigmaRow::Variable(image.row(y)),
            SigmaSource::Constant(sigma) => SigmaRow::Constant(*sigma),
        }
    }
}

#[inline(always)]
pub(super) fn prepare_sad_mul_storage(x: usize, y: usize, sm: f32, bsm: f32) -> [f32; 24] {
    let mut sad_mul_storage = [bsm; 24];
    if ![0, BLOCK_DIM - 1].contains(&(y % BLOCK_DIM)) {
        for (i, s) in sad_mul_storage.iter_mut().enumerate().take(16) {
            if ![0, BLOCK_DIM - 1].contains(&((x + i) % BLOCK_DIM)) {
                *s = sm;
            }
        }
    }
    sad_mul_storage
}

#[inline(always)]
fn get_sigma_from_row<D: SimdDescriptor>(
    d: D,
    x: usize,
    xpos_mod: usize,
    row_sigma: &[f32],
) -> D::F32Vec {
    const { assert!(BLOCK_DIM == 8) }
    const { assert!(D::F32Vec::LEN <= 16) }
    let iota = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    let iota = D::I32Vec::load(d, &iota);
    if D::F32Vec::LEN > 8 {
        let sigma_start = x / BLOCK_DIM;
        let offset = D::I32Vec::splat(d, xpos_mod as i32) + iota;
        let &[sigma0, sigma1, sigma2] = &row_sigma[sigma_start..sigma_start + 3] else {
            unreachable!();
        };
        let sigma0 = D::F32Vec::splat(d, sigma0);
        let sigma1 = D::F32Vec::splat(d, sigma1);
        let sigma2 = D::F32Vec::splat(d, sigma2);
        let above_8 = offset.gt(D::I32Vec::splat(d, 7));
        let above_16 = offset.gt(D::I32Vec::splat(d, 15));
        above_16.if_then_else_f32(sigma2, above_8.if_then_else_f32(sigma1, sigma0))
    } else if D::F32Vec::LEN == 8 {
        let sigma_start = x / BLOCK_DIM;
        let offset = D::I32Vec::splat(d, xpos_mod as i32) + iota;
        let &[sigma0, sigma1] = &row_sigma[sigma_start..sigma_start + 2] else {
            unreachable!();
        };
        let sigma0 = D::F32Vec::splat(d, sigma0);
        let sigma1 = D::F32Vec::splat(d, sigma1);
        let above_8 = offset.gt(D::I32Vec::splat(d, 7));
        above_8.if_then_else_f32(sigma1, sigma0)
    } else {
        let pos = x + xpos_mod;
        let sigma_start = pos / BLOCK_DIM;
        let offset = D::I32Vec::splat(d, (pos % BLOCK_DIM) as i32) + iota;
        let Some(&[sigma0, sigma1]) = row_sigma.get(sigma_start..sigma_start + 2) else {
            return D::F32Vec::splat(d, 0.0);
        };
        let sigma0 = D::F32Vec::splat(d, sigma0);
        let sigma1 = D::F32Vec::splat(d, sigma1);
        let above_8 = offset.gt(D::I32Vec::splat(d, 7));
        above_8.if_then_else_f32(sigma1, sigma0)
    }
}

/// Shared outer row-chunk driver for EPF stages 0, 1, and 2.
///
/// Processes SADs channel-by-channel (`0`, `1`, `2`) and then accumulates weighted outputs
/// one channel at a time, so only a single channel's neighborhood pixels are live in SIMD
/// registers at any point.
#[inline(always)]
#[allow(clippy::needless_range_loop, clippy::too_many_arguments)]
pub(super) fn epf_process_row_chunk<
    D: SimdDescriptor,
    const STEP: u8,
    const BORDER: u8,
    const ROWS: usize,
    const RADIUS: usize,
    const N: usize,
>(
    d: D,
    stage: &EpfStage<STEP, BORDER>,
    xpos: usize,
    xsize: usize,
    row_sigma: &[f32],
    sad_mul_storage: &[f32; 24],
    input_rows: &Channels<f32>,
    output_rows: &mut ChannelsMut<f32>,
    offsets: [(isize, isize); N],
    channel_sads: impl Fn(&ChannelsView<f32, 1, ROWS, RADIUS>) -> [D::F32Vec; N],
) {
    const { assert!(ROWS == 2 * RADIUS + 1) };
    const { assert!(D::F32Vec::LEN <= 16) };
    if xsize == 0 {
        return;
    }

    let len = D::F32Vec::LEN;
    let xpos_mod = xpos % BLOCK_DIM;
    let sigma_elems = if len > 8 { 3 } else { 2 };
    let num_chunks = (xsize - 1) / len + 1;
    assert!(num_chunks <= usize::MAX / len);
    let max_sigma_start = if len >= BLOCK_DIM {
        (num_chunks - 1) * (len / BLOCK_DIM)
    } else {
        ((num_chunks - 1) * len).checked_add(xpos_mod).unwrap() / BLOCK_DIM
    };
    let needed = max_sigma_start + sigma_elems;
    let row_sigma = &row_sigma[..needed];

    let scales = [
        D::F32Vec::splat(d, stage.channel_scale[0]),
        D::F32Vec::splat(d, stage.channel_scale[1]),
        D::F32Vec::splat(d, stage.channel_scale[2]),
    ];
    let c_one = D::F32Vec::splat(d, 1.0);
    let c_zero = D::F32Vec::splat(d, 0.0);
    let min_sigma = D::F32Vec::splat(d, MIN_SIGMA);

    for_each_chunk(
        d,
        xsize,
        input_rows.view::<3, ROWS, RADIUS>(),
        output_rows.view::<3, 1, 1>(),
        #[inline(always)]
        |x, inv, outv| {
            let sigma = get_sigma_from_row(d, x, xpos_mod, row_sigma);
            let sad_mul = D::F32Vec::load(d, &sad_mul_storage[x % 8..x % 8 + D::F32Vec::LEN]);

            let sigma_mask = min_sigma.gt(sigma);
            if sigma_mask.all() {
                outv.store::<_, 0>(d, 0, inv.load::<_, 0>(d, 0, 0));
                outv.store::<_, 1>(d, 0, inv.load::<_, 1>(d, 0, 0));
                outv.store::<_, 2>(d, 0, inv.load::<_, 2>(d, 0, 0));
                return;
            }

            let mut sads = channel_sads(&inv.select_channel::<0>());
            for i in 0..N {
                sads[i] *= scales[0];
            }

            let sads1 = channel_sads(&inv.select_channel::<1>());
            for i in 0..N {
                sads[i] = sads1[i].mul_add(scales[1], sads[i]);
            }

            let sads2 = channel_sads(&inv.select_channel::<2>());
            for i in 0..N {
                sads[i] = sads2[i].mul_add(scales[2], sads[i]);
            }

            let inv_sigma = sigma * sad_mul;
            let mut w = c_one;
            for i in 0..N {
                let weight = sads[i].mul_add(inv_sigma, c_one).max(c_zero);
                w += weight;
                sads[i] = weight;
            }
            let weights = sads;
            let inv_w = c_one / w;

            macro_rules! accumulate_channel {
                ($c:literal) => {{
                    let cc = inv.load::<_, $c>(d, 0, 0);
                    let mut acc = cc;
                    for i in 0..N {
                        let (dr, dc) = offsets[i];
                        acc = inv.load::<_, $c>(d, dr, dc).mul_add(weights[i], acc);
                    }
                    outv.store::<_, $c>(d, 0, sigma_mask.if_then_else_f32(cc, acc * inv_w));
                }};
            }
            accumulate_channel!(2);
            accumulate_channel!(0);
            accumulate_channel!(1);
        },
    );
}

/// Computes the 5-point plus-shaped SAD for a single channel at candidate offset `(DR, DC)`.
/// By ordering the two points of every absolute difference canonically at compile time, any
/// shared differences across opposite offsets `(DR, DC)` and `(-DR, -DC)` produce identical
/// expressions and are automatically deduplicated by LLVM CSE.
#[inline(always)]
pub(super) fn plus_sad<
    D: SimdDescriptor,
    const ROWS: usize,
    const RADIUS: usize,
    const DR: isize,
    const DC: isize,
>(
    d: D,
    inv_c: &ChannelsView<f32, 1, ROWS, RADIUS>,
) -> D::F32Vec {
    macro_rules! canon_diff {
        ($r1:expr, $c1:expr, $r2:expr, $c2:expr) => {{
            let (ar, ac, br, bc) = const {
                let p1 = ($r1, $c1);
                let p2 = ($r2, $c2);
                if p1.0 < p2.0 || (p1.0 == p2.0 && p1.1 < p2.1) {
                    (p1.0, p1.1, p2.0, p2.1)
                } else {
                    (p2.0, p2.1, p1.0, p1.1)
                }
            };
            (inv_c.load::<_, 0>(d, ar, ac) - inv_c.load::<_, 0>(d, br, bc)).abs()
        }};
    }

    canon_diff!(DR, DC, 0, 0)
        + canon_diff!(DR - 1, DC, -1, 0)
        + canon_diff!(DR + 1, DC, 1, 0)
        + canon_diff!(DR, DC - 1, 0, -1)
        + canon_diff!(DR, DC + 1, 0, 1)
}
