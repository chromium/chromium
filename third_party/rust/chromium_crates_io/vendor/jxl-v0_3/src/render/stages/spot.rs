// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::RenderPipelineInPlaceStage;

/// Render spot color
pub struct SpotColorStage {
    /// Spot color channel index
    spot_c: usize,
    /// Spot color in linear RGBA
    spot_color: [f32; 4],
}

impl std::fmt::Display for SpotColorStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "spot color stage for channel {}", self.spot_c)
    }
}

impl SpotColorStage {
    #[allow(unused, reason = "remove once we actually use this")]
    pub fn new(spot_c_offset: usize, spot_color: [f32; 4]) -> Self {
        Self {
            spot_c: 3 + spot_c_offset,
            spot_color,
        }
    }
}

impl RenderPipelineInPlaceStage for SpotColorStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        c < 3 || c == self.spot_c
    }

    // `row` should only contain color channels and the spot channel.
    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let [row_r, row_g, row_b, row_s] = row else {
            panic!(
                "incorrect number of channels; expected 4, found {}",
                row.len()
            );
        };

        let scale = self.spot_color[3];
        assert!(
            xsize <= row_r.len()
                && xsize <= row_g.len()
                && xsize <= row_b.len()
                && xsize <= row_s.len()
        );
        for idx in 0..xsize {
            let mix = scale * row_s[idx];
            row_r[idx] = mix * self.spot_color[0] + (1.0 - mix) * row_r[idx];
            row_g[idx] = mix * self.spot_color[1] + (1.0 - mix) * row_g[idx];
            row_b[idx] = mix * self.spot_color[2] + (1.0 - mix) * row_b[idx];
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
        crate::render::test::test_stage_consistency(
            || SpotColorStage::new(0, [0.0; 4]),
            (500, 500),
            4,
        )
    }

    #[test]
    fn srgb_primaries() -> Result<()> {
        let mut input_r = Image::new((3, 1))?;
        let mut input_g = Image::new((3, 1))?;
        let mut input_b = Image::new((3, 1))?;
        let mut input_s = Image::new((3, 1))?;
        input_r.row_mut(0).copy_from_slice(&[1.0, 0.0, 0.0]);
        input_g.row_mut(0).copy_from_slice(&[0.0, 1.0, 0.0]);
        input_b.row_mut(0).copy_from_slice(&[0.0, 0.0, 1.0]);
        input_s.row_mut(0).copy_from_slice(&[1.0, 1.0, 1.0]);

        let stage = SpotColorStage::new(0, [0.5; 4]);
        let output = make_and_run_simple_pipeline(
            stage,
            &[input_r, input_g, input_b, input_s],
            (3, 1),
            0,
            256,
        )?;

        assert_all_almost_abs_eq(output[0].row(0), &[0.75, 0.25, 0.25], 1e-6);
        assert_all_almost_abs_eq(output[1].row(0), &[0.25, 0.75, 0.25], 1e-6);
        assert_all_almost_abs_eq(output[2].row(0), &[0.25, 0.25, 0.75], 1e-6);

        Ok(())
    }
}
