// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::{
    JxlColorEncoding, JxlPrimaries, JxlTransferFunction, JxlWhitePoint, adapt_to_xyz_d50,
    primaries_to_xyz, primaries_to_xyz_d50,
};
use crate::error::Result;
use crate::headers::{FileHeader, OpsinInverseMatrix};
use crate::render::RenderPipelineInPlaceStage;
use crate::render::stages::from_linear;
use crate::util::{Matrix3x3, inv_3x3_matrix, mul_3x3_matrix};
use jxl_simd::{F32SimdVec, simd_function};

const SRGB_LUMINANCES: [f32; 3] = [0.2126, 0.7152, 0.0722];

#[derive(Clone)]
pub struct OutputColorInfo {
    // Luminance of each primary.
    pub luminances: [f32; 3],
    pub intensity_target: f32,
    pub opsin: OpsinInverseMatrix,
    pub tf: from_linear::TransferFunction,
}

#[cfg(test)]
impl Default for OutputColorInfo {
    fn default() -> Self {
        use crate::headers::encodings::Empty;
        Self {
            luminances: SRGB_LUMINANCES,
            intensity_target: 255.0,
            opsin: OpsinInverseMatrix::default(&Empty {}),
            tf: from_linear::TransferFunction::Srgb,
        }
    }
}

impl OutputColorInfo {
    fn opsin_matrix_to_matrix3x3(matrix: [f32; 9]) -> Matrix3x3<f64> {
        [
            [matrix[0] as f64, matrix[1] as f64, matrix[2] as f64],
            [matrix[3] as f64, matrix[4] as f64, matrix[5] as f64],
            [matrix[6] as f64, matrix[7] as f64, matrix[8] as f64],
        ]
    }

    fn matrix3x3_to_opsin_matrix(matrix: Matrix3x3<f64>) -> [f32; 9] {
        [
            matrix[0][0] as f32,
            matrix[0][1] as f32,
            matrix[0][2] as f32,
            matrix[1][0] as f32,
            matrix[1][1] as f32,
            matrix[1][2] as f32,
            matrix[2][0] as f32,
            matrix[2][1] as f32,
            matrix[2][2] as f32,
        ]
    }

    pub fn from_header(header: &FileHeader) -> Result<Self> {
        let srgb_output = OutputColorInfo {
            luminances: SRGB_LUMINANCES,
            intensity_target: header.image_metadata.tone_mapping.intensity_target,
            opsin: header.transform_data.opsin_inverse_matrix.clone(),
            tf: from_linear::TransferFunction::Srgb,
        };
        if header.image_metadata.color_encoding.want_icc {
            return Ok(srgb_output);
        }

        let tf;
        let mut inverse_matrix = Self::opsin_matrix_to_matrix3x3(
            header.transform_data.opsin_inverse_matrix.inverse_matrix,
        );
        let mut luminances = SRGB_LUMINANCES;
        let desired_colorspace =
            JxlColorEncoding::from_internal(&header.image_metadata.color_encoding)?;
        match &desired_colorspace {
            JxlColorEncoding::XYB { .. } => {
                return Ok(srgb_output);
            }
            JxlColorEncoding::RgbColorSpace {
                white_point,
                primaries,
                transfer_function,
                ..
            } => {
                tf = transfer_function;
                if *primaries != JxlPrimaries::SRGB || *white_point != JxlWhitePoint::D65 {
                    let [r, g, b] = JxlPrimaries::SRGB.to_xy_coords();
                    let w = JxlWhitePoint::D65.to_xy_coords();
                    let srgb_to_xyzd50 =
                        primaries_to_xyz_d50(r.0, r.1, g.0, g.1, b.0, b.1, w.0, w.1)?;
                    let [r, g, b] = primaries.to_xy_coords();
                    let w = white_point.to_xy_coords();
                    let original_to_xyz = primaries_to_xyz(r.0, r.1, g.0, g.1, b.0, b.1, w.0, w.1)?;
                    luminances = original_to_xyz[1].map(|lum| lum as f32);
                    let adapt_to_d50 = adapt_to_xyz_d50(w.0, w.1)?;
                    let original_to_xyzd50 = mul_3x3_matrix(&adapt_to_d50, &original_to_xyz);
                    let xyzd50_to_original = inv_3x3_matrix(&original_to_xyzd50)?;
                    let srgb_to_original = mul_3x3_matrix(&xyzd50_to_original, &srgb_to_xyzd50);
                    inverse_matrix = mul_3x3_matrix(&srgb_to_original, &inverse_matrix);
                }
            }

            JxlColorEncoding::GrayscaleColorSpace {
                transfer_function, ..
            } => {
                tf = transfer_function;
                let f64_luminances = luminances.map(|lum| lum as f64);
                let srgb_to_luminance: Matrix3x3<f64> =
                    [f64_luminances, f64_luminances, f64_luminances];
                inverse_matrix = mul_3x3_matrix(&srgb_to_luminance, &inverse_matrix);
            }
        }

        let mut opsin = header.transform_data.opsin_inverse_matrix.clone();
        opsin.inverse_matrix = Self::matrix3x3_to_opsin_matrix(inverse_matrix);
        let intensity_target = header.image_metadata.tone_mapping.intensity_target;
        let from_linear_tf = match tf {
            JxlTransferFunction::PQ => from_linear::TransferFunction::Pq { intensity_target },
            JxlTransferFunction::HLG => from_linear::TransferFunction::Hlg {
                intensity_target,
                luminance_rgb: luminances,
            },
            JxlTransferFunction::BT709 => from_linear::TransferFunction::Bt709,
            JxlTransferFunction::Linear => from_linear::TransferFunction::Gamma(1.0),
            JxlTransferFunction::SRGB => from_linear::TransferFunction::Srgb,
            JxlTransferFunction::DCI => from_linear::TransferFunction::Gamma(2.6_f32.recip()),
            JxlTransferFunction::Gamma(g) => from_linear::TransferFunction::Gamma(*g),
        };
        Ok(OutputColorInfo {
            luminances,
            intensity_target,
            opsin,
            tf: from_linear_tf,
        })
    }
}

