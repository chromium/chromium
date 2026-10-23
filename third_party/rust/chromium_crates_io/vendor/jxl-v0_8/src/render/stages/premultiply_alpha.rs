// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, SimdDescriptor, simd_function};

use crate::render::{ErasedLocalState, RenderPipelineInPlaceStage};

// Small value 2^-26 to avoid division by zero when unpremultiplying alpha,
// matching libjxl's kSmallAlpha.
const SMALL_ALPHA: f32 = 1.0 / ((1u32 << 26) as f32);

pub struct PremultiplyAlphaStage {
    first_color_channel: usize,
    num_color_channels: usize,
    alpha_channel: usize,
    unpremultiply: bool,
}

impl std::fmt::Display for PremultiplyAlphaStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let op = if self.unpremultiply {
            "unpremultiply"
        } else {
            "premultiply"
        };
        write!(
            f,
            "{op} alpha stage for color channels {}-{} with alpha channel {}",
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
        unpremultiply: bool,
    ) -> Self {
        Self {
            first_color_channel,
            num_color_channels,
            alpha_channel,
            unpremultiply,
        }
    }
}

#[inline(always)]
fn apply_alpha<D: SimdDescriptor>(
    d: D,
    color_rows: &mut [&mut [f32]],
    alpha_row: &[f32],
    xsize: usize,
    alpha_mul: impl Fn(D::F32Vec) -> D::F32Vec,
) {
    let num_chunks = xsize.div_ceil(D::F32Vec::LEN);
    if let [row_r, row_g, row_b] = color_rows {
        for (((r_chunk, g_chunk), b_chunk), alpha_chunk) in row_r
            .chunks_exact_mut(D::F32Vec::LEN)
            .zip(row_g.chunks_exact_mut(D::F32Vec::LEN))
            .zip(row_b.chunks_exact_mut(D::F32Vec::LEN))
            .zip(alpha_row.chunks_exact(D::F32Vec::LEN))
            .take(num_chunks)
        {
            let mul = alpha_mul(D::F32Vec::load(d, alpha_chunk));
            (D::F32Vec::load(d, r_chunk) * mul).store(r_chunk);
            (D::F32Vec::load(d, g_chunk) * mul).store(g_chunk);
            (D::F32Vec::load(d, b_chunk) * mul).store(b_chunk);
        }
    } else {
        for color_row in color_rows.iter_mut() {
            for (color_chunk, alpha_chunk) in color_row
                .chunks_exact_mut(D::F32Vec::LEN)
                .zip(alpha_row.chunks_exact(D::F32Vec::LEN))
                .take(num_chunks)
            {
                let mul = alpha_mul(D::F32Vec::load(d, alpha_chunk));
                (D::F32Vec::load(d, color_chunk) * mul).store(color_chunk);
            }
        }
    }
}

simd_function!(
    premultiply_rows_simd_dispatch,
    d: D,
    fn premultiply_rows_simd(
        color_rows: &mut [&mut [f32]],
        alpha_row: &[f32],
        xsize: usize,
        unpremultiply: bool,
    ) {
        if unpremultiply {
            let one = D::F32Vec::splat(d, 1.0);
            let small_alpha = D::F32Vec::splat(d, SMALL_ALPHA);
            apply_alpha(
                d,
                color_rows,
                alpha_row,
                xsize,
                #[inline(always)]
                |a| one / a.max(small_alpha),
            );
        } else {
            apply_alpha(
                d,
                color_rows,
                alpha_row,
                xsize,
                #[inline(always)]
                |a| a,
            );
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
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        let num_channels = row.len();
        if num_channels < 2 {
            return;
        }

        let (color_rows, alpha_row) = row.split_at_mut(num_channels - 1);
        let alpha_row = &alpha_row[0][..];

        premultiply_rows_simd_dispatch(color_rows, alpha_row, xsize, self.unpremultiply);
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
        for unpremultiply in [false, true] {
            crate::render::test::test_stage_consistency(
                || PremultiplyAlphaStage::new(0, 3, 3, unpremultiply),
                (500, 500),
                4,
            )?;
            crate::render::test::test_stage_consistency(
                || PremultiplyAlphaStage::new(0, 1, 1, unpremultiply),
                (500, 500),
                2,
            )?;
        }
        Ok(())
    }

    fn assert_alpha_stage(
        unpremultiply: bool,
        inputs: [[f32; 4]; 4],
        expected: [[f32; 4]; 4],
    ) -> Result<()> {
        let mut images = Vec::with_capacity(4);
        for row_vals in &inputs {
            let mut img = Image::new((4, 1))?;
            img.row_mut(0).copy_from_slice(row_vals);
            images.push(img);
        }

        let stage = PremultiplyAlphaStage::new(0, 3, 3, unpremultiply);
        let output = make_and_run_simple_pipeline(stage, &images, (4, 1), 0, 256)?;
        for (out_img, exp_row) in output.iter().zip(expected.iter()) {
            assert_close!(all, out_img.row(0), exp_row, 1e-6);
        }
        Ok(())
    }

    #[test]
    fn premultiply_basic() -> Result<()> {
        assert_alpha_stage(
            false,
            [
                [1.0, 1.0, 0.5, 0.0],
                [0.5, 0.5, 0.5, 1.0],
                [0.0, 0.25, 1.0, 0.5],
                [1.0, 0.5, 0.0, 0.5],
            ],
            [
                [1.0, 0.5, 0.0, 0.0],
                [0.5, 0.25, 0.0, 0.5],
                [0.0, 0.125, 0.0, 0.25],
                [1.0, 0.5, 0.0, 0.5],
            ],
        )
    }

    #[test]
    fn unpremultiply_basic() -> Result<()> {
        assert_alpha_stage(
            true,
            [
                [1.0, 0.5, 0.0, 0.0],
                [0.5, 0.25, 0.0, 0.5],
                [0.0, 0.125, 0.0, 0.25],
                [1.0, 0.5, 0.0, 0.5],
            ],
            [
                [1.0, 1.0, 0.0, 0.0],
                [0.5, 0.5, 0.0, 1.0],
                [0.0, 0.25, 0.0, 0.5],
                [1.0, 0.5, 0.0, 0.5],
            ],
        )
    }
}
