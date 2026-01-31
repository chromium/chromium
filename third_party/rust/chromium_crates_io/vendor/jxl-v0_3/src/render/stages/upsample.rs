// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::needless_range_loop)]
#![allow(clippy::too_many_arguments)]

use std::any::Any;

use crate::{
    headers::CustomTransformData,
    render::{Channels, ChannelsMut, RenderPipelineInOutStage},
};
use jxl_simd::{F32SimdVec, simd_function};

pub struct Upsample<const N: usize, const SHIFT: u8> {
    // Precomputed flattened kernels for SIMD optimization
    // Stored as N*N kernels in row-major order: kernel[oy][ox] -> flat_kernels[oy * N + ox]
    flat_kernels: Vec<[f32; 25]>,
    channel: usize,
}

impl<const N: usize, const SHIFT: u8> Upsample<N, SHIFT> {
    pub fn new(ups_factors: &CustomTransformData, channel: usize) -> Self {
        const { assert!(SHIFT >= 1 && SHIFT <= 3) }
        const { assert!(1 << SHIFT == N) }

        let weights: &[f32] = match N {
            2 => &ups_factors.weights2,
            4 => &ups_factors.weights4,
            8 => &ups_factors.weights8,
            _ => unreachable!(),
        };

        let mut kernel = [[[[0.0; 5]; 5]; N]; N];
        let n = N / 2;
        for i in 0..5 * n {
            for j in 0..5 * n {
                let y = i.min(j);
                let x = i.max(j);
                let y = y as isize;
                let x = x as isize;
                let n = n as isize;
                let index = (5 * n * y - y * (y - 1) / 2 + x - y) as usize;
                // Filling in the top left corner from the weights
                kernel[j / 5][i / 5][j % 5][i % 5] = weights[index];
                // Mirroring to get the rest of the kernel.
                kernel[(2 * n as usize - 1) - j / 5][i / 5][4 - (j % 5)][i % 5] = weights[index];
                kernel[j / 5][(2 * n as usize - 1) - i / 5][j % 5][4 - (i % 5)] = weights[index];
                kernel[(2 * n as usize - 1) - j / 5][(2 * n as usize - 1) - i / 5][4 - (j % 5)]
                    [4 - (i % 5)] = weights[index];
            }
        }

        // Precompute flattened kernels for SIMD optimization
        // Stored in row-major order: kernel[oy][ox] -> flat_kernels[oy * N + ox]
        let mut flat_kernels = Vec::with_capacity(N * N);
        for di in 0..N {
            for dj in 0..N {
                let mut k = [0.0f32; 25];
                for i in 0..5 {
                    for j in 0..5 {
                        k[i * 5 + j] = kernel[di][dj][i][j];
                    }
                }
                flat_kernels.push(k);
            }
        }

        Self {
            flat_kernels,
            channel,
        }
    }
}

impl<const N: usize, const SHIFT: u8> std::fmt::Display for Upsample<N, SHIFT> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{N}x{N} upsampling of channel {}", self.channel)
    }
}

/// State for upsampling stage containing reusable temp buffers
struct UpsampleState {
    col_min: Vec<f32>,
    col_max: Vec<f32>,
    mins: Vec<f32>,
    maxs: Vec<f32>,
}

impl UpsampleState {
    fn new() -> Self {
        Self {
            col_min: Vec::new(),
            col_max: Vec::new(),
            mins: Vec::new(),
            maxs: Vec::new(),
        }
    }

    fn ensure_capacity(&mut self, xsize: usize) {
        // Generous padding for SIMD operations:
        // Step 1: needs xsize + 4 (col_fill_len) elements
        // Step 2: loads at offset + 4, needs SIMD_LEN more elements from there
        // For AVX-512 (SIMD_LEN=16), we need: xsize + 4 + 16 = xsize + 20
        // Add extra padding to be safe with future SIMD widths
        let needed = xsize + 24;
        if self.col_min.len() < needed {
            self.col_min.resize(needed, 0.0);
            self.col_max.resize(needed, 0.0);
            self.mins.resize(needed, 0.0);
            self.maxs.resize(needed, 0.0);
        }
    }
}

