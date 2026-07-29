// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//! XYB color space constants (matching libjxl)
//!
//! Allow excessive precision as these constants are copied verbatim from libjxl for compatibility

#![allow(clippy::excessive_precision)]

pub const OPSIN_ABSORBANCE_BIAS: f32 = 0.0037930732552754493;

#[allow(dead_code)]
pub const NEG_OPSIN_ABSORBANCE_BIAS_RGB: [f32; 3] = [
    -OPSIN_ABSORBANCE_BIAS,
    -OPSIN_ABSORBANCE_BIAS,
    -OPSIN_ABSORBANCE_BIAS,
];

const SCALED_XYB_OFFSET: [f32; 3] = [0.015386134, 0.0, 0.27770459];
const SCALED_XYB_SCALE: [f32; 3] = [22.995788804, 1.183000077, 1.502141333];

const fn reciprocal_sum(r1: f32, r2: f32) -> f32 {
    (r1 * r2) / (r1 + r2)
}

pub const XYB_OFFSET: [f32; 3] = [
    SCALED_XYB_OFFSET[0] + SCALED_XYB_OFFSET[1],
    SCALED_XYB_OFFSET[1] - SCALED_XYB_OFFSET[0] + (1.0 / SCALED_XYB_SCALE[0]),
    SCALED_XYB_OFFSET[1] + SCALED_XYB_OFFSET[2],
];

pub const fn xyb_scale() -> [f32; 3] {
    [
        reciprocal_sum(SCALED_XYB_SCALE[0], SCALED_XYB_SCALE[1]),
        reciprocal_sum(SCALED_XYB_SCALE[0], SCALED_XYB_SCALE[1]),
        reciprocal_sum(SCALED_XYB_SCALE[1], SCALED_XYB_SCALE[2]),
    ]
}

const fn xyb_corner(x: usize, y: usize, b: usize, idx: usize) -> f32 {
    let val = match idx {
        0 => x,
        1 => y,
        _ => b,
    };
    (val as f32 / SCALED_XYB_SCALE[idx]) - SCALED_XYB_OFFSET[idx]
}

const fn scaled_a2b_corner(x: usize, y: usize, b: usize, idx: usize) -> f32 {
    match idx {
        0 => xyb_corner(x, y, b, 1) + xyb_corner(x, y, b, 0),
        1 => xyb_corner(x, y, b, 1) - xyb_corner(x, y, b, 0),
        _ => xyb_corner(x, y, b, 2) + xyb_corner(x, y, b, 1),
    }
}

const fn unscaled_a2b_corner(x: usize, y: usize, b: usize) -> [f32; 3] {
    let scale = xyb_scale();
    [
        (scaled_a2b_corner(x, y, b, 0) + XYB_OFFSET[0]) * scale[0],
        (scaled_a2b_corner(x, y, b, 1) + XYB_OFFSET[1]) * scale[1],
        (scaled_a2b_corner(x, y, b, 2) + XYB_OFFSET[2]) * scale[2],
    ]
}

/// Compute the 2x2x2 CLUT cube for XYB to linear RGB conversion.
pub const fn unscaled_a2b_cube_full() -> [[[[f32; 3]; 2]; 2]; 2] {
    [
        [
            [unscaled_a2b_corner(0, 0, 0), unscaled_a2b_corner(0, 0, 1)],
            [unscaled_a2b_corner(0, 1, 0), unscaled_a2b_corner(0, 1, 1)],
        ],
        [
            [unscaled_a2b_corner(1, 0, 0), unscaled_a2b_corner(1, 0, 1)],
            [unscaled_a2b_corner(1, 1, 0), unscaled_a2b_corner(1, 1, 1)],
        ],
    ]
}

/// Matrix for XYB ICC profile (from libjxl).
pub const XYB_ICC_MATRIX: [f64; 9] = [
    1.5170095, -1.1065225, 0.071623, -0.050022, 0.5683655, -0.018344, -1.387676, 1.1145555,
    0.6857255,
];
