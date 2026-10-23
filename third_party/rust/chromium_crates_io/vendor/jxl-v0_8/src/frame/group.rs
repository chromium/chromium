// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask, simd_function};
use jxl_transforms::transform::*;
use jxl_transforms::transform_map::*;
use num_traits::Float;

use crate::bit_reader::BitReader;
use crate::entropy_coding::decode::{Codes, Histograms, SymbolReader, unpack_signed};
use crate::error::{Error, Result};
use crate::frame::block_context_map::*;
use crate::frame::color_correlation_map::COLOR_TILE_DIM_IN_BLOCKS;
use crate::frame::quant_weights::DequantMatrices;
use crate::frame::{HfGlobalState, HfMetadata, LfGlobalState};
use crate::headers::frame_header::FrameHeader;
use crate::headers::permutation::Permutation;
use crate::image::{Image, ImageRect, Rect};
use crate::render::low_memory_pipeline::row_buffers::RowBuffer;
use crate::util::tracing_wrappers::*;
use crate::util::{
    CacheLine, CeilLog2, ShiftRightCeil, SmallVec, num_cache_lines_for, slice_from_cachelines_mut,
};
use crate::{BLOCK_DIM, BLOCK_SIZE, GROUP_DIM};

const LF_BUFFER_SIZE: usize = 32 * 32;

/// Reusable buffers for VarDCT group decoding to avoid repeated allocations.
pub(crate) struct VarDctBuffers {
    pub(super) scratch: Vec<f32>,
    pub(super) transform_buffer: [Vec<f32>; 3],
    /// Coefficient storage for single-pass decoding (when hf_coefficients is None)
    pub(super) coeffs_storage: Vec<CacheLine>,
    pub(super) num_nzeros_storage: Vec<[u8; 32]>,
    pub(super) lf_upsample_bufs: Option<(RowBuffer, RowBuffer, usize)>,
    pub(super) lz77_windows: Vec<Vec<u32>>,
}

impl VarDctBuffers {
    pub(crate) fn new() -> Self {
        Self {
            scratch: vec![],
            transform_buffer: [vec![], vec![], vec![]],
            coeffs_storage: vec![],
            num_nzeros_storage: vec![],
            lf_upsample_bufs: None,
            lz77_windows: vec![],
        }
    }

    pub(super) fn ensure_num_nzeros(&mut self, num_passes: usize) {
        let needed = num_passes * 3;
        self.num_nzeros_storage.clear();
        self.num_nzeros_storage.resize(needed, [0u8; 32]);
        if self.lz77_windows.len() < num_passes {
            self.lz77_windows.resize_with(num_passes, Vec::new);
        }
    }

    pub(super) fn ensure_allocated(&mut self) -> Result<()> {
        if !self.scratch.is_empty() {
            return Ok(());
        }
        self.scratch.try_reserve_exact(LF_BUFFER_SIZE)?;
        self.scratch.resize(LF_BUFFER_SIZE, 0.0);
        for b in self.transform_buffer.iter_mut() {
            b.try_reserve_exact(64 * 64)?;
            b.resize(64 * 64, 0.0);
        }
        let num_cache_lines = num_cache_lines_for::<i32>(3 * 64 * 64);
        self.coeffs_storage.try_reserve_exact(num_cache_lines)?;
        self.coeffs_storage
            .resize(num_cache_lines, CacheLine::default());
        Ok(())
    }
}

fn get_single_pass_coeffs<'a>(
    coeffs_storage: &'a mut Vec<CacheLine>,
    use_i16: bool,
    num_coeffs: usize,
) -> CoeffsMut<'a> {
    let needed_cache_lines = num_cache_lines_for::<i32>(3 * num_coeffs);
    if coeffs_storage.len() < needed_cache_lines {
        coeffs_storage.resize(needed_cache_lines, CacheLine::default());
    }
    CoeffsMut::from_cachelines(&mut coeffs_storage[..], use_i16, num_coeffs)
}

#[inline]
fn predict_num_nonzeros(nzeros_row: &[u8; 32], bx: usize, by: usize) -> usize {
    if bx == 0 {
        if by == 0 { 32 } else { nzeros_row[0] as usize }
    } else if by == 0 {
        nzeros_row[bx - 1] as usize
    } else {
        (nzeros_row[bx] as usize + nzeros_row[bx - 1] as usize).div_ceil(2)
    }
}