// Shared min/max computation - generic helper function called from within dispatched functions
#[inline(always)]
fn compute_minmax<D: jxl_simd::SimdDescriptor>(
    d: D,
    input: &[&[f32]],
    xsize: usize,
    col_min: &mut [f32],
    col_max: &mut [f32],
    mins: &mut [f32],
    maxs: &mut [f32],
) {
    let r0 = input[0];
    let r1 = input[1];
    let r2 = input[2];
    let r3 = input[3];
    let r4 = input[4];

    // Step 1: Compute column-wise min/max (vertical reduction across 5 rows)
    // Use div_ceil to process all elements (may over-read but buffers are padded)
    let col_fill_len = xsize + 4;
    let num_vecs = col_fill_len.div_ceil(D::F32Vec::LEN);
    for i in 0..num_vecs {
        let offset = i * D::F32Vec::LEN;
        let v0 = D::F32Vec::load(d, &r0[offset..]);
        let v1 = D::F32Vec::load(d, &r1[offset..]);
        let v2 = D::F32Vec::load(d, &r2[offset..]);
        let v3 = D::F32Vec::load(d, &r3[offset..]);
        let v4 = D::F32Vec::load(d, &r4[offset..]);

        let col_min_v = v0.min(v1).min(v2).min(v3).min(v4);
        let col_max_v = v0.max(v1).max(v2).max(v3).max(v4);

        col_min_v.store(&mut col_min[offset..]);
        col_max_v.store(&mut col_max[offset..]);
    }

    // Step 2: Compute row-wise min/max from column temps (horizontal 5-wide window)
    let num_output_vecs = xsize.div_ceil(D::F32Vec::LEN);
    for i in 0..num_output_vecs {
        let offset = i * D::F32Vec::LEN;
        let m0 = D::F32Vec::load(d, &col_min[offset..]);
        let m1 = D::F32Vec::load(d, &col_min[offset + 1..]);
        let m2 = D::F32Vec::load(d, &col_min[offset + 2..]);
        let m3 = D::F32Vec::load(d, &col_min[offset + 3..]);
        let m4 = D::F32Vec::load(d, &col_min[offset + 4..]);
        let min_v = m0.min(m1).min(m2).min(m3).min(m4);
        min_v.store(&mut mins[offset..]);

        let m0 = D::F32Vec::load(d, &col_max[offset..]);
        let m1 = D::F32Vec::load(d, &col_max[offset + 1..]);
        let m2 = D::F32Vec::load(d, &col_max[offset + 2..]);
        let m3 = D::F32Vec::load(d, &col_max[offset + 3..]);
        let m4 = D::F32Vec::load(d, &col_max[offset + 4..]);
        let max_v = m0.max(m1).max(m2).max(m3).max(m4);
        max_v.store(&mut maxs[offset..]);
    }
}

