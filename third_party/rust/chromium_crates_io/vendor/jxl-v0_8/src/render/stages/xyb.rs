// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, SimdDescriptor, simd_function};

use crate::api::{
    JxlColorEncoding, JxlPrimaries, JxlTransferFunction, JxlWhitePoint, adapt_to_xyz_d50,
    primaries_to_xyz, primaries_to_xyz_d50,
};
use crate::color::tf::{self, TransferFunction};
use crate::error::Result;
use crate::headers::{FileHeader, OpsinInverseMatrix};
use crate::render::{ErasedLocalState, RenderPipelineInPlaceStage};
use crate::util::{Matrix3x3, inv_3x3_matrix, mul_3x3_matrix};

const SRGB_LUMINANCES: [f32; 3] = [0.2126, 0.7152, 0.0722];

#[derive(Clone)]
pub struct OutputColorInfo {
    // Luminance of each primary.
    pub luminances: [f32; 3],
    pub intensity_target: f32,
    pub opsin: OpsinInverseMatrix,
    pub tf: TransferFunction,
}

#[cfg(test)]
impl Default for OutputColorInfo {
    fn default() -> Self {
        use crate::headers::encodings::Empty;
        Self {
            luminances: SRGB_LUMINANCES,
            intensity_target: 255.0,
            opsin: OpsinInverseMatrix::default(&Empty {}),
            tf: TransferFunction::Srgb,
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
            tf: TransferFunction::Srgb,
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
            JxlTransferFunction::PQ => TransferFunction::Pq { intensity_target },
            JxlTransferFunction::HLG => TransferFunction::Hlg {
                intensity_target,
                luminance_rgb: luminances,
            },
            JxlTransferFunction::BT709 => TransferFunction::Bt709,
            JxlTransferFunction::Linear => TransferFunction::Gamma(1.0),
            JxlTransferFunction::SRGB => TransferFunction::Srgb,
            JxlTransferFunction::DCI => TransferFunction::Gamma(2.6_f32.recip()),
            JxlTransferFunction::Gamma(g) => TransferFunction::Gamma(*g),
        };
        Ok(OutputColorInfo {
            luminances,
            intensity_target,
            opsin,
            tf: from_linear_tf,
        })
    }
}

/// Precomputed per-frame constants for `xyb_process`.
struct XybParams {
    mat: [f32; 9],
    bias_cbrt: [f32; 3],
    scaled_bias: [f32; 3],
    intensity_scale: f32,
}

impl XybParams {
    fn new(opsin: &OpsinInverseMatrix, intensity_target: f32) -> Self {
        let intensity_scale = 255.0 / intensity_target;
        Self {
            mat: opsin.inverse_matrix,
            bias_cbrt: opsin.opsin_biases.map(|x| x.cbrt()),
            scaled_bias: opsin.opsin_biases.map(|x| x * intensity_scale),
            intensity_scale,
        }
    }
}

/// Convert XYB to RGB with appropriate primaries and transfer function.
pub struct XybColorConvertStage {
    first_channel: usize,
    params: XybParams,
    tf: TransferFunction,
}

impl XybColorConvertStage {
    pub fn new(first_channel: usize, output_color_info: OutputColorInfo) -> Self {
        Self {
            first_channel,
            params: XybParams::new(&output_color_info.opsin, output_color_info.intensity_target),
            tf: output_color_info.tf,
        }
    }
}

impl std::fmt::Display for XybColorConvertStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let channel = self.first_channel;
        write!(
            f,
            "XYB to RGB ({:?}) for channel [{},{},{}]",
            self.tf,
            channel,
            channel + 1,
            channel + 2
        )
    }
}

#[inline(always)]
fn process_xyb<D: SimdDescriptor>(
    d: D,
    params: &XybParams,
    xsize: usize,
    row_x: &mut [f32],
    row_y: &mut [f32],
    row_b: &mut [f32],
    apply_tf: impl Fn([D::F32Vec; 3]) -> [D::F32Vec; 3],
) {
    let mat = params.mat.map(|x| D::F32Vec::splat(d, x));
    let bias_cbrt = params.bias_cbrt.map(|x| D::F32Vec::splat(d, x));
    let scaled_bias = params.scaled_bias.map(|x| D::F32Vec::splat(d, x));
    let intensity_scale = D::F32Vec::splat(d, params.intensity_scale);

    let vlen = D::F32Vec::LEN;
    let iter = row_x
        .chunks_exact_mut(vlen)
        .zip(row_y.chunks_exact_mut(vlen))
        .zip(row_b.chunks_exact_mut(vlen))
        .take(xsize.div_ceil(vlen));

    for ((xm, ym), bm) in iter {
        let x = D::F32Vec::load(d, xm);
        let y = D::F32Vec::load(d, ym);
        let b = D::F32Vec::load(d, bm);

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

        // Apply opsin inverse matrix (linear LMS to linear RGB)
        let r = mat[0].mul_add(l, mat[1].mul_add(m, mat[2] * s));
        let g = mat[3].mul_add(l, mat[4].mul_add(m, mat[5] * s));
        let b = mat[6].mul_add(l, mat[7].mul_add(m, mat[8] * s));

        let [r, g, b] = apply_tf([r, g, b]);

        r.store(xm);
        g.store(ym);
        b.store(bm);
    }
}

