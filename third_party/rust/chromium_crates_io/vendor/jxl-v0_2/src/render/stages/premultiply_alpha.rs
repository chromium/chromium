// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::RenderPipelineInPlaceStage;
use jxl_simd::{F32SimdVec, simd_function};

/// Premultiply color channels by alpha.
/// This multiplies RGB values by the alpha channel value.
pub struct PremultiplyAlphaStage {
    /// First color channel index (typically 0 for R)
    first_color_channel: usize,
    /// Number of color channels (typically 3 for RGB)
    num_color_channels: usize,
    /// Alpha channel index
    alpha_channel: usize,
}

impl std::fmt::Display for PremultiplyAlphaStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "premultiply alpha stage for color channels {}-{} with alpha channel {}",
            self.first_color_channel,
            self.first_color_channel + self.num_color_channels - 1,
            self.alpha_channel
        )
    }
}

impl PremultiplyAlphaStage {
    pub fn new(
        first_color_channel: usize,
        num_color_channels: usize,
        alpha_channel: usize,
    ) -> Self {
        Self {
            first_color_channel,
            num_color_channels,
            alpha_channel,
        }
    }
}

// SIMD premultiply: color = color * alpha
simd_function!(
    premultiply_rows_simd_dispatch,
    d: D,
    fn premultiply_rows_simd(color_rows: &mut [&mut [f32]], alpha_row: &[f32], xsize: usize) {
        for color_row in color_rows.iter_mut() {
            let iter_color = color_row.chunks_exact_mut(D::F32Vec::LEN);
            let iter_alpha = alpha_row.chunks_exact(D::F32Vec::LEN);
            for (color_chunk, alpha_chunk) in iter_color.zip(iter_alpha).take(xsize.div_ceil(D::F32Vec::LEN)) {
                let color_vec = D::F32Vec::load(d, color_chunk);
                let alpha_vec = D::F32Vec::load(d, alpha_chunk);
                let result = color_vec * alpha_vec;
                result.store(color_chunk);
            }
        }
    }
);

impl RenderPipelineInPlaceStage for PremultiplyAlphaStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        (self.first_color_channel..self.first_color_channel + self.num_color_channels).contains(&c)
            || c == self.alpha_channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        _state: Option<&mut dyn std::any::Any>,
    ) {
        // The row slice contains only the channels we said we use.
        // The last channel is alpha (since alpha_channel > color channels).
        let num_channels = row.len();
        if num_channels < 2 {
            return;
        }

        // Alpha is the last channel in the row slice
        let (color_rows, alpha_row) = row.split_at_mut(num_channels - 1);
        let alpha_row = &alpha_row[0][..];

        premultiply_rows_simd_dispatch(color_rows, alpha_row, xsize);
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
            || PremultiplyAlphaStage::new(0, 3, 3),
            (500, 500),
            4,
        )
    }

    #[test]
    fn premultiply_basic() -> Result<()> {
        let mut input_r = Image::new((4, 1))?;
        let mut input_g = Image::new((4, 1))?;
        let mut input_b = Image::new((4, 1))?;
        let mut input_a = Image::new((4, 1))?;

        // Test values: full color with varying alpha
        input_r.row_mut(0).copy_from_slice(&[1.0, 1.0, 0.5, 0.0]);
        input_g.row_mut(0).copy_from_slice(&[0.5, 0.5, 0.5, 1.0]);
        input_b.row_mut(0).copy_from_slice(&[0.0, 0.25, 1.0, 0.5]);
        input_a.row_mut(0).copy_from_slice(&[1.0, 0.5, 0.0, 0.5]);

        let stage = PremultiplyAlphaStage::new(0, 3, 3);
        let output = make_and_run_simple_pipeline(
            stage,
            &[input_r, input_g, input_b, input_a],
            (4, 1),
            0,
            256,
        )?;

        // Expected: color * alpha
        assert_all_almost_abs_eq(output[0].row(0), &[1.0, 0.5, 0.0, 0.0], 1e-6);
        assert_all_almost_abs_eq(output[1].row(0), &[0.5, 0.25, 0.0, 0.5], 1e-6);
        assert_all_almost_abs_eq(output[2].row(0), &[0.0, 0.125, 0.0, 0.25], 1e-6);
        // Alpha unchanged
        assert_all_almost_abs_eq(output[3].row(0), &[1.0, 0.5, 0.0, 0.5], 1e-6);

        Ok(())
    }
}
