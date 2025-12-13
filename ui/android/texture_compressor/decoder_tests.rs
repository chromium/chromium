// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//ui/android:texture_compressor";
}

use rust_gtest_interop::expect_eq;
use rust_gtest_interop::prelude::*;
use texture_compressor::decoder::apply_modifier;
use texture_compressor::decoder::decode_etc1_block;
use texture_compressor::decoder::morton_interleave;
use texture_compressor::decoder::parse_block_metadata;
use texture_compressor::decoder::read_delta_bits;
use texture_compressor::decoder::scale_4bit_to_8bit;
use texture_compressor::decoder::scale_5bit_to_8bit;
use texture_compressor::decoder::BlockMetadata;

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

#[gtest(TextureCompressorTest, ReadDeltaBitsNegative)]
fn test() {
    expect_eq!(-4, read_delta_bits(0b100));
}

#[gtest(TextureCompressorTest, ReadDeltaBitsNonNegative)]
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

#[gtest(TextureCompressorTest, ApplyModifier)]
fn test() {
    // In this tast case, we use the modifier table: [-8, -2, 2, 8](table codeword
    // is 0b000), and base color: [R, G, B] = [16, 16, 16] Input format is [R,
    // G, B], and Output format is 0xAABBGGRR.
    let base = [16, 16, 16];

    // If negative = true, large = true, pixel_mod is 0b_11.
    // This results in a modifier value of -8.
    // Therefore, the expected components are [16-8, 16-8, 16-8].
    expect_eq!(0x_FF_08_08_08, apply_modifier(base, 0b000, 0b_11));

    // If negative = true, large = false, pixel_mod is 0b_10.
    // This results in a modifier value of -2.
    // Therefore, the expected components are [16-2, 16-2, 16-2].
    expect_eq!(0x_FF_0E_0E_0E, apply_modifier(base, 0b000, 0b_10));

    // If negative = false, large = false, pixel_mod is 0b_00.
    // This results in a modifier value of 2.
    // Therefore, the expected components are [16+2, 16+2, 16+2].
    expect_eq!(0x_FF_12_12_12, apply_modifier(base, 0b000, 0b_00));

    // If negative = false, large = true, pixel_mod is 0b_01.
    // This results in a modifier value of 8.
    // Therefore, the expected components are [16+8, 16+8, 16+8].
    expect_eq!(0x_FF_18_18_18, apply_modifier(base, 0b000, 0b_01));
}

#[gtest(TextureCompressorTest, ApplyModifierClampToMax)]
fn test() {
    let base = [231, 8, 16];
    // If negative = false, large = true, and the modifier table [-29, -9, 9, 29]
    // is used, then the modifier value is +29. So expected components is
    // [231+29, 8+29, 16+29], resulting in the color[255, 37, 45]
    expect_eq!(0b_11111111_00101101_00100101_11111111, apply_modifier(base, 0b010, 0b01));
}

#[gtest(TextureCompressorTest, ApplyModifierClampToMin)]
fn test() {
    let base = [231, 8, 16];
    // If negative = true, large = true, and the modifier table [-29, -9, 9, 29] is
    // used, then the modifier value is -29. So expected components is [231-29,
    // 8-29, 16-29], resulting in the color[202, 0, 0]
    expect_eq!(0b_11111111_00000000_00000000_11001010, apply_modifier(base, 0b010, 0b11));
}

#[gtest(TextureCompressorTest, DecodeETC1BlockFlipFalse)]
fn test() {
    // If flip is false, the block is divided into two 2x4 subblocks side-by-side.
    // basecolor1 fills the left one, and basecolor2 fills the right one.
    // Input (upper 32 bits): 0b_RRRR_RRRR_GGGG_GGGG_BBBB_BBBB_TTT_TTT_D_F
    // basecolor_1: FF0000FF
    // basecolor_2: 0000FFFF
    // offset: 2 (table codeword = 0, pixel index value = 00)
    // Note: Output format is 0xAABBGGRR.
    let input = 0b_1111_0000_0000_0000_0000_1111_000_000_0_0 << 32;

    let expected = [
        [0xff0202ff, 0xff0202ff, 0xffff0202, 0xffff0202],
        [0xff0202ff, 0xff0202ff, 0xffff0202, 0xffff0202],
        [0xff0202ff, 0xff0202ff, 0xffff0202, 0xffff0202],
        [0xff0202ff, 0xff0202ff, 0xffff0202, 0xffff0202],
    ];

    expect_eq!(expected, decode_etc1_block(input));
}

#[gtest(TextureCompressorTest, DecodeETC1BlockFlipTrue)]
fn test() {
    // If flip is true, the block is divided into two 4x2 subblocks on top of each
    // other. basecolor1 fills the top one, and basecolor2 fills the bottom one.
    // `input` is same as above.
    let input = 0b_1111_0000_0000_0000_0000_1111_000_000_0_1 << 32;

    let expected = [
        [0xff0202ff, 0xff0202ff, 0xff0202ff, 0xff0202ff],
        [0xff0202ff, 0xff0202ff, 0xff0202ff, 0xff0202ff],
        [0xffff0202, 0xffff0202, 0xffff0202, 0xffff0202],
        [0xffff0202, 0xffff0202, 0xffff0202, 0xffff0202],
    ];

    expect_eq!(expected, decode_etc1_block(input));
}

#[gtest(TextureCompressorTest, BitInterleaving)]
fn test() {
    expect_eq!(morton_interleave(0b1_00000000_00000001), 0b_11);
    expect_eq!(
        morton_interleave(0b_10000000_00000000_10000000_00000000),
        0b_11000000_00000000_00000000_00000000
    );
    expect_eq!(
        morton_interleave(0b_00000001_00000000_00000001_00000000),
        0b_00000000_00000011_00000000_00000000
    );
}
