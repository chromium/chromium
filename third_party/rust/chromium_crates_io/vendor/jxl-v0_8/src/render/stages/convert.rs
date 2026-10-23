// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, I32SimdVec, ScalarDescriptor, SimdDescriptor, simd_function};

use crate::frame::quantizer::LfQuantFactors;
use crate::headers::bit_depth::BitDepth;
use crate::render::{
    Channels, ChannelsMut, ErasedLocalState, RenderPipelineInOutStage, VecLoad, for_each_chunk,
};
use crate::util::sync::{Arc, RwLock};

pub struct ConvertModularXYBToF32Stage {
    first_channel: usize,
    lf_quant: Arc<RwLock<LfQuantFactors>>,
}

impl ConvertModularXYBToF32Stage {
    pub fn new(
        first_channel: usize,
        lf_quant: Arc<RwLock<LfQuantFactors>>,
    ) -> ConvertModularXYBToF32Stage {
        ConvertModularXYBToF32Stage {
            first_channel,
            lf_quant,
        }
    }
}

impl std::fmt::Display for ConvertModularXYBToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular xyb data to F32 in channels {}..{}",
            self.first_channel,
            self.first_channel + 2,
        )
    }
}

#[inline(always)]
fn modular_xyb_to_float_simd<D: SimdDescriptor, T: VecLoad<D, Vec = D::I32Vec>>(
    d: D,
    input_rows: &Channels<T>,
    output_rows: &mut ChannelsMut<f32>,
    [scale_x, scale_y, scale_b]: [f32; 3],
    xsize: usize,
) {
    let scale_x = D::F32Vec::splat(d, scale_x);
    let scale_y = D::F32Vec::splat(d, scale_y);
    let scale_b = D::F32Vec::splat(d, scale_b);

    // Input channels: [Y, X, B] (modular XYB order)
    // Output channels: [X, Y, B] (standard XYB order)
    for_each_chunk(
        d,
        xsize,
        input_rows.view::<3, 1, 0>(),
        output_rows.view::<3, 1, 1>(),
        |_x, inv, outv| {
            let vy = inv.load::<_, 0>(d, 0, 0).as_f32();
            let vx = inv.load::<_, 1>(d, 0, 0).as_f32();
            let vb = inv.load::<_, 2>(d, 0, 0).as_f32();

            outv.store::<_, 0>(d, 0, vx * scale_x);
            outv.store::<_, 1>(d, 0, vy * scale_y);
            outv.store::<_, 2>(d, 0, (vb + vy) * scale_b);
        },
    );
}

simd_function!(
    modular32_xyb_to_float_simd_dispatch,
    d: D,
    fn modular32_xyb_to_float(
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        scales: [f32; 3],
        xsize: usize,
    ) {
        modular_xyb_to_float_simd(d, input_rows, output_rows, scales, xsize);
    }
);

simd_function!(
    modular16_xyb_to_float_simd_dispatch,
    d: D,
    fn modular16_xyb_to_float(
        input_rows: &Channels<i16>,
        output_rows: &mut ChannelsMut<f32>,
        scales: [f32; 3],
        xsize: usize,
    ) {
        modular_xyb_to_float_simd(d, input_rows, output_rows, scales, xsize);
    }
);

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
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        let lf_quant = self.lf_quant.try_read().unwrap();
        modular32_xyb_to_float_simd_dispatch(
            input_rows,
            output_rows,
            lf_quant.quant_factors,
            xsize,
        );
    }
}

pub struct ConvertModular16XYBToF32Stage {
    first_channel: usize,
    lf_quant: Arc<RwLock<LfQuantFactors>>,
}

impl ConvertModular16XYBToF32Stage {
    pub fn new(
        first_channel: usize,
        lf_quant: Arc<RwLock<LfQuantFactors>>,
    ) -> ConvertModular16XYBToF32Stage {
        ConvertModular16XYBToF32Stage {
            first_channel,
            lf_quant,
        }
    }
}

