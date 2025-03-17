// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::simd::prelude::*;
use std::simd::Simd;

use crate::Reg;

const fn create_bayer_matrix(num: i32, denom: i32) -> [[i16; 4]; 4] {
    #[rustfmt::skip]
    let base_pattern: [[i32; 4]; 4] = [
        [ 0,  8,  2, 10],
        [12,  4, 14,  6],
        [ 3, 11,  1,  9],
        [15,  7, 13,  5],
    ];
    let mut matrix = [[0i16; 4]; 4];
    // Only while loops are possible, not for, in const fns
    let mut y = 0;
    let mut x = 0;
    while y < 4 {
        while x < 4 {
            let value = (base_pattern[y][x] - 8) * num / denom / 16;
            matrix[y][x] = value as i16;
            x += 1;
        }
        y += 1;
    }
    matrix
}

// RGB565 dither table, strengthened by 1.33x (for better masking of banding).
const BAYER_31: [[i16; 4]; 4] = create_bayer_matrix(255 * 4, 31 * 3);
const BAYER_63: [[i16; 4]; 4] = create_bayer_matrix(255 * 4, 63 * 3);

/// Bayer dithering. The purpose of the dithering is mostly to mask artifacts;
/// the strength of dithering is not really related to the quantization scheme
/// (444 or 555) nor the selector table values.
#[inline]
pub fn dither(data: &[[[Reg; 3]; 4]; 4]) -> [[[Reg; 3]; 4]; 4] {
    let mut out = [[[Reg::default(); 3]; 4]; 4];
    for y in 0..4 {
        for x in 0..4 {
            for (ch, matrix) in [(0, &BAYER_31), (1, &BAYER_63), (2, &BAYER_31)] {
                out[y][x][ch] = (data[y][x][ch] + Simd::splat(matrix[y][x]))
                    .simd_clamp(Simd::splat(0), Simd::splat(255));
            }
        }
    }
    out
}
