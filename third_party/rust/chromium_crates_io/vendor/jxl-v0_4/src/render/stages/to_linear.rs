// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::color::tf;
use crate::headers::color_encoding::CustomTransferFunction;
use crate::render::RenderPipelineInPlaceStage;
use crate::render::stages::from_linear;
use jxl_simd::{F32SimdVec, simd_function};

/// Convert encoded non-linear color samples to display-referred linear color samples.
#[derive(Debug)]
pub struct ToLinearStage {
    first_channel: usize,
    tf: TransferFunction,
}

impl ToLinearStage {
    pub fn new(first_channel: usize, tf: TransferFunction) -> Self {
        Self { first_channel, tf }
    }

    #[allow(unused, reason = "tirr-c: remove once we use this!")]
    pub fn sdr(first_channel: usize, tf: CustomTransferFunction) -> Self {
        let tf = TransferFunction::try_from(tf).expect("transfer function is not an SDR one");
        Self::new(first_channel, tf)
    }

    #[allow(unused, reason = "tirr-c: remove once we use this!")]
    pub fn pq(first_channel: usize, intensity_target: f32) -> Self {
        let tf = TransferFunction::Pq { intensity_target };
        Self::new(first_channel, tf)
    }

    #[allow(unused, reason = "tirr-c: remove once we use this!")]
    pub fn hlg(first_channel: usize, intensity_target: f32, luminance_rgb: [f32; 3]) -> Self {
        let tf = TransferFunction::Hlg {
            intensity_target,
            luminance_rgb,
        };
        Self::new(first_channel, tf)
    }
}

impl std::fmt::Display for ToLinearStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let channel = self.first_channel;
        write!(
            f,
            "Convert transfer function {:?} to display-referred linear TF for channel [{},{},{}]",
            self.tf,
            channel,
            channel + 1,
            channel + 2
        )
    }
}

simd_function!(
to_linear_process_dispatch,
d: D,
fn to_linear_process(tf: &TransferFunction, xsize: usize, row: &mut [&mut [f32]]) {
    let [row_r, row_g, row_b] = row else {
        panic!(
            "incorrect number of channels; expected 3, found {}",
            row.len()
        );
    };

    match *tf {
        TransferFunction::Bt709 => {
            for row in row {
                tf::bt709_to_linear_simd(d, xsize, row);
            }
        }
        TransferFunction::Srgb => {
            for row in row {
                tf::srgb_to_linear_simd(d, &mut row[..xsize.next_multiple_of(D::F32Vec::LEN)]);
            }
        }
        TransferFunction::Pq { intensity_target } => {
            for row in row {
                tf::pq_to_linear_simd(d, intensity_target, xsize, row);
            }
        }
        TransferFunction::Hlg {
            intensity_target,
            luminance_rgb,
        } => {
            tf::hlg_to_scene(&mut row_r[..xsize]);
            tf::hlg_to_scene(&mut row_g[..xsize]);
            tf::hlg_to_scene(&mut row_b[..xsize]);

            let rows = [
                &mut row_r[..xsize],
                &mut row_g[..xsize],
                &mut row_b[..xsize],
            ];
            tf::hlg_scene_to_display(intensity_target, luminance_rgb, rows);
        }
        TransferFunction::Gamma(g) => {
            for row in row {
                for values in
                    row[..xsize.next_multiple_of(D::F32Vec::LEN)].chunks_exact_mut(D::F32Vec::LEN)
                {
                    let v = D::F32Vec::load(d, values);
                    crate::util::fast_powf_simd(d, v.abs(), D::F32Vec::splat(d, g))
                        .copysign(v)
                        .store(values);
                }
            }
        }
    }
});

impl RenderPipelineInPlaceStage for ToLinearStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        (self.first_channel..self.first_channel + 3).contains(&c)
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        _state: Option<&mut dyn std::any::Any>,
    ) {
        to_linear_process_dispatch(&self.tf, xsize, row)
    }
}

pub type TransferFunction = from_linear::TransferFunction;

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::image::Image;
    use crate::render::test::make_and_run_simple_pipeline;
    use crate::util::test::assert_all_almost_abs_eq;

    const LUMINANCE_BT2020: [f32; 3] = [0.2627, 0.678, 0.0593];

    #[test]
    fn consistency_hlg() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ToLinearStage::hlg(0, 1000f32, LUMINANCE_BT2020),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_pq() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ToLinearStage::pq(0, 10000f32),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_srgb() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ToLinearStage::new(0, TransferFunction::Srgb),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_bt709() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ToLinearStage::new(0, TransferFunction::Bt709),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_gamma22() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ToLinearStage::new(0, TransferFunction::Gamma(0.4545455)),
            (500, 500),
            3,
        )
    }

    #[test]
    fn sdr_white_hlg() -> Result<()> {
        let intensity_target = 1000f32;
        // Reversed version of FromLinear test
        let input_r = Image::new_with_value((1, 1), 0.75)?;
        let input_g = Image::new_with_value((1, 1), 0.75)?;
        let input_b = Image::new_with_value((1, 1), 0.75)?;

        // 75% HLG
        let stage = ToLinearStage::hlg(0, intensity_target, LUMINANCE_BT2020);
        let output =
            make_and_run_simple_pipeline(stage, &[input_r, input_g, input_b], (1, 1), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[0.203], 1e-3);
        assert_all_almost_abs_eq(output[1].row(0), &[0.203], 1e-3);
        assert_all_almost_abs_eq(output[2].row(0), &[0.203], 1e-3);

        Ok(())
    }

    #[test]
    fn sdr_white_pq() -> Result<()> {
        let intensity_target = 1000f32;
        // Reversed version of FromLinear test
        let input_r = Image::new_with_value((1, 1), 0.5807)?;
        let input_g = Image::new_with_value((1, 1), 0.5807)?;
        let input_b = Image::new_with_value((1, 1), 0.5807)?;

        // 58% PQ
        let stage = ToLinearStage::pq(0, intensity_target);
        let output =
            make_and_run_simple_pipeline(stage, &[input_r, input_g, input_b], (1, 1), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[0.203], 1e-3);
        assert_all_almost_abs_eq(output[1].row(0), &[0.203], 1e-3);
        assert_all_almost_abs_eq(output[2].row(0), &[0.203], 1e-3);

        Ok(())
    }
}
