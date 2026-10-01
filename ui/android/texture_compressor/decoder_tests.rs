// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bits of inputs are grouped in a way that follows their inner structure.
#![allow(clippy::unusual_byte_groupings)]

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
    // base_color2 is base_color1 + delta, but additions are not allowed to
    // under- or overflow. Since the behavior for over- or underflowing
    // values is undefined, but we clamp it to [0,31].
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
    // In this tast case, we use the modifier table: [-8, -2, 2, 8](table
    // codeword is 0b000), and base color: [R, G, B] = [16, 16, 16] Input
    // format is [R, G, B], and Output format is 0xAABBGGRR.
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
    // If negative = false, large = true, and the modifier table [-29, -9, 9,
    // 29] is used, then the modifier value is +29. So expected components
    // is [231+29, 8+29, 16+29], resulting in the color[255, 37, 45]
    expect_eq!(0b_11111111_00101101_00100101_11111111, apply_modifier(base, 0b010, 0b01));
}

#[gtest(TextureCompressorTest, ApplyModifierClampToMin)]
fn test() {
    let base = [231, 8, 16];
    // If negative = true, large = true, and the modifier table [-29, -9, 9, 29]
    // is used, then the modifier value is -29. So expected components is
    // [231-29, 8-29, 16-29], resulting in the color[202, 0, 0]
    expect_eq!(0b_11111111_00000000_00000000_11001010, apply_modifier(base, 0b010, 0b11));
}

#[gtest(TextureCompressorTest, DecodeETC1BlockFlipFalse)]
fn test() {
    // If flip is false, the block is divided into two 2x4 subblocks
    // side-by-side. basecolor1 fills the left one, and basecolor2 fills the
    // right one. Input (upper 32 bits):
    // 0b_RRRR_RRRR_GGGG_GGGG_BBBB_BBBB_TTT_TTT_D_F basecolor_1: FF0000FF
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
    // If flip is true, the block is divided into two 4x2 subblocks on top of
    // each other. basecolor1 fills the top one, and basecolor2 fills the
    // bottom one. `input` is same as above.
    let input = 0b_1111_0000_0000_0000_0000_1111_000_000_0_1 << 32;

    let expected = [
        [0xff0202ff, 0xff0202ff, 0xff0202ff, 0xff0202ff],
        [0xff0202ff, 0xff0202ff, 0xff0202ff, 0xff0202ff],
        [0xffff0202, 0xffff0202, 0xffff0202, 0xffff0202],
        [0xffff0202, 0xffff0202, 0xffff0202, 0xffff0202],
    ];

    expect_eq!(expected, decode_etc1_block(input));
}

#[gtest(TextureCompressorTest, DecodeETC1BlockSubblock2TableIndex)]
fn test() {
    // Subblock 1 uses table_idx_1 = 0b000 (offset +2 for selector 00) and
    // subblock 2 uses table_idx_2 = 0b001 (offset +5 for selector 00).
    // basecolor_1: FF0000FF, basecolor_2: 0000FFFF.
    let input_flip_false = 0b_1111_0000_0000_0000_0000_1111_000_001_0_0 << 32;
    let expected_flip_false = [
        [0xff0202ff, 0xff0202ff, 0xffff0505, 0xffff0505],
        [0xff0202ff, 0xff0202ff, 0xffff0505, 0xffff0505],
        [0xff0202ff, 0xff0202ff, 0xffff0505, 0xffff0505],
        [0xff0202ff, 0xff0202ff, 0xffff0505, 0xffff0505],
    ];
    expect_eq!(expected_flip_false, decode_etc1_block(input_flip_false));

    let input_flip_true = 0b_1111_0000_0000_0000_0000_1111_000_001_0_1 << 32;
    let expected_flip_true = [
        [0xff0202ff, 0xff0202ff, 0xff0202ff, 0xff0202ff],
        [0xff0202ff, 0xff0202ff, 0xff0202ff, 0xff0202ff],
        [0xffff0505, 0xffff0505, 0xffff0505, 0xffff0505],
        [0xffff0505, 0xffff0505, 0xffff0505, 0xffff0505],
    ];
    expect_eq!(expected_flip_true, decode_etc1_block(input_flip_true));
}

#[gtest(TextureCompressorTest, DecodeETC1BlockPixelSelectorOrder)]
fn test() {
    // Verify that all 16 texels (a..p) in column-major order (k = row + col *
    // 4) apply their respective 2-bit selector (msb at bit k + 16, lsb at
    // bit k). Use differential mode with base1 = (16, 16, 16) -> scaled
    // (132, 132, 132), delta = (0, 0, 0), table_idx_1 = 0b000 ([-8, -2, +2,
    // +8]), table_idx_2 = 0b010 ([-29, -9, +9, +29]), flip = false.
    let header: u64 = 0b_10000_000_10000_000_10000_000_000_010_1_0 << 32;
    // Assign selector index (row + col) % 4 to each pixel (row, col):
    //   0 (0b00): +small
    //   1 (0b01): +large
    //   2 (0b10): -small
    //   3 (0b11): -large
    let mut msb: u64 = 0;
    let mut lsb: u64 = 0;
    for col in 0..4 {
        for row in 0..4 {
            let k = row + col * 4;
            let sel = ((row + col) % 4) as u64;
            lsb |= (sel & 1) << k;
            msb |= ((sel >> 1) & 1) << k;
        }
    }
    let block = header | (msb << 16) | lsb;
    let decoded = decode_etc1_block(block);

    let subblock1_deltas = [2_i32, 8, -2, -8];
    let subblock2_deltas = [9_i32, 29, -9, -29];
    for row in 0..4 {
        for col in 0..4 {
            let sel = (row + col) % 4;
            let delta = if col < 2 { subblock1_deltas[sel] } else { subblock2_deltas[sel] };
            let c = (132 + delta) as u32;
            let expected_pixel = 0xFF000000 | (c << 16) | (c << 8) | c;
            expect_eq!(expected_pixel, decoded[row][col]);
        }
    }

    // Also verify all 8 modifier tables (0..=7) on both subblock 1 and subblock
    // 2.
    let expected_small_large: [[i32; 2]; 8] =
        [[2, 8], [5, 17], [9, 29], [13, 42], [18, 60], [24, 80], [33, 106], [47, 183]];
    for t1 in 0u64..8 {
        let t2 = 7 - t1;
        // Base1 = (128, 128, 128) in 5-bit (16 -> 132), Base2 = Base1 - 1 (15
        // -> 123).
        let hdr =
            (0b_10000_111_10000_111_10000_111_u64 << 40) | (t1 << 37) | (t2 << 34) | 0b10_u64 << 32;
        // Selector 0b01 (+large) for all pixels (lsb = 0xFFFF, msb = 0x0000).
        let blk = hdr | 0x0000_FFFF;
        let out = decode_etc1_block(blk);
        let c1 = (132 + expected_small_large[t1 as usize][1]).clamp(0, 255) as u32;
        let c2 = (123 + expected_small_large[t2 as usize][1]).clamp(0, 255) as u32;
        expect_eq!(out[0][0], 0xFF000000 | (c1 << 16) | (c1 << 8) | c1);
        expect_eq!(out[0][2], 0xFF000000 | (c2 << 16) | (c2 << 8) | c2);
    }
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