simd_function!(
    xyb_color_convert_process_dispatch,
    d: D,
    fn xyb_color_convert_process(
        params: &XybParams,
        tf: &TransferFunction,
        xsize: usize,
        x: &mut [f32],
        y: &mut [f32],
        b: &mut [f32],
    ) {
        match tf {
            TransferFunction::Srgb => {
                process_xyb(d, params, xsize, x, y, b, {
                    #[inline(always)]
                    |[r, g, b]| {
                        [
                            tf::linear_to_srgb_simd_vec(d, r),
                            tf::linear_to_srgb_simd_vec(d, g),
                            tf::linear_to_srgb_simd_vec(d, b),
                        ]
                    }
                });
            }
            TransferFunction::Bt709 => {
                process_xyb(d, params, xsize, x, y, b, {
                    #[inline(always)]
                    |[r, g, b]| {
                        [
                            tf::linear_to_bt709_simd_vec(d, r),
                            tf::linear_to_bt709_simd_vec(d, g),
                            tf::linear_to_bt709_simd_vec(d, b),
                        ]
                    }
                });
            }
            TransferFunction::Pq { intensity_target } => {
                let y_mult = D::F32Vec::splat(d, intensity_target * 10000f32.recip());
                let threshold = D::F32Vec::splat(d, 1e-4);
                process_xyb(d, params, xsize, x, y, b, {
                    #[inline(always)]
                    |[r, g, b]| {
                        [
                            tf::linear_to_pq_simd_vec(d, y_mult, threshold, r),
                            tf::linear_to_pq_simd_vec(d, y_mult, threshold, g),
                            tf::linear_to_pq_simd_vec(d, y_mult, threshold, b),
                        ]
                    }
                });
            }
            TransferFunction::Gamma(g) if (*g - 1.0).abs() < f32::EPSILON => {
                process_xyb(d, params, xsize, x, y, b, {
                    #[inline(always)]
                    |arr| arr
                });
            }
            TransferFunction::Gamma(g) => {
                let g_vec = D::F32Vec::splat(d, *g);
                process_xyb(d, params, xsize, x, y, b, {
                    #[inline(always)]
                    |[r, g, b]| {
                        [
                            crate::util::fast_powf_simd(d, r.abs(), g_vec).copysign(r),
                            crate::util::fast_powf_simd(d, g.abs(), g_vec).copysign(g),
                            crate::util::fast_powf_simd(d, b.abs(), g_vec).copysign(b),
                        ]
                    }
                });
            }
            TransferFunction::Hlg {
                intensity_target,
                luminance_rgb,
            } => {
                let system_gamma = D::F32Vec::splat(d, tf::hlg_system_gamma(*intensity_target));
                let luminance_rgb = luminance_rgb.map(|v| D::F32Vec::splat(d, v));
                process_xyb(d, params, xsize, x, y, b, {
                    #[inline(always)]
                    |arr| {
                        let [r, g, b] =
                            tf::hlg_display_to_scene_vec(d, system_gamma, luminance_rgb, arr);
                        [
                            tf::scene_to_hlg_vec(d, r),
                            tf::scene_to_hlg_vec(d, g),
                            tf::scene_to_hlg_vec(d, b),
                        ]
                    }
                });
            }
        }
    }
);

impl RenderPipelineInPlaceStage for XybColorConvertStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        (self.first_channel..self.first_channel + 3).contains(&c)
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        let [row_x, row_y, row_b] = row else {
            panic!(
                "incorrect number of channels; expected 3, found {}",
                row.len()
            );
        };

        xyb_color_convert_process_dispatch(&self.params, &self.tf, xsize, row_x, row_y, row_b);
    }
}

#[cfg(test)]
mod test {
    use jxl_simd::{ScalarDescriptor, SimdDescriptor, test_all_instruction_sets};
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::headers::encodings::Empty;
    use crate::image::Image;
    use crate::render::test::make_and_run_simple_pipeline;
    use crate::tests::assert_close;
    use crate::util::round_up_size_to_cache_line;

    #[test]
    fn consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || XybColorConvertStage::new(0, OutputColorInfo::default()),
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

        let info = OutputColorInfo {
            tf: TransferFunction::Gamma(1.0),
            ..Default::default()
        };
        let stage = XybColorConvertStage::new(0, info);
        let output =
            make_and_run_simple_pipeline(stage, &[input_x, input_y, input_b], (3, 1), 0, 256)?;

        assert_close!(all, output[0].row(0), &[1.0, 0.0, 0.0], 1e-6);
        assert_close!(all, output[1].row(0), &[0.0, 1.0, 0.0], 1e-6);
        assert_close!(all, output[2].row(0), &[0.0, 0.0, 1.0], 1e-6);