impl std::fmt::Display for ConvertModular16XYBToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular xyb data to F32 in channels {}..{}",
            self.first_channel,
            self.first_channel + 2,
        )
    }
}

impl RenderPipelineInOutStage for ConvertModular16XYBToF32Stage {
    type InputT = i16;
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
        input_rows: &Channels<i16>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        let lf_quant = self.lf_quant.try_read().unwrap();
        modular16_xyb_to_float_simd_dispatch(
            input_rows,
            output_rows,
            lf_quant.quant_factors,
            xsize,
        );
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
    fn int_to_float_32bit_simd(
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        xsize: usize,
    ) {
        for_each_chunk(
            d,
            xsize,
            input_rows.view::<1, 1, 0>(),
            output_rows.view::<1, 1, 1>(),
            |_x, inv, outv| {
                let val = inv.load::<_, 0>(d, 0, 0);
                outv.store::<_, 0>(d, 0, val.bitcast_to_f32());
            },
        );
    }
);

// SIMD 16-bit float (half-precision) to 32-bit float conversion
// Uses hardware F16C/NEON instructions when available via F32Vec::load_f16_bits()
#[inline(always)]
fn int_to_float_16bit_simd<D: SimdDescriptor, T: VecLoad<D, Vec = D::I32Vec>>(
    d: D,
    input_rows: &Channels<T>,
    output_rows: &mut ChannelsMut<f32>,
    xsize: usize,
) {
    let simd_width = D::F32Vec::LEN;

    // Temporary buffer for i32->u16 conversion via SIMD
    // Note: Using constant 16 (max AVX-512 width) because D::F32Vec::LEN
    // cannot be used as array size in Rust (const generics limitation)
    const { assert!(D::F32Vec::LEN <= 16) }
    let mut u16_buf = [0u16; 16];

    for_each_chunk(
        d,
        xsize,
        input_rows.view::<1, 1, 0>(),
        output_rows.view::<1, 1, 1>(),
        |_x, inv, outv| {
            // Use SIMD to extract lower 16 bits from each i32 lane
            let i32_vec = inv.load::<_, 0>(d, 0, 0);
            i32_vec.store_u16(&mut u16_buf[..simd_width]);
            // Use hardware f16->f32 conversion
            let result = D::F32Vec::load_f16_bits(d, &u16_buf[..simd_width]);
            outv.store::<_, 0>(d, 0, result);
        },
    );
}

simd_function!(
    int32_to_float_16bit_simd_dispatch,
    d: D,
    fn int32_to_float_16bit(
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        xsize: usize,
    ) {
        int_to_float_16bit_simd(d, input_rows, output_rows, xsize);
    }
);

simd_function!(
    int16_to_float_16bit_simd_dispatch,
    d: D,
    fn int16_to_float_16bit(
        input_rows: &Channels<i16>,
        output_rows: &mut ChannelsMut<f32>,
        xsize: usize,
    ) {
        int_to_float_16bit_simd(d, input_rows, output_rows, xsize);
    }
);

// Converts custom [bits]-bit float (with [exp_bits] exponent bits) stored as
// int back to binary32 float.
fn int_to_float(
    input_rows: &Channels<i32>,
    output_rows: &mut ChannelsMut<f32>,
    bit_depth: &BitDepth,
    xsize: usize,
) {
    let bits = bit_depth.bits_per_sample();
    let exp_bits = bit_depth.exponent_bits_per_sample();

    // Use SIMD fast paths for common formats
    if bits == 32 && exp_bits == 8 {
        // 32-bit float passthrough
        int_to_float_32bit_simd_dispatch(input_rows, output_rows, xsize);
        return;
    }

    if bits == 16 && exp_bits == 5 {
        // IEEE 754 half-precision (f16) - common HDR format
        int32_to_float_16bit_simd_dispatch(input_rows, output_rows, xsize);
        return;
    }

    // Generic scalar path for other custom float formats
    int_to_float_generic(input_rows, output_rows, bits, exp_bits, xsize);
}

