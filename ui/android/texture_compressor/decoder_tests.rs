// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::expect_eq;
use rust_gtest_interop::prelude::*;
use ui_sandroid_ctexture_ucompressor::decoder::parse_block_metadata;
use ui_sandroid_ctexture_ucompressor::decoder::read_delta_bits;
use ui_sandroid_ctexture_ucompressor::decoder::scale_4bit_to_8bit;
use ui_sandroid_ctexture_ucompressor::decoder::scale_5bit_to_8bit;
use ui_sandroid_ctexture_ucompressor::decoder::BlockMetadata;

#[gtest(TextureCompressorTest, CalculateColorIndividualMode)]
fn test() {
    // Test input colors in the individual mode.
    //
    // The upper-32-bit is 0b_RRRR_RRRR_GGGG_GGGG_BBBB_BBBB_TTT_TTT_D_F
    // where T are table indices,
    //       D is the diff bit,
    //       F is the flip bit.
    // The lower-32 bit isn't used.
    // Individual mode is when D is 0.
    let input = 0b_1110_0001_0011_0100_1000_0110_111_110_0_1 << 32;
    let expected = BlockMetadata {
        base: [[238, 51, 136], [17, 68, 102]],
        table_idx_1: 0b111,
        table_idx_2: 0b110,
        flip: true,
    };

    let result = parse_block_metadata(input);
    expect_eq!(expected, result);
}

#[gtest(TextureCompressorTest, CalculateColorDifferentialMode)]
fn test() {
    // Test input colors in the differential mode.
    //
    // The upper-32 bit is 0b_RRRRR_RRR_GGGGG_GGG_BBBBB_BBB_TTT_TTT_D_F.
    // The definition of T, D, and F are same as above.
    // The lower-32 bit isn't used.
    // Differential mode is when D is 1.
    let input = 0b_11100_100_00100_010_00011_000_111_110_1_1 << 32;
    let expected = BlockMetadata {
        base: [[231, 33, 24], [198, 49, 24]],
        table_idx_1: 0b111,
        table_idx_2: 0b110,
        flip: true,
    };

    let result = parse_block_metadata(input);
    expect_eq!(expected, result);
}

#[gtest(TextureCompressorTest, CalculateColorDifferentialModeInvalid)]
fn test() {
    // Test invalid input colors in the differential mode.
    // base_color2 is base_color1 + delta, but additions are not allowed to under-
    // or overflow. Since the behavior for over- or underflowing values is
    // undefined, but we clamp it to [0,31].
    let input =
        0b_00010_100_11111_011_00011_000_111_110_1_1_0000_0000_0000_0000_0000_0000_0000_0000;

    let expected = BlockMetadata {
        base: [[16, 255, 24], [0, 255, 24]],
        table_idx_1: 0b111,
        table_idx_2: 0b110,
        flip: true,
    };
    let result = parse_block_metadata(input);
    expect_eq!(expected, result);
}

#[gtest(TextureCompressorTest, SignExtend3bitNegative)]
fn test() {
    expect_eq!(-4, read_delta_bits(0b100));
}

#[gtest(TextureCompressorTest, SignExtend3bitNonNegative)]
fn test() {
    expect_eq!(0, read_delta_bits(0b000));
    expect_eq!(1, read_delta_bits(0b001));
}

#[gtest(TextureCompressorTest, Expand4bitto8bit)]
fn test() {
    expect_eq!(0b11101110, scale_4bit_to_8bit(0b1110));
}

#[gtest(TextureCompressorTest, Expand5bitto8bit)]
fn test() {
    expect_eq!(0b11100111, scale_5bit_to_8bit(0b11100));
}
