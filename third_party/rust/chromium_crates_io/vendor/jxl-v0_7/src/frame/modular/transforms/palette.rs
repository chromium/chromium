// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::error::Result;
use crate::frame::modular::predict::{PredictionData, WeightedPredictorState};
use crate::frame::modular::{ModularChannel, ModularStorage, Predictor};
use crate::headers::bit_depth::BitDepth;
use crate::headers::modular::WeightedHeader;
use crate::image::{Image, ImageRect, ImageRectMut, OwnedRawImage};
use crate::util::sync::{OnceLock, RwLockWriteGuard};

const RGB_CHANNELS: usize = 3;

// 5x5x5 color cube for the larger cube.
const LARGE_CUBE: usize = 5;
const LARGE_CUBE_ENTRIES: usize = LARGE_CUBE * LARGE_CUBE * LARGE_CUBE;

// Smaller interleaved color cube to fill the holes of the larger cube.
const SMALL_CUBE: usize = 4;
const SMALL_CUBE_BITS: usize = 2;
// SMALL_CUBE ** 3
const LARGE_CUBE_OFFSET: usize = SMALL_CUBE * SMALL_CUBE * SMALL_CUBE;

const DELTA_PALETTE_ENTRIES: usize = 143;

#[derive(Debug)]
struct ImplicitPalette {
    deltas: [[i32; DELTA_PALETTE_ENTRIES]; RGB_CHANNELS],
    small_cube: [[i32; LARGE_CUBE_OFFSET]; RGB_CHANNELS],
    large_cube: [[i32; LARGE_CUBE_ENTRIES]; RGB_CHANNELS],
}

impl ImplicitPalette {
    fn new(bit_depth: usize) -> Box<Self> {
        const DELTA_PALETTE: [[i32; 3]; 72] = [
            [0, 0, 0],
            [4, 4, 4],
            [11, 0, 0],
            [0, 0, -13],
            [0, -12, 0],
            [-10, -10, -10],
            [-18, -18, -18],
            [-27, -27, -27],
            [-18, -18, 0],
            [0, 0, -32],
            [-32, 0, 0],
            [-37, -37, -37],
            [0, -32, -32],
            [24, 24, 45],
            [50, 50, 50],
            [-45, -24, -24],
            [-24, -45, -45],
            [0, -24, -24],
            [-34, -34, 0],
            [-24, 0, -24],
            [-45, -45, -24],
            [64, 64, 64],
            [-32, 0, -32],
            [0, -32, 0],
            [-32, 0, 32],
            [-24, -45, -24],
            [45, 24, 45],
            [24, -24, -45],
            [-45, -24, 24],
            [80, 80, 80],
            [64, 0, 0],
            [0, 0, -64],
            [0, -64, -64],
            [-24, -24, 45],
            [96, 96, 96],
            [64, 64, 0],
            [45, -24, -24],
            [34, -34, 0],
            [112, 112, 112],
            [24, -45, -45],
            [45, 45, -24],
            [0, -32, 32],
            [24, -24, 45],
            [0, 96, 96],
            [45, -24, 24],
            [24, -45, -24],
            [-24, -45, 24],
            [0, -64, 0],
            [96, 0, 0],
            [128, 128, 128],
            [64, 0, 64],
            [144, 144, 144],
            [96, 96, 0],
            [-36, -36, 36],
            [45, -24, -45],
            [45, -45, -24],
            [0, 0, -96],
            [0, 128, 128],
            [0, 96, 0],
            [45, 24, -45],
            [-128, 0, 0],
            [24, -45, 24],
            [-45, 24, -45],
            [64, 0, -64],
            [64, -64, -64],
            [96, 0, 96],
            [45, -45, 24],
            [24, 45, -45],
            [64, 64, -64],
            [128, 128, 0],
            [0, 0, -128],
            [-24, 45, -45],
        ];
        const MULTIPLIER: [i32; 2] = [-1, 1];

        let mut deltas = [[0; DELTA_PALETTE_ENTRIES]; RGB_CHANNELS];
        for (c, deltas_c) in deltas.iter_mut().enumerate() {
            for (idx, slot) in deltas_c.iter_mut().enumerate() {
                let mut result = DELTA_PALETTE[(idx + 1) >> 1][c] * MULTIPLIER[idx & 1];
                if bit_depth > 8 {
                    result *= 1 << (bit_depth - 8);
                }
                *slot = result;
            }
        }

        let scale = |value: usize, bit_depth: usize| ((value * ((1 << bit_depth) - 1)) / 4) as i32;

        let mut small_cube = [[0; LARGE_CUBE_OFFSET]; RGB_CHANNELS];
        for (c, cube_c) in small_cube.iter_mut().enumerate() {
            for (idx, slot) in cube_c.iter_mut().enumerate() {
                let shifted = idx >> (c * SMALL_CUBE_BITS);
                *slot =
                    scale(shifted % SMALL_CUBE, bit_depth) + (1 << (0.max(bit_depth as isize - 3)));
            }
        }

        let mut large_cube = [[0; LARGE_CUBE_ENTRIES]; RGB_CHANNELS];
        for (c, cube_c) in large_cube.iter_mut().enumerate() {
            for (idx, slot) in cube_c.iter_mut().enumerate() {
                let val = match c {
                    0 => idx,
                    1 => idx / LARGE_CUBE,
                    2 => idx / (LARGE_CUBE * LARGE_CUBE),
                    _ => unreachable!(),
                };
                *slot = scale(val % LARGE_CUBE, bit_depth);
            }
        }

        Box::new(ImplicitPalette {
            deltas,
            small_cube,
            large_cube,
        })
    }
}

