// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::too_many_arguments)]

use jxl_simd::{F32SimdVec, simd_function};

use super::Epf2Stage;
use super::common::epf_process_row_chunk;
use crate::render::{Channels, ChannelsMut};

const OFFSETS: [(isize, isize); 4] = [(-1, 0), (1, 0), (0, -1), (0, 1)];

simd_function!(
    epf2_process_row_chunk_dispatch,
    d: D,
    pub(super) fn epf2_process_row_chunk_simd(
        stage: &Epf2Stage,
        xpos: usize,
        xsize: usize,
        row_sigma: &[f32],
        sad_mul_storage: &[f32; 24],
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
    ) {
        epf_process_row_chunk::<D, 2, 1, 3, 1, 4>(
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
                let cc = inv_c.load::<_, 0>(d, 0, 0);
                [
                    (inv_c.load::<_, 0>(d, -1, 0) - cc).abs(),
                    (inv_c.load::<_, 0>(d, 1, 0) - cc).abs(),
                    (inv_c.load::<_, 0>(d, 0, -1) - cc).abs(),
                    (inv_c.load::<_, 0>(d, 0, 1) - cc).abs(),
                ]
            },
        );
    }
);
