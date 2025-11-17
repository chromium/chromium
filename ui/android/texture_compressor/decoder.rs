// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
