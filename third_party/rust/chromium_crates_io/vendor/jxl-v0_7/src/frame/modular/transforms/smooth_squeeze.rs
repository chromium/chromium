// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::sync::OnceLock;

use jxl_simd::{F32SimdVec, I32SimdVec, SimdDescriptor, simd_function};

use super::step::TiledChannelView;
use crate::frame::modular::ModularStorage;
use crate::image::{ImageRectMut, OwnedRawImage, Rect};
use crate::util::{DITHER_TABLE, fast_jinc_windowed_sq_simd};

fn compute_jinc_subkernel(delta_x: f32, delta_y: f32) -> [f32; 25] {
    let mut w = [0.0f32; 25];
    let mut sum = 0.0f32;
    for ty in 0..5 {
        let py = (ty as f32 - 2.0) - delta_y;
        for tx in 0..5 {
            let px = (tx as f32 - 2.0) - delta_x;
            let r2 = px * px + py * py;
            let weight = crate::util::fast_jinc_windowed_sq(r2);
            w[ty * 5 + tx] = weight;
            sum += weight;
        }
    }
    let inv_sum = 1.0 / sum;
    for weight in &mut w {
        *weight *= inv_sum;
    }
    w
}

#[inline(always)]
fn compute_row_kernel<D: SimdDescriptor>(
    d: D,
    fx: usize,
    delta_y: f32,
    num_weights: usize,
    kernel_storage: &mut [f32],
) {
    const IOTA: [f32; 16] = [
        0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0,
    ];
    const { assert!(D::F32Vec::LEN <= 16) };
    let lanes = D::F32Vec::LEN;
    let inv_fx = 1.0 / (fx as f32);
    let num_chunks = num_weights.div_ceil(lanes);

    let idx_vec = D::F32Vec::load(d, &IOTA[..lanes]);

    for chunk_idx in 0..num_chunks {
        let ox_base = chunk_idx * lanes;
        let ox_vec = D::F32Vec::splat(d, ox_base as f32) + idx_vec;
        let delta_x_vec = (ox_vec + D::F32Vec::splat(d, 0.5)) * D::F32Vec::splat(d, inv_fx)
            - D::F32Vec::splat(d, 0.5);

        let mut subk_weights = [D::F32Vec::zero(d); 25];
        let mut sum_vec = D::F32Vec::zero(d);

        for ty in 0..5 {
            let py = (ty as f32 - 2.0) - delta_y;
            let py2_vec = D::F32Vec::splat(d, py * py);
            for tx in 0..5 {
                let px_vec = D::F32Vec::splat(d, tx as f32 - 2.0) - delta_x_vec;
                let r2_vec = px_vec.mul_add(px_vec, py2_vec);
                let w_vec = fast_jinc_windowed_sq_simd(d, r2_vec);
                subk_weights[ty * 5 + tx] = w_vec;
                sum_vec += w_vec;
            }
        }

        let inv_sum_vec = D::F32Vec::splat(d, 1.0) / sum_vec;
        let chunk_offset = chunk_idx * 25 * lanes;
        for (tap, w_vec) in subk_weights.iter().enumerate() {
            let norm_w = *w_vec * inv_sum_vec;
            norm_w.store(&mut kernel_storage[chunk_offset + tap * lanes..]);
        }
    }
}

fn compute_jinc_squeeze_kernel(shift_diff: (usize, usize)) -> Vec<[f32; 25]> {
    let (dx, dy) = shift_diff;
    let fx = 1usize << dx;
    let fy = 1usize << dy;
    let mut kernel = vec![[0.0f32; 25]; fx * fy];

    for oy in 0..fy {
        let delta_y = (oy as f32 + 0.5) / (fy as f32) - 0.5;
        for ox in 0..fx {
            let delta_x = (ox as f32 + 0.5) / (fx as f32) - 0.5;
            kernel[oy * fx + ox] = compute_jinc_subkernel(delta_x, delta_y);
        }
    }
    kernel
}

const SMALL_SHIFT_LIMIT: usize = 4;
static SMALL_KERNEL_CACHE: [[OnceLock<Vec<[f32; 25]>>; SMALL_SHIFT_LIMIT]; SMALL_SHIFT_LIMIT] =
    [const { [const { OnceLock::new() }; SMALL_SHIFT_LIMIT] }; SMALL_SHIFT_LIMIT];

