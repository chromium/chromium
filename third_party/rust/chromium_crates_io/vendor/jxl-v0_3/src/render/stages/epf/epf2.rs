// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    BLOCK_DIM, MIN_SIGMA,
    features::epf::SigmaSource,
    render::{
        Channels, ChannelsMut, RenderPipelineInOutStage,
        stages::epf::common::{get_sigma, prepare_sad_mul_storage},
    },
};

use jxl_simd::{F32SimdVec, SimdMask, simd_function};

/// 3x3 plus-shaped kernel with 1 SAD per pixel. So this makes this filter a 3x3 filter.
pub struct Epf2Stage {
    /// Multiplier for sigma in pass 2
    sigma_scale: f32,
    /// (inverse) multiplier for sigma on borders
    border_sad_mul: f32,
    channel_scale: [f32; 3],
    sigma: SigmaSource,
}

impl std::fmt::Display for Epf2Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "EPF stage 2 with sigma scale: {}, border_sad_mul: {}",
            self.sigma_scale, self.border_sad_mul
        )
    }
}

impl Epf2Stage {
    pub fn new(
        sigma_scale: f32,
        border_sad_mul: f32,
        channel_scale: [f32; 3],
        sigma: SigmaSource,
    ) -> Self {
        Self {
            sigma,
            sigma_scale,
            channel_scale,
            border_sad_mul,
        }
    }
}

simd_function!(
epf2_process_row_chunk_dispatch,
d: D,
fn epf2_process_row_chunk(
    stage: &Epf2Stage,
    pos: (usize, usize),
    xsize: usize,
    input_rows: &Channels<f32>,
    output_rows: &mut ChannelsMut<f32>,
) {
    let (xpos, ypos) = pos;
    assert_eq!(input_rows.len(), 3, "Expected 3 channels, got {}", input_rows.len());
    let (input_x, input_y, input_b) = (&input_rows[0], &input_rows[1], &input_rows[2]);
    let (output_x, output_y, output_b) = output_rows.split_first_3_mut();

    let row_sigma = stage.sigma.row(ypos / BLOCK_DIM);

    const { assert!(D::F32Vec::LEN <= 16) };

    let sm = stage.sigma_scale * 1.65;
    let bsm = sm * stage.border_sad_mul;
    let sad_mul_storage = prepare_sad_mul_storage(xpos, ypos, sm, bsm);

    for x in (0..xsize).step_by(D::F32Vec::LEN) {
        let sigma = get_sigma(d, x + xpos, row_sigma);
        let sad_mul = D::F32Vec::load(d, &sad_mul_storage[x % 8..]);

        if D::F32Vec::splat(d, MIN_SIGMA).gt(sigma).all() {
            D::F32Vec::load(d, &input_x[1][1 + x..]).store(&mut output_x[0][x..]);
            D::F32Vec::load(d, &input_y[1][1 + x..]).store(&mut output_y[0][x..]);
            D::F32Vec::load(d, &input_b[1][1 + x..]).store(&mut output_b[0][x..]);
            continue;
        }

        let inv_sigma = sigma * sad_mul;

        let x_cc = D::F32Vec::load(d, &input_x[1][1 + x..]);
        let y_cc = D::F32Vec::load(d, &input_y[1][1 + x..]);
        let b_cc = D::F32Vec::load(d, &input_b[1][1 + x..]);

        let mut w_acc = D::F32Vec::splat(d, 1.0);
        let mut x_acc = x_cc;
        let mut y_acc = y_cc;
        let mut b_acc = b_cc;

        for (y_off, x_off) in [(0, 1), (1, 0), (1, 2), (2, 1)] {
            let (cx, cy, cb) = (
                D::F32Vec::load(d, &input_x[y_off as usize][x_off + x..]),
                D::F32Vec::load(d, &input_y[y_off as usize][x_off + x..]),
                D::F32Vec::load(d, &input_b[y_off as usize][x_off + x..]),
            );
            let sad = (cx - x_cc).abs().mul_add(
                D::F32Vec::splat(d, stage.channel_scale[0]),
                (cy - y_cc).abs().mul_add(
                    D::F32Vec::splat(d, stage.channel_scale[1]),
                    (cb - b_cc).abs() * D::F32Vec::splat(d, stage.channel_scale[2]),
                ),
            );
            let weight = sad
                .mul_add(inv_sigma, D::F32Vec::splat(d, 1.0))
                .max(D::F32Vec::splat(d, 0.0));
            w_acc += weight;
            x_acc = weight.mul_add(cx, x_acc);
            y_acc = weight.mul_add(cy, y_acc);
            b_acc = weight.mul_add(cb, b_acc);
        }

        let inv_w = D::F32Vec::splat(d, 1.0) / w_acc;

        (x_acc * inv_w).store(&mut output_x[0][x..]);
        (y_acc * inv_w).store(&mut output_y[0][x..]);
        (b_acc * inv_w).store(&mut output_b[0][x..]);
    }
});

impl RenderPipelineInOutStage for Epf2Stage {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (1, 1);

    fn uses_channel(&self, c: usize) -> bool {
        c < 3
    }

    fn process_row_chunk(
        &self,
        (xpos, ypos): (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        epf2_process_row_chunk_dispatch(self, (xpos, ypos), xsize, input_rows, output_rows);
    }
}