#[inline(always)]
fn adjust_quant_bias<D: SimdDescriptor>(
    d: D,
    quant_i: D::I32Vec,
    bias_c: f32,
    bias_num: f32,
) -> D::F32Vec {
    let quant = quant_i.as_f32();
    let adjusted = quant - D::F32Vec::splat(d, bias_num) / quant;
    D::I32Vec::splat(d, 2)
        .gt(quant_i.abs())
        .if_then_else_f32(quant_i.as_f32() * D::F32Vec::splat(d, bias_c), adjusted)
}

pub trait CoeffStorage: Sized {
    fn load_simd<D: SimdDescriptor>(slice: &[Self], d: D, offset: usize) -> D::I32Vec;
    fn add_coeff(&mut self, val: i32);
}

impl CoeffStorage for i32 {
    #[inline(always)]
    fn load_simd<D: SimdDescriptor>(slice: &[i32], d: D, offset: usize) -> D::I32Vec {
        D::I32Vec::load(d, &slice[offset..])
    }
    #[inline(always)]
    fn add_coeff(&mut self, val: i32) {
        *self = self.wrapping_add(val);
    }
}

impl CoeffStorage for i16 {
    #[inline(always)]
    fn load_simd<D: SimdDescriptor>(slice: &[i16], d: D, offset: usize) -> D::I32Vec {
        D::I32Vec::load_from_i16(d, &slice[offset..])
    }
    #[inline(always)]
    fn add_coeff(&mut self, val: i32) {
        *self = self.wrapping_add(val as i16);
    }
}

