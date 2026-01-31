// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    frame::quantizer::LfQuantFactors,
    headers::bit_depth::BitDepth,
    render::{Channels, ChannelsMut, RenderPipelineInOutStage},
};
use jxl_simd::{F32SimdVec, I32SimdVec, simd_function};

pub struct ConvertU8F32Stage {
    channel: usize,
}

impl ConvertU8F32Stage {
    pub fn new(channel: usize) -> ConvertU8F32Stage {
        ConvertU8F32Stage { channel }
    }
}

impl std::fmt::Display for ConvertU8F32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "convert U8 data to F32 in channel {}", self.channel)
    }
}

impl RenderPipelineInOutStage for ConvertU8F32Stage {
    type InputT = u8;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<u8>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];
        for i in 0..xsize {
            output_rows[0][0][i] = input[0][i] as f32 * (1.0 / 255.0);
        }
    }
}

pub struct ConvertModularXYBToF32Stage {
    first_channel: usize,
    scale: [f32; 3],
}

impl ConvertModularXYBToF32Stage {
    pub fn new(first_channel: usize, lf_quant: &LfQuantFactors) -> ConvertModularXYBToF32Stage {
        ConvertModularXYBToF32Stage {
            first_channel,
            scale: lf_quant.quant_factors,
        }
    }
}

impl std::fmt::Display for ConvertModularXYBToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular xyb data to F32 in channels {}..{} with scales {:?}",
            self.first_channel,
            self.first_channel + 2,
            self.scale
        )
    }
}

impl RenderPipelineInOutStage for ConvertModularXYBToF32Stage {
    type InputT = i32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        (self.first_channel..self.first_channel + 3).contains(&c)
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let [scale_x, scale_y, scale_b] = self.scale;
        assert_eq!(
            input_rows.len(),
            3,
            "incorrect number of channels; expected 3, found {}",
            input_rows.len()
        );
        // Input channels: [Y, X, B] (modular XYB order)
        // Output channels: [X, Y, B] (standard XYB order)
        let (input_y, input_x, input_b) = (&input_rows[0], &input_rows[1], &input_rows[2]);
        let (output_x, output_y, output_b) = output_rows.split_first_3_mut();
        for i in 0..xsize {
            output_x[0][i] = input_x[0][i] as f32 * scale_x;
            output_y[0][i] = input_y[0][i] as f32 * scale_y;
            output_b[0][i] = (input_b[0][i] + input_y[0][i]) as f32 * scale_b;
        }
    }
}

pub struct ConvertModularToF32Stage {
    channel: usize,
    bit_depth: BitDepth,
}

impl ConvertModularToF32Stage {
    pub fn new(channel: usize, bit_depth: BitDepth) -> ConvertModularToF32Stage {
        ConvertModularToF32Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertModularToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular data to F32 in channel {} with bit depth {:?}",
            self.channel, self.bit_depth
        )
    }
}

// SIMD 32-bit float passthrough (bitcast i32 to f32)
simd_function!(
    int_to_float_32bit_simd_dispatch,
    d: D,
    fn int_to_float_32bit_simd(input: &[i32], output: &mut [f32], xsize: usize) {
        let simd_width = D::I32Vec::LEN;

        // Process complete SIMD vectors
        for (in_chunk, out_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::I32Vec::load(d, in_chunk);
            val.bitcast_to_f32().store(out_chunk);
        }
    }
);

// SIMD 16-bit float (half-precision) to 32-bit float conversion
// Uses hardware F16C/NEON instructions when available via F32Vec::load_f16_bits()
simd_function!(
    int_to_float_16bit_simd_dispatch,
    d: D,
    fn int_to_float_16bit_simd(input: &[i32], output: &mut [f32], xsize: usize) {
        let simd_width = D::F32Vec::LEN;

        // Temporary buffer for i32->u16 conversion via SIMD
        // Note: Using constant 16 (max AVX-512 width) because D::F32Vec::LEN
        // cannot be used as array size in Rust (const generics limitation)
        const { assert!(D::F32Vec::LEN <= 16) }
        let mut u16_buf = [0u16; 16];

        // Process complete SIMD vectors
        for (in_chunk, out_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            // Use SIMD to extract lower 16 bits from each i32 lane
            let i32_vec = D::I32Vec::load(d, in_chunk);
            i32_vec.store_u16(&mut u16_buf[..simd_width]);
            // Use hardware f16->f32 conversion
            let result = D::F32Vec::load_f16_bits(d, &u16_buf[..simd_width]);
            result.store(out_chunk);
        }
    }
);