        Ok(())
    }

    const LUMINANCE_BT2020: [f32; 3] = [0.2627, 0.678, 0.0593];

    fn xyb_for_neutral_linear(linear: f32, intensity_target: f32) -> (f32, f32, f32) {
        let opsin = OpsinInverseMatrix::default(&Empty {});
        let bias = opsin.opsin_biases[0];
        let bias_cbrt = bias.cbrt();
        let intensity_scale = 255.0 / intensity_target;
        let y = (linear / intensity_scale - bias).cbrt() + bias_cbrt;
        (0.0, y, y)
    }

    #[test]
    fn consistency_hlg() -> Result<()> {
        let info = OutputColorInfo {
            intensity_target: 1000.0,
            tf: TransferFunction::Hlg {
                intensity_target: 1000.0,
                luminance_rgb: LUMINANCE_BT2020,
            },
            ..Default::default()
        };
        crate::render::test::test_stage_consistency(
            || XybColorConvertStage::new(0, info.clone()),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_pq() -> Result<()> {
        let info = OutputColorInfo {
            intensity_target: 10000.0,
            tf: TransferFunction::Pq {
                intensity_target: 10000.0,
            },
            ..Default::default()
        };
        crate::render::test::test_stage_consistency(
            || XybColorConvertStage::new(0, info.clone()),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_srgb() -> Result<()> {
        let info = OutputColorInfo {
            tf: TransferFunction::Srgb,
            ..Default::default()
        };
        crate::render::test::test_stage_consistency(
            || XybColorConvertStage::new(0, info.clone()),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_bt709() -> Result<()> {
        let info = OutputColorInfo {
            tf: TransferFunction::Bt709,
            ..Default::default()
        };
        crate::render::test::test_stage_consistency(
            || XybColorConvertStage::new(0, info.clone()),
            (500, 500),
            3,
        )
    }

    #[test]
    fn consistency_gamma22() -> Result<()> {
        let info = OutputColorInfo {
            tf: TransferFunction::Gamma(0.4545455),
            ..Default::default()
        };
        crate::render::test::test_stage_consistency(
            || XybColorConvertStage::new(0, info.clone()),
            (500, 500),
            3,
        )
    }

    #[test]
    fn sdr_white_hlg() -> Result<()> {
        let intensity_target = 1000f32;
        let (_, y, b) = xyb_for_neutral_linear(0.203, intensity_target);
        let input_x = Image::new_with_value((1, 1), 0.0)?;
        let input_y = Image::new_with_value((1, 1), y)?;
        let input_b = Image::new_with_value((1, 1), b)?;

        let info = OutputColorInfo {
            intensity_target,
            tf: TransferFunction::Hlg {
                intensity_target,
                luminance_rgb: LUMINANCE_BT2020,
            },
            ..Default::default()
        };
        let stage = XybColorConvertStage::new(0, info);
        let output =
            make_and_run_simple_pipeline(stage, &[input_x, input_y, input_b], (1, 1), 0, 256)?;

        assert_close!(all, output[0].row(0), &[0.75], 1e-3);
        assert_close!(all, output[1].row(0), &[0.75], 1e-3);
        assert_close!(all, output[2].row(0), &[0.75], 1e-3);

        Ok(())
    }

    #[test]
    fn sdr_white_pq() -> Result<()> {
        let intensity_target = 1000f32;
        let (_, y, b) = xyb_for_neutral_linear(0.203, intensity_target);
        let input_x = Image::new_with_value((1, 1), 0.0)?;
        let input_y = Image::new_with_value((1, 1), y)?;
        let input_b = Image::new_with_value((1, 1), b)?;

        let info = OutputColorInfo {
            intensity_target,
            tf: TransferFunction::Pq { intensity_target },
            ..Default::default()
        };
        let stage = XybColorConvertStage::new(0, info);
        let output =
            make_and_run_simple_pipeline(stage, &[input_x, input_y, input_b], (1, 1), 0, 256)?;

        assert_close!(all, output[0].row(0), &[0.58], 1e-3);
        assert_close!(all, output[1].row(0), &[0.58], 1e-3);
        assert_close!(all, output[2].row(0), &[0.58], 1e-3);

        Ok(())
    }

    fn xyb_process_scalar_equivalent<D: SimdDescriptor>(d: D) {
        let opsin = OpsinInverseMatrix::default(&Empty {});
        arbtest::arbtest(|u| {
            let xsize = u.arbitrary_len::<usize>()?;
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

            // Real-world display luminances: 20 nits (dim mobile in darkness),
            // 255 nits (standard SDR), up to 10,000 nits (BT.2100 peak HDR).
            let intensity_target = u.int_in_range(20..=10000)? as f32;
            let params = XybParams::new(&opsin, intensity_target);
            let tf = TransferFunction::Gamma(1.0);

            xyb_color_convert_process(d, &params, &tf, xsize, &mut row_x, &mut row_y, &mut row_b);
            xyb_color_convert_process(
                ScalarDescriptor::new().unwrap(),
                &params,
                &tf,
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
                        abs < 1e-4 || rel < 1e-3,
                        "simd {simd}, scalar {scalar}, abs {abs:?} rel {rel:?}",
                    );
                }
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(xyb_process_scalar_equivalent);
}
