// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::simd::prelude::*;

chromium::import! {
    "//ui/android:texture_compressor";
}

use texture_compressor::{compress_etc1, decompress_etc1, interleave_etc1, load_input_block};

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

#[allow(clippy::needless_range_loop)] // Range loops seem more readable here.
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

#[gtest(TextureCompressorTest, CompressAndDecompressRoundTrip)]
fn test() {
    // Test a 7x5 non-block-aligned image with padded row strides and distinct
    // R, G, B values.
    let width = 7u32;
    let height = 5u32;
    let src_row_width = 10u32;
    let dst_blocks_row_width = 4u32; // padded beyond width.div_ceil(4) = 2
    let decompressed_row_width = 9u32;

    let mut src = vec![0u32; (src_row_width * height) as usize];
    for y in 0..height {
        for x in 0..width {
            let luma = x * 12 + y * 8;
            let r = 30 + luma;
            let g = 70 + luma;
            let b = 120 + luma;
            src[(y * src_row_width + x) as usize] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
    }

    let num_block_rows = height.div_ceil(4);
    let mut compressed = vec![0u8; (dst_blocks_row_width * num_block_rows * 8) as usize];
    compress_etc1(&src, &mut compressed, width, height, src_row_width, dst_blocks_row_width);

    let sentinel = 0xDEADBEEFu32;
    let mut decompressed = vec![sentinel; (decompressed_row_width * height) as usize];
    decompress_etc1(
        &compressed,
        &mut decompressed,
        width,
        height,
        dst_blocks_row_width,
        decompressed_row_width,
    );

    for y in 0..height {
        for x in 0..decompressed_row_width {
            let actual = decompressed[(y * decompressed_row_width + x) as usize];
            if x >= width {
                // Out-of-bounds stride padding must not be overwritten.
                expect_eq!(actual, sentinel);
            } else {
                let expected = src[(y * src_row_width + x) as usize];
                // Alpha must be 0xFF.
                expect_eq!(actual >> 24, 0xFF);
                for shift in [0, 8, 16] {
                    let expected_ch = ((expected >> shift) & 0xFF) as i32;
                    let actual_ch = ((actual >> shift) & 0xFF) as i32;
                    expect_true!((expected_ch - actual_ch).abs() <= 20);
                }
            }
        }
    }
}