// Converts custom [bits]-bit float (with [exp_bits] exponent bits) stored as
// int back to binary32 float.
fn int_to_float(input: &[i32], output: &mut [f32], bit_depth: &BitDepth, xsize: usize) {
    assert_eq!(input.len(), output.len());
    let bits = bit_depth.bits_per_sample();
    let exp_bits = bit_depth.exponent_bits_per_sample();

    // Use SIMD fast paths for common formats
    if bits == 32 && exp_bits == 8 {
        // 32-bit float passthrough
        int_to_float_32bit_simd_dispatch(input, output, xsize);
        return;
    }

    if bits == 16 && exp_bits == 5 {
        // IEEE 754 half-precision (f16) - common HDR format
        int_to_float_16bit_simd_dispatch(input, output, xsize);
        return;
    }

    // Generic scalar path for other custom float formats
    int_to_float_generic(input, output, bits, exp_bits);
}

// Generic scalar conversion for arbitrary bit-depth floats
// TODO: SIMD optimization for custom float formats
fn int_to_float_generic(input: &[i32], output: &mut [f32], bits: u32, exp_bits: u32) {
    let exp_bias = (1 << (exp_bits - 1)) - 1;
    let sign_shift = bits - 1;
    let mant_bits = bits - exp_bits - 1;
    let mant_shift = 23 - mant_bits;
    for (&in_val, out_val) in input.iter().zip(output) {
        let mut f = in_val as u32;
        let signbit = (f >> sign_shift) != 0;
        f &= (1 << sign_shift) - 1;
        if f == 0 {
            *out_val = if signbit { -0.0 } else { 0.0 };
            continue;
        }
        let mut exp = (f >> mant_bits) as i32;
        let mut mantissa = f & ((1 << mant_bits) - 1);
        if exp == (1 << exp_bits) - 1 {
            // NaN or infinity
            f = if signbit { 0x80000000 } else { 0 };
            f |= 0b11111111 << 23;
            f |= mantissa << mant_shift;
            *out_val = f32::from_bits(f);
            continue;
        }
        mantissa <<= mant_shift;
        // Try to normalize only if there is space for maneuver.
        if exp == 0 && exp_bits < 8 {
            // subnormal number
            while (mantissa & 0x800000) == 0 {
                mantissa <<= 1;
                exp -= 1;
            }
            exp += 1;
            // remove leading 1 because it is implicit now
            mantissa &= 0x7fffff;
        }
        exp -= exp_bias;
        // broke up the arbitrary float into its parts, now reassemble into
        // binary32
        exp += 127;
        assert!(exp >= 0);
        f = if signbit { 0x80000000 } else { 0 };
        f |= (exp as u32) << 23;
        f |= mantissa;
        *out_val = f32::from_bits(f);
    }
}

impl RenderPipelineInOutStage for ConvertModularToF32Stage {
    type InputT = i32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];
        if self.bit_depth.floating_point_sample() {
            int_to_float(input[0], output_rows[0][0], &self.bit_depth, xsize);
        } else {
            // TODO(veluca): SIMDfy this code.
            let scale = 1.0 / ((1u64 << self.bit_depth.bits_per_sample()) - 1) as f32;
            for i in 0..xsize {
                output_rows[0][0][i] = input[0][i] as f32 * scale;
            }
        }
    }
}

/// Stage that converts f32 values in [0, 1] range to u8 values.
pub struct ConvertF32ToU8Stage {
    channel: usize,
    bit_depth: u8,
}

