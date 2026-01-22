// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use num_traits::Float;

use jxl_transforms::{transform::*, transform_map::*};

use crate::{
    BLOCK_DIM, BLOCK_SIZE, GROUP_DIM,
    bit_reader::BitReader,
    entropy_coding::decode::SymbolReader,
    error::{Error, Result},
    frame::{
        HfGlobalState, HfMetadata, LfGlobalState, block_context_map::*,
        color_correlation_map::COLOR_TILE_DIM_IN_BLOCKS, quant_weights::DequantMatrices,
    },
    headers::frame_header::FrameHeader,
    image::{Image, ImageRect, Rect},
    util::{CeilLog2, ShiftRightCeil, tracing_wrappers::*},
};
use jxl_simd::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask, simd_function};

const LF_BUFFER_SIZE: usize = 32 * 32;

/// Reusable buffers for VarDCT group decoding to avoid repeated allocations.
pub struct VarDctBuffers {
    pub scratch: Vec<f32>,
    pub transform_buffer: [Vec<f32>; 3],
    /// Coefficient storage for single-pass decoding (when hf_coefficients is None)
    pub coeffs_storage: Vec<i32>,
}

impl VarDctBuffers {
    pub fn new() -> Self {
        Self {
            scratch: vec![0.0; LF_BUFFER_SIZE],
            transform_buffer: [
                vec![0.0; MAX_COEFF_AREA],
                vec![0.0; MAX_COEFF_AREA],
                vec![0.0; MAX_COEFF_AREA],
            ],
            coeffs_storage: vec![0; 3 * GROUP_DIM * GROUP_DIM],
        }
    }

    /// Reset buffers to zero for reuse.
    pub fn reset(&mut self) {
        self.scratch.fill(0.0);
        for buf in &mut self.transform_buffer {
            buf.fill(0.0);
        }
        self.coeffs_storage.fill(0);
    }
}

impl Default for VarDctBuffers {
    fn default() -> Self {
        Self::new()
    }
}

#[inline]
fn predict_num_nonzeros(nzeros_map: &Image<u32>, bx: usize, by: usize) -> usize {
    if bx == 0 {
        if by == 0 {
            32
        } else {
            nzeros_map.row(by - 1)[0] as usize
        }
    } else if by == 0 {
        nzeros_map.row(by)[bx - 1] as usize
    } else {
        (nzeros_map.row(by - 1)[bx] + nzeros_map.row(by)[bx - 1]).div_ceil(2) as usize
    }
}