struct Palette<'a> {
    implicit: &'static ImplicitPalette,
    explicit: &'a [u8],
    storage: ModularStorage,
    c: usize,
}

impl<'a> Palette<'a> {
    fn new(
        bit_depth: &BitDepth,
        c: usize,
        buf: &'a ModularChannel,
        storage: ModularStorage,
    ) -> Self {
        static IMPLICIT_PALETTES: [OnceLock<Box<ImplicitPalette>>; 25] =
            [const { OnceLock::new() }; 25];
        let bit_depth = bit_depth.bits_per_sample().min(24) as usize;
        let explicit = {
            if buf.size(storage).0 > 0 {
                buf.data.row(c)
            } else {
                &[]
            }
        };
        Self {
            implicit: IMPLICIT_PALETTES[bit_depth].get_or_init(|| ImplicitPalette::new(bit_depth)),
            explicit,
            storage,
            c,
        }
    }

    #[inline(always)]
    fn get(&self, index: isize) -> i32 {
        let Self {
            implicit,
            explicit,
            storage,
            c,
        } = self;
        let palette_size = explicit.len() / storage.sample_size();
        let uindex = index as usize;
        if index >= 0 && uindex < palette_size {
            if *storage == ModularStorage::I16 {
                i16::from_ne_bytes(explicit[2 * uindex..][..2].try_into().unwrap()) as i32
            } else {
                i32::from_ne_bytes(explicit[4 * uindex..][..4].try_into().unwrap())
            }
        } else if *c >= RGB_CHANNELS {
            0
        } else if index < 0 {
            let idx = (-(index + 1) as usize) % DELTA_PALETTE_ENTRIES;
            implicit.deltas[*c][idx]
        } else {
            let cube_idx = uindex - palette_size;
            if cube_idx < LARGE_CUBE_OFFSET {
                implicit.small_cube[*c][cube_idx]
            } else {
                implicit.large_cube[*c][(cube_idx - LARGE_CUBE_OFFSET) % LARGE_CUBE_ENTRIES]
            }
        }
    }
}

pub(super) struct PaletteStep<'a, 'b> {
    pub buf_in: &'a [&'b ModularChannel],
    pub buf_pal: &'b ModularChannel,
    pub buf_out: &'a mut [&'b mut ModularChannel],
    pub num_deltas: usize,
    pub predictor: Predictor,
    pub wp_header: &'a WeightedHeader,
    pub grid_xsize: usize,
    pub buf_left: Option<&'a [&'b OwnedRawImage]>,
    pub buf_top: Option<&'a [&'b OwnedRawImage]>,
    pub buf_topleft: Option<&'a [&'b OwnedRawImage]>,
    pub prev_aux: Option<&'a [Option<&'b Image<i32>>]>,
    pub aux_out: &'a mut [RwLockWriteGuard<'b, Option<Image<i32>>>],
    pub storage: ModularStorage,
    pub is_partial: bool,
}

#[inline(always)]
fn get_border_pixel(img: &OwnedRawImage, x: usize, y: usize, storage: ModularStorage) -> i32 {
    if storage == ModularStorage::I16 {
        ImageRect::<i16>::from_raw(img.as_rect()).row(y)[x] as i32
    } else {
        ImageRect::<i32>::from_raw(img.as_rect()).row(y)[x]
    }
}

