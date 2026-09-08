// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::sync::LazyLock;

// 32x32 blue noise dithering pattern from
// PBBN_32T.png by DZgas (CC BY)
// https://gate-dzgas.com/Perfect-best-blue-noise.html
// Stored as 8-bit rank values p in 0..255 (1,024 bytes).
// Scaled to zero-centered floats in (-0.5, +0.5) via (2.0 * p - 255.0) / 512.0.
// Rows are expanded to 64 (32 + 32) at runtime to allow wrap-around SIMD loads.
const DITHER_TABLE_U8: &[u8; 1024] = include_bytes!("dither_32x32.bin");

pub(crate) static DITHER_TABLE: LazyLock<Box<[[f32; 64]; 32]>> = LazyLock::new(|| {
    let mut table = [[0.0f32; 64]; 32];
    for y in 0..32 {
        for x in 0..32 {
            let p = DITHER_TABLE_U8[y * 32 + x] as f32;
            let v = (2.0 * p - 255.0) / 512.0;
            table[y][x] = v;
            table[y][x + 32] = v;
        }
    }
    Box::new(table)
});