fn custom_float_sample_to_f32(mut f: u32, bits: u32, exp_bits: u32) -> f32 {
    let exp_bias = (1 << (exp_bits - 1)) - 1;
    let sign_shift = bits - 1;
    let mant_bits = bits - exp_bits - 1;
    let mant_shift = 23 - mant_bits;
    let signbit = ((f >> sign_shift) & 1) != 0;
    f &= (1 << sign_shift) - 1;
    if f == 0 {
        return if signbit { -0.0 } else { 0.0 };
    }
    let mut exp = (f >> mant_bits) as i32;
    let mut mantissa = f & ((1 << mant_bits) - 1);
    if exp == (1 << exp_bits) - 1 {
        // NaN or infinity
        f = if signbit { 0x80000000 } else { 0 };
        f |= 0b11111111 << 23;
        f |= mantissa << mant_shift;
        return f32::from_bits(f);
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
    f32::from_bits(f)
}

// Generic scalar conversion for arbitrary bit-depth floats
// TODO: SIMD optimization for custom float formats
fn int_to_float_generic<T>(
    input_rows: &Channels<T>,
    output_rows: &mut ChannelsMut<f32>,
    bits: u32,
    exp_bits: u32,
    xsize: usize,
) where
    T: VecLoad<ScalarDescriptor, Vec = std::num::Wrapping<i32>>,
{
    let d = ScalarDescriptor::new().unwrap();
    for_each_chunk(
        d,
        xsize,
        input_rows.view::<1, 1, 0>(),
        output_rows.view::<1, 1, 1>(),
        |_x, inv, outv| {
            let in_val = inv.load::<_, 0>(d, 0, 0).0;
            let out_val = custom_float_sample_to_f32(in_val as u32, bits, exp_bits);
            outv.store::<_, 0>(d, 0, out_val);
        },
    );
}

#[inline(always)]
fn modular_to_float_simd<D: SimdDescriptor, T: VecLoad<D, Vec = D::I32Vec>>(
    d: D,
    input_rows: &Channels<T>,
    output_rows: &mut ChannelsMut<f32>,
    scale: f32,
    xsize: usize,
) {
    let scale = D::F32Vec::splat(d, scale);

    for_each_chunk(
        d,
        xsize,
        input_rows.view::<1, 1, 0>(),
        output_rows.view::<1, 1, 1>(),
        |_x, inv, outv| {
            let val = inv.load::<_, 0>(d, 0, 0);
            outv.store::<_, 0>(d, 0, val.as_f32() * scale);
        },
    );
}

simd_function!(
    modular32_to_float_simd_dispatch,
    d: D,
    fn modular32_to_float(
        input_rows: &Channels<i32>,
        output_rows: &mut ChannelsMut<f32>,
        scale: f32,
        xsize: usize,
    ) {
        modular_to_float_simd(d, input_rows, output_rows, scale, xsize);
    }
);

simd_function!(
    modular16_to_float_simd_dispatch,
    d: D,
    fn modular16_to_float(
        input_rows: &Channels<i16>,
        output_rows: &mut ChannelsMut<f32>,
        scale: f32,
        xsize: usize,
    ) {
        modular_to_float_simd(d, input_rows, output_rows, scale, xsize);
    }
);

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
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        if self.bit_depth.floating_point_sample() {
            int_to_float(input_rows, output_rows, &self.bit_depth, xsize);
        } else {
            let scale = 1.0 / ((1u64 << self.bit_depth.bits_per_sample()) - 1) as f32;
            modular32_to_float_simd_dispatch(input_rows, output_rows, scale, xsize);
        }
    }
}

pub struct ConvertModular16ToF32Stage {
    channel: usize,
    bit_depth: BitDepth,
}

impl ConvertModular16ToF32Stage {
    pub fn new(channel: usize, bit_depth: BitDepth) -> ConvertModular16ToF32Stage {
        ConvertModular16ToF32Stage { channel, bit_depth }
    }
}

impl std::fmt::Display for ConvertModular16ToF32Stage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "convert modular data to F32 in channel {} with bit depth {:?}",
            self.channel, self.bit_depth
        )
    }
}

