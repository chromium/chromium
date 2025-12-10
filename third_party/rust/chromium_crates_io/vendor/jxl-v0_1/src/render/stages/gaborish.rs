// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::{Channels, ChannelsMut, RenderPipelineInOutStage};
use jxl_simd::{F32SimdVec, simd_function};

/// Apply Gabor-like filter to a channel.
#[derive(Debug)]
pub struct GaborishStage {
    channel: usize,
    weight0: f32,
    weight1: f32,
    weight2: f32,
}

impl GaborishStage {
    pub fn new(channel: usize, weight1: f32, weight2: f32) -> Self {
        let weight_total = 1.0 + weight1 * 4.0 + weight2 * 4.0;
        Self {
            channel,
            weight0: 1.0 / weight_total,
            weight1: weight1 / weight_total,
            weight2: weight2 / weight_total,
        }
    }
}

impl std::fmt::Display for GaborishStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Gaborish filter for channel {}", self.channel)
    }
}

simd_function!(
    gaborish_process_dispatch,
    d: D,
    fn gaborish_process(
        stage: &GaborishStage,
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
    ) {
        let row_out = &mut output_rows[0][0];

        let w0 = D::F32Vec::splat(d, stage.weight0);
        let w1 = D::F32Vec::splat(d, stage.weight1);
        let w2 = D::F32Vec::splat(d, stage.weight2);

        let [row_top, row_center, row_bottom] = input_rows[0] else {
            unreachable!();
        };

        // These asserts help the compiler skip checks in the loop.
        assert_eq!(row_top.len(), row_center.len());
        assert_eq!(row_top.len(), row_bottom.len());

        let num_vec = xsize.div_ceil(D::F32Vec::LEN);

        let len = D::F32Vec::LEN;
        let window_len = len + 2;

        for (((top, center), bottom), out) in row_top
            .windows(window_len)
            .step_by(len)
            .zip(row_center.windows(window_len).step_by(len))
            .zip(row_bottom.windows(window_len).step_by(len))
            .zip(row_out.chunks_exact_mut(D::F32Vec::LEN))
            .take(num_vec)
        {
            let p00 = D::F32Vec::load(d, top);
            let p01 = D::F32Vec::load(d, &top[1..]);
            let p02 = D::F32Vec::load(d, &top[2..]);
            let p10 = D::F32Vec::load(d, center);
            let p11 = D::F32Vec::load(d, &center[1..]);
            let p12 = D::F32Vec::load(d, &center[2..]);
            let p20 = D::F32Vec::load(d, bottom);
            let p21 = D::F32Vec::load(d, &bottom[1..]);
            let p22 = D::F32Vec::load(d, &bottom[2..]);

            let sum = p11 * w0;
            let sum = w1.mul_add(p01 + p10 + p21 + p12, sum);
            let sum = w2.mul_add(p00 + p02 + p20 + p22, sum);
            sum.store(out);
        }
    }
);

impl RenderPipelineInOutStage for GaborishStage {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (1, 1);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        gaborish_process_dispatch(self, xsize, input_rows, output_rows);
    }
}

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::image::Image;
    use crate::render::test::make_and_run_simple_pipeline;
    use crate::util::test::assert_all_almost_abs_eq;

    #[test]
    fn consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || GaborishStage::new(0, 0.115169525, 0.061248592),
            (500, 500),
            1,
        )
    }

    #[test]
    fn checkerboard() -> Result<()> {
        let mut image = Image::new((2, 2))?;
        image.row_mut(0).copy_from_slice(&[0.0, 1.0]);
        image.row_mut(1).copy_from_slice(&[1.0, 0.0]);

        let stage = GaborishStage::new(0, 0.115169525, 0.061248592);
        let output = make_and_run_simple_pipeline(stage, &[image], (2, 2), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[0.20686048, 0.7931395], 1e-6);
        assert_all_almost_abs_eq(output[0].row(1), &[0.7931395, 0.20686048], 1e-6);

        Ok(())
    }
}
