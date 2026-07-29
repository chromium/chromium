// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::render::{Channels, ChannelsMut, RenderPipelineInOutStage};
pub struct NearestNeighbourUpsample {
    channel: usize,
}

impl NearestNeighbourUpsample {
    pub fn new(channel: usize) -> NearestNeighbourUpsample {
        NearestNeighbourUpsample { channel }
    }
}

impl std::fmt::Display for NearestNeighbourUpsample {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "2x2 nearest neighbour upsample of channel {}",
            self.channel
        )
    }
}

impl RenderPipelineInOutStage for NearestNeighbourUpsample {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (1, 1);
    const BORDER: (u8, u8) = (0, 0);

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
            output[0][i * 2] = input[0][i];
            output[0][i * 2 + 1] = input[0][i];
            output[1][i * 2] = input[0][i];
            output[1][i * 2 + 1] = input[0][i];
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{error::Result, image::Image, render::test::make_and_run_simple_pipeline};
    use rand::SeedableRng;
    use test_log::test;

    #[test]
    fn nn_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || NearestNeighbourUpsample::new(0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn test_nn() -> Result<()> {
        let image_size = (500, 400);
        let input_size = (image_size.0 / 2, image_size.1 / 2);
        let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
        let input = vec![Image::<f32>::new_random(input_size, &mut rng)?];
        let stage = NearestNeighbourUpsample::new(0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &input, image_size, 0, 256)?;
        assert_eq!(image_size, output[0].size());
        for y in 0..image_size.1 {
            for x in 0..image_size.0 {
                let ix = x / 2;
                let iy = y / 2;
                let i = input[0].row(iy)[ix];
                let o = output[0].row(y)[x];
                assert_eq!(
                    i, o,
                    "mismatch at output position {x}x{y}: {i} vs output {o}"
                );
            }
        }

        Ok(())
    }
}
