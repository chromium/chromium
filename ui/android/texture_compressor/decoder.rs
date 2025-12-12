// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::selectors::TABLES;

#[derive(Debug, PartialEq, Eq)]
pub struct BlockMetadata {
    pub base: [[i32; 3]; 2],
    pub table_idx_1: u32,
    pub table_idx_2: u32,
    pub flip: bool,
}

// Extract base color in the individual mode.
// Return [base_color1, base_color2]
// See "Khronos Data Format Specification v1.4.0", Section 21, "In the
// 'individual mode'..."
fn parse_individual_base(patch: u64) -> [[i32; 3]; 2] {
    // The bit layout for individual mode is:
    // 63..60: base color 1 R
    // 59..56: base color 2 R
    // 55..52: base color 1 G
    // 51..48: base color 2 G
    // 49..44: base color 1 B
    // 43..40: base color 2 B
    // The remaining bits are handled in 'parse_block_metadata'

    let base = [0, 1].map(|j| {
        // i = 0,1,2 maps to [R, G, B] respectively
        [0, 1, 2].map(|i| {
            let shift = 60 - 4 * (i * 2 + j);
            let read_4bit = ((patch >> shift) & 0b1111) as i32;
            scale_4bit_to_8bit(read_4bit)
        })
    });
    return base;
}

// Expands a 4-bit color component to an 8-bit component
// according to the specification.
//
// We want to scale value in [0, 15] to value in [0, 255].
// The ideal scaling  is (input / 15) * 255 = input * 17.
// The bitwise operation of this function:
//      (input << 4) | input
// is equivalent to:
//      (input * 2^4) + input = input * 17.
// Thus, the two results are identical.
//
// In addition, the operation of this function is hardware-friendly and
// correctly maps the endpoints:
// - Minimum: 0b_0000(0) becomes 0b_00000000(0).
// - Maximum: 0b_1111(15) becomes 0b_11111111(255).
pub fn scale_4bit_to_8bit(input: i32) -> i32 {
    assert!(input >= 0 && input < 16);
    input << 4 | input
}

// Extract base color in the differential mode.
// Return [base_color1, base_color2], where base_color2 is base_color1 + delta.
// See "Khronos Data Format Specification v1.4.0", Section 21, "In the
// 'differential mode'..."
fn parse_differential_base(patch: u64) -> [[i32; 3]; 2] {
    // The bit layout for differential mode is
    // 63..59: base color R
    // 58..56: color delta R
    // 55..51: base color G
    // 50..48: color delta G
    // 47..43: base color B
    // 42..40: color delta B
    // The remaining bits are handled in 'parse_block_metadata'

    // i = 0,1,2 maps to [R, G, B] respectively
    let base_1_5bit = [0, 1, 2].map(|i| {
        let shift = 59 - 8 * i;
        ((patch >> shift) & 0b11111) as i32
    });

    let base_1 = base_1_5bit.map(|i| scale_5bit_to_8bit(i));

    let base_2 = [0, 1, 2].map(|i| {
        let shift = 56 - 8 * i;
        let delta = read_delta_bits(((patch >> shift) & 0b111) as u32);
        let base_2_5bit = (base_1_5bit[i] + delta).clamp(0, 31);
        scale_5bit_to_8bit(base_2_5bit)
    });
    [base_1, base_2]
}

// Parses a 3-bit binary string as a two's complement signed integer.
// The range covered is [-4,3].
pub fn read_delta_bits(input: u32) -> i32 {
    assert!(input < 8);
    if input >= 4 {
        (input as i32) - 8
    } else {
        input as i32
    }
}

