// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::color::tf;
use crate::headers::color_encoding::CustomTransferFunction;
use crate::render::RenderPipelineInPlaceStage;
use jxl_simd::{F32SimdVec, simd_function};

/// Apply transfer function to display-referred linear color samples.
#[derive(Debug)]
pub struct FromLinearStage {
    first_channel: usize,
    tf: TransferFunction,
}

impl FromLinearStage {
    pub fn new(first_channel: usize, tf: TransferFunction) -> Self {
        Self { first_channel, tf }
    }

    pub fn sdr(first_channel: usize, tf: CustomTransferFunction) -> Self {
        let tf = TransferFunction::try_from(tf).expect("transfer function is not an SDR one");
        Self::new(first_channel, tf)
    }

    pub fn pq(first_channel: usize, intensity_target: f32) -> Self {
        let tf = TransferFunction::Pq { intensity_target };
        Self::new(first_channel, tf)
    }

    pub fn hlg(first_channel: usize, intensity_target: f32, luminance_rgb: [f32; 3]) -> Self {
        let tf = TransferFunction::Hlg {
            intensity_target,
            luminance_rgb,
        };
        Self::new(first_channel, tf)
    }
}

impl std::fmt::Display for FromLinearStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let channel = self.first_channel;
        write!(
            f,
            "Apply transfer function {:?} to channel [{},{},{}]",
            self.tf,
            channel,
            channel + 1,
            channel + 2
        )
    }
}

