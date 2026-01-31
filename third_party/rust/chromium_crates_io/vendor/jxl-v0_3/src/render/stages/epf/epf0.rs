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

/// 5x5 plus-shaped kernel with 5 SADs per pixel (3x3 plus-shaped). So this makes this filter a 7x7 filter.
pub struct Epf0Stage {
    /// Multiplier for sigma in pass 0
    sigma_scale: f32,
    /// (inverse) multiplier for sigma on borders
    border_sad_mul: f32,
    channel_scale: [f32; 3],
    sigma: SigmaSource,
}

impl std::fmt::Display for Epf0Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "EPF stage 0 with sigma scale: {}, border_sad_mul: {}",
            self.sigma_scale, self.border_sad_mul
        )
    }
}

impl Epf0Stage {
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
    epf0_process_row_chunk_dispatch,
    d: D,
    fn epf0_process_row_chunk_simd(
    stage: &Epf0Stage,
    pos: (usize, usize),
    xsize: usize,
    input_rows: &Channels<f32>,
    output_rows: &mut ChannelsMut<f32>,
) {
    let (xpos, ypos) = pos;
    assert_eq!(input_rows.len(), 3);
    assert_eq!(output_rows.len(), 3);

    let row_sigma = stage.sigma.row(ypos / BLOCK_DIM);

    const { assert!(D::F32Vec::LEN <= 16) };

    let sm = stage.sigma_scale * 1.65;
    let bsm = sm * stage.border_sad_mul;
    let sad_mul_storage = prepare_sad_mul_storage(xpos, ypos, sm, bsm);

    for x in (0..xsize).step_by(D::F32Vec::LEN) {
        let sigma = get_sigma(d, x + xpos, row_sigma);
        let sad_mul = D::F32Vec::load(d, &sad_mul_storage[x % 8..]);

        if D::F32Vec::splat(d, MIN_SIGMA).gt(sigma).all() {
            for (input_c, output_c) in input_rows.iter().zip(output_rows.iter_mut()) {
                D::F32Vec::load(d, &input_c[3][3 + x..]).store(&mut output_c[0][x..]);
            }
            continue;
        }

        // Compute SADs
        let mut sads = [D::F32Vec::splat(d, 0.0); 12];
        for (input_c, scale) in input_rows.iter().zip(stage.channel_scale) {
            let scale = D::F32Vec::splat(d, scale);

            let p30 = D::F32Vec::load(d, &input_c[0][3 + x..]);
            let p21 = D::F32Vec::load(d, &input_c[1][2 + x..]);
            let p31 = D::F32Vec::load(d, &input_c[1][3 + x..]);
            let p41 = D::F32Vec::load(d, &input_c[1][4 + x..]);
            let p12 = D::F32Vec::load(d, &input_c[2][1 + x..]);
            let p22 = D::F32Vec::load(d, &input_c[2][2 + x..]);
            let p32 = D::F32Vec::load(d, &input_c[2][3 + x..]);
            let p42 = D::F32Vec::load(d, &input_c[2][4 + x..]);
            let p52 = D::F32Vec::load(d, &input_c[2][5 + x..]);
            let p03 = D::F32Vec::load(d, &input_c[3][x..]);
            let p13 = D::F32Vec::load(d, &input_c[3][1 + x..]);
            let p23 = D::F32Vec::load(d, &input_c[3][2 + x..]);
            let p33 = D::F32Vec::load(d, &input_c[3][3 + x..]);
            let p43 = D::F32Vec::load(d, &input_c[3][4 + x..]);
            let p53 = D::F32Vec::load(d, &input_c[3][5 + x..]);
            let p63 = D::F32Vec::load(d, &input_c[3][6 + x..]);
            let p14 = D::F32Vec::load(d, &input_c[4][1 + x..]);
            let p24 = D::F32Vec::load(d, &input_c[4][2 + x..]);
            let p34 = D::F32Vec::load(d, &input_c[4][3 + x..]);
            let p44 = D::F32Vec::load(d, &input_c[4][4 + x..]);
            let p54 = D::F32Vec::load(d, &input_c[4][5 + x..]);
            let p25 = D::F32Vec::load(d, &input_c[5][2 + x..]);
            let p35 = D::F32Vec::load(d, &input_c[5][3 + x..]);
            let p45 = D::F32Vec::load(d, &input_c[5][4 + x..]);
            let p36 = D::F32Vec::load(d, &input_c[6][3 + x..]);
            let d32_30 = (p32 - p30).abs();
            let d32_21 = (p32 - p21).abs();
            let d32_31 = (p32 - p31).abs();
            let d32_41 = (p32 - p41).abs();
            let d32_12 = (p32 - p12).abs();
            let d32_22 = (p32 - p22).abs();
            let d32_42 = (p32 - p42).abs();
            let d32_52 = (p32 - p52).abs();
            let d32_23 = (p32 - p23).abs();
            let d32_34 = (p32 - p34).abs();
            let d32_43 = (p32 - p43).abs();
            let d32_33 = (p32 - p33).abs();
            let d23_21 = (p23 - p21).abs();
            let d23_12 = (p23 - p12).abs();
            let d23_22 = (p23 - p22).abs();
            let d23_03 = (p23 - p03).abs();
            let d23_13 = (p23 - p13).abs();
            let d23_33 = (p23 - p33).abs();
            let d23_43 = (p23 - p43).abs();
            let d23_14 = (p23 - p14).abs();
            let d23_24 = (p23 - p24).abs();
            let d23_34 = (p23 - p34).abs();
            let d23_25 = (p23 - p25).abs();
            let d33_31 = (p33 - p31).abs();
            let d33_22 = (p33 - p22).abs();
            let d33_42 = (p33 - p42).abs();
            let d33_13 = (p33 - p13).abs();
            let d33_43 = (p33 - p43).abs();
            let d33_53 = (p33 - p53).abs();
            let d33_24 = (p33 - p24).abs();
            let d33_34 = (p33 - p34).abs();
            let d33_44 = (p33 - p44).abs();
            let d33_35 = (p33 - p35).abs();
            let d43_41 = (p43 - p41).abs();
            let d43_42 = (p43 - p42).abs();
            let d43_52 = (p43 - p52).abs();
            let d43_53 = (p43 - p53).abs();
            let d43_63 = (p43 - p63).abs();
            let d43_34 = (p43 - p34).abs();
            let d43_44 = (p43 - p44).abs();
            let d43_54 = (p43 - p54).abs();
            let d43_45 = (p43 - p45).abs();
            let d34_14 = (p34 - p14).abs();
            let d34_24 = (p34 - p24).abs();
            let d34_44 = (p34 - p44).abs();
            let d34_54 = (p34 - p54).abs();
            let d34_25 = (p34 - p25).abs();
            let d34_35 = (p34 - p35).abs();
            let d34_45 = (p34 - p45).abs();
            let d34_36 = (p34 - p36).abs();
            sads[0] = scale.mul_add(d32_30 + d23_21 + d33_31 + d43_41 + d32_34, sads[0]);
            sads[1] = scale.mul_add(d32_21 + d23_12 + d33_22 + d32_43 + d23_34, sads[1]);
            sads[2] = scale.mul_add(d32_31 + d23_22 + d32_33 + d43_42 + d33_34, sads[2]);
            sads[3] = scale.mul_add(d32_41 + d32_23 + d33_42 + d43_52 + d43_34, sads[3]);
            sads[4] = scale.mul_add(d32_12 + d23_03 + d33_13 + d23_43 + d34_14, sads[4]);
            sads[5] = scale.mul_add(d32_22 + d23_13 + d23_33 + d33_43 + d34_24, sads[5]);
            sads[6] = scale.mul_add(d32_42 + d23_33 + d33_43 + d43_53 + d34_44, sads[6]);
            sads[7] = scale.mul_add(d32_52 + d23_43 + d33_53 + d43_63 + d34_54, sads[7]);
            sads[8] = scale.mul_add(d32_23 + d23_14 + d33_24 + d43_34 + d34_25, sads[8]);
            sads[9] = scale.mul_add(d32_33 + d23_24 + d33_34 + d43_44 + d34_35, sads[9]);
            sads[10] = scale.mul_add(d32_43 + d23_34 + d33_44 + d43_54 + d34_45, sads[10]);
            sads[11] = scale.mul_add(d32_34 + d23_25 + d33_35 + d43_45 + d34_36, sads[11]);
        }
        // Compute output based on SADs
        let inv_sigma = sigma * sad_mul;
        let mut w = D::F32Vec::splat(d, 1.0);
        for sad in sads.iter_mut() {
            *sad = sad
                .mul_add(inv_sigma, D::F32Vec::splat(d, 1.0))
                .max(D::F32Vec::splat(d, 0.0));
            w += *sad;
        }
        let inv_w = D::F32Vec::splat(d, 1.0) / w;
        for (input_c, output_c) in input_rows.iter().zip(output_rows.iter_mut()) {
            let mut out = D::F32Vec::load(d, &input_c[3][3 + x..]);
            for (row_idx, col_idx, sad_idx) in [
                (5, 3+x, 11),
                (4, 4+x, 10),
                (4, 3+x, 9),
                (4, 2+x, 8),
                (3, 5+x, 7),
                (3, 4+x, 6),
                (3, 2+x, 5),
                (3, 1+x, 4),
                (2, 4+x, 3),
                (2, 3+x, 2),
                (2, 2+x, 1),
                (1, 3+x, 0),
            ] {
                out = D::F32Vec::load(d, &input_c[row_idx][col_idx..]).mul_add(sads[sad_idx], out);
            }
            (out * inv_w).store(&mut output_c[0][x..]);
        }
    }
});

impl RenderPipelineInOutStage for Epf0Stage {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (3, 3);

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
        epf0_process_row_chunk_dispatch(self, (xpos, ypos), xsize, input_rows, output_rows);
    }
}