#[inline(always)]
fn adjust_quant_bias<D: SimdDescriptor>(
    d: D,
    c: usize,
    quant_i: D::I32Vec,
    biases: &[f32; 4],
) -> D::F32Vec {
    let quant = quant_i.as_f32();
    let adjusted = quant - D::F32Vec::splat(d, biases[3]) / quant;
    D::I32Vec::splat(d, 2)
        .gt(quant_i.abs())
        .if_then_else_f32(quant_i.as_f32() * D::F32Vec::splat(d, biases[c]), adjusted)
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
fn dequant_lane<D: SimdDescriptor>(
    d: D,
    scaled_dequant_x: f32,
    scaled_dequant_y: f32,
    scaled_dequant_b: f32,
    dequant_matrices: &[f32],
    size: usize,
    k: usize,
    x_cc_mul: f32,
    b_cc_mul: f32,
    biases: &[f32; 4],
    qblock: &[&[i32]; 3],
    block: &mut [Vec<f32>; 3],
) {
    let x_mul = D::F32Vec::load(d, &dequant_matrices[k..]) * D::F32Vec::splat(d, scaled_dequant_x);
    let y_mul =
        D::F32Vec::load(d, &dequant_matrices[size + k..]) * D::F32Vec::splat(d, scaled_dequant_y);
    let b_mul = D::F32Vec::load(d, &dequant_matrices[2 * size + k..])
        * D::F32Vec::splat(d, scaled_dequant_b);

    let quantized_x = D::I32Vec::load(d, &qblock[0][k..]);
    let quantized_y = D::I32Vec::load(d, &qblock[1][k..]);
    let quantized_b = D::I32Vec::load(d, &qblock[2][k..]);

    let dequant_x_cc = adjust_quant_bias(d, 0, quantized_x, biases) * x_mul;
    let dequant_y = adjust_quant_bias(d, 1, quantized_y, biases) * y_mul;
    let dequant_b_cc = adjust_quant_bias(d, 2, quantized_b, biases) * b_mul;

    let dequant_x = D::F32Vec::splat(d, x_cc_mul).mul_add(dequant_y, dequant_x_cc);
    let dequant_b = D::F32Vec::splat(d, b_cc_mul).mul_add(dequant_y, dequant_b_cc);
    dequant_x.store(&mut block[0][k..]);
    dequant_y.store(&mut block[1][k..]);
    dequant_b.store(&mut block[2][k..]);
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
fn dequant_block<D: SimdDescriptor>(
    d: D,
    hf_type: HfTransformType,
    inv_global_scale: f32,
    quant: u32,
    x_dm_multiplier: f32,
    b_dm_multiplier: f32,
    x_cc_mul: f32,
    b_cc_mul: f32,
    size: usize,
    dequant_matrices: &DequantMatrices,
    covered_blocks: usize,
    biases: &[f32; 4],
    qblock: &[&[i32]; 3],
    block: &mut [Vec<f32>; 3],
) {
    let scaled_dequant_y = inv_global_scale / (quant as f32);

    let scaled_dequant_x = scaled_dequant_y * x_dm_multiplier;
    let scaled_dequant_b = scaled_dequant_y * b_dm_multiplier;

    let matrices = dequant_matrices.matrix(hf_type, 0);

    assert!(BLOCK_SIZE.is_multiple_of(D::F32Vec::LEN));
    for k in (0..covered_blocks * BLOCK_SIZE).step_by(D::F32Vec::LEN) {
        dequant_lane(
            d,
            scaled_dequant_x,
            scaled_dequant_y,
            scaled_dequant_b,
            matrices,
            size,
            k,
            x_cc_mul,
            b_cc_mul,
            biases,
            qblock,
            block,
        );
    }
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
fn dequant_and_transform_to_pixels<D: SimdDescriptor>(
    d: D,
    quant_biases: &[f32; 4],
    x_dm_multiplier: f32,
    b_dm_multiplier: f32,
    pixels: &mut [Image<f32>; 3],
    scratch: &mut [f32],
    inv_global_scale: f32,
    transform_buffer: &mut [Vec<f32>; 3],
    hshift: [usize; 3],
    vshift: [usize; 3],
    by: usize,
    sby: [usize; 3],
    bx: usize,
    sbx: [usize; 3],
    x_cc_mul: f32,
    b_cc_mul: f32,
    raw_quant: u32,
    lf_rects: &Option<[ImageRect<f32>; 3]>,
    transform_type: HfTransformType,
    block_rect: Rect,
    num_blocks: usize,
    num_coeffs: usize,
    qblock: &[&[i32]; 3],
    dequant_matrices: &DequantMatrices,
) -> Result<(), Error> {
    dequant_block::<D>(
        d,
        transform_type,
        inv_global_scale,
        raw_quant,
        x_dm_multiplier,
        b_dm_multiplier,
        x_cc_mul,
        b_cc_mul,
        num_coeffs,
        dequant_matrices,
        num_blocks,
        quant_biases,
        qblock,
        transform_buffer,
    );
    for c in [1, 0, 2] {
        if (sbx[c] << hshift[c]) != bx || (sby[c] << vshift[c] != by) {
            continue;
        }
        let lf = &mut scratch[..];
        {
            let xs = covered_blocks_x(transform_type) as usize;
            let ys = covered_blocks_y(transform_type) as usize;
            let rect = lf_rects.as_ref().unwrap()[c];
            for (y, lf) in lf.chunks_exact_mut(xs).enumerate().take(ys) {
                lf.copy_from_slice(&rect.row(y)[0..xs]);
            }
        }
        transform_to_pixels(transform_type, lf, &mut transform_buffer[c]);
        let downsampled_rect = Rect {
            origin: (
                block_rect.origin.0 >> hshift[c],
                block_rect.origin.1 >> vshift[c],
            ),
            size: block_rect.size,
        };
        let mut output_rect = pixels[c].get_rect_mut(downsampled_rect);
        for i in 0..downsampled_rect.size.1 {
            let offset = i * downsampled_rect.size.0;
            output_rect
                .row(i)
                .copy_from_slice(&transform_buffer[c][offset..offset + downsampled_rect.size.0]);
        }
    }
    Ok(())
}

simd_function!(
    dequant_and_transform_to_pixels_dispatch,
    d: D,
    #[allow(clippy::too_many_arguments)]
    pub fn dequant_and_transform_to_pixels_fwd(
        quant_biases: &[f32; 4],
        x_dm_multiplier: f32,
        b_dm_multiplier: f32,
        pixels: &mut [Image<f32>; 3],
        scratch: &mut [f32],
        inv_global_scale: f32,
        transform_buffer: &mut [Vec<f32>; 3],
        hshift: [usize; 3],
        vshift: [usize; 3],
        by: usize,
        sby: [usize; 3],
        bx: usize,
        sbx: [usize; 3],
        x_cc_mul: f32,
        b_cc_mul: f32,
        raw_quant: u32,
        lf_rects: &Option<[ImageRect<f32>; 3]>,
        transform_type: HfTransformType,
        block_rect: Rect,
        num_blocks: usize,
        num_coeffs: usize,
        qblock: &[&[i32]; 3],
        dequant_matrices: &DequantMatrices,
    ) -> Result<(), Error> {
        dequant_and_transform_to_pixels(
            d,
            quant_biases,
            x_dm_multiplier,
            b_dm_multiplier,
            pixels,
            scratch,
            inv_global_scale,
            transform_buffer,
            hshift,
            vshift,
            by,
            sby,
            bx,
            sbx,
            x_cc_mul,
            b_cc_mul,
            raw_quant,
            lf_rects,
            transform_type,
            block_rect,
            num_blocks,
            num_coeffs,
            qblock,
            dequant_matrices,
        )
    }
);

#[allow(clippy::too_many_arguments)]
#[allow(clippy::type_complexity)]
pub fn decode_vardct_group(
    group: usize,
    pass: usize,
    frame_header: &FrameHeader,
    lf_global: &mut LfGlobalState,
    hf_global: &mut HfGlobalState,
    hf_meta: &HfMetadata,
    lf_image: &Option<[Image<f32>; 3]>,
    quant_lf: &Image<u8>,
    quant_biases: &[f32; 4],
    pixels: &mut [Image<f32>; 3],
    br: &mut BitReader,
    buffers: &mut VarDctBuffers,
) -> Result<(), Error> {
    let x_dm_multiplier = (1.0 / (1.25)).powf(frame_header.x_qm_scale as f32 - 2.0);
    let b_dm_multiplier = (1.0 / (1.25)).powf(frame_header.b_qm_scale as f32 - 2.0);

    let num_histo_bits = hf_global.num_histograms.ceil_log2();
    let histogram_index: usize = br.read(num_histo_bits as usize)? as usize;
    debug!(?histogram_index);
    let mut reader = SymbolReader::new(&hf_global.passes[pass].histograms, br, None)?;
    let block_group_rect = frame_header.block_group_rect(group);
    debug!(?block_group_rect);
    // Reset and use pooled buffers
    buffers.reset();
    let scratch = &mut buffers.scratch;
    let color_correlation_params = lf_global.color_correlation_params.as_ref().unwrap();
    let cmap_rect = Rect {
        origin: (
            block_group_rect.origin.0 / COLOR_TILE_DIM_IN_BLOCKS,
            block_group_rect.origin.1 / COLOR_TILE_DIM_IN_BLOCKS,
        ),
        size: (
            block_group_rect.size.0.div_ceil(COLOR_TILE_DIM_IN_BLOCKS),
            block_group_rect.size.1.div_ceil(COLOR_TILE_DIM_IN_BLOCKS),
        ),
    };
    let quant_params = lf_global.quant_params.as_ref().unwrap();
    let inv_global_scale = quant_params.inv_global_scale();
    let ytox_map = hf_meta.ytox_map.get_rect(cmap_rect);
    let ytob_map = hf_meta.ytob_map.get_rect(cmap_rect);
    let transform_map = hf_meta.transform_map.get_rect(block_group_rect);
    let raw_quant_map = hf_meta.raw_quant_map.get_rect(block_group_rect);
    let mut num_nzeros: [Image<u32>; 3] = [
        Image::new((
            block_group_rect.size.0 >> frame_header.hshift(0),
            block_group_rect.size.1 >> frame_header.vshift(0),
        ))?,
        Image::new((
            block_group_rect.size.0 >> frame_header.hshift(1),
            block_group_rect.size.1 >> frame_header.vshift(1),
        ))?,
        Image::new((
            block_group_rect.size.0 >> frame_header.hshift(2),
            block_group_rect.size.1 >> frame_header.vshift(2),
        ))?,
    ];
    let quant_lf_rect = quant_lf.get_rect(block_group_rect);
    let block_context_map = lf_global.block_context_map.as_mut().unwrap();
    let context_offset = histogram_index * block_context_map.num_ac_contexts();
    let coeffs = match hf_global.hf_coefficients.as_mut() {
        Some(hf_coefficients) => [
            hf_coefficients.0.row_mut(group),
            hf_coefficients.1.row_mut(group),
            hf_coefficients.2.row_mut(group),
        ],
        None => {
            // Use pooled buffer (already reset to zero in buffers.reset() above)
            let (coeffs_x, coeffs_y_b) = buffers.coeffs_storage.split_at_mut(GROUP_DIM * GROUP_DIM);
            let (coeffs_y, coeffs_b) = coeffs_y_b.split_at_mut(GROUP_DIM * GROUP_DIM);
            [coeffs_x, coeffs_y, coeffs_b]
        }
    };
    let shift_for_pass = if pass < frame_header.passes.shift.len() {
        frame_header.passes.shift[pass]
    } else {
        0
    };
    let mut coeffs_offset = 0;
    let transform_buffer = &mut buffers.transform_buffer;

    let hshift = [
        frame_header.hshift(0),
        frame_header.hshift(1),
        frame_header.hshift(2),
    ];
    let vshift = [
        frame_header.vshift(0),
        frame_header.vshift(1),
        frame_header.vshift(2),
    ];
    let lf = match lf_image.as_ref() {
        None => None,
        Some(lf_planes) => {
            let r: [Rect; 3] = core::array::from_fn(|i| Rect {
                origin: (
                    block_group_rect.origin.0 >> hshift[i],
                    block_group_rect.origin.1 >> vshift[i],
                ),
                size: (
                    block_group_rect.size.0 >> hshift[i],
                    block_group_rect.size.1 >> vshift[i],
                ),
            });

            let [lf_x, lf_y, lf_b] = lf_planes.each_ref();
            Some([
                lf_x.get_rect(r[0]),
                lf_y.get_rect(r[1]),
                lf_b.get_rect(r[2]),
            ])
        }
    };
    for by in 0..block_group_rect.size.1 {
        let sby = [by >> vshift[0], by >> vshift[1], by >> vshift[2]];
        let ty = by / COLOR_TILE_DIM_IN_BLOCKS;

        let row_cmap_x = ytox_map.row(ty);
        let row_cmap_b = ytob_map.row(ty);

        for bx in 0..block_group_rect.size.0 {
            let sbx = [bx >> hshift[0], bx >> hshift[1], bx >> hshift[2]];
            let tx = bx / COLOR_TILE_DIM_IN_BLOCKS;
            let x_cc_mul = color_correlation_params.y_to_x(row_cmap_x[tx] as i32);
            let b_cc_mul = color_correlation_params.y_to_b(row_cmap_b[tx] as i32);
            let raw_quant = raw_quant_map.row(by)[bx] as u32;
            let quant_lf = quant_lf_rect.row(by)[bx] as usize;
            let raw_transform_id = transform_map.row(by)[bx];
            let transform_id = raw_transform_id & 127;
            let is_first_block = raw_transform_id >= 128;
            if !is_first_block {
                continue;
            }
            let lf_rects = match lf.as_ref() {
                None => None,
                Some(lf) => {
                    let [lf_x, lf_y, lf_b] = lf.each_ref();
                    Some([
                        lf_x.rect(Rect {
                            origin: (sbx[0], sby[0]),
                            size: (lf_x.size().0 - sbx[0], lf_x.size().1 - sby[0]),
                        }),
                        lf_y.rect(Rect {
                            origin: (sbx[1], sby[1]),
                            size: (lf_y.size().0 - sbx[1], lf_y.size().1 - sby[1]),
                        }),
                        lf_b.rect(Rect {
                            origin: (sbx[2], sby[2]),
                            size: (lf_b.size().0 - sbx[2], lf_b.size().1 - sby[2]),
                        }),
                    ])
                }
            };

            let transform_type = HfTransformType::from_usize(transform_id as usize)
                .ok_or(Error::InvalidVarDCTTransform(transform_id as usize))?;
            let cx = covered_blocks_x(transform_type) as usize;
            let cy = covered_blocks_y(transform_type) as usize;
            let shape_id = block_shape_id(transform_type) as usize;
            let block_size = (cx * BLOCK_DIM, cy * BLOCK_DIM);
            let block_rect = Rect {
                origin: (bx * BLOCK_DIM, by * BLOCK_DIM),
                size: block_size,
            };
            let num_blocks = cx * cy;
            let num_coeffs = num_blocks * BLOCK_SIZE;
            let log_num_blocks = num_blocks.ilog2() as usize;
            let pass_info = &hf_global.passes[pass];
            for c in [1, 0, 2] {
                if (sbx[c] << hshift[c]) != bx || (sby[c] << vshift[c] != by) {
                    continue;
                }
                trace!(
                    "Decoding block ({},{}) channel {} with {}x{} block transform {} (shape id {})",
                    sbx[c], sby[c], c, cx, cy, transform_id, shape_id
                );
                let predicted_nzeros = predict_num_nonzeros(&num_nzeros[c], sbx[c], sby[c]);
                let block_context =
                    block_context_map.block_context(quant_lf, raw_quant, shape_id, c);
                let nonzero_context = block_context_map
                    .nonzero_context(predicted_nzeros, block_context)
                    + context_offset;
                let mut nonzeros =
                    reader.read_unsigned(&pass_info.histograms, br, nonzero_context) as usize;
                trace!(
                    "block ({},{},{c}) predicted_nzeros: {predicted_nzeros} \
                       nzero_ctx: {nonzero_context} (offset: {context_offset}) \
                       nzeros: {nonzeros}",
                    sbx[c], sby[c]
                );
                if nonzeros + num_blocks > num_coeffs {
                    return Err(Error::InvalidNumNonZeros(nonzeros, num_blocks));
                }
                for iy in 0..cy {
                    let nzrow = num_nzeros[c].row_mut(sby[c] + iy);
                    for ix in 0..cx {
                        nzrow[sbx[c] + ix] = nonzeros.shrc(log_num_blocks) as u32;
                    }
                }
                let histo_offset =
                    block_context_map.zero_density_context_offset(block_context) + context_offset;
                let mut prev = if nonzeros > num_coeffs / 16 { 0 } else { 1 };
                let permutation = &pass_info.coeff_orders[shape_id * 3 + c];
                let current_coeffs = &mut coeffs[c][coeffs_offset..coeffs_offset + num_coeffs];
                for k in num_blocks..num_coeffs {
                    if nonzeros == 0 {
                        break;
                    }
                    let ctx =
                        histo_offset + zero_density_context(nonzeros, k, log_num_blocks, prev);
                    let coeff =
                        reader.read_signed(&pass_info.histograms, br, ctx) << shift_for_pass;
                    prev = if coeff != 0 { 1 } else { 0 };
                    nonzeros -= prev;
                    let coeff_index = permutation[k] as usize;
                    current_coeffs[coeff_index] += coeff;
                }
                if nonzeros != 0 {
                    return Err(Error::EndOfBlockResidualNonZeros(nonzeros));
                }
            }
            let qblock = [
                &coeffs[0][coeffs_offset..],
                &coeffs[1][coeffs_offset..],
                &coeffs[2][coeffs_offset..],
            ];
            let dequant_matrices = &hf_global.dequant_matrices;
            dequant_and_transform_to_pixels_dispatch(
                quant_biases,
                x_dm_multiplier,
                b_dm_multiplier,
                pixels,
                scratch,
                inv_global_scale,
                transform_buffer,
                hshift,
                vshift,
                by,
                sby,
                bx,
                sbx,
                x_cc_mul,
                b_cc_mul,
                raw_quant,
                &lf_rects,
                transform_type,
                block_rect,
                num_blocks,
                num_coeffs,
                &qblock,
                dequant_matrices,
            )?;
            coeffs_offset += num_coeffs;
        }
    }
    reader.check_final_state(&hf_global.passes[pass].histograms, br)?;
    Ok(())
}
