// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::simd::prelude::*;

chromium::import! {
    "//ui/android:texture_compressor";
}

use texture_compressor::quant::{fast_div_255_round, prepare_averages, quantize_averages};

#[gtest(TextureCompressorTest, FastDiv255)]
fn test() {
    let multipliers = [15, 31];
    for m in multipliers {
        for i in 0..255 {
            let expected = (m * i + 128) / 255;
            expect_eq!(fast_div_255_round(Simd::splat(m * i)), Simd::splat(expected));
        }
    }
}

#[gtest(TextureCompressorTest, Averages)]
fn test() {
    let values = [[0, 255, 0, 255]; 4].map(|row| row.map(|x| [Simd::splat(x); 3]));
    let result = prepare_averages(&values);
    for i in 0..4 {
        // NB: Each subblock is 8 pixels.
        expect_eq!(result[i].sum, [Simd::splat(255 * 2 * 2); 3]);
        expect_eq!(result[i].avg, [Simd::splat(128); 3]);
    }
}

#[gtest(TextureCompressorTest, AveragesMax)]
// Check that the maximum value doesn't overflow.
fn test() {
    let values = [[255; 4]; 4].map(|row| row.map(|x| [Simd::splat(x); 3]));
    let result = prepare_averages(&values);
    for i in 0..4 {
        // NB: Each subblock is 8 pixels.
        expect_eq!(result[i].sum, [Simd::splat(255 * 4 * 2); 3]);
        expect_eq!(result[i].avg, [Simd::splat(255); 3]);
    }
}

#[gtest(TextureCompressorTest, QuantDiff)]
fn test() {
    // Test input colors that are perfectly quantizable in diff mode.
    // We don't strictly require diff mode to be selected however, because it is
    // possible for a value to be perfectly quantizable in both modes.
    for c_quantized in 0..31 {
        let c_scaled = (c_quantized << 3) | (c_quantized >> 2);
        let values = [[c_scaled; 4]; 4].map(|row| row.map(|x| [Simd::splat(x); 3]));
        let result = quantize_averages(&values);
        expect_eq!(result.scaled0, [Simd::splat(c_scaled); 3]);
        expect_eq!(result.scaled1, [Simd::splat(c_scaled); 3]);
    }
}

#[gtest(TextureCompressorTest, QuantIndiv)]
fn test() {
    let c1 = [0, 0, 0].map(|x| Simd::splat(x));
    let c2 = [255, 255, 255].map(|x| Simd::splat(x));
    let values = [[c1, c1, c2, c2]; 4];
    let result = quantize_averages(&values);
    expect_eq!(result.scaled0, [Simd::splat(0); 3]);
    expect_eq!(result.scaled1, [Simd::splat(255); 3]);

    let flip = (result.lo & Simd::splat(0x1)).simd_eq(Simd::splat(0x1));
    expect_false!(flip.any());
}
