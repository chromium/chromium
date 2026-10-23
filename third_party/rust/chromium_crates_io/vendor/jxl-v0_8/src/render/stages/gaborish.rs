// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, simd_function};

use crate::render::{
    Channels, ChannelsMut, ErasedLocalState, RenderPipelineInOutStage, for_each_chunk,
};

/// Apply Gabor-like filter to the 3 XYB channels.
#[derive(Debug)]
pub struct GaborishStage {
    weight0: [f32; 3],
    weight1: [f32; 3],
    weight2: [f32; 3],
}

impl GaborishStage {
    pub fn new(weight1: [f32; 3], weight2: [f32; 3]) -> Self {
        let mut w0 = [0.0; 3];
        let mut w1 = [0.0; 3];
        let mut w2 = [0.0; 3];
        for i in 0..3 {
            let weight_total = 1.0 + weight1[i] * 4.0 + weight2[i] * 4.0;
            w0[i] = 1.0 / weight_total;
            w1[i] = weight1[i] / weight_total;
            w2[i] = weight2[i] / weight_total;
        }
        Self {
            weight0: w0,
            weight1: w1,
            weight2: w2,
        }
    }
}

impl std::fmt::Display for GaborishStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Gaborish filter")
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
        let w0 = stage.weight0.map(|w| D::F32Vec::splat(d, w));
        let w1 = stage.weight1.map(|w| D::F32Vec::splat(d, w));
        let w2 = stage.weight2.map(|w| D::F32Vec::splat(d, w));

        for_each_chunk(
            d,
            xsize,
            input_rows.view::<3, 3, 1>(),
            output_rows.view::<3, 1, 1>(),
            |_x, inv, outv| {
                macro_rules! step {
                    ($ch:literal) => {{
                        let p00 = inv.load::<_, $ch>(d, -1, -1);
                        let p01 = inv.load::<_, $ch>(d, -1, 0);
                        let p02 = inv.load::<_, $ch>(d, -1, 1);
                        let p10 = inv.load::<_, $ch>(d, 0, -1);
                        let p11 = inv.load::<_, $ch>(d, 0, 0);
                        let p12 = inv.load::<_, $ch>(d, 0, 1);
                        let p20 = inv.load::<_, $ch>(d, 1, -1);
                        let p21 = inv.load::<_, $ch>(d, 1, 0);
                        let p22 = inv.load::<_, $ch>(d, 1, 1);

                        let sum = p11 * w0[$ch];
                        let sum = w1[$ch].mul_add(p01 + p10 + p21 + p12, sum);
                        let sum = w2[$ch].mul_add(p00 + p02 + p20 + p22, sum);
                        outv.store::<_, $ch>(d, 0, sum);
                    }};
                }
                step!(0);
                step!(1);
                step!(2);
            },
        );
    }
);

impl RenderPipelineInOutStage for GaborishStage {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (1, 1);

    fn uses_channel(&self, c: usize) -> bool {
        c < 3
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
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
    use crate::tests::assert_close;

    #[test]
    fn consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || GaborishStage::new([0.115169525; 3], [0.061248592; 3]),
            (500, 500),
            3,
        )
    }

    #[test]
    fn checkerboard() -> Result<()> {
        let mut images = [
            Image::new((2, 2))?,
            Image::new((2, 2))?,
            Image::new((2, 2))?,
        ];
        for image in &mut images {
            image.row_mut(0).copy_from_slice(&[0.0, 1.0]);
            image.row_mut(1).copy_from_slice(&[1.0, 0.0]);
        }

        let stage = GaborishStage::new([0.115169525; 3], [0.061248592; 3]);
        let output = make_and_run_simple_pipeline(stage, &images, (2, 2), 0, 256)?;

        for out_ch in output.iter().take(3) {
            assert_close!(all, out_ch.row(0), &[0.20686048, 0.7931395], 1e-6);
            assert_close!(all, out_ch.row(1), &[0.7931395, 0.20686048], 1e-6);
        }

        Ok(())
    }
}