/// Convert XYB to linear RGB with appropriate primaries, where 1.0 corresponds to `intensity_target` nits.
pub struct XybStage {
    first_channel: usize,
    output_color_info: OutputColorInfo,
}

impl XybStage {
    pub fn new(first_channel: usize, output_color_info: OutputColorInfo) -> Self {
        Self {
            first_channel,
            output_color_info,
        }
    }
}

impl std::fmt::Display for XybStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let channel = self.first_channel;
        write!(
            f,
            "XYB to linear for channel [{},{},{}]",
            channel,
            channel + 1,
            channel + 2
        )
    }
}

simd_function!(
    xyb_process_dispatch,
    d: D,
    fn xyb_process(
        opsin: &OpsinInverseMatrix,
        intensity_target: f32,
        xsize: usize,
        row_x: &mut [f32],
        row_y: &mut [f32],
        row_b: &mut [f32],
    ) {
        let OpsinInverseMatrix {
            inverse_matrix: mat,
            opsin_biases: bias,
            ..
        } = opsin;
        // TODO(veluca): consider computing the cbrt in advance.
        let bias_cbrt = bias.map(|x| D::F32Vec::splat(d, x.cbrt()));
        let intensity_scale = 255.0 / intensity_target;
        let scaled_bias = bias.map(|x| D::F32Vec::splat(d, x * intensity_scale));
        let mat = mat.map(|x| D::F32Vec::splat(d, x));
        let intensity_scale = D::F32Vec::splat(d, intensity_scale);

        for idx in (0..xsize).step_by(D::F32Vec::LEN) {
            let x = D::F32Vec::load(d, &row_x[idx..]);
            let y = D::F32Vec::load(d, &row_y[idx..]);
            let b = D::F32Vec::load(d, &row_b[idx..]);

            // Mix and apply bias
            let l = y + x - bias_cbrt[0];
            let m = y - x - bias_cbrt[1];
            let s = b - bias_cbrt[2];

            // Apply biased inverse gamma and scale (1.0 corresponds to `intensity_target` nits)
            let l2 = l * l;
            let m2 = m * m;
            let s2 = s * s;
            let scaled_l = l * intensity_scale;
            let scaled_m = m * intensity_scale;
            let scaled_s = s * intensity_scale;
            let l = l2.mul_add(scaled_l, scaled_bias[0]);
            let m = m2.mul_add(scaled_m, scaled_bias[1]);
            let s = s2.mul_add(scaled_s, scaled_bias[2]);

            // Apply opsin inverse matrix (linear LMS to linear sRGB)
            let r = mat[0].mul_add(l, mat[1].mul_add(m, mat[2] * s));
            let g = mat[3].mul_add(l, mat[4].mul_add(m, mat[5] * s));
            let b = mat[6].mul_add(l, mat[7].mul_add(m, mat[8] * s));
            r.store(&mut row_x[idx..]);
            g.store(&mut row_y[idx..]);
            b.store(&mut row_b[idx..]);
        }
    }
);

