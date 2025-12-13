// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::RenderPipelineInPlaceStage;

/// Convert YCbCr to RGB
pub struct YcbcrToRgbStage {
    first_channel: usize,
}

impl YcbcrToRgbStage {
    pub fn new(first_channel: usize) -> Self {
        Self { first_channel }
    }
}

impl std::fmt::Display for YcbcrToRgbStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let channel = self.first_channel;
        write!(
            f,
            "YCbCr to RGB for channel [{},{},{}]",
            channel,
            channel + 1,
            channel + 2
        )
    }
}

impl RenderPipelineInPlaceStage for YcbcrToRgbStage {
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
        // pixels are stored in `Cb Y Cr` order to mimic XYB colorspace
        let [row_cb, row_y, row_cr] = row else {
            panic!(
                "incorrect number of channels; expected 3, found {}",
                row.len()
            );
        };

        assert!(xsize <= row_cb.len() && xsize <= row_y.len() && xsize <= row_cr.len());
        for idx in 0..xsize {
            let y = row_y[idx] + 128.0 / 255.0; // shift Y from [-0.5, 0.5] to [0, 1], matching JPEG spec
            let cb = row_cb[idx];
            let cr = row_cr[idx];

            // Full-range BT.601 as defined by JFIF Clause 7:
            // https://www.itu.int/rec/T-REC-T.871-201105-I/en
            row_cb[idx] = cr.mul_add(1.402, y);
            row_y[idx] = cr.mul_add(
                -0.299 * 1.402 / 0.587,
                cb.mul_add(-0.114 * 1.772 / 0.587, y),
            );
            row_cr[idx] = cb.mul_add(1.772, y);
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

    #[test]
    fn consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(|| YcbcrToRgbStage::new(0), (500, 500), 3)
    }

    #[test]
    fn srgb_primaries() -> Result<()> {
        let mut input_y = Image::new((3, 1))?;
        let mut input_cb = Image::new((3, 1))?;
        let mut input_cr = Image::new((3, 1))?;
        input_y
            .row_mut(0)
            .copy_from_slice(&[-0.20296079, 0.08503921, -0.3879608]);
        input_cb
            .row_mut(0)
            .copy_from_slice(&[-0.16873589, -0.3312641, 0.5]);
        input_cr
            .row_mut(0)
            .copy_from_slice(&[0.5, -0.41868758, -0.08131241]);

        let stage = YcbcrToRgbStage::new(0);
        let output =
            make_and_run_simple_pipeline(stage, &[input_cb, input_y, input_cr], (3, 1), 0, 256)?;

        assert_all_almost_abs_eq(output[0].row(0), &[1.0, 0.0, 0.0], 1e-6);
        assert_all_almost_abs_eq(output[1].row(0), &[0.0, 1.0, 0.0], 1e-6);
        assert_all_almost_abs_eq(output[2].row(0), &[0.0, 0.0, 1.0], 1e-6);

        Ok(())
    }
}