// Macro to generate the kernel convolution code (shared across 2x, 4x, 8x)
macro_rules! kernel_conv {
    ($d:expr, $kv:expr, $r0:expr, $r1:expr, $r2:expr, $r3:expr, $r4:expr, $x:expr) => {{
        // Compute 5x5 kernel using FMA with 3-way ILP
        // Row 0
        let mut acc0 = <D::F32Vec>::load($d, &$r0[$x..]) * $kv[0];
        let mut acc1 = <D::F32Vec>::load($d, &$r0[$x + 1..]) * $kv[1];
        let mut acc2 = <D::F32Vec>::load($d, &$r0[$x + 2..]) * $kv[2];
        acc0 = <D::F32Vec>::load($d, &$r0[$x + 3..]).mul_add($kv[3], acc0);
        acc1 = <D::F32Vec>::load($d, &$r0[$x + 4..]).mul_add($kv[4], acc1);
        // Row 1
        acc2 = <D::F32Vec>::load($d, &$r1[$x..]).mul_add($kv[5], acc2);
        acc0 = <D::F32Vec>::load($d, &$r1[$x + 1..]).mul_add($kv[6], acc0);
        acc1 = <D::F32Vec>::load($d, &$r1[$x + 2..]).mul_add($kv[7], acc1);
        acc2 = <D::F32Vec>::load($d, &$r1[$x + 3..]).mul_add($kv[8], acc2);
        acc0 = <D::F32Vec>::load($d, &$r1[$x + 4..]).mul_add($kv[9], acc0);
        // Row 2
        acc1 = <D::F32Vec>::load($d, &$r2[$x..]).mul_add($kv[10], acc1);
        acc2 = <D::F32Vec>::load($d, &$r2[$x + 1..]).mul_add($kv[11], acc2);
        acc0 = <D::F32Vec>::load($d, &$r2[$x + 2..]).mul_add($kv[12], acc0);
        acc1 = <D::F32Vec>::load($d, &$r2[$x + 3..]).mul_add($kv[13], acc1);
        acc2 = <D::F32Vec>::load($d, &$r2[$x + 4..]).mul_add($kv[14], acc2);
        // Row 3
        acc0 = <D::F32Vec>::load($d, &$r3[$x..]).mul_add($kv[15], acc0);
        acc1 = <D::F32Vec>::load($d, &$r3[$x + 1..]).mul_add($kv[16], acc1);
        acc2 = <D::F32Vec>::load($d, &$r3[$x + 2..]).mul_add($kv[17], acc2);
        acc0 = <D::F32Vec>::load($d, &$r3[$x + 3..]).mul_add($kv[18], acc0);
        acc1 = <D::F32Vec>::load($d, &$r3[$x + 4..]).mul_add($kv[19], acc1);
        // Row 4
        acc2 = <D::F32Vec>::load($d, &$r4[$x..]).mul_add($kv[20], acc2);
        acc0 = <D::F32Vec>::load($d, &$r4[$x + 1..]).mul_add($kv[21], acc0);
        acc1 = <D::F32Vec>::load($d, &$r4[$x + 2..]).mul_add($kv[22], acc1);
        acc2 = <D::F32Vec>::load($d, &$r4[$x + 3..]).mul_add($kv[23], acc2);
        acc0 = <D::F32Vec>::load($d, &$r4[$x + 4..]).mul_add($kv[24], acc0);

        acc0 + acc1 + acc2
    }};
}