fn get_small_squeeze_kernel(shift_diff: (usize, usize)) -> &'static [[f32; 25]] {
    let (dx, dy) = shift_diff;
    debug_assert!(dx < SMALL_SHIFT_LIMIT && dy < SMALL_SHIFT_LIMIT);
    SMALL_KERNEL_CACHE[dx][dy].get_or_init(|| compute_jinc_squeeze_kernel(shift_diff))
}

#[derive(Debug, Default)]
pub(crate) struct SmoothUpsampleScratch {
    buffer: [Vec<f32>; 5],
    ibuf: Vec<i32>,
    out_buf: Vec<i32>,
    kernel_storage: Vec<f32>,
    row_float: Vec<f32>,
}

impl SmoothUpsampleScratch {
    fn init(&mut self, in_len: usize, out_len: usize, kernel_len: usize) {
        for b in &mut self.buffer {
            b.resize(in_len, 0.0);
        }
        self.ibuf.resize(in_len, 0);
        self.out_buf.resize(out_len, 0);
        self.row_float.resize(out_len, 0.0);
        self.kernel_storage.resize(kernel_len, 0.0);
    }
}

fn make_float<D: SimdDescriptor>(d: D, inp: &[i32], out: &mut [f32]) {
    for (i, o) in inp
        .chunks_exact(D::I32Vec::LEN)
        .zip(out.chunks_exact_mut(D::F32Vec::LEN))
    {
        D::I32Vec::load(d, i).as_f32().store(o);
    }
}

#[inline(always)]
fn store_interleaved_f32<D: SimdDescriptor, const FX: usize>(
    out: &[D::F32Vec; FX],
    dest: &mut [f32],
) {
    match FX {
        1 => out[0].store(dest),
        2 => D::F32Vec::store_interleaved_2(out[0], out[1], dest),
        4 => D::F32Vec::store_interleaved_4(out[0], out[1], out[2], out[3], dest),
        8 => D::F32Vec::store_interleaved_8(
            out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7], dest,
        ),
        _ => unreachable!(),
    }
}

#[inline(always)]
fn process_row<D: SimdDescriptor, const FX: usize>(
    d: D,
    weights: &[[f32; 25]],
    in_xs: usize,
    buffer: &[Vec<f32>; 5],
    row_float: &mut [f32],
) {
    let lanes = D::F32Vec::LEN;
    let num_chunks = in_xs.div_ceil(lanes);

    for chunk_idx in 0..num_chunks {
        let ix = chunk_idx * lanes;
        let mut out = [D::F32Vec::zero(d); FX];

        for ty in 0..5 {
            let row = &buffer[ty][ix..];
            for tx in 0..5 {
                let v = D::F32Vec::load(d, &row[tx..]);
                for (ox, out_vec) in out.iter_mut().enumerate() {
                    let w = weights[ox][ty * 5 + tx];
                    *out_vec = v.mul_add(D::F32Vec::splat(d, w), *out_vec);
                }
            }
        }

        store_interleaved_f32::<D, FX>(&out, &mut row_float[FX * ix..]);
    }
}

#[inline(always)]
fn process_row_large<D: SimdDescriptor>(
    d: D,
    kernel_storage: &[f32],
    fx: usize,
    xs: usize,
    buffer: &[Vec<f32>; 5],
    row_float: &mut [f32],
) {
    let lanes = D::F32Vec::LEN;
    let in_xs = xs.div_ceil(fx);

    for ix in 0..in_xs {
        let base_x = ix * fx;
        let mut c = [D::F32Vec::zero(d); 25];
        for ty in 0..5 {
            let row = &buffer[ty][ix..];
            for tx in 0..5 {
                c[ty * 5 + tx] = D::F32Vec::splat(d, row[tx]);
            }
        }

        let num_ox = (xs - base_x).min(fx);
        let num_chunks = num_ox.div_ceil(lanes);
        for ox_chunk in 0..num_chunks {
            let ox_base = ox_chunk * lanes;
            let chunk_offset = ox_chunk * 25 * lanes;
            let mut out_vec = D::F32Vec::zero(d);
            for tap in 0..25 {
                let w_vec = D::F32Vec::load(d, &kernel_storage[chunk_offset + tap * lanes..]);
                out_vec = c[tap].mul_add(w_vec, out_vec);
            }
            out_vec.store(&mut row_float[base_x + ox_base..]);
        }
    }
}