// Expands a 5-bit color component to an 8-bit component
// according to the specification.
//
// We want to scale value in [0, 31] to value in [0, 255].
// The ideal scaling  is (input / 31) * 255 ~ input * 8.22580...
// The bitwise operation of this function:
//      (input <<3) | (input >> 2)
// is equivalent to:
//      (input * 2^3) + floor(input / 2^2) ~ input * 8.25.
// The scaling value (8.25) approximates the ideal scaling (8.225...).
//
// In addition, the operation of this function is hardware-friendly and
// correctly maps the endpoints:
// - Minimum: 0b_00000(0) becomes 0b_00000000(0).
// - Maximum: 0b_11111(31) becomes 0b_11111111(255).
pub fn scale_5bit_to_8bit(input: i32) -> i32 {
    assert!(input >= 0 && input < 32);
    input << 3 | input >> 2
}

// Extract block meta-data from the upper half of 64 bits.
// (See "Khronos Data Format Specification v1.4.0", Section21, Table 138)
// There are 2 ways to extract the base colors.
// One is 'individual' mode (See Table 138 part a), and the other is
// 'differential' mode (See Table 138 part b).
pub fn parse_block_metadata(etc1_block: u64) -> BlockMetadata {
    let diff0 = ((etc1_block >> 33) & 0b1) == 0;
    let base =
        if diff0 { parse_individual_base(etc1_block) } else { parse_differential_base(etc1_block) };
    BlockMetadata {
        base,
        table_idx_1: ((etc1_block >> 37) & 0b111) as u32,
        table_idx_2: ((etc1_block >> 34) & 0b111) as u32,
        flip: ((etc1_block >> 32) & 0b1) == 1,
    }
}

// The spec specifies the modifier table in the order of -large, -small, +small,
// +large. However, in order to look up with the negative and large bit as-is,
// table needs to be in the order of +small, +large, -small, -large.
// This creates a shuffled table to allow such access.
pub const OPTIMIZED_MODIFIER_TABLE: [[i32; 4]; 8] = generate_modifier_table();
const fn generate_modifier_table() -> [[i32; 4]; 8] {
    let mut table = [[0; 4]; 8];
    let mut i = 0;
    // The modifier table provides pairs of [small, large] for each table index.
    // The two bits extracted for a pixel select one of four possible deltas.
    //
    // Selector bits logic: index = (negative << 1) | large
    // | Index | Bits (Neg, Large) | Sign | Magnitude | Value  |
    // |-------|-------------------|------|-----------|--------|
    // |   0   |       0, 0        |  +   |   small   | +small |
    // |   1   |       0, 1        |  +   |   large   | +large |
    // |   2   |       1, 0        |  -   |   small   | -small |
    // |   3   |       1, 1        |  -   |   large   | -large |

    while i < 8 {
        let small = TABLES[i][0] as i32;
        let large = TABLES[i][1] as i32;
        table[i][0] = small;
        table[i][1] = large;
        table[i][2] = -small;
        table[i][3] = -large;
        i += 1;
    }
    table
}

// Applies the modifier (luminance change) to a single pixel and calculates the
// RGBA value.
// - `base`: should be the base color as an [R, G, B].
// - `subblock_mod`: should be the index for the modifier table. (range 0-7).
// - `pixel_mod': should be the index for the specific delta value. (range 0-3).
// The return value is a u32 packed in the 0xAABBGGRR layout.
pub fn apply_modifier(base: [i32; 3], subblock_mod: u32, pixel_mod: u32) -> u32 {
    let modifier_delta = OPTIMIZED_MODIFIER_TABLE[subblock_mod as usize][pixel_mod as usize];

    let mut output_rgba: u32 = 0xFF000000; // Set alpha channel to 255

    for i in 0..3 {
        let channel_color = (base[i] + modifier_delta).clamp(0, 255) as u32;
        // Pack the R,G,B values into a single 32-bit
        let shift = i * 8;
        output_rgba |= channel_color << shift;
    }
    return output_rgba;
}