// 2x upsampling SIMD implementation - single dispatch with integrated minmax
simd_function!(
    upsample_2x_simd_dispatch,
    d: D,
    fn upsample_2x_simd(
        input: &[&[f32]],
        xsize: usize,
        flat_kernels: &[[f32; 25]],
        col_min: &mut [f32],
        col_max: &mut [f32],
        mins: &mut [f32],
        maxs: &mut [f32],
        output: &mut [&mut [f32]],
    ) {
        // Compute min/max using shared helper
        compute_minmax(d, input, xsize, col_min, col_max, mins, maxs);

        let r0 = input[0];
        let r1 = input[1];
        let r2 = input[2];
        let r3 = input[3];
        let r4 = input[4];

        // Pre-broadcast kernel weights
        // flat_kernels layout: kernel[oy][ox] -> flat_kernels[oy * 2 + ox]
        let mut kernel_vecs = [[D::F32Vec::splat(d, 0.0); 25]; 4];
        for idx in 0..4 {
            let k = &flat_kernels[idx];
            for i in 0..25 {
                kernel_vecs[idx][i] = D::F32Vec::splat(d, k[i]);
            }
        }

        // Process using iterators for mins/maxs, manual indexing for output
        let mins_iter = mins.chunks_exact(D::F32Vec::LEN);
        let maxs_iter = maxs.chunks_exact(D::F32Vec::LEN);

        for ((mins_chunk, maxs_chunk), x) in mins_iter
            .zip(maxs_iter)
            .zip((0..xsize).step_by(D::F32Vec::LEN))
            .take(xsize.div_ceil(D::F32Vec::LEN))
        {
            let minval = D::F32Vec::load(d, mins_chunk);
            let maxval = D::F32Vec::load(d, maxs_chunk);
            let out_x = x * 2;

            // Row 0
            let r0_0 = kernel_conv!(d, kernel_vecs[0], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
            let r0_1 = kernel_conv!(d, kernel_vecs[1], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
            D::F32Vec::store_interleaved_2(r0_0, r0_1, &mut output[0][out_x..]);

            // Row 1
            let r1_0 = kernel_conv!(d, kernel_vecs[2], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
            let r1_1 = kernel_conv!(d, kernel_vecs[3], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
            D::F32Vec::store_interleaved_2(r1_0, r1_1, &mut output[1][out_x..]);
        }
    }
);

// 4x upsampling SIMD implementation - single dispatch with integrated minmax
simd_function!(
    upsample_4x_simd_dispatch,
    d: D,
    fn upsample_4x_simd(
        input: &[&[f32]],
        xsize: usize,
        flat_kernels: &[[f32; 25]],
        col_min: &mut [f32],
        col_max: &mut [f32],
        mins: &mut [f32],
        maxs: &mut [f32],
        output: &mut [&mut [f32]],
    ) {
        // Compute min/max using shared helper
        compute_minmax(d, input, xsize, col_min, col_max, mins, maxs);

        let r0 = input[0];
        let r1 = input[1];
        let r2 = input[2];
        let r3 = input[3];
        let r4 = input[4];

        // Pre-broadcast kernel weights
        // flat_kernels layout: kernel[oy][ox] -> flat_kernels[oy * 4 + ox]
        let mut kernel_vecs = [[D::F32Vec::splat(d, 0.0); 25]; 16];
        for idx in 0..16 {
            let k = &flat_kernels[idx];
            for i in 0..25 {
                kernel_vecs[idx][i] = D::F32Vec::splat(d, k[i]);
            }
        }

        // Process using iterators for mins/maxs, manual indexing for output
        let mins_iter = mins.chunks_exact(D::F32Vec::LEN);
        let maxs_iter = maxs.chunks_exact(D::F32Vec::LEN);

        for ((mins_chunk, maxs_chunk), x) in mins_iter
            .zip(maxs_iter)
            .zip((0..xsize).step_by(D::F32Vec::LEN))
            .take(xsize.div_ceil(D::F32Vec::LEN))
        {
            let minval = D::F32Vec::load(d, mins_chunk);
            let maxval = D::F32Vec::load(d, maxs_chunk);
            let out_x = x * 4;

            // Process all 4 output rows using a loop
            for oy in 0..4 {
                let base = oy * 4;
                let v0 = kernel_conv!(d, kernel_vecs[base], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v1 = kernel_conv!(d, kernel_vecs[base + 1], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v2 = kernel_conv!(d, kernel_vecs[base + 2], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v3 = kernel_conv!(d, kernel_vecs[base + 3], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                D::F32Vec::store_interleaved_4(v0, v1, v2, v3, &mut output[oy][out_x..]);
            }
        }
    }
);

// 8x upsampling SIMD implementation - single dispatch with integrated minmax
simd_function!(
    upsample_8x_simd_dispatch,
    d: D,
    fn upsample_8x_simd(
        input: &[&[f32]],
        xsize: usize,
        flat_kernels: &[[f32; 25]],
        col_min: &mut [f32],
        col_max: &mut [f32],
        mins: &mut [f32],
        maxs: &mut [f32],
        output: &mut [&mut [f32]],
    ) {
        // Compute min/max using shared helper
        compute_minmax(d, input, xsize, col_min, col_max, mins, maxs);

        let r0 = input[0];
        let r1 = input[1];
        let r2 = input[2];
        let r3 = input[3];
        let r4 = input[4];

        // Pre-broadcast kernel weights
        // flat_kernels layout: kernel[oy][ox] -> flat_kernels[oy * 8 + ox]
        let mut kernel_vecs = [[D::F32Vec::splat(d, 0.0); 25]; 64];
        for idx in 0..64 {
            let k = &flat_kernels[idx];
            for i in 0..25 {
                kernel_vecs[idx][i] = D::F32Vec::splat(d, k[i]);
            }
        }

        // Process using iterators for mins/maxs, manual indexing for output
        let mins_iter = mins.chunks_exact(D::F32Vec::LEN);
        let maxs_iter = maxs.chunks_exact(D::F32Vec::LEN);

        for ((mins_chunk, maxs_chunk), x) in mins_iter
            .zip(maxs_iter)
            .zip((0..xsize).step_by(D::F32Vec::LEN))
            .take(xsize.div_ceil(D::F32Vec::LEN))
        {
            let minval = D::F32Vec::load(d, mins_chunk);
            let maxval = D::F32Vec::load(d, maxs_chunk);
            let out_x = x * 8;

            // Process all 8 output rows using a loop
            for oy in 0..8 {
                let base = oy * 8;
                let v0 = kernel_conv!(d, kernel_vecs[base], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v1 = kernel_conv!(d, kernel_vecs[base + 1], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v2 = kernel_conv!(d, kernel_vecs[base + 2], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v3 = kernel_conv!(d, kernel_vecs[base + 3], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v4 = kernel_conv!(d, kernel_vecs[base + 4], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v5 = kernel_conv!(d, kernel_vecs[base + 5], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v6 = kernel_conv!(d, kernel_vecs[base + 6], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                let v7 = kernel_conv!(d, kernel_vecs[base + 7], r0, r1, r2, r3, r4, x).max(minval).min(maxval);
                D::F32Vec::store_interleaved_8(v0, v1, v2, v3, v4, v5, v6, v7, &mut output[oy][out_x..]);
            }
        }
    }
);

impl<const N: usize, const SHIFT: u8> RenderPipelineInOutStage for Upsample<N, SHIFT> {
    type InputT = f32;
    type OutputT = f32;
    const SHIFT: (u8, u8) = (SHIFT, SHIFT);
    const BORDER: (u8, u8) = (2, 2);

    fn uses_channel(&self, c: usize) -> bool {
        c == self.channel
    }

    fn init_local_state(&self, _thread_index: usize) -> crate::error::Result<Option<Box<dyn Any>>> {
        Ok(Some(Box::new(UpsampleState::new()) as Box<dyn Any>))
    }

    /// Processes a chunk of a row, applying NxN upsampling using a 5x5 kernel.
    /// Each input value expands into a NxN region in the output, based on neighboring inputs.
    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        input_rows: &Channels<f32>,
        output_rows: &mut ChannelsMut<f32>,
        state: Option<&mut dyn std::any::Any>,
    ) {
        let input = &input_rows[0];
        let state: &mut UpsampleState = state.unwrap().downcast_mut().unwrap();
        state.ensure_capacity(xsize);

        // Dispatch to appropriate upsampling function based on N
        // Each dispatch function computes min/max and performs upsampling in one call
        match N {
            2 => {
                upsample_2x_simd_dispatch(
                    input,
                    xsize,
                    self.flat_kernels.as_slice(),
                    &mut state.col_min,
                    &mut state.col_max,
                    &mut state.mins,
                    &mut state.maxs,
                    &mut output_rows[0],
                );
            }
            4 => {
                upsample_4x_simd_dispatch(
                    input,
                    xsize,
                    self.flat_kernels.as_slice(),
                    &mut state.col_min,
                    &mut state.col_max,
                    &mut state.mins,
                    &mut state.maxs,
                    &mut output_rows[0],
                );
            }
            8 => {
                upsample_8x_simd_dispatch(
                    input,
                    xsize,
                    self.flat_kernels.as_slice(),
                    &mut state.col_min,
                    &mut state.col_max,
                    &mut state.mins,
                    &mut state.maxs,
                    &mut output_rows[0],
                );
            }
            _ => unreachable!(),
        }
    }
}

pub type Upsample2x = Upsample<2, 1>;
pub type Upsample4x = Upsample<4, 2>;
pub type Upsample8x = Upsample<8, 3>;

#[cfg(test)]
mod test {
    use super::*;
    use crate::{
        error::Result, headers::CustomTransformDataNonserialized, image::Image,
        render::test::make_and_run_simple_pipeline, util::test::assert_almost_abs_eq,
    };
    use test_log::test;

    fn ups_factors() -> CustomTransformData {
        CustomTransformData::default(&CustomTransformDataNonserialized { xyb_encoded: true })
    }

    #[test]
    fn upsample2x_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || Upsample2x::new(&ups_factors(), 0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn upsample4x_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || Upsample4x::new(&ups_factors(), 0),
            (500, 500),
            1,
        )
    }

    #[test]
    fn upsample8x_consistency() -> Result<()> {
        crate::render::test::test_stage_consistency(
            || Upsample8x::new(&ups_factors(), 0),
            (504, 504),
            1,
        )
    }

    #[test]
    fn upsample2x_constant() -> Result<()> {
        let image_size = (238, 412);
        let input_size = (image_size.0 / 2, image_size.1 / 2);
        let val = 0.777f32;
        let input = Image::new_with_value(input_size, val)?;
        let stage = Upsample2x::new(&ups_factors(), 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], image_size, 0, 123)?;
        for x in 0..image_size.0 {
            for y in 0..image_size.1 {
                assert_almost_abs_eq(output[0].row(y)[x], val, 0.0000001);
            }
        }
        Ok(())
    }

    #[test]
    fn upsample4x_constant() -> Result<()> {
        let image_size = (240, 412);
        let input_size = (image_size.0 / 4, image_size.1 / 4);
        let val = 0.777f32;
        let input = Image::new_with_value(input_size, val)?;
        let stage = Upsample4x::new(&ups_factors(), 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], image_size, 0, 123)?;
        for x in 0..image_size.0 {
            for y in 0..image_size.1 {
                assert_almost_abs_eq(output[0].row(y)[x], val, 0.00001);
            }
        }
        Ok(())
    }

    #[test]
    fn upsample8x_constant() -> Result<()> {
        let image_size = (240, 416);
        let input_size = (image_size.0 / 8, image_size.1 / 8);
        let val = 0.777f32;
        let input = Image::new_with_value(input_size, val)?;
        let stage = Upsample8x::new(&ups_factors(), 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], image_size, 0, 123)?;
        for x in 0..image_size.0 {
            for y in 0..image_size.1 {
                assert_almost_abs_eq(output[0].row(y)[x], val, 0.00001);
            }
        }
        Ok(())
    }

    #[test]
    fn test_upsample2() -> Result<()> {
        let eps = 0.0000001;
        let mut input = Image::new((7, 7))?;
        // Put a single "1.0" in the middle of the image.
        input.row_mut(3)[3] = 1.0f32;
        let ups_factors = ups_factors();
        let stage = Upsample2x::new(&ups_factors, 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (14, 14), 0, 77)?;
        assert_eq!(output[0].size(), (14, 14));
        // Check we have a border with zeros
        for i in 0..14 {
            for j in 0..2 {
                assert_almost_abs_eq(output[0].row(j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[j], 0.0, eps);
                assert_almost_abs_eq(output[0].row(13 - j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[13 - j], 0.0, eps);
            }
        }
        // Define the mapping for the symmetric top-left kernel
        let index_map = [
            [0, 1, 2, 3, 4],
            [1, 5, 6, 7, 8],
            [2, 6, 9, 10, 11],
            [3, 7, 10, 12, 13],
            [4, 8, 11, 13, 14],
        ];

        // Validate weights from the kernel
        let kernel_size = 5;
        let kernel_offset = 2;
        let weights = &ups_factors.weights2;
        for di in 0..2 {
            for dj in 0..2 {
                for i in 0..kernel_size {
                    for j in 0..kernel_size {
                        let output_value =
                            output[0].row(kernel_offset + di + 2 * i)[kernel_offset + dj + 2 * j];
                        let mapped_i = if di == 0 { kernel_size - 1 - i } else { i };
                        let mapped_j = if dj == 0 { kernel_size - 1 - j } else { j };
                        let weight_index = index_map[mapped_i][mapped_j];
                        assert_almost_abs_eq(
                            output_value,
                            weights[weight_index].clamp(0.0, 1.0),
                            eps,
                        );
                    }
                }
            }
        }

        Ok(())
    }

    #[test]
    fn test_upsample4() -> Result<()> {
        let eps = 0.0000001;
        let mut input = Image::new((7, 7))?;
        // Put a single "1.0" in the middle of the image.
        input.row_mut(3)[3] = 1.0f32;
        let ups_factors = ups_factors();
        let stage = Upsample4x::new(&ups_factors, 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (28, 28), 0, 1024)?;

        assert_eq!(output[0].size(), (28, 28));

        // Check we have a border with zeros
        for i in 0..28 {
            for j in 0..4 {
                assert_almost_abs_eq(output[0].row(j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[j], 0.0, eps);
                assert_almost_abs_eq(output[0].row(27 - j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[27 - j], 0.0, eps);
            }
        }

        // Define the mapping for the symmetric top-left kernel
        let index_map = [
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
            [1, 10, 11, 12, 13, 14, 15, 16, 17, 18],
            [2, 11, 19, 20, 21, 22, 23, 24, 25, 26],
            [3, 12, 20, 27, 28, 29, 30, 31, 32, 33],
            [4, 13, 21, 28, 34, 35, 36, 37, 38, 39],
            [5, 14, 22, 29, 35, 40, 41, 42, 43, 44],
            [6, 15, 23, 30, 36, 41, 45, 46, 47, 48],
            [7, 16, 24, 31, 37, 42, 46, 49, 50, 51],
            [8, 17, 25, 32, 38, 43, 47, 50, 52, 53],
            [9, 18, 26, 33, 39, 44, 48, 51, 53, 54],
        ];

        // Validate weights from the kernel
        let kernel_size = 5;
        let kernel_offset = 4;
        let weights = &ups_factors.weights4;
        let row_size = output[0].size().0;
        let column_size = row_size;
        for di in 0..4 {
            for dj in 0..4 {
                for ki in 0..kernel_size {
                    for kj in 0..kernel_size {
                        let i = kernel_size * di + ki;
                        let j = kernel_size * dj + kj;
                        let offset_i = kernel_offset + i;
                        let offset_j = kernel_offset + j;
                        // Testing symmetry
                        let output_value = output[0].row(offset_i)[offset_j];
                        let output_value_mirrored_right =
                            output[0].row(row_size - offset_i - 1)[offset_j];
                        let output_value_mirrored_down =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];
                        let output_value_mirrored_down_right =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];

                        assert_almost_abs_eq(output_value, output_value_mirrored_right, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down_right, eps);

                        // Testing if we get the expected weights, appropriately mapped.
                        let mapped_i = if (i % 4) < 2 {
                            4 - (i / 4) + (i % 2) * 5
                        } else {
                            i / 4 + (1 - (i % 2)) * 5
                        };
                        let mapped_j = if (j % 4) < 2 {
                            4 - (j / 4) + (j % 2) * 5
                        } else {
                            j / 4 + (1 - (j % 2)) * 5
                        };
                        let weight_index = index_map[mapped_i][mapped_j];
                        assert_almost_abs_eq(
                            output_value,
                            weights[weight_index].clamp(0.0, 1.0),
                            eps,
                        );
                    }
                }
            }
        }

        Ok(())
    }

    #[test]
    fn test_upsample8() -> Result<()> {
        let eps = 0.0000001;
        let mut input = Image::new((7, 7))?;
        // Put a single "1.0" in the middle of the image.
        input.row_mut(3)[3] = 1.0f32;
        let ups_factors = ups_factors();
        let stage = Upsample8x::new(&ups_factors, 0);
        let output: Vec<Image<f32>> =
            make_and_run_simple_pipeline(stage, &[input], (56, 56), 0, 1024)?;

        assert_eq!(output[0].size(), (56, 56));

        // Check we have a border with zeros
        for i in 0..56 {
            for j in 0..8 {
                assert_almost_abs_eq(output[0].row(j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[j], 0.0, eps);
                assert_almost_abs_eq(output[0].row(55 - j)[i], 0.0, eps);
                assert_almost_abs_eq(output[0].row(i)[55 - j], 0.0, eps);
            }
        }

        // Define the mapping for the symmetric top-left kernel
        let index_map = [
            [
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
                0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
            ],
            [
                0x01, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
            ],
            [
                0x02, 0x15, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
                0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
            ],
            [
                0x03, 0x16, 0x28, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
                0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            ],
            [
                0x04, 0x17, 0x29, 0x3a, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
                0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
            ],
            [
                0x05, 0x18, 0x2a, 0x3b, 0x4b, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62,
                0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
            ],
            [
                0x06, 0x19, 0x2b, 0x3c, 0x4c, 0x5b, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
                0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
            ],
            [
                0x07, 0x1a, 0x2c, 0x3d, 0x4d, 0x5c, 0x6a, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d,
                0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
            ],
            [
                0x08, 0x1b, 0x2d, 0x3e, 0x4e, 0x5d, 0x6b, 0x78, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
                0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
            ],
            [
                0x09, 0x1c, 0x2e, 0x3f, 0x4f, 0x5e, 0x6c, 0x79, 0x85, 0x90, 0x91, 0x92, 0x93, 0x94,
                0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
            ],
            [
                0x0a, 0x1d, 0x2f, 0x40, 0x50, 0x5f, 0x6d, 0x7a, 0x86, 0x91, 0x9b, 0x9c, 0x9d, 0x9e,
                0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
            ],
            [
                0x0b, 0x1e, 0x30, 0x41, 0x51, 0x60, 0x6e, 0x7b, 0x87, 0x92, 0x9c, 0xa5, 0xa6, 0xa7,
                0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
            ],
            [
                0x0c, 0x1f, 0x31, 0x42, 0x52, 0x61, 0x6f, 0x7c, 0x88, 0x93, 0x9d, 0xa6, 0xae, 0xaf,
                0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5,
            ],
            [
                0x0d, 0x20, 0x32, 0x43, 0x53, 0x62, 0x70, 0x7d, 0x89, 0x94, 0x9e, 0xa7, 0xaf, 0xb6,
                0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
            ],
            [
                0x0e, 0x21, 0x33, 0x44, 0x54, 0x63, 0x71, 0x7e, 0x8a, 0x95, 0x9f, 0xa8, 0xb0, 0xb7,
                0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2,
            ],
            [
                0x0f, 0x22, 0x34, 0x45, 0x55, 0x64, 0x72, 0x7f, 0x8b, 0x96, 0xa0, 0xa9, 0xb1, 0xb8,
                0xbe, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
            ],
            [
                0x10, 0x23, 0x35, 0x46, 0x56, 0x65, 0x73, 0x80, 0x8c, 0x97, 0xa1, 0xaa, 0xb2, 0xb9,
                0xbf, 0xc4, 0xc8, 0xc9, 0xca, 0xcb,
            ],
            [
                0x11, 0x24, 0x36, 0x47, 0x57, 0x66, 0x74, 0x81, 0x8d, 0x98, 0xa2, 0xab, 0xb3, 0xba,
                0xc0, 0xc5, 0xc9, 0xcc, 0xcd, 0xce,
            ],
            [
                0x12, 0x25, 0x37, 0x48, 0x58, 0x67, 0x75, 0x82, 0x8e, 0x99, 0xa3, 0xac, 0xb4, 0xbb,
                0xc1, 0xc6, 0xca, 0xcd, 0xcf, 0xd0,
            ],
            [
                0x13, 0x26, 0x38, 0x49, 0x59, 0x68, 0x76, 0x83, 0x8f, 0x9a, 0xa4, 0xad, 0xb5, 0xbc,
                0xc2, 0xc7, 0xcb, 0xce, 0xd0, 0xd1,
            ],
        ];

        // Validate weights from the kernel
        let kernel_size = 5;
        let kernel_offset = 8;
        let weights = &ups_factors.weights8;
        let row_size = output[0].size().0;
        let column_size = row_size;
        for di in 0..8 {
            for dj in 0..8 {
                for ki in 0..kernel_size {
                    for kj in 0..kernel_size {
                        let i = kernel_size * di + ki;
                        let j = kernel_size * dj + kj;
                        let offset_i = kernel_offset + i;
                        let offset_j = kernel_offset + j;
                        // Testing symmetry
                        let output_value = output[0].row(offset_i)[offset_j];
                        let output_value_mirrored_right =
                            output[0].row(row_size - offset_i - 1)[offset_j];
                        let output_value_mirrored_down =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];
                        let output_value_mirrored_down_right =
                            output[0].row(row_size - offset_i - 1)[column_size - offset_j - 1];

                        assert_almost_abs_eq(output_value, output_value_mirrored_right, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down, eps);
                        assert_almost_abs_eq(output_value, output_value_mirrored_down_right, eps);

                        // Testing if we get the expected weights, appropriately mapped.
                        let mapped_i = if (i % 8) < 4 {
                            4 - (i / 8) + (i % 4) * 5
                        } else {
                            i / 8 + (3 - (i % 4)) * 5
                        };
                        let mapped_j = if (j % 8) < 4 {
                            4 - (j / 8) + (j % 4) * 5
                        } else {
                            j / 8 + (3 - (j % 4)) * 5
                        };
                        let weight_index = index_map[mapped_i][mapped_j];
                        assert_almost_abs_eq(
                            output_value,
                            weights[weight_index].clamp(0.0, 1.0),
                            eps,
                        );
                    }
                }
            }
        }

        Ok(())
    }
}