#[inline(always)]
fn dither_round_and_store<D: SimdDescriptor>(
    d: D,
    dither: bool,
    dither_y: usize,
    x0: usize,
    xs: usize,
    row_float: &[f32],
    output_row: &mut [i32],
) {
    const { assert!(D::F32Vec::LEN <= 16) };
    let lanes = D::F32Vec::LEN;
    let half = D::F32Vec::splat(d, 0.5);
    let num_chunks = xs.div_ceil(lanes);

    for chunk_idx in 0..num_chunks {
        let x = chunk_idx * lanes;
        let cur_len = (xs - x).min(lanes);
        let val = D::F32Vec::load(d, &row_float[x..]);
        let dither_val = if dither {
            let dither_x = (x0 + x) % 32;
            D::F32Vec::load(d, &DITHER_TABLE[dither_y][dither_x..])
        } else {
            D::F32Vec::zero(d)
        };
        let dithered = val + dither_val;
        let rounded = (dithered + half.copysign(dithered)).as_i32();
        if cur_len == lanes && output_row.len() >= x + lanes {
            rounded.store(&mut output_row[x..]);
        } else {
            let mut temp = [0i32; 16];
            rounded.store(&mut temp[..lanes]);
            let to_copy = cur_len.min(output_row.len() - x);
            output_row[x..x + to_copy].copy_from_slice(&temp[..to_copy]);
        }
    }
}