impl ConvertF32ToU8Stage {
    pub fn new(channel: usize, bit_depth: u8) -> ConvertF32ToU8Stage {
        ConvertF32ToU8Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertF32ToU8Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert F32 to U8 in channel {} with bit depth {}",
            self.channel, self.bit_depth
        )
    }
}

// SIMD F32 to U8 conversion
simd_function!(
    f32_to_u8_simd_dispatch,
    d: D,
    fn f32_to_u8_simd(input: &[f32], output: &mut [u8], max: f32, xsize: usize) {
        let simd_width = D::F32Vec::LEN;
        let zero = D::F32Vec::splat(d, 0.0);
        let one = D::F32Vec::splat(d, 1.0);
        let scale = D::F32Vec::splat(d, max);

        // Process SIMD vectors using div_ceil (buffers are padded)
        for (input_chunk, output_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::F32Vec::load(d, input_chunk);
            // Clamp to [0, 1] and scale
            let clamped = val.max(zero).min(one);
            let scaled = clamped * scale;
            scaled.round_store_u8(output_chunk);
        }
    }
);

impl RenderPipelineInOutStage for ConvertF32ToU8Stage {
    type InputT = f32;
    type OutputT = u8;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<u8>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = input_rows[0][0];
        let output = &mut output_rows[0][0];
        let max = ((1u32 << self.bit_depth) - 1) as f32;
        f32_to_u8_simd_dispatch(input, output, max, xsize);
    }
}

/// Stage that converts f32 values in [0, 1] range to u16 values.
pub struct ConvertF32ToU16Stage {
    channel: usize,
    bit_depth: u8,
}

impl ConvertF32ToU16Stage {
    pub fn new(channel: usize, bit_depth: u8) -> ConvertF32ToU16Stage {
        ConvertF32ToU16Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertF32ToU16Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert F32 to U16 in channel {} with bit depth {}",
            self.channel, self.bit_depth
        )
    }
}

// SIMD F32 to U16 conversion
simd_function!(
    f32_to_u16_simd_dispatch,
    d: D,
    fn f32_to_u16_simd(input: &[f32], output: &mut [u16], max: f32, xsize: usize) {
        let simd_width = D::F32Vec::LEN;
        let zero = D::F32Vec::splat(d, 0.0);
        let one = D::F32Vec::splat(d, 1.0);
        let scale = D::F32Vec::splat(d, max);

        // Process SIMD vectors using div_ceil (buffers are padded)
        for (input_chunk, output_chunk) in input
            .chunks_exact(simd_width)
            .zip(output.chunks_exact_mut(simd_width))
            .take(xsize.div_ceil(simd_width))
        {
            let val = D::F32Vec::load(d, input_chunk);
            // Clamp to [0, 1] and scale
            let clamped = val.max(zero).min(one);
            let scaled = clamped * scale;
            scaled.round_store_u16(output_chunk);
        }
    }
);

impl RenderPipelineInOutStage for ConvertF32ToU16Stage {
    type InputT = f32;
    type OutputT = u16;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<u16>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = input_rows[0][0];
        let output = &mut output_rows[0][0];
        let max = ((1u32 << self.bit_depth) - 1) as f32;
        f32_to_u16_simd_dispatch(input, output, max, xsize);
    }
}

/// Stage that converts f32 values to f16 (half-precision float) values.
pub struct ConvertF32ToF16Stage {
    channel: usize,
}

impl ConvertF32ToF16Stage {
    pub fn new(channel: usize) -> ConvertF32ToF16Stage {
        ConvertF32ToF16Stage { channel }
    }
}

impl std::fmt::Display for ConvertF32ToF16Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "convert F32 to F16 in channel {}", self.channel)
    }
}