simd_function!(
from_linear_process_dispatch,
d: D,
fn from_linear_process(tf: &TransferFunction, xsize: usize, row: &mut [&mut [f32]]) {
    let [row_r, row_g, row_b] = row else {
        panic!(
            "incorrect number of channels; expected 3, found {}",
            row.len()
        );
    };

    match *tf {
        TransferFunction::Bt709 => {
            for row in row {
                tf::linear_to_bt709_simd(d, &mut row[..xsize.next_multiple_of(D::F32Vec::LEN)]);
            }
        }
        TransferFunction::Srgb => {
            for row in row {
                tf::linear_to_srgb_simd(d, &mut row[..xsize.next_multiple_of(D::F32Vec::LEN)]);
            }
        }
        TransferFunction::Pq { intensity_target } => {
            for row in row {
                tf::linear_to_pq_simd(d, intensity_target, xsize, row);
            }
        }
        TransferFunction::Hlg {
            intensity_target,
            luminance_rgb,
        } => {
            let rows = [
                &mut row_r[..xsize],
                &mut row_g[..xsize],
                &mut row_b[..xsize],
            ];
            tf::hlg_display_to_scene(intensity_target, luminance_rgb, rows);

            tf::scene_to_hlg(&mut row_r[..xsize]);
            tf::scene_to_hlg(&mut row_g[..xsize]);
            tf::scene_to_hlg(&mut row_b[..xsize]);
        }
        TransferFunction::Gamma(g) => {
            for row in row {
                for values in row[..xsize.next_multiple_of(D::F32Vec::LEN)]
                    .chunks_exact_mut(D::F32Vec::LEN)
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

impl RenderPipelineInPlaceStage for FromLinearStage {
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
        from_linear_process_dispatch(&self.tf, xsize, row)
    }
}

#[derive(Clone, Debug)]
pub enum TransferFunction {
    Bt709,
    Srgb,
    Pq {
        intensity_target: f32,
    },
    Hlg {
        intensity_target: f32,
        luminance_rgb: [f32; 3],
    },
    /// Inverse gamma in range `(0, 1]`
    Gamma(f32),
}

impl TransferFunction {
    /// Returns true if this transfer function is linear (i.e., Gamma(1.0)).
    /// When linear, FromLinearStage can be skipped as it would be a no-op.
    pub fn is_linear(&self) -> bool {
        matches!(self, Self::Gamma(g) if (*g - 1.0).abs() < f32::EPSILON)
    }

    /// Create a TransferFunction from a JxlTransferFunction for use in FromLinearStage.
    /// For PQ/HLG, requires intensity_target and luminances from tone mapping info.
    /// Note: JxlTransferFunction::Gamma stores the encoding exponent (e.g., 1/2.2 for gamma 2.2).
    pub fn from_api_tf(
        api_tf: &crate::api::JxlTransferFunction,
        intensity_target: f32,
        luminances: [f32; 3],
    ) -> Self {
        use crate::api::JxlTransferFunction;
        match api_tf {
            JxlTransferFunction::BT709 => Self::Bt709,
            JxlTransferFunction::Linear => Self::Gamma(1.0),
            JxlTransferFunction::SRGB => Self::Srgb,
            JxlTransferFunction::PQ => Self::Pq { intensity_target },
            JxlTransferFunction::DCI => Self::Gamma(2.6_f32.recip()),
            JxlTransferFunction::HLG => Self::Hlg {
                intensity_target,
                luminance_rgb: luminances,
            },
            JxlTransferFunction::Gamma(g) => Self::Gamma(*g),
        }
    }
}

impl TryFrom<CustomTransferFunction> for TransferFunction {
    type Error = ();

    fn try_from(ctf: CustomTransferFunction) -> Result<Self, ()> {
        use crate::headers::color_encoding::TransferFunction;

        if ctf.have_gamma {
            Ok(Self::Gamma(ctf.gamma()))
        } else {
            match ctf.transfer_function {
                TransferFunction::BT709 => Ok(Self::Bt709),
                TransferFunction::Unknown => Err(()),
                TransferFunction::Linear => Ok(Self::Gamma(1.0)),
                TransferFunction::SRGB => Ok(Self::Srgb),
                TransferFunction::PQ => Err(()),
                TransferFunction::DCI => Ok(Self::Gamma(2.6_f32.recip())),
                TransferFunction::HLG => Err(()),
            }
        }
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

    const LUMINANCE_BT2020: [f32; 3] = [0.2627, 0.678, 0.0593];

    #[test]
    fn consistency_hlg() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || FromLinearStage::hlg(0, 1000f32, LUMINANCE_BT2020),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_pq() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || FromLinearStage::pq(0, 10000f32),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_srgb() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || FromLinearStage::new(0, TransferFunction::Srgb),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_bt709() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || FromLinearStage::new(0, TransferFunction::Bt709),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_gamma22() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || FromLinearStage::new(0, TransferFunction::Gamma(0.4545455)),
            (500, 500),
            3,
        )
    }

    #[test]
    fn sdr_white_hlg() -> Result<()> {
        let intensity_target = 1000f32;
        let input_r = Image::new_with_value((1, 1), 0.203)?;
        let input_g = Image::new_with_value((1, 1), 0.203)?;
        let input_b = Image::new_with_value((1, 1), 0.203)?;

        // 75% HLG
        let stage = FromLinearStage::hlg(0, intensity_target, LUMINANCE_BT2020);
        let output =
            make_and_run_simple_pipeline(stage, &[input_r, input_g, input_b], (1, 1), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[0.75], 1e-3);
        assert_all_almost_abs_eq(output[1].row(0), &[0.75], 1e-3);
        assert_all_almost_abs_eq(output[2].row(0), &[0.75], 1e-3);

        Ok(())
    }

    #[test]
    fn sdr_white_pq() -> Result<()> {
        let intensity_target = 1000f32;
        let input_r = Image::new_with_value((1, 1), 0.203)?;
        let input_g = Image::new_with_value((1, 1), 0.203)?;
        let input_b = Image::new_with_value((1, 1), 0.203)?;

        // 58% PQ
        let stage = FromLinearStage::pq(0, intensity_target);
        let output =
            make_and_run_simple_pipeline(stage, &[input_r, input_g, input_b], (1, 1), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[0.58], 1e-3);
        assert_all_almost_abs_eq(output[1].row(0), &[0.58], 1e-3);
        assert_all_almost_abs_eq(output[2].row(0), &[0.58], 1e-3);

        Ok(())
    }
}