#[allow(clippy::too_many_arguments)]
#[inline(always)]
fn smooth_upsample_simd_impl<D: SimdDescriptor>(
    d: D,
    input: &TiledChannelView<'_>,
    shift_diff: (usize, usize),
    dither: bool,
    rect: Rect,
    output: &mut OwnedRawImage,
    storage: ModularStorage,
    scratch: &mut SmoothUpsampleScratch,
) {
    let (dx, dy) = shift_diff;
    let (fx, fy) = (1usize << dx, 1usize << dy);
    let (x0, y0) = (rect.origin.0, rect.origin.1);
    let (xs, ys) = (rect.size.0, rect.size.1);
    let (in_xs, in_ys) = (xs.div_ceil(fx), ys.div_ceil(fy));
    let (col_offset, row_offset) = (x0 / fx, y0 / fy);
    let lanes = D::I32Vec::LEN;

    if in_xs == 0 || in_ys == 0 {
        return;
    }
    let num_chunks = in_xs.div_ceil(lanes);
    let in_len = (num_chunks + 1) * lanes + 8;
    let out_len = if fx <= 8 {
        (num_chunks + 1) * lanes * fx
    } else {
        xs.next_multiple_of(lanes)
    };
    let num_weights = xs.min(fx);
    let kernel_len = if fx > 8 {
        num_weights.div_ceil(lanes) * 25 * lanes
    } else {
        0
    };
    scratch.init(in_len, out_len, kernel_len);

    for (dy_idx, buf) in scratch.buffer.iter_mut().enumerate().take(4) {
        let yg = (row_offset + dy_idx) as isize - 2;
        if storage == ModularStorage::I16 {
            input.load_row_to_scratch::<i16>(yg, col_offset, in_xs + 4, &mut scratch.ibuf);
        } else {
            input.load_row_to_scratch::<i32>(yg, col_offset, in_xs + 4, &mut scratch.ibuf);
        }
        make_float(d, &scratch.ibuf, buf);
    }

    let is_small = dx < SMALL_SHIFT_LIMIT && dy < SMALL_SHIFT_LIMIT;
    let small_kernel = if is_small {
        Some(get_small_squeeze_kernel(shift_diff))
    } else {
        None
    };

    let mut small_dynamic_weights = [[0.0f32; 25]; 8];

    for iy_center in 0..in_ys {
        let yg = (row_offset + iy_center) as isize + 2;
        if storage == ModularStorage::I16 {
            input.load_row_to_scratch::<i16>(yg, col_offset, in_xs + 4, &mut scratch.ibuf);
        } else {
            input.load_row_to_scratch::<i32>(yg, col_offset, in_xs + 4, &mut scratch.ibuf);
        }
        make_float(d, &scratch.ibuf, &mut scratch.buffer[4]);

        for oy in 0..fy {
            let yout = fy * iy_center + oy;
            if yout >= ys {
                continue;
            }
            let dither_y = (y0 + yout) % 32;
            let delta_y = (oy as f32 + 0.5) / (fy as f32) - 0.5;

            if fx <= 8 {
                let row_weights: &[[f32; 25]] = if let Some(k) = small_kernel {
                    &k[oy * fx..(oy + 1) * fx]
                } else {
                    for (ox, w) in small_dynamic_weights.iter_mut().enumerate().take(fx) {
                        let delta_x = (ox as f32 + 0.5) / (fx as f32) - 0.5;
                        *w = compute_jinc_subkernel(delta_x, delta_y);
                    }
                    &small_dynamic_weights[..fx]
                };

                match fx {
                    1 => process_row::<D, 1>(
                        d,
                        row_weights,
                        in_xs,
                        &scratch.buffer,
                        &mut scratch.row_float,
                    ),
                    2 => process_row::<D, 2>(
                        d,
                        row_weights,
                        in_xs,
                        &scratch.buffer,
                        &mut scratch.row_float,
                    ),
                    4 => process_row::<D, 4>(
                        d,
                        row_weights,
                        in_xs,
                        &scratch.buffer,
                        &mut scratch.row_float,
                    ),
                    8 => process_row::<D, 8>(
                        d,
                        row_weights,
                        in_xs,
                        &scratch.buffer,
                        &mut scratch.row_float,
                    ),
                    _ => unreachable!(),
                }
            } else {
                compute_row_kernel(d, fx, delta_y, num_weights, &mut scratch.kernel_storage);
                process_row_large(
                    d,
                    &scratch.kernel_storage,
                    fx,
                    xs,
                    &scratch.buffer,
                    &mut scratch.row_float,
                );
            }

            let mut img;
            let out_row = if storage == ModularStorage::I16 {
                &mut scratch.out_buf
            } else {
                img = ImageRectMut::<i32>::from_raw(output.as_rect_mut());
                img.row(yout)
            };
            dither_round_and_store(d, dither, dither_y, x0, xs, &scratch.row_float, out_row);
            let mut img;
            if storage == ModularStorage::I16 {
                img = ImageRectMut::<i16>::from_raw(output.as_rect_mut());
                for (dst, &src) in img.row(yout).iter_mut().zip(&scratch.out_buf) {
                    *dst = src as i16;
                }
            }
        }
        scratch.buffer.rotate_left(1);
    }
}