impl RenderPipelineInPlaceStage for XybStage {
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
        let [row_x, row_y, row_b] = row else {
            panic!(
                "incorrect number of channels; expected 3, found {}",
                row.len()
            );
        };

        xyb_process_dispatch(
            &self.output_color_info.opsin,
            self.output_color_info.intensity_target,
            xsize,
            row_x,
            row_y,
            row_b,
        );
    }
}

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::headers::encodings::Empty;
    use crate::image::Image;
    use crate::render::test::make_and_run_simple_pipeline;
    use crate::util::round_up_size_to_cache_line;
    use crate::util::test::assert_all_almost_abs_eq;
    use jxl_simd::{ScalarDescriptor, SimdDescriptor, test_all_instruction_sets};

    #[test]
    fn consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || XybStage::new(0, OutputColorInfo::default()),
            (500, 500),
            3,
        )
    }

    #[test]
    fn srgb_primaries() -> Result<()> {
        let mut input_x = Image::new((3, 1))?;
        let mut input_y = Image::new((3, 1))?;
        let mut input_b = Image::new((3, 1))?;
        input_x
            .row_mut(0)
            .copy_from_slice(&[0.028100073, -0.015386105, 0.0]);
        input_y
            .row_mut(0)
            .copy_from_slice(&[0.4881882, 0.71478134, 0.2781282]);
        input_b
            .row_mut(0)
            .copy_from_slice(&[0.471659, 0.43707693, 0.66613984]);

        let stage = XybStage::new(0, OutputColorInfo::default());
        let output =
            make_and_run_simple_pipeline(stage, &[input_x, input_y, input_b], (3, 1), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[1.0, 0.0, 0.0], 1e-6);
        assert_all_almost_abs_eq(output[1].row(0), &[0.0, 1.0, 0.0], 1e-6);
        assert_all_almost_abs_eq(output[2].row(0), &[0.0, 0.0, 1.0], 1e-6);

        Ok(())
    }

    fn xyb_process_scalar_equivalent<D: SimdDescriptor>(d: D) {
        let opsin = OpsinInverseMatrix::default(&Empty {});
        arbtest::arbtest(|u| {
            let xsize = u.arbitrary_len::<usize>()?;
            let intensity_target = u.arbitrary::<u8>()? as f32 * 2.0 + 1.0;
            let mut row_x = vec![0.0; round_up_size_to_cache_line::<f32>(xsize)];
            let mut row_y = vec![0.0; round_up_size_to_cache_line::<f32>(xsize)];
            let mut row_b = vec![0.0; round_up_size_to_cache_line::<f32>(xsize)];

            for i in 0..xsize {
                row_x[i] = u.arbitrary::<i16>()? as f32 * (0.07 / i16::MAX as f32);
                row_y[i] = u.arbitrary::<u16>()? as f32 * (1.0 / u16::MAX as f32);
                row_b[i] = u.arbitrary::<u16>()? as f32 * (1.0 / u16::MAX as f32);
            }

            let mut scalar_x = row_x.clone();
            let mut scalar_y = row_y.clone();
            let mut scalar_b = row_b.clone();

            xyb_process(
                d,
                &opsin,
                intensity_target,
                xsize,
                &mut row_x,
                &mut row_y,
                &mut row_b,
            );

            xyb_process(
                ScalarDescriptor::new().unwrap(),
                &opsin,
                intensity_target,
                xsize,
                &mut scalar_x,
                &mut scalar_y,
                &mut scalar_b,
            );

            for i in 0..xsize {
                for (simd, scalar) in [
                    (row_x[i], scalar_x[i]),
                    (row_y[i], scalar_y[i]),
                    (row_b[i], scalar_b[i]),
                ] {
                    let abs = (simd - scalar).abs();
                    let max = simd.abs().max(scalar.abs());
                    let rel = abs / max;
                    assert!(
                        abs < 1e-3 || rel < 1e-3,
                        "simd {simd}, scalar {scalar}, abs {abs:?} rel {rel:?}",
                    );
                }
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(xyb_process_scalar_equivalent);
}