impl<'a, 'b> PaletteStep<'a, 'b> {
    pub fn run(self, scratch: &mut [Vec<i32>; 4]) -> Result<()> {
        let PaletteStep {
            buf_in,
            buf_pal,
            buf_out,
            num_deltas,
            predictor,
            wp_header,
            grid_xsize,
            buf_left,
            buf_top,
            buf_topleft,
            prev_aux,
            aux_out,
            storage,
            is_partial,
        } = self;
        let (w0, h) = buf_in[0].size(storage);
        if w0 == 0 || h == 0 {
            return Ok(());
        }

        let num_c = buf_out.len() / grid_xsize;

        if predictor == Predictor::Zero {
            assert_eq!(grid_xsize, 1);
            assert_eq!(buf_in.len(), 1);
            let is_single_channel = buf_out.len() == 1;
            for (c, out_buf) in buf_out.iter_mut().enumerate() {
                let palette = Palette::new(&out_buf.bit_depth, c, buf_pal, storage);
                // Avoid partial render overshoots going into the implicit cube / deltas.
                // If we are not doing delta palette, we have a single channel, we have an actual
                // palette, and we have partial data, almost certainly any under/over shoot is just
                // due to partial rendering noise, so clip the index in those cases.
                let palette_size = (palette.explicit.len() / storage.sample_size()) as i32;
                let clip =
                    is_partial && palette_size != 0 && self.num_deltas == 0 && is_single_channel;
                if storage == ModularStorage::I16 {
                    let in_rect = ImageRect::<i16>::from_raw(buf_in[0].data.as_rect());
                    let mut out_rect = ImageRectMut::<i16>::from_raw(out_buf.data.as_rect_mut());
                    for y in 0..h {
                        let index_row = in_rect.row(y);
                        let out_row = out_rect.row(y);
                        if clip {
                            for (out, &index) in out_row.iter_mut().zip(index_row.iter()) {
                                *out = palette.get(index.clamp(0, palette_size as i16 - 1) as isize)
                                    as i16;
                            }
                        } else {
                            for (out, &index) in out_row.iter_mut().zip(index_row.iter()) {
                                *out = palette.get(index as isize) as i16;
                            }
                        }
                    }
                } else {
                    let in_rect = ImageRect::<i32>::from_raw(buf_in[0].data.as_rect());
                    let mut out_rect = ImageRectMut::<i32>::from_raw(out_buf.data.as_rect_mut());
                    if clip {
                        for y in 0..h {
                            let index_row = in_rect.row(y);
                            let out_row = out_rect.row(y);
                            for (out, &index) in out_row.iter_mut().zip(index_row.iter()) {
                                *out = palette.get(index.clamp(0, palette_size - 1) as isize);
                            }
                        }
                    } else {
                        for y in 0..h {
                            let index_row = in_rect.row(y);
                            let out_row = out_rect.row(y);
                            for (out, &index) in out_row.iter_mut().zip(index_row.iter()) {
                                *out = palette.get(index as isize);
                            }
                        }
                    }
                }
            }
            return Ok(());
        }

        let total_w: usize = buf_out[..grid_xsize]
            .iter()
            .map(|b| b.size(storage).0)
            .sum();
        let left_offset = if buf_left.is_some() { 2 } else { 0 };
        let row_len = total_w + left_offset;
        for s in scratch.iter_mut() {
            s.resize(row_len, 0);
        }

        let element_size = storage.sample_size();

        for c in 0..num_c {
            let out_row_idx = c * grid_xsize;
            let palette = Palette::new(&buf_out[out_row_idx].bit_depth, c, buf_pal, storage);
            let mut wp_state = if predictor == Predictor::Weighted {
                let mut state = WeightedPredictorState::new(wp_header, total_w);
                if let Some(Some(aux_img)) = prev_aux.and_then(|aux| aux.get(c)) {
                    state.restore_state(aux_img);
                }
                Some(state)
            } else {
                None
            };

            let (scratch, in_scratch) = scratch.as_chunks_mut::<3>();
            let scratch = &mut scratch[0];
            let in_scratch = &mut in_scratch[0];

            if let Some(prev) = buf_top {
                let mut x_offset = 0;
                for grid_x in 0..grid_xsize {
                    let prev_img = prev[out_row_idx + grid_x];
                    let w = prev_img.byte_size().0 / element_size;
                    let [_, t, tt] = scratch;
                    let row_top = &mut t[left_offset + x_offset..][..w];
                    let row_toptop = &mut tt[left_offset + x_offset..][..w];
                    if storage == ModularStorage::I16 {
                        let rect = ImageRect::<i16>::from_raw(prev_img.as_rect());
                        for (d, &s) in row_top.iter_mut().zip(rect.row(3)) {
                            *d = s as i32;
                        }
                        for (d, &s) in row_toptop.iter_mut().zip(rect.row(2)) {
                            *d = s as i32;
                        }
                    } else {
                        let rect = ImageRect::<i32>::from_raw(prev_img.as_rect());
                        row_top.copy_from_slice(rect.row(3));
                        row_toptop.copy_from_slice(rect.row(2));
                    }
                    x_offset += w;
                }
                if let Some(left_border) = buf_left {
                    let tl = if let Some(tl_border) = buf_topleft {
                        get_border_pixel(
                            tl_border[c],
                            tl_border[c].byte_size().0 / element_size - 1,
                            3,
                            storage,
                        )
                    } else {
                        get_border_pixel(left_border[c], 3, 0, storage)
                    };
                    scratch[1][1] = tl;
                    scratch[1][0] = tl;
                }
            }

            for y in 0..h {
                // y+2 is not the correct y value, but it suffices for things to work.
                let effective_y = if buf_top.is_some() { y + 2 } else { y };

                if let Some(left_border) = buf_left {
                    let left_img = left_border[c];
                    scratch[0][1] = get_border_pixel(left_img, 3, y, storage);
                    scratch[0][0] = get_border_pixel(left_img, 2, y, storage);
                }

                let [row_cur, row_top, row_toptop] = scratch;

                let mut gx = 0;
                for (grid_x, index_buf) in buf_in.iter().enumerate().take(grid_xsize) {
                    let w = index_buf.size(storage).0;
                    let index_row = if storage == ModularStorage::I16 {
                        let in_rect = ImageRect::<i16>::from_raw(index_buf.data.as_rect());
                        for (i, s) in in_rect.row(y).iter().zip(in_scratch.iter_mut()) {
                            *s = *i as i32;
                        }
                        &in_scratch[..w]
                    } else {
                        let in_rect = ImageRect::<i32>::from_raw(index_buf.data.as_rect());
                        in_rect.row(y)
                    };
                    for &index in index_row.iter() {
                        let palette_entry = palette.get(index as isize);
                        let x_scratch = left_offset + gx;
                        let prediction_data = PredictionData::get_rows(
                            row_cur,
                            row_top,
                            row_toptop,
                            x_scratch,
                            effective_y,
                        );
                        let val = if let Some(wp) = &mut wp_state {
                            let (pred, _) = wp.predict_and_property((gx, y & 1), &prediction_data);
                            let val = if index < num_deltas as i32 {
                                (pred + palette_entry as i64) as i32
                            } else {
                                palette_entry
                            };
                            wp.update_errors(val, (gx, y & 1));
                            val
                        } else if index < num_deltas as i32 {
                            let pred = predictor.predict_one(prediction_data, /*wp_pred=*/ 0);
                            (pred + palette_entry as i64) as i32
                        } else {
                            palette_entry
                        };
                        row_cur[x_scratch] = val;
                        gx += 1;
                    }
                    let out_idx = out_row_idx + grid_x;
                    let src = &row_cur[left_offset + gx - w..][..w];
                    if storage == ModularStorage::I16 {
                        let mut out_rect =
                            ImageRectMut::<i16>::from_raw(buf_out[out_idx].data.as_rect_mut());
                        for (o, s) in out_rect.row(y).iter_mut().zip(src.iter()) {
                            *o = *s as i16;
                        }
                    } else {
                        let mut out_rect =
                            ImageRectMut::<i32>::from_raw(buf_out[out_idx].data.as_rect_mut());
                        out_rect.row(y).copy_from_slice(src);
                    }
                }

                scratch.rotate_right(1);
            }

            if let (Some(wp), Some(aux)) = (wp_state, aux_out.get_mut(c)) {
                let mut wp_image = Image::<i32>::new((total_w + 1, 5))?;
                wp.save_state(&mut wp_image);
                **aux = Some(wp_image);
            }
        }

        Ok(())
    }
}

pub fn zero_palette_step_one_group(
    buf_pal: &ModularChannel,
    buf_out: &mut [&mut ModularChannel],
    storage: ModularStorage,
) {
    let (_w, h) = buf_out[0].size(storage);
    for (c, out) in buf_out.iter_mut().enumerate() {
        let palette = Palette::new(&out.bit_depth, c, buf_pal, storage);
        let palette_entry = palette.get(0);
        if storage == ModularStorage::I16 {
            let mut out_rect = ImageRectMut::<i16>::from_raw(out.data.as_rect_mut());
            for y in 0..h {
                out_rect.row(y).fill(palette_entry as i16);
            }
        } else {
            let mut out_rect = ImageRectMut::<i32>::from_raw(out.data.as_rect_mut());
            for y in 0..h {
                out_rect.row(y).fill(palette_entry);
            }
        }
    }
}