simd_function!(
    smooth_upsample,
    d: D,
    #[allow(clippy::too_many_arguments)]
    pub(super) fn smooth_upsample_dispatch(
        input: &TiledChannelView<'_>,
        shift_diff: (usize, usize),
        dither: bool,
        rect: Rect,
        output: &mut OwnedRawImage,
        storage: ModularStorage,
        scratch: &mut SmoothUpsampleScratch,
    ) {
        smooth_upsample_simd_impl(
            d, input, shift_diff, dither, rect, output, storage, scratch,
        );
    }
);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_compute_jinc_squeeze_kernel_normalized() {
        for &(dx, dy) in &[
            (0, 0),
            (1, 0),
            (0, 1),
            (1, 1),
            (2, 2),
            (3, 3),
            (1, 2),
            (2, 1),
            (4, 4),
        ] {
            let kernel = compute_jinc_squeeze_kernel((dx, dy));
            let fx = 1 << dx;
            let fy = 1 << dy;
            assert_eq!(kernel.len(), fx * fy);
            for w in &kernel {
                let sum: f32 = w.iter().sum();
                assert!(
                    (sum - 1.0).abs() < 1e-5,
                    "Kernel normalization failed for shift ({dx}, {dy}): sum={sum}"
                );
            }
        }
    }

    #[test]
    fn test_compute_jinc_subkernel_symmetry() {
        let k_center = compute_jinc_subkernel(0.0, 0.0);
        // Top-left vs bottom-right symmetry at (0, 0)
        for ty in 0..5 {
            for tx in 0..5 {
                let w1 = k_center[ty * 5 + tx];
                let w2 = k_center[(4 - ty) * 5 + (4 - tx)];
                assert!(
                    (w1 - w2).abs() < 1e-6,
                    "Symmetry mismatch at ({tx}, {ty}): {w1} vs {w2}"
                );
            }
        }

        // Horizontal flip with +/- delta_x
        let k_left = compute_jinc_subkernel(-0.25, 0.0);
        let k_right = compute_jinc_subkernel(0.25, 0.0);
        for ty in 0..5 {
            for tx in 0..5 {
                let w_l = k_left[ty * 5 + tx];
                let w_r = k_right[ty * 5 + (4 - tx)];
                assert!(
                    (w_l - w_r).abs() < 1e-6,
                    "Horizontal symmetry mismatch at ({tx}, {ty}): {w_l} vs {w_r}"
                );
            }
        }
    }

    fn test_compute_row_kernel_simd_equivalent<D: SimdDescriptor>(d: D) {
        let lanes = D::F32Vec::LEN;
        for fx in [8, 15, 16, 32, 64] {
            for num_weights in [1usize, 3, 7, 8, 15, 16, fx] {
                if num_weights > fx {
                    continue;
                }
                for delta_y in [-0.5, -0.25, 0.0, 0.123, 0.456] {
                    let num_chunks = num_weights.div_ceil(lanes);
                    let mut simd_storage = vec![0.0f32; num_chunks * 25 * lanes];
                    compute_row_kernel(d, fx, delta_y, num_weights, &mut simd_storage);
                    for ox in 0..num_weights {
                        let delta_x = (ox as f32 + 0.5) / (fx as f32) - 0.5;
                        let scalar_subk = compute_jinc_subkernel(delta_x, delta_y);
                        let ox_chunk = ox / lanes;
                        let ox_lane = ox % lanes;
                        let chunk_offset = ox_chunk * 25 * lanes;
                        for tap in 0..25 {
                            let simd_val = simd_storage[chunk_offset + tap * lanes + ox_lane];
                            let abs_err = (simd_val - scalar_subk[tap]).abs();
                            assert!(
                                abs_err < 1e-6,
                                "Mismatch at fx={fx}, delta_y={delta_y}, ox={ox}, tap={tap}: simd={}, scalar={}, err={}",
                                simd_val,
                                scalar_subk[tap],
                                abs_err
                            );
                        }
                    }
                }
            }
        }
    }

    jxl_simd::test_all_instruction_sets!(test_compute_row_kernel_simd_equivalent);

    fn test_process_row_large_constant<D: SimdDescriptor>(d: D) {
        let lanes = D::F32Vec::LEN;
        for fx in [16, 32, 64] {
            for xs in [1usize, 3, 7, 15, 16, 20, 32, 50, 64, 100] {
                let in_xs = xs.div_ceil(fx);
                let buffer = [
                    vec![42.0f32; in_xs + 8],
                    vec![42.0f32; in_xs + 8],
                    vec![42.0f32; in_xs + 8],
                    vec![42.0f32; in_xs + 8],
                    vec![42.0f32; in_xs + 8],
                ];
                let mut row_float = vec![0.0f32; xs.next_multiple_of(lanes)];
                let num_weights = xs.min(fx);
                let num_chunks = num_weights.div_ceil(lanes);
                let mut kernel_storage = vec![0.0f32; num_chunks * 25 * lanes];
                compute_row_kernel(d, fx, 0.0, num_weights, &mut kernel_storage);

                process_row_large(d, &kernel_storage, fx, xs, &buffer, &mut row_float);

                for &val in &row_float[..xs] {
                    assert!(
                        (val - 42.0).abs() < 1e-4,
                        "Mismatch at fx={fx}, xs={xs}: val={val}"
                    );
                }
            }
        }
    }

    jxl_simd::test_all_instruction_sets!(test_process_row_large_constant);
}
