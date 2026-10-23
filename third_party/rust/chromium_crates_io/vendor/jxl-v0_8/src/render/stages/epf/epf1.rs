// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::too_many_arguments)]

use jxl_simd::simd_function;

use super::Epf1Stage;
use super::common::{epf_process_row_chunk, plus_sad};
use crate::render::{Channels, ChannelsMut};

const OFFSETS: [(isize, isize); 4] = [(-1, 0), (1, 0), (0, -1), (0, 1)];

simd_function!(
    epf1_process_row_chunk_dispatch,
    d: D,
    pub(super) fn epf1_process_row_chunk_simd(
        stage: &Epf1Stage,
        xpos: usize,
        xsize: usize,
        row_sigma: &[f32],
        sad_mul_storage: &[f32; 24],
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
    ) {
        epf_process_row_chunk::<D, 1, 2, 5, 2, 4>(
            d,
            stage,
            xpos,
            xsize,
            row_sigma,
            sad_mul_storage,
            input_rows,
            output_rows,
            OFFSETS,
            #[inline(always)]
            |inv_c| {
                [
                    plus_sad::<D, 5, 2, -1, 0>(d, inv_c),
                    plus_sad::<D, 5, 2, 1, 0>(d, inv_c),
                    plus_sad::<D, 5, 2, 0, -1>(d, inv_c),
                    plus_sad::<D, 5, 2, 0, 1>(d, inv_c),
                ]
            },
        );
    }
);