fn int16_to_float(
    input_rows: &Channels<i16>,
    output_rows: &mut ChannelsMut<f32>,
    bit_depth: &BitDepth,
    xsize: usize,
) {
    let bits = bit_depth.bits_per_sample();
    let exp_bits = bit_depth.exponent_bits_per_sample();

    if bits == 16 && exp_bits == 5 {
        int16_to_float_16bit_simd_dispatch(input_rows, output_rows, xsize);
        return;
    }

    int_to_float_generic(input_rows, output_rows, bits, exp_bits, xsize);
}

impl RenderPipelineInOutStage for ConvertModular16ToF32Stage {
    type InputT = i16;
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
        input_rows: &Channels<i16>,
        output_rows: &mut ChannelsMut<f32>,
        _state: Option<&mut ErasedLocalState>,
        _previous_call_was_previous_row: bool,
    ) {
        if self.bit_depth.floating_point_sample() {
            int16_to_float(input_rows, output_rows, &self.bit_depth, xsize);
        } else {
            let scale = 1.0 / ((1u64 << self.bit_depth.bits_per_sample()) - 1) as f32;
            modular16_to_float_simd_dispatch(input_rows, output_rows, scale, xsize);
        }
    }
}

#[cfg(test)]
mod test {
    use test_log::test;

    use super::*;
    use crate::error::Result;
    use crate::headers::bit_depth::BitDepth;
    use crate::image::DataTypeTag;
    use crate::render::low_memory_pipeline::row_buffers::RowBuffer;

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
    fn modular16_to_f32_8bit_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertModular16ToF32Stage::new(0, BitDepth::integer_samples(8)),
            (500, 500),
            1,
        )
    }

    #[test]
    fn modular16_to_f32_16bit_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || ConvertModular16ToF32Stage::new(0, BitDepth::integer_samples(16)),
            (500, 500),
            1,
        )
    }

    #[test]
    fn modular16_xyb_to_f32_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || {
                ConvertModular16XYBToF32Stage::new(
                    0,
                    Arc::new(RwLock::new(LfQuantFactors::default())),
                )
            },
            (500, 500),
            3,
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
        let x0 = RowBuffer::x0_offset::<i32>();
        let mut in_buf = RowBuffer::new(DataTypeTag::I32, 0, 0, 0, 16).unwrap();
        let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, 16).unwrap();

        let in_row = in_buf.get_row_mut::<i32>(0);
        for (i, &f) in test_values.iter().enumerate() {
            in_row[x0 + i] = f.to_bits() as i32;
        }

        let in_refs = [&in_buf];
        let in_channels = Channels::from_row_buffers(&in_refs, x0, 0, 1, 0);
        {
            let mut out_channels =
                ChannelsMut::from_row_buffers(std::slice::from_mut(&mut out_buf), x0, 0, 1);

            int_to_float(
                &in_channels,
                &mut out_channels,
                &bit_depth,
                test_values.len(),
            );
        }

        let output = &out_buf.get_row::<f32>(0)[x0..x0 + test_values.len()];
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

        let x0 = RowBuffer::x0_offset::<i32>();
        let mut in_buf = RowBuffer::new(DataTypeTag::I32, 0, 0, 0, 16).unwrap();
        let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, 16).unwrap();

        let in_row = in_buf.get_row_mut::<i32>(0);
        for (i, &(bits, _)) in test_cases.iter().enumerate() {
            in_row[x0 + i] = bits as i32;
        }

        let in_refs = [&in_buf];
        let in_channels = Channels::from_row_buffers(&in_refs, x0, 0, 1, 0);
        {
            let mut out_channels =
                ChannelsMut::from_row_buffers(std::slice::from_mut(&mut out_buf), x0, 0, 1);

            int_to_float(
                &in_channels,
                &mut out_channels,
                &bit_depth,
                test_cases.len(),
            );
        }

        let output = &out_buf.get_row::<f32>(0)[x0..x0 + test_cases.len()];
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