// Expands the lower 16 bits into 32 bits by inserting a 0 bit between each bit.
// Input: 0b_abcd..op
// Output: 0b_0a0b0c..0o0p
fn morton_spread(x: u32) -> u32 {
    let x = x & 0x0000FFFF;
    let x = (x | (x << 8)) & 0x00FF00FF;
    let x = (x | (x << 4)) & 0x0F0F0F0F;
    let x = (x | (x << 2)) & 0x33333333;
    (x | (x << 1)) & 0x55555555
}

// Interleaves the upper and lower 16-bit halves into a single 32-bit sequence.
// To calculate the lookup index for each pixel, we need to combine 1 bit from
// the upper half and 1 bit from the lower half.
// Comparison:
//  - Naive: 32 shifts + 32 masks (total: 64 ops).
//  - Interleaved: 13 ops (shuffle) * 2 + 16 shifts + 16 masks (total: 58 ops).
//  -> This results in fewer operations and faster execution.
// This function is for performing the bit shuffling.
// Input: 0b_ab..op_AB..OP
// Output: 0b_aAbB..oOpP
pub fn morton_interleave(input_etc1: u32) -> u32 {
    let upper = morton_spread(input_etc1 >> 16);
    let lower = morton_spread(input_etc1);
    (upper << 1) | lower
}

// Extracts the 2-bit pixel modifier index for the specified coordinates.
fn get_pixel_modifier_index(row: usize, col: usize, morton_interleaved: u32) -> u32 {
    let shift = (row + col * 4) * 2;
    (morton_interleaved >> shift) & 0b11
}

pub fn decode_etc1_block(input_etc1: u64) -> [[u32; 4]; 4] {
    let metadata = parse_block_metadata(input_etc1);
    assert!(metadata.table_idx_1 < 8);
    assert!(metadata.table_idx_2 < 8);

    let morton_interleaved = morton_interleave(input_etc1 as u32);

    // The output layout is
    // [[ a e i m ],
    //  [ b f j n ],
    //  [ c g k o ],
    //  [ d h l p ]].
    let mut output = [[0 as u32; 4]; 4];

    // In ETC1, each block can be divided into two subblocks, either vertically or
    // horizontally depending on the flip bit. Regardless of the direction, the
    // loop is structured so that pixels in the first subblock is processed first,
    // then the second. Within each loop, the same base color and table index is
    // used. This is faster than iterating the block in pixel order, which requires
    // switching between the base colors inside the loop.
    if metadata.flip {
        // When flip bit = 1, the block is divided into two 4×2 subblocks.
        // - Pixels a,e,i,m, b,f,j,n (rows 0, 1) use base color 1.
        // - Pixels c,g,k,o, d,h,l,p (rows 2, 3) use base color 2.

        for row in 0..2 {
            for col in 0..4 {
                // TODO: base[0] actually means base color 1, and base[1] means base color 2.
                //       Refactor this section for clarity.
                output[row][col] = apply_modifier(
                    metadata.base[0],
                    metadata.table_idx_1,
                    get_pixel_modifier_index(row, col, morton_interleaved),
                );
            }
        }
        for row in 2..4 {
            for col in 0..4 {
                output[row][col] = apply_modifier(
                    metadata.base[1],
                    metadata.table_idx_1,
                    get_pixel_modifier_index(row, col, morton_interleaved),
                )
            }
        }
    } else {
        // When flip bit = 0, the block is divided into two 2×4 subblocks.
        // - Pixels a,b,c,d, e,f,g,h (columns 0, 1) use base color 1.
        // - Pixels i,j,k,l, m,n,o,p (columns 2, 3) use base color 2.
        for row in 0..4 {
            for col in 0..2 {
                output[row][col] = apply_modifier(
                    metadata.base[0],
                    metadata.table_idx_1,
                    get_pixel_modifier_index(row, col, morton_interleaved),
                )
            }
        }
        for row in 0..4 {
            for col in 2..4 {
                output[row][col] = apply_modifier(
                    metadata.base[1],
                    metadata.table_idx_1,
                    get_pixel_modifier_index(row, col, morton_interleaved),
                )
            }
        }
    }
    return output;
}