impl RenderPipelineInOutStage for ConvertF32ToF16Stage {
    type InputT = f32;
    type OutputT = crate::util::f16;
    const SHIFT: (u8, u8) = (0, 0);
    const BORDER: (u8, u8) = (0, 0);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<crate::util::f16>,
        _state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];
        for i in 0..xsize {
            output_rows[0][0][i] = crate::util::f16::from_f32(input[0][i]);
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::error::Result;
    use crate::headers::bit_depth::BitDepth;
    use test_log::test;

    #[test]
    fn u8_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(|| ConvertU8F32Stage::new(0), (500, 500), 1)
    }

    #[test]
    fn f32_to_u8_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertF32ToU8Stage::new(0, 8),
            (500, 500),
            1,
        )
    }

    #[test]
    fn f32_to_u16_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertF32ToU16Stage::new(0, 16),
            (500, 500),
            1,
        )
    }

    #[test]
    fn f32_to_f16_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(|| ConvertF32ToF16Stage::new(0), (500, 500), 1)
    }

    /// Test ConvertModularToF32Stage consistency with different bit depths.
    #[test]
    fn modular_to_f32_8bit_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertModularToF32Stage::new(0, BitDepth::integer_samples(8)),
            (500, 500),
            1,
        )
    }

    #[test]
    fn modular_to_f32_16bit_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertModularToF32Stage::new(0, BitDepth::integer_samples(16)),
            (500, 500),
            1,
        )
    }

    #[test]
    fn test_int_to_float_32bit() {
        // Test 32-bit float passthrough
        let bit_depth = BitDepth::f32();
        let test_values: Vec<f32> = vec![
            0.0,
            1.0,
            -1.0,
            0.5,
            -0.5,
            f32::INFINITY,
            f32::NEG_INFINITY,
            1e-30,
            1e30,
        ];
        let input: Vec<i32> = test_values
            .iter()
            .map(|&f| f.to_bits() as i32)
            .chain(std::iter::repeat(0))
            .take(16)
            .collect();
        let mut output = vec![0.0f32; 16];

        int_to_float(&input, &mut output, &bit_depth, test_values.len());

        for (i, (&expected, &actual)) in test_values.iter().zip(output.iter()).enumerate() {
            if expected.is_nan() {
                assert!(actual.is_nan(), "index {}: expected NaN, got {}", i, actual);
            } else {
                assert_eq!(expected, actual, "index {}: mismatch", i);
            }
        }
    }

    #[test]
    fn test_int_to_float_16bit() {
        // Test 16-bit float (f16) conversion for normal values
        let bit_depth = BitDepth::f16();

        // f16 format: 1 sign, 5 exp, 10 mantissa
        // Test cases: (f16_bits, expected_f32)
        let test_cases: Vec<(u16, f32)> = vec![
            (0x0000, 0.0),               // +0
            (0x8000, -0.0),              // -0
            (0x3C00, 1.0),               // 1.0
            (0xBC00, -1.0),              // -1.0
            (0x3800, 0.5),               // 0.5
            (0x4000, 2.0),               // 2.0
            (0x4400, 4.0),               // 4.0
            (0x7BFF, 65504.0),           // max normal f16
            (0x7C00, f32::INFINITY),     // +inf
            (0xFC00, f32::NEG_INFINITY), // -inf
            (0x0001, 5.960_464_5e-8),    // smallest positive subnormal
            (0x03FF, 6.097_555e-5),      // largest positive subnormal
            (0x8001, -5.960_464_5e-8),   // smallest negative subnormal
        ];

        let input: Vec<i32> = test_cases
            .iter()
            .map(|(bits, _)| *bits as i32)
            .chain(std::iter::repeat(0))
            .take(16)
            .collect();
        let mut output = vec![0.0f32; 16];

        int_to_float(&input, &mut output, &bit_depth, test_cases.len());

        for (i, (&(_, expected), &actual)) in test_cases.iter().zip(output.iter()).enumerate() {
            assert!(
                (expected - actual).abs() < 1e-6
                    || expected == actual
                    || (expected.is_sign_negative() == actual.is_sign_negative()
                        && expected == 0.0
                        && actual == 0.0),
                "index {}: expected {}, got {}",
                i,
                expected,
                actual
            );
        }
    }
}
