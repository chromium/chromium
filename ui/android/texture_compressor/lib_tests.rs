// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::simd::prelude::*;

chromium::import! {
    "//ui/android:texture_compressor";
}

use texture_compressor::{interleave_etc1, load_input_block};

#[gtest(TextureCompressorTest, InterleaveEtc1)]
fn test() {
    let input =
        [Simd::splat(0x1234), Simd::splat(0x5678), Simd::splat(0x9ABC), Simd::splat(0xDEF0)];
    let expected = [Simd::splat(0x3412_7856_BC9A_F0DE); 4];
    let result = interleave_etc1(input);
    expect_eq!(result, expected);
}

#[gtest(TextureCompressorTest, LoadInputMirror)]
fn test() {
    // Skip rustfmt to keep this formatted as a 6x2 image.
    #[rustfmt::skip]
    let input = [
        0xFFFFFF, 0xEEEEEE, 0xDDDDDD, 0xCCCCCC, 0xBBBBBB, 0xAAAAAA,
        0x999999, 0x888888, 0x777777, 0x666666, 0x555555, 0x444444,
    ];
    let expected0 = [
        [0xFF, 0xEE, 0xDD, 0xCC],
        [0x99, 0x88, 0x77, 0x66],
        [0x99, 0x88, 0x77, 0x66],
        [0xFF, 0xEE, 0xDD, 0xCC],
    ];
    let expected1 = [
        [0xBB, 0xAA, 0xAA, 0xBB],
        [0x55, 0x44, 0x44, 0x55],
        [0x55, 0x44, 0x44, 0x55],
        [0xBB, 0xAA, 0xAA, 0xBB],
    ];
    let result = load_input_block(&input, 6, 2, 6, 0, 0);
    for ch in 0..3 {
        expect_eq!(result.map(|row| row.map(|x| x[ch].as_array()[0])), expected0);
        expect_eq!(result.map(|row| row.map(|x| x[ch].as_array()[1])), expected1);
    }
}

#[gtest(TextureCompressorTest, LoadInputMirror1x1)]
fn test() {
    let input = [0x999999];
    let expected = 0x99;
    let result = load_input_block(&input, 1, 1, 1, 0, 0);

    for y in 0..4 {
        for x in 0..4 {
            for ch in 0..3 {
                expect_eq!(result[y][x][ch].as_array()[0], expected);
            }
        }
    }
}