enum CoeffsMut<'a> {
    I32([&'a mut [i32]; 3]),
    I16([&'a mut [i16]; 3]),
}

#[allow(clippy::too_many_arguments)]
fn decode_channel_coeffs_ans_no_lz77<T: CoeffStorage, const LOG_NUM_BLOCKS: usize>(
    current_coeffs: &mut [T],
    permutation: &Permutation,
    histo_offset: usize,
    shift: u32,
    mut prev: usize,
    nonzeros: &mut usize,
    reader: &mut SymbolReader,
    histograms: &Histograms,
    ans: &crate::entropy_coding::ans::AnsCodes,
    br: &mut BitReader,
) {
    let num_blocks = 1usize << LOG_NUM_BLOCKS;
    let num_coeffs = 64usize << LOG_NUM_BLOCKS;
    assert!(permutation.len() >= num_coeffs);
    assert!(*nonzeros > 0 && *nonzeros <= 63usize << LOG_NUM_BLOCKS);
    let mut nz = *nonzeros;
    let mut nz_ctx = histo_offset + COEFF_NUM_NONZERO_CONTEXT[((nz - 1) >> LOG_NUM_BLOCKS) + 1];
    for k in num_blocks..num_coeffs {
        let ctx = nz_ctx + COEFF_FREQ_CONTEXT[k >> LOG_NUM_BLOCKS] + prev;
        let cluster = histograms.map_context_to_cluster(ctx);
        let token = reader.read_token_ans_no_lz77(ans, br, cluster);
        if token == 0 {
            prev = 0;
            continue;
        }
        let coeff = unpack_signed(histograms.uint(cluster).read(token, br)) << shift;
        let coeff_index = permutation[k] as usize;
        current_coeffs[coeff_index].add_coeff(coeff);
        nz -= 1;
        if nz == 0 {
            break;
        }
        prev = 1;
        nz_ctx = histo_offset + COEFF_NUM_NONZERO_CONTEXT[((nz - 1) >> LOG_NUM_BLOCKS) + 1];
    }
    *nonzeros = nz;
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
fn decode_channel_coeffs<T: CoeffStorage>(
    current_coeffs: &mut [T],
    permutation: &Permutation,
    num_blocks: usize,
    num_coeffs: usize,
    log_num_blocks: usize,
    histo_offset: usize,
    shift: u32,
    mut nonzeros: usize,
    reader: &mut SymbolReader,
    histograms: &Histograms,
    br: &mut BitReader,
) -> Result<(), Error> {
    if nonzeros == 0 {
        return Ok(());
    }
    // Asserting once lets the compiler elide the bounds check on
    // `permutation[k]` inside the loop given `k < num_coeffs`.
    assert!(permutation.len() >= num_coeffs);
    let mut prev = if nonzeros > num_coeffs / 16 { 0 } else { 1 };
    if !reader.has_lz77() {
        match histograms.codes() {
            Codes::Ans(ans) => {
                macro_rules! dispatch_ans {
                    ($($l:literal),*) => {
                        match log_num_blocks {
                            $(
                                $l => decode_channel_coeffs_ans_no_lz77::<T, $l>(
                                    current_coeffs,
                                    permutation,
                                    histo_offset,
                                    shift,
                                    prev,
                                    &mut nonzeros,
                                    reader,
                                    histograms,
                                    ans,
                                    br,
                                ),
                            )*
                            _ => unreachable!(),
                        }
                    };
                }
                dispatch_ans!(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
            }
            Codes::Huffman(hc) => {
                let mut nz_ctx =
                    histo_offset + COEFF_NUM_NONZERO_CONTEXT[nonzeros.shrc(log_num_blocks) & 63];
                for k in num_blocks..num_coeffs {
                    let ctx = nz_ctx + COEFF_FREQ_CONTEXT[(k >> log_num_blocks) & 63] + prev;
                    let cluster = histograms.map_context_to_cluster(ctx);
                    let token = reader.read_token_huffman_no_lz77(hc, br, cluster);
                    if token == 0 {
                        prev = 0;
                        continue;
                    }
                    let coeff = unpack_signed(histograms.uint(cluster).read(token, br)) << shift;
                    let coeff_index = permutation[k] as usize;
                    current_coeffs[coeff_index].add_coeff(coeff);
                    nonzeros -= 1;
                    if nonzeros == 0 {
                        break;
                    }
                    prev = 1;
                    nz_ctx = histo_offset
                        + COEFF_NUM_NONZERO_CONTEXT[nonzeros.shrc(log_num_blocks) & 63];
                }
            }
        }
    } else {
        let mut nz_ctx =
            histo_offset + COEFF_NUM_NONZERO_CONTEXT[nonzeros.shrc(log_num_blocks) & 63];
        for k in num_blocks..num_coeffs {
            let ctx = nz_ctx + COEFF_FREQ_CONTEXT[(k >> log_num_blocks) & 63] + prev;
            let coeff = reader.read_signed_inline(histograms, br, ctx) << shift;
            if coeff == 0 {
                prev = 0;
                continue;
            }
            let coeff_index = permutation[k] as usize;
            current_coeffs[coeff_index].add_coeff(coeff);
            nonzeros -= 1;
            if nonzeros == 0 {
                break;
            }
            prev = 1;
            nz_ctx = histo_offset + COEFF_NUM_NONZERO_CONTEXT[nonzeros.shrc(log_num_blocks) & 63];
        }
    }
    if nonzeros != 0 {
        return Err(Error::EndOfBlockResidualNonZeros(nonzeros));
    }
    Ok(())
}

impl<'a> CoeffsMut<'a> {
    fn from_cachelines(cachelines: &'a mut [CacheLine], use_i16: bool, stride: usize) -> Self {
        if use_i16 {
            let slice = slice_from_cachelines_mut::<i16>(cachelines);
            let (x, y_b) = slice.split_at_mut(stride);
            let (y, b) = y_b.split_at_mut(stride);
            CoeffsMut::I16([x, y, b])
        } else {
            let slice = slice_from_cachelines_mut::<i32>(cachelines);
            let (x, y_b) = slice.split_at_mut(stride);
            let (y, b) = y_b.split_at_mut(stride);
            CoeffsMut::I32([x, y, b])
        }
    }

    #[inline(always)]
    fn zero_channel(&mut self, c: usize, num_coeffs: usize) {
        match self {
            Self::I32(c_arr) => c_arr[c][..num_coeffs].fill(0),
            Self::I16(c_arr) => c_arr[c][..num_coeffs].fill(0),
        }
    }

    #[allow(clippy::too_many_arguments)]
    pub fn decode_channel(
        &mut self,
        c: usize,
        offset: usize,
        permutation: &Permutation,
        num_blocks: usize,
        num_coeffs: usize,
        log_num_blocks: usize,
        histo_offset: usize,
        shift: u32,
        nonzeros: usize,
        reader: &mut SymbolReader,
        histograms: &Histograms,
        br: &mut BitReader,
    ) -> Result<(), Error> {
        match self {
            Self::I32(c_arr) => decode_channel_coeffs(
                &mut c_arr[c][offset..offset + num_coeffs],
                permutation,
                num_blocks,
                num_coeffs,
                log_num_blocks,
                histo_offset,
                shift,
                nonzeros,
                reader,
                histograms,
                br,
            ),
            Self::I16(c_arr) => decode_channel_coeffs(
                &mut c_arr[c][offset..offset + num_coeffs],
                permutation,
                num_blocks,
                num_coeffs,
                log_num_blocks,
                histo_offset,
                shift,
                nonzeros,
                reader,
                histograms,
                br,
            ),
        }
    }
}

#[inline(always)]
fn dequant_channel<D: SimdDescriptor, S: CoeffStorage>(
    d: D,
    scaled_dequant: f32,
    dequant_matrix: &[f32],
    (bias_c, bias_num): (f32, f32),
    qblock_c: &[S],
    cfl: Option<(f32, &[f32])>,
    block_c: &mut [f32],
) {
    let dm_mul = D::F32Vec::splat(d, scaled_dequant);
    if let Some((cc_mul, block_y)) = cfl {
        let cc_vec = D::F32Vec::splat(d, cc_mul);
        for k in (0..dequant_matrix.len()).step_by(D::F32Vec::LEN) {
            let mul = D::F32Vec::load(d, &dequant_matrix[k..]) * dm_mul;
            let quantized = S::load_simd(qblock_c, d, k);
            let dequant_c = adjust_quant_bias(d, quantized, bias_c, bias_num) * mul;
            let dequant_y = D::F32Vec::load(d, &block_y[k..]);
            cc_vec
                .mul_add(dequant_y, dequant_c)
                .store(&mut block_c[k..]);
        }
    } else {
        for k in (0..dequant_matrix.len()).step_by(D::F32Vec::LEN) {
            let mul = D::F32Vec::load(d, &dequant_matrix[k..]) * dm_mul;
            let quantized = S::load_simd(qblock_c, d, k);
            let dequant = adjust_quant_bias(d, quantized, bias_c, bias_num) * mul;
            dequant.store(&mut block_c[k..]);
        }
    }
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
fn dequant_block_impl<D: SimdDescriptor, S: CoeffStorage>(
    d: D,
    scaled_dequant_y: f32,
    x_dm_multiplier: f32,
    b_dm_multiplier: f32,
    x_cc_mul: f32,
    b_cc_mul: f32,
    size: usize,
    matrices: &[f32],
    biases: &[f32; 4],
    qb: &[&[S]; 3],
    offset: usize,
    block: &mut [Vec<f32>; 3],
    active: [bool; 3],
    nz: [usize; 3],
    dc_only: [bool; 3],
) {
    let [block_x, block_y, block_b] = block;

    // Channel 1 (Y)
    if nz[1] > 0 {
        dequant_channel(
            d,
            scaled_dequant_y,
            &matrices[size..2 * size],
            (biases[1], biases[3]),
            &qb[1][offset..],
            None,
            block_y,
        );
    } else if !dc_only[1] {
        block_y[..size].fill(0.0);
    }

    // Channels 0 (X) and 2 (B)
    for (c, block_c, dm_mul, cc_mul) in [
        (0, block_x, x_dm_multiplier, x_cc_mul),
        (2, block_b, b_dm_multiplier, b_cc_mul),
    ] {
        if !active[c] || dc_only[c] {
            continue;
        }
        let has_cfl = nz[1] > 0 && cc_mul != 0.0;
        if nz[c] == 0 {
            if has_cfl {
                let cc_vec = D::F32Vec::splat(d, cc_mul);
                for k in (0..size).step_by(D::F32Vec::LEN) {
                    (D::F32Vec::load(d, &block_y[k..]) * cc_vec).store(&mut block_c[k..]);
                }
            } else {
                block_c[..size].fill(0.0);
            }
        } else {
            dequant_channel(
                d,
                scaled_dequant_y * dm_mul,
                &matrices[c * size..(c + 1) * size],
                (biases[c], biases[3]),
                &qb[c][offset..],
                has_cfl.then(|| (cc_mul, &block_y[..size])),
                block_c,
            );
        }
    }
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
    biases: &[f32; 4],
    coeffs: &CoeffsMut,
    offset: usize,
    block: &mut [Vec<f32>; 3],
    active: [bool; 3],
    nz: [usize; 3],
    dc_only: [bool; 3],
) {
    let scaled_dequant_y = inv_global_scale / (quant as f32);
    let matrices = dequant_matrices.matrix(hf_type, 0);

    assert!(BLOCK_SIZE.is_multiple_of(D::F32Vec::LEN));
    match coeffs {
        CoeffsMut::I32(qb) => {
            let qb_refs = [&qb[0][..], &qb[1][..], &qb[2][..]];
            dequant_block_impl(
                d,
                scaled_dequant_y,
                x_dm_multiplier,
                b_dm_multiplier,
                x_cc_mul,
                b_cc_mul,
                size,
                matrices,
                biases,
                &qb_refs,
                offset,
                block,
                active,
                nz,
                dc_only,
            );
        }
        CoeffsMut::I16(qb) => {
            let qb_refs = [&qb[0][..], &qb[1][..], &qb[2][..]];
            dequant_block_impl(
                d,
                scaled_dequant_y,
                x_dm_multiplier,
                b_dm_multiplier,
                x_cc_mul,
                b_cc_mul,
                size,
                matrices,
                biases,
                &qb_refs,
                offset,
                block,
                active,
                nz,
                dc_only,
            );
        }
    }
}

#[inline(always)]
fn copy_lf_rows<const XS: usize>(
    lf: &mut [f32],
    rect: &ImageRect<f32>,
    x0: usize,
    y0: usize,
    ys: usize,
) {
    for (y, dst) in lf.chunks_exact_mut(XS).take(ys).enumerate() {
        dst.copy_from_slice(&rect.row(y0 + y)[x0..x0 + XS]);
    }
}

#[inline(always)]
fn copy_block_rows<const W: usize>(
    src: &[f32],
    output_rect: &mut crate::image::ImageRectMut<f32>,
    h: usize,
) {
    for (i, chunk) in src.chunks_exact(W).take(h).enumerate() {
        output_rect.row(i)[..W].copy_from_slice(chunk);
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
    lf_rects: &[ImageRect<f32>; 3],
    transform_type: HfTransformType,
    block_rect: Rect,
    num_blocks: usize,
    num_coeffs: usize,
    coeffs: &CoeffsMut,
    offset: usize,
    dequant_matrices: &DequantMatrices,
    nz: [usize; 3],
) -> Result<(), Error> {
    let active = [
        (sbx[0] << hshift[0]) == bx && (sby[0] << vshift[0]) == by,
        (sbx[1] << hshift[1]) == bx && (sby[1] << vshift[1]) == by,
        (sbx[2] << hshift[2]) == bx && (sby[2] << vshift[2]) == by,
    ];
    let dc_only = [
        num_blocks == 1 && nz[0] == 0 && (nz[1] == 0 || x_cc_mul == 0.0),
        num_blocks == 1 && nz[1] == 0,
        num_blocks == 1 && nz[2] == 0 && (nz[1] == 0 || b_cc_mul == 0.0),
    ];
    if !dc_only[0] || !dc_only[1] || !dc_only[2] {
        for (c, b) in transform_buffer.iter_mut().enumerate() {
            if active[c] && !dc_only[c] && b.len() < num_coeffs {
                b.resize(num_coeffs, 0.0);
            }
        }
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
            quant_biases,
            coeffs,
            offset,
            transform_buffer,
            active,
            nz,
            dc_only,
        );
    }
    let xs = covered_blocks_x(transform_type) as usize;
    let ys = covered_blocks_y(transform_type) as usize;
    for c in [1, 0, 2] {
        if !active[c] {
            continue;
        }
        let downsampled_rect = Rect {
            origin: (
                block_rect.origin.0 >> hshift[c],
                block_rect.origin.1 >> vshift[c],
            ),
            size: block_rect.size,
        };
        if dc_only[c] {
            let dc = lf_rects[c].row(sby[c])[sbx[c]];
            let mut output_rect = pixels[c].get_rect_mut(downsampled_rect);
            let row_val = [dc; 8];
            for i in 0..8 {
                output_rect.row(i)[..8].copy_from_slice(&row_val);
            }
            continue;
        }
        let lf = &mut scratch[..];
        let rect = &lf_rects[c];
        let x0 = sbx[c];
        let y0 = sby[c];
        match (xs, ys) {
            (1, 1) => lf[0] = rect.row(y0)[x0],
            (1, _) => copy_lf_rows::<1>(lf, rect, x0, y0, ys),
            (2, _) => copy_lf_rows::<2>(lf, rect, x0, y0, ys),
            (4, _) => copy_lf_rows::<4>(lf, rect, x0, y0, ys),
            (8, _) => copy_lf_rows::<8>(lf, rect, x0, y0, ys),
            (16, _) => copy_lf_rows::<16>(lf, rect, x0, y0, ys),
            (32, _) => copy_lf_rows::<32>(lf, rect, x0, y0, ys),
            _ => unreachable!(),
        }
        let tb = &mut transform_buffer[c][..];
        d.call(
            #[inline(always)]
            |d| transform_to_pixels_impl(d, transform_type, lf, tb),
        );
        let mut output_rect = pixels[c].get_rect_mut(downsampled_rect);
        let h = downsampled_rect.size.1;
        let src = &transform_buffer[c];
        match downsampled_rect.size.0 {
            8 => copy_block_rows::<8>(src, &mut output_rect, h),
            16 => copy_block_rows::<16>(src, &mut output_rect, h),
            32 => copy_block_rows::<32>(src, &mut output_rect, h),
            64 => copy_block_rows::<64>(src, &mut output_rect, h),
            128 => copy_block_rows::<128>(src, &mut output_rect, h),
            256 => copy_block_rows::<256>(src, &mut output_rect, h),
            _ => unreachable!(),
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
        lf_rects: &[ImageRect<f32>; 3],
        transform_type: HfTransformType,
        block_rect: Rect,
        num_blocks: usize,
        num_coeffs: usize,
        coeffs: &CoeffsMut,
        offset: usize,
        dequant_matrices: &DequantMatrices,
        nz: [usize; 3],
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
            coeffs,
            offset,
            dequant_matrices,
            nz,
        )
    }
);

struct PassInfo<'a, 'b> {
    histogram_index: usize,
    reader: Option<SymbolReader>,
    br: &'a mut BitReader<'b>,
    shift: u32,
    pass: usize,
    num_nzeros: &'a mut [[u8; 32]; 3],
}

impl<'a, 'b> PassInfo<'a, 'b> {
    fn new(
        hf_global: &HfGlobalState,
        frame_header: &FrameHeader,
        pass: usize,
        br: &'a mut BitReader<'b>,
        num_nzeros: &'a mut [[u8; 32]; 3],
        lz77_window: &mut Vec<u32>,
    ) -> Result<Self> {
        let num_histo_bits = hf_global.num_histograms.ceil_log2();
        debug!(?pass);
        let histogram_index = br.read(num_histo_bits as usize)? as usize;
        debug!(?histogram_index);
        // `num_histo_bits` is `ceil_log2(num_histograms)`, so the bitstream can
        // legally encode values >= `num_histograms` when it is not a power of
        // two. Reject those here, otherwise downstream indexing into the
        // per-pass histogram context map can go out of bounds.
        if histogram_index >= hf_global.num_histograms as usize {
            return Err(Error::InvalidHistogramIndex(
                histogram_index,
                hf_global.num_histograms as usize,
            ));
        }
        let reader = Some(SymbolReader::new(
            &hf_global.passes[pass].histograms,
            br,
            None,
            lz77_window,
        )?);
        let shift = if pass < frame_header.passes.shift.len() {
            frame_header.passes.shift[pass]
        } else {
            0
        };

        Ok(Self {
            histogram_index,
            reader,
            br,
            shift,
            pass,
            num_nzeros,
        })
    }
}

#[allow(clippy::too_many_arguments)]
#[allow(clippy::type_complexity)]
pub fn decode_vardct_group(
    group: usize,
    passes: &mut [(usize, BitReader)],
    frame_header: &FrameHeader,
    lf_global: &LfGlobalState,
    hf_global: &HfGlobalState,
    hf_meta: &HfMetadata,
    lf_image: &[Image<f32>; 3],
    quant_biases: &[f32; 4],
    pixels: &mut Option<[Image<f32>; 3]>,
    buffers: &mut VarDctBuffers,
) -> Result<(), Error> {
    let x_dm_multiplier = (1.0 / (1.25)).powf(frame_header.x_qm_scale as f32 - 2.0);
    let b_dm_multiplier = (1.0 / (1.25)).powf(frame_header.b_qm_scale as f32 - 2.0);

    let block_group_rect = frame_header.block_group_rect(group);
    debug!(?block_group_rect);
    let log_group_dim = frame_header.log_group_dim();
    buffers.ensure_num_nzeros(passes.len());
    let (pass_chunks, _) = buffers.num_nzeros_storage[..passes.len() * 3].as_chunks_mut::<3>();

    let mut pass_info = passes
        .iter_mut()
        .zip(pass_chunks)
        .zip(buffers.lz77_windows.iter_mut())
        .map(|(((pass, br), nz), lz77_window)| {
            PassInfo::new(hf_global, frame_header, *pass, br, nz, lz77_window)
        })
        .collect::<Result<SmallVec<_, 4>>>()?;

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
    let quant_lf_rect = hf_meta.quant_lf.get_rect(block_group_rect);
    let block_context_map = lf_global.block_context_map.as_ref().unwrap();
    let use_i16 = hf_global.use_i16;
    let is_multi_pass =
        !hf_global.hf_coefficients.is_empty() && pass_info.len() < hf_global.passes.len();
    let mut locked_coeffs = if is_multi_pass {
        let mut guard = hf_global.hf_coefficients[group].try_lock().unwrap();
        if guard.is_empty() {
            let num_cache_lines = if use_i16 {
                num_cache_lines_for::<i16>(GROUP_DIM * GROUP_DIM * 3)
            } else {
                num_cache_lines_for::<i32>(GROUP_DIM * GROUP_DIM * 3)
            };
            guard.try_reserve_exact(num_cache_lines)?;
            guard.resize(num_cache_lines, CacheLine::default());
        }
        Some(guard)
    } else {
        None
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

    let lf_rects = {
        // Subsampled LF image is at the top-left corner of each LF group.
        let lfgx = block_group_rect.origin.0 >> log_group_dim;
        let lfgy = block_group_rect.origin.1 >> log_group_dim;
        let lfbx = lfgx << log_group_dim;
        let lfby = lfgy << log_group_dim;

        let lf_area: [Rect; 3] = core::array::from_fn(|i| Rect {
            origin: (
                lfbx + ((block_group_rect.origin.0 - lfbx) >> hshift[i]),
                lfby + ((block_group_rect.origin.1 - lfby) >> vshift[i]),
            ),
            size: (
                block_group_rect.size.0 >> hshift[i],
                block_group_rect.size.1 >> vshift[i],
            ),
        });
        [
            lf_image[0].get_rect(lf_area[0]),
            lf_image[1].get_rect(lf_area[1]),
            lf_image[2].get_rect(lf_area[2]),
        ]
    };

    for by in 0..block_group_rect.size.1 {
        let sby = [by >> vshift[0], by >> vshift[1], by >> vshift[2]];
        let ty = by / COLOR_TILE_DIM_IN_BLOCKS;

        let row_cmap_x = ytox_map.row(ty);
        let row_cmap_b = ytob_map.row(ty);
        let row_raw_quant = raw_quant_map.row(by);
        let row_quant_lf = quant_lf_rect.row(by);
        let row_transform = transform_map.row(by);

        for bx in 0..block_group_rect.size.0 {
            let raw_transform_id = row_transform[bx];
            let transform_id = raw_transform_id & 127;
            let is_first_block = raw_transform_id >= 128;
            if !is_first_block {
                continue;
            }
            let sbx = [bx >> hshift[0], bx >> hshift[1], bx >> hshift[2]];
            let tx = bx / COLOR_TILE_DIM_IN_BLOCKS;
            let x_cc_mul = color_correlation_params.y_to_x(row_cmap_x[tx] as i32);
            let b_cc_mul = color_correlation_params.y_to_b(row_cmap_b[tx] as i32);
            let raw_quant = row_raw_quant[bx] as u32;
            let quant_lf = row_quant_lf[bx] as usize;
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

            let active = [
                (sbx[0] << hshift[0]) == bx && (sby[0] << vshift[0]) == by,
                (sbx[1] << hshift[1]) == bx && (sby[1] << vshift[1]) == by,
                (sbx[2] << hshift[2]) == bx && (sby[2] << vshift[2]) == by,
            ];

            let (mut coeffs, offset) = match &mut locked_coeffs {
                Some(locked) => (
                    CoeffsMut::from_cachelines(&mut locked[..], use_i16, GROUP_DIM * GROUP_DIM),
                    coeffs_offset,
                ),
                None => (
                    get_single_pass_coeffs(&mut buffers.coeffs_storage, use_i16, num_coeffs),
                    0,
                ),
            };

            let mut block_nonzeros = if is_multi_pass { [1; 3] } else { [0; 3] };

            for PassInfo {
                histogram_index,
                reader,
                br,
                shift,
                pass,
                num_nzeros,
            } in pass_info.iter_mut()
            {
                let reader = reader.as_mut().unwrap();
                let pass_info = &hf_global.passes[*pass];
                let context_offset = *histogram_index * block_context_map.num_ac_contexts();
                for c in [1, 0, 2] {
                    if !active[c] {
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
                    let nonzeros =
                        reader.read_unsigned_inline(&pass_info.histograms, br, nonzero_context)
                            as usize;
                    trace!(
                        "block ({},{},{c}) predicted_nzeros: {predicted_nzeros} \
                       nzero_ctx: {nonzero_context} (offset: {context_offset}) \
                       nzeros: {nonzeros}",
                        sbx[c], sby[c]
                    );
                    if nonzeros + num_blocks > num_coeffs {
                        return Err(Error::InvalidNumNonZeros(nonzeros, num_blocks));
                    }
                    let val = nonzeros.shrc(log_num_blocks) as u8;
                    num_nzeros[c][sbx[c]..sbx[c] + cx].fill(val);
                    if nonzeros == 0 {
                        continue;
                    }
                    if !is_multi_pass && block_nonzeros[c] == 0 {
                        coeffs.zero_channel(c, num_coeffs);
                    }
                    block_nonzeros[c] += nonzeros;
                    let histo_offset = block_context_map.zero_density_context_offset(block_context)
                        + context_offset;
                    let permutation = &pass_info.coeff_orders[shape_id * 3 + c];
                    coeffs.decode_channel(
                        c,
                        offset,
                        permutation,
                        num_blocks,
                        num_coeffs,
                        log_num_blocks,
                        histo_offset,
                        *shift,
                        nonzeros,
                        reader,
                        &pass_info.histograms,
                        br,
                    )?;
                }
            }
            if let Some(pixels) = pixels {
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
                    &coeffs,
                    offset,
                    dequant_matrices,
                    block_nonzeros,
                )?;
            }
            if is_multi_pass {
                coeffs_offset += num_coeffs;
            }
        }
    }
    for (
        PassInfo {
            pass, br, reader, ..
        },
        lz77_window,
    ) in pass_info.iter_mut().zip(buffers.lz77_windows.iter_mut())
    {
        let mut reader = std::mem::take(reader).unwrap();
        reader.reclaim_lz77_scratch(lz77_window);
        reader.check_final_state(&hf_global.passes[*pass].histograms, br)?;
    }
    Ok(())
}

#[cfg(test)]
mod test {
    use test_log::test;

    use crate::error::Result;
    use crate::image::Rect;
    use crate::tests::decode::decode;

    #[test]
    fn subsampled_chroma() -> Result<()> {
        let mut frames = decode::<f32>(
            include_bytes!("../../resources/test/multiple_lf_420.jxl"),
            Default::default(),
        )?;
        let frame = frames.pop().unwrap();
        let [image]: [_; 1] = frame.try_into().unwrap();

        let rect_lfs = [
            // Green rect
            Rect {
                origin: (2048 * 3, 0),
                size: (16 * 3, 16),
            },
            // Red rect
            Rect {
                origin: (0, 2048),
                size: (16 * 3, 16),
            },
        ];
        for rect in rect_lfs {
            let view = image.get_rect(rect);
            for y in 0..view.size().1 {
                let row = view.row(y);
                for pixel in row.chunks(3) {
                    let &[r, g, b] = pixel else {
                        unreachable!();
                    };
                    let max = r.max(g).max(b);
                    let min = r.min(g).min(b);
                    assert!(max - min > 0.5);
                }
            }
        }

        Ok(())
    }

    #[test]
    fn dequant_i16_matches_i32() {
        use super::*;

        let coeffs_i32_0 = vec![10i32; 64];
        let coeffs_i32_1 = vec![-5i32; 64];
        let coeffs_i32_2 = vec![20i32; 64];
        let coeffs_i16_0: Vec<i16> = coeffs_i32_0.iter().map(|&x| x as i16).collect();
        let coeffs_i16_1: Vec<i16> = coeffs_i32_1.iter().map(|&x| x as i16).collect();
        let coeffs_i16_2: Vec<i16> = coeffs_i32_2.iter().map(|&x| x as i16).collect();

        let q_i32 = [&coeffs_i32_0[..], &coeffs_i32_1[..], &coeffs_i32_2[..]];
        let q_i16 = [&coeffs_i16_0[..], &coeffs_i16_1[..], &coeffs_i16_2[..]];

        let d = jxl_simd::ScalarDescriptor::new().unwrap();
        let dequant_matrices = vec![1.0f32; 3 * 64];
        let biases = [0.5, 0.5, 0.5, 0.5];

        let mut block_i32 = [vec![0.0f32; 64], vec![0.0f32; 64], vec![0.0f32; 64]];
        let mut block_i16 = [vec![0.0f32; 64], vec![0.0f32; 64], vec![0.0f32; 64]];

        dequant_block_impl(
            d,
            1.0,
            1.0,
            1.0,
            0.0,
            0.0,
            64,
            &dequant_matrices,
            &biases,
            &q_i32,
            0,
            &mut block_i32,
            [true; 3],
            [64; 3],
            [false; 3],
        );
        dequant_block_impl(
            d,
            1.0,
            1.0,
            1.0,
            0.0,
            0.0,
            64,
            &dequant_matrices,
            &biases,
            &q_i16,
            0,
            &mut block_i16,
            [true; 3],
            [64; 3],
            [false; 3],
        );

        for c in 0..3 {
            for i in 0..64 {
                assert_eq!(block_i32[c][i], block_i16[c][i], "c: {c}, i: {i}");
            }
        }
    }
}
