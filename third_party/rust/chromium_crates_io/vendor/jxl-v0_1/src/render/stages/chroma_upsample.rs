// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::{Channels, ChannelsMut, RenderPipelineInOutStage};

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
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];
        for i in 0..xsize {
            let scaled_cur = input[0][i + 1] * 0.75;
            let prev = input[0][i];
            let next = input[0][i + 2];
            let left = 0.25 * prev + scaled_cur;
            let right = 0.25 * next + scaled_cur;
            output_rows[0][0][2 * i] = left;
            output_rows[0][0][2 * i + 1] = right;
        }
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
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];
        let output = &mut output_rows[0];
        for i in 0..xsize {
            let scaled_cur = input[1][i] * 0.75;
            let prev = input[0][i];
            let next = input[2][i];
            let up = 0.25 * prev + scaled_cur;
            let down = 0.25 * next + scaled_cur;
            output[0][i] = up;
            output[1][i] = down;
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{error::Result, image::Image, render::test::make_and_run_simple_pipeline};
    use test_log::test;

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
