// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, simd_function};

use crate::render::{
    Channels, ChannelsMut, ErasedLocalState, RenderPipelineInOutStage, for_each_chunk,
};

pub struct HorizontalChromaUpsample {
    channel: usize,
}

impl HorizontalChromaUpsample {
    pub fn new(channel: usize) -> HorizontalChromaUpsample {
        HorizontalChromaUpsample { channel }
    }
}

impl std::fmt::Display for HorizontalChromaUpsample {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "chroma upsample of channel {}, horizontally",
            self.channel
        )
    }
}

// SIMD horizontal chroma upsampling
simd_function!(
    hchroma_upsample_simd_dispatch,
    d: D,
    fn hchroma_upsample_simd(
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        xsize: usize,
    ) {
        // Precompute constants
        let c025 = D::F32Vec::splat(d, 0.25);
        let c075 = D::F32Vec::splat(d, 0.75);

        for_each_chunk(
            d,
            xsize,
            input_rows.view::<1, 1, 1>(),
            output_rows.view::<1, 1, 2>(),
            |_x, inv, outv| {
                // Load: prev, cur, next
                let prev_vec = inv.load::<_, 0>(d, 0, -1);
                let cur_vec = inv.load::<_, 0>(d, 0, 0);
                let next_vec = inv.load::<_, 0>(d, 0, 1);

                // Compute: left = 0.25 * prev + 0.75 * cur
                let left = prev_vec.mul_add(c025, cur_vec * c075);

                // Compute: right = 0.25 * next + 0.75 * cur
                let right = next_vec.mul_add(c025, cur_vec * c075);

                // Interleave and store: [left0, right0, left1, right1, ...]
                outv.store_interleaved::<_, 0>(d, 0, [left, right]);
            },
        );
    }
);

impl RenderPipelineInOutStage for HorizontalChromaUpsample {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (1, 0);
    const BORDER: (u8, u8) = (1, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
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
        hchroma_upsample_simd_dispatch(input_rows, output_rows, xsize);
    }
}

pub struct VerticalChromaUpsample {
    channel: usize,
}

impl VerticalChromaUpsample {
    pub fn new(channel: usize) -> VerticalChromaUpsample {
        VerticalChromaUpsample { channel }
    }
}

impl std::fmt::Display for VerticalChromaUpsample {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "chroma upsample of channel {}, vertically", self.channel)
    }
}

// SIMD vertical chroma upsampling
simd_function!(
    vchroma_upsample_simd_dispatch,
    d: D,
    fn vchroma_upsample_simd(
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        xsize: usize,
    ) {
        // Precompute constants
        let c025 = D::F32Vec::splat(d, 0.25);
        let c075 = D::F32Vec::splat(d, 0.75);

        for_each_chunk(
            d,
            xsize,
            input_rows.view::<1, 3, 0>(),
            output_rows.view::<1, 2, 1>(),
            |_x, inv, outv| {
                let prev_vec = inv.load::<_, 0>(d, -1, 0);
                let cur_vec = inv.load::<_, 0>(d, 0, 0);
                let next_vec = inv.load::<_, 0>(d, 1, 0);

                // Compute: up = 0.25 * prev + 0.75 * cur
                let up = prev_vec.mul_add(c025, cur_vec * c075);

                // Compute: down = 0.25 * next + 0.75 * cur
                let down = next_vec.mul_add(c025, cur_vec * c075);

                // Store results
                outv.store::<_, 0>(d, 0, up);
                outv.store::<_, 0>(d, 1, down);
            },
        );
    }
);

impl RenderPipelineInOutStage for VerticalChromaUpsample {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 1);
    const BORDER: (u8, u8) = (0, 1);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
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
        vchroma_upsample_simd_dispatch(input_rows, output_rows, xsize);
    }
}

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::image::Image;
    use crate::render::test::make_and_run_simple_pipeline;

    #[test]
    fn hchr_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || HorizontalChromaUpsample::new(0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn test_hchr() -> Result<()> {
        let mut input = Image::new((3, 1))?;
        input.row_mut(0).copy_from_slice(&[1.0f32, 2.0, 4.0]);
        let stage = HorizontalChromaUpsample::new(0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (6, 1), 0, 256)?;
        assert_eq!(output[0].row(0), [1.0, 1.25, 1.75, 2.5, 3.5, 4.0]);
        Ok(())
    }

    #[test]
    fn vchr_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || VerticalChromaUpsample::new(0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn test_vchr() -> Result<()> {
        let mut input = Image::new((1, 3))?;
        input.row_mut(0)[0] = 1.0f32;
        input.row_mut(1)[0] = 2.0f32;
        input.row_mut(2)[0] = 4.0f32;
        let stage = VerticalChromaUpsample::new(0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (1, 6), 0, 256)?;
        assert_eq!(output[0].row(0)[0], 1.0);
        assert_eq!(output[0].row(1)[0], 1.25);
        assert_eq!(output[0].row(2)[0], 1.75);
        assert_eq!(output[0].row(3)[0], 2.5);
        assert_eq!(output[0].row(4)[0], 3.5);
        assert_eq!(output[0].row(5)[0], 4.0);
        Ok(())
    }
}
