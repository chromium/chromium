// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{
    F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask, U32SimdVec, shl, shr, simd_function,
};

use crate::{
    error::{Error, Result},
    frame::modular::{ChannelInfo, IMAGE_OFFSET, ModularChannel},
    headers::modular::SqueezeParams,
    image::{Image, ImageRect},
    util::AtomicRef,
};

use crate::util::tracing_wrappers::*;

#[instrument(level = "trace", err)]
pub fn check_squeeze_params(
    channels: &[(usize, ChannelInfo)],
    params: &SqueezeParams,
) -> Result<()> {
    let end_channel = (params.begin_channel + params.num_channels) as usize;
    if end_channel > channels.len() {
        return Err(Error::InvalidChannelRange(
            params.begin_channel as usize,
            params.num_channels as usize,
            channels.len(),
        ));
    }
    if channels[params.begin_channel as usize].1.is_meta() != channels[end_channel - 1].1.is_meta()
    {
        return Err(Error::MixingDifferentChannels);
    }
    if channels[params.begin_channel as usize].1.is_meta() && !params.in_place {
        return Err(Error::MetaSqueezeRequiresInPlace);
    }
    Ok(())
}

pub fn default_squeeze(data_channel_info: &[(usize, ChannelInfo)]) -> Vec<SqueezeParams> {
    let num_meta_channels = data_channel_info
        .iter()
        .take_while(|x| x.1.is_meta())
        .count();

    let mut w = data_channel_info[num_meta_channels].1.size.0;
    let mut h = data_channel_info[num_meta_channels].1.size.1;
    let nc = data_channel_info.len() - num_meta_channels;

    let mut params = vec![];

    if nc > 2 && data_channel_info[num_meta_channels + 1].1.size == (w, h) {
        // 420 previews
        let sp = SqueezeParams {
            horizontal: true,
            in_place: false,
            begin_channel: num_meta_channels as u32 + 1,
            num_channels: 2,
        };
        if w > 1 {
            params.push(sp);
        }
        if h > 1 {
            params.push(SqueezeParams {
                horizontal: false,
                ..sp
            });
        }
    }

    const MAX_FIRST_PREVIEW_SIZE: usize = 8;

    let sp = SqueezeParams {
        begin_channel: num_meta_channels as u32,
        num_channels: nc as u32,
        in_place: true,
        horizontal: false,
    };

    // vertical first on tall images
    if w <= h && h > MAX_FIRST_PREVIEW_SIZE {
        params.push(SqueezeParams {
            horizontal: false,
            ..sp
        });
        h = h.div_ceil(2);
    }
    while w > MAX_FIRST_PREVIEW_SIZE || h > MAX_FIRST_PREVIEW_SIZE {
        if w > MAX_FIRST_PREVIEW_SIZE {
            params.push(SqueezeParams {
                horizontal: true,
                ..sp
            });
            w = w.div_ceil(2);
        }
        if h > MAX_FIRST_PREVIEW_SIZE {
            params.push(SqueezeParams {
                horizontal: false,
                ..sp
            });
            h = h.div_ceil(2);
        }
    }

    params
}

#[inline(always)]
fn smooth_tendency_impl<D: SimdDescriptor>(
    d: D,
    a: D::I32Vec,
    b: D::I32Vec,
    c: D::I32Vec,
) -> D::I32Vec {
    let a_b = a - b;
    let b_c = b - c;
    let a_c = a - c;
    let abs_a_b = a_b.abs();
    let abs_b_c = b_c.abs();
    let abs_a_c = a_c.abs();
    let non_monotonic = (a_b ^ b_c).lt_zero();
    let skip = a_b.eq_zero().andnot(non_monotonic);
    let skip = b_c.eq_zero().andnot(skip);

    let abs_a_b_3 = abs_a_b.mul_wide_take_high(D::I32Vec::splat(d, 0x55555556));

    let x = shr!(D::I32Vec::splat(d, 2) + abs_a_c + abs_a_b_3, 2);

    let abs_a_b_2_add_x = shl!(abs_a_b, 1) + (x & D::I32Vec::splat(d, 1));
    let x = x
        .gt(abs_a_b_2_add_x)
        .if_then_else_i32(shl!(abs_a_b, 1) + D::I32Vec::splat(d, 1), x);

    let abs_b_c_2 = shl!(abs_b_c, 1);
    let x = (x + (x & D::I32Vec::splat(d, 1)))
        .gt(abs_b_c_2)
        .if_then_else_i32(abs_b_c_2, x);

    let need_neg = a_c.lt_zero();
    let x = skip.maskz_i32(x);
    need_neg.if_then_else_i32(-x, x)
}

#[inline(always)]
fn smooth_tendency_scalar(b: i64, a: i64, n: i64) -> i64 {
    let mut diff = 0;
    if b >= a && a >= n {
        diff = (4 * b - 3 * n - a + 6) / 12;
        //      2c = a<<1 + diff - diff&1 <= 2b  so diff - diff&1 <= 2b - 2a
        //      2d = a<<1 - diff - diff&1 >= 2n  so diff + diff&1 <= 2a - 2n
        if diff - (diff & 1) > 2 * (b - a) {
            diff = 2 * (b - a) + 1;
        }
        if diff + (diff & 1) > 2 * (a - n) {
            diff = 2 * (a - n);
        }
    } else if b <= a && a <= n {
        diff = (4 * b - 3 * n - a - 6) / 12;
        //      2c = a<<1 + diff + diff&1 >= 2b  so diff + diff&1 >= 2b - 2a
        //      2d = a<<1 - diff + diff&1 <= 2n  so diff - diff&1 >= 2a - 2n
        if diff + (diff & 1) < 2 * (b - a) {
            diff = 2 * (b - a) - 1;
        }
        if diff - (diff & 1) < 2 * (a - n) {
            diff = 2 * (a - n);
        }
    }
    diff
}

#[inline(always)]
fn unsqueeze_impl<D: SimdDescriptor>(
    d: D,
    avg: D::I32Vec,
    res: D::I32Vec,
    next_avg: D::I32Vec,
    prev: D::I32Vec,
) -> (D::I32Vec, D::I32Vec) {
    let tendency = smooth_tendency_impl(d, prev, avg, next_avg);
    let diff = res + tendency;
    let sign = shr!(diff.bitcast_to_u32(), 31).bitcast_to_i32();
    let diff_2 = shr!(diff + sign, 1);
    let a = avg + diff_2;
    let b = a - diff;
    (a, b)
}

#[inline(always)]
fn unsqueeze_scalar(avg: i32, res: i32, next_avg: i32, prev: i32) -> (i32, i32) {
    let tendency = smooth_tendency_scalar(prev as i64, avg as i64, next_avg as i64);
    let diff = (res as i64) + tendency;
    let a = (avg as i64) + (diff / 2);
    let b = a - diff;
    (a as i32, b as i32)
}

#[inline(always)]
fn hsqueeze_impl<D: SimdDescriptor>(
    d: D,
    y_start: usize,
    in_avg: &ImageRect<'_, i32>,
    in_res: &ImageRect<'_, i32>,
    in_next_avg: &Option<ImageRect<'_, i32>>,
    out_prev: &Option<AtomicRef<'_, ModularChannel>>,
    out: &mut Image<i32>,
) {
    const {
        assert!(D::I32Vec::LEN <= 16);
        assert!(D::I32Vec::LEN.is_power_of_two());
    }

    let lanes = D::I32Vec::LEN;
    assert_eq!(y_start % lanes, 0);

    let (w, h) = in_res.size();
    if lanes == 1 {
        return hsqueeze_scalar(y_start, in_avg, in_res, in_next_avg, out_prev, out);
    }

    let has_tail = out.size().0 & 1 == 1;
    if has_tail {
        debug_assert!(in_avg.size().0 == w + 1);
        debug_assert!(out.size().0 == 2 * w + 1);
    }

    let mask = !(lanes - 1);
    let y_limit = if w >= lanes { h & mask } else { y_start };

    let mut buf = [0f32; 512];
    for y in (y_start..y_limit).step_by(lanes) {
        for dy in 0..lanes {
            buf[dy] = f32::from_bits(in_avg.row(y + dy)[0] as u32);
            buf[lanes + dy] = f32::from_bits(in_res.row(y + dy)[0] as u32);
        }
        let mut avg_first = D::F32Vec::load(d, &buf).bitcast_to_i32();
        let mut res_first = D::F32Vec::load(d, &buf[lanes..]).bitcast_to_i32();

        let mut prev_b = match out_prev {
            None => avg_first,
            Some(mc) => {
                let mc_w = mc.data.size().0;
                let mc = &mc.data;
                for (dy, out) in buf[..lanes].iter_mut().enumerate() {
                    *out = f32::from_bits(mc.row(y + dy)[mc_w - 1] as u32);
                }
                D::F32Vec::load(d, &buf).bitcast_to_i32()
            }
        };

        let remainder_start = ((w - 1) & mask) + 1;
        let remainder_count = w - remainder_start;
        for x in (1..remainder_start).step_by(lanes) {
            let buf_arr = D::F32Vec::make_array_slice_mut(&mut buf);

            for dy in 0..lanes {
                let avg_row = &in_avg.row(y + dy)[x..][..lanes];
                let res_row = &in_res.row(y + dy)[x..][..lanes];
                let avg = D::I32Vec::load(d, avg_row);
                let res = D::I32Vec::load(d, res_row);
                avg.bitcast_to_f32().store_array(&mut buf_arr[2 * dy]);
                res.bitcast_to_f32().store_array(&mut buf_arr[2 * dy + 1]);
            }
            D::F32Vec::transpose_square(d, buf_arr, 2);
            D::F32Vec::transpose_square(d, &mut buf_arr[1..], 2);

            for idx in 0..lanes {
                let avg_next = D::F32Vec::load_array(d, &buf_arr[2 * idx]).bitcast_to_i32();
                let res_next = D::F32Vec::load_array(d, &buf_arr[2 * idx + 1]).bitcast_to_i32();
                let (a, b) = unsqueeze_impl(d, avg_first, res_first, avg_next, prev_b);
                a.bitcast_to_f32().store_array(&mut buf_arr[2 * idx]);
                b.bitcast_to_f32().store_array(&mut buf_arr[2 * idx + 1]);
                avg_first = avg_next;
                res_first = res_next;
                prev_b = b;
            }

            D::F32Vec::transpose_square(d, buf_arr, 1);
            D::F32Vec::transpose_square(d, &mut buf_arr[lanes..], 1);
            for dy in 0..lanes {
                let out_row = &mut out.row_mut(y + dy)[2 * x - 2..][..2 * lanes];
                for group in 0..2 {
                    let v = D::F32Vec::load_array(d, &buf_arr[dy + group * lanes]).bitcast_to_i32();
                    v.store(&mut out_row[group * lanes..]);
                }
            }
        }

        let x = remainder_start;
        if remainder_count == 0 {
            let avg_last = if has_tail {
                for (idx, out) in buf[..lanes].iter_mut().enumerate() {
                    *out = f32::from_bits(in_avg.row(y + idx)[w] as u32);
                }
                D::F32Vec::load(d, &buf).bitcast_to_i32()
            } else if let Some(mc) = in_next_avg {
                for (idx, out) in buf[..lanes].iter_mut().enumerate() {
                    *out = f32::from_bits(mc.row(y + idx)[0] as u32);
                }
                D::F32Vec::load(d, &buf).bitcast_to_i32()
            } else {
                avg_first
            };

            let buf_arr = D::F32Vec::make_array_slice_mut(&mut buf);
            let (a, b) = unsqueeze_impl(d, avg_first, res_first, avg_last, prev_b);
            a.bitcast_to_f32().store_array(&mut buf_arr[0]);
            b.bitcast_to_f32().store_array(&mut buf_arr[1]);
        } else {
            for dy in 0..lanes {
                let avg_row = in_avg.row(y + dy);
                let res_row = in_res.row(y + dy);
                for dx in 0..remainder_count {
                    buf[dx + lanes * 2 * dy] = f32::from_bits(avg_row[x + dx] as u32);
                    buf[dx + lanes * (2 * dy + 1)] = f32::from_bits(res_row[x + dx] as u32);
                }

                buf[remainder_count + lanes * 2 * dy] = if has_tail {
                    f32::from_bits(avg_row[w] as u32)
                } else if let Some(mc) = in_next_avg {
                    f32::from_bits(mc.row(y + dy)[0] as u32)
                } else {
                    buf[remainder_count - 1 + lanes * 2 * dy]
                };
            }

            let buf_arr = D::F32Vec::make_array_slice_mut(&mut buf);
            D::F32Vec::transpose_square(d, buf_arr, 2);
            D::F32Vec::transpose_square(d, &mut buf_arr[1..], 2);

            for idx in 0..=remainder_count {
                let avg_next = D::F32Vec::load_array(d, &buf_arr[2 * idx]).bitcast_to_i32();
                let res_next = D::F32Vec::load_array(d, &buf_arr[2 * idx + 1]).bitcast_to_i32();
                let (a, b) = unsqueeze_impl(d, avg_first, res_first, avg_next, prev_b);
                a.bitcast_to_f32().store_array(&mut buf_arr[2 * idx]);
                b.bitcast_to_f32().store_array(&mut buf_arr[2 * idx + 1]);
                avg_first = avg_next;
                res_first = res_next;
                prev_b = b;
            }
        }

        let buf_arr = D::F32Vec::make_array_slice_mut(&mut buf);
        D::F32Vec::transpose_square(d, buf_arr, 1);
        D::F32Vec::transpose_square(d, &mut buf_arr[lanes..], 1);

        let x_limit = 2 * (remainder_count + 1);
        for dy in 0..lanes {
            let out_row = &mut out.row_mut(y + dy)[2 * x - 2..];
            for (dx, out) in out_row[..x_limit].iter_mut().enumerate() {
                let group = dx / lanes;
                let group_x = dx % lanes;
                *out = buf[(dy + group * lanes) * lanes + group_x].to_bits() as i32;
            }
        }

        if has_tail {
            for dy in 0..lanes {
                out.row_mut(y + dy)[2 * w] = in_avg.row(y + dy)[w];
            }
        }
    }

    let remainder_rows = h - y_limit;
    // We need `lanes > N` to convince the compiler that this function does not recurse
    if lanes > 8 && remainder_rows >= 8 && w >= 8 {
        return hsqueeze_impl(
            d.maybe_downgrade_256bit(),
            y_limit,
            in_avg,
            in_res,
            in_next_avg,
            out_prev,
            out,
        );
    }
    if lanes > 4 && remainder_rows >= 4 && w >= 4 {
        return hsqueeze_impl(
            d.maybe_downgrade_128bit(),
            y_limit,
            in_avg,
            in_res,
            in_next_avg,
            out_prev,
            out,
        );
    }

    hsqueeze_scalar(y_limit, in_avg, in_res, in_next_avg, out_prev, out)
}

#[inline(always)]
fn hsqueeze_scalar(
    y_start: usize,
    in_avg: &ImageRect<'_, i32>,
    in_res: &ImageRect<'_, i32>,
    in_next_avg: &Option<ImageRect<'_, i32>>,
    out_prev: &Option<AtomicRef<'_, ModularChannel>>,
    out: &mut Image<i32>,
) {
    let (w, h) = in_res.size();

    debug_assert!(w >= 1);
    let has_tail = out.size().0 & 1 == 1;
    if has_tail {
        debug_assert!(in_avg.size().0 == w + 1);
        debug_assert!(out.size().0 == 2 * w + 1);
    }

    for y in y_start..h {
        let avg_row = in_avg.row(y);
        let res_row = in_res.row(y);
        let mut prev_b = match out_prev {
            None => avg_row[0],
            Some(mc) => mc.data.row(y)[mc.data.size().0 - 1],
        };
        // Guarantee that `avg_row[x + 1]` is available.
        let x_end = if has_tail { w } else { w - 1 };
        for x in 0..x_end {
            let (a, b) = unsqueeze_scalar(avg_row[x], res_row[x], avg_row[x + 1], prev_b);
            out.row_mut(y)[2 * x] = a;
            out.row_mut(y)[2 * x + 1] = b;
            prev_b = b;
        }
        if !has_tail {
            let last_avg = match in_next_avg {
                None => avg_row[w - 1],
                Some(mc) => mc.row(y)[0],
            };
            let (a, b) = unsqueeze_scalar(avg_row[w - 1], res_row[w - 1], last_avg, prev_b);
            out.row_mut(y)[2 * w - 2] = a;
            out.row_mut(y)[2 * w - 1] = b;
        } else {
            // 1 last pixel
            out.row_mut(y)[2 * w] = in_avg.row(y)[w];
        }
    }
}

simd_function!(
    hsqueeze,
    d: D,
    pub fn hsqueeze_fwd(
        in_avg: &ImageRect<'_, i32>,
        in_res: &ImageRect<'_, i32>,
        in_next_avg: &Option<ImageRect<'_, i32>>,
        out_prev: &Option<AtomicRef<'_, ModularChannel>>,
        out: &mut Image<i32>,
    ) {
        hsqueeze_impl(d, 0, in_avg, in_res, in_next_avg, out_prev, out)
    }
);

#[inline(always)]
pub fn do_hsqueeze_step(
    in_avg: &ImageRect<'_, i32>,
    in_res: &ImageRect<'_, i32>,
    in_next_avg: &Option<ImageRect<'_, i32>>,
    out_prev: &Option<AtomicRef<'_, ModularChannel>>,
    buffers: &mut [&mut ModularChannel],
) {
    trace!("hsqueeze step in_avg: {in_avg:?} in_res: {in_res:?} in_next_avg: {in_next_avg:?}");
    let out = buffers.first_mut().unwrap();
    // Shortcut: guarantees that row is at least 1px in the main loop
    if out.data.size().0 == 0 || out.data.size().1 == 0 {
        return;
    }

    let w = in_res.size().0;
    // Another shortcut: when output row has just 1px
    if w == 0 {
        let out_h = out.data.size().1;
        for y in 0..out_h {
            out.data.row_mut(y)[0] = in_avg.row(y)[0];
        }
        return;
    }
    // Otherwise: 2 or more in in row
    hsqueeze(in_avg, in_res, in_next_avg, out_prev, &mut out.data);
}

#[inline(always)]
fn vsqueeze_impl<D: SimdDescriptor>(
    d: D,
    x_start: usize,
    in_avg: &ImageRect<'_, i32>,
    in_res: &ImageRect<'_, i32>,
    in_next_avg: &Option<ImageRect<'_, i32>>,
    out_prev: &Option<AtomicRef<'_, ModularChannel>>,
    out: &mut Image<i32>,
) {
    const { assert!(D::I32Vec::LEN.is_power_of_two()) };

    let lanes = D::I32Vec::LEN;
    assert_eq!(x_start % lanes, 0);

    let (w, h) = in_res.size();
    if lanes == 1 {
        return vsqueeze_scalar(x_start, in_avg, in_res, in_next_avg, out_prev, out);
    }

    let has_tail = out.size().1 & 1 == 1;
    if has_tail {
        debug_assert!(in_avg.size().1 == h + 1);
        debug_assert!(out.size().1 == 2 * h + 1);
    }

    let mask = !(lanes - 1);
    let x_limit = if w >= lanes { w & mask } else { x_start };

    let prev_b_row = match out_prev {
        None => in_avg.row(0),
        Some(mc) => mc.data.row(mc.data.size().1 - 1),
    };

    for x in (x_start..x_limit).step_by(lanes) {
        let mut prev_b = D::I32Vec::load(d, &prev_b_row[x..]);
        let mut avg_first = D::I32Vec::load(d, &in_avg.row(0)[x..]);
        let mut res_first = D::I32Vec::load(d, &in_res.row(0)[x..]);
        for y in 0..h - 1 {
            let avg_next = D::I32Vec::load(d, &in_avg.row(y + 1)[x..]);
            let (a, b) = unsqueeze_impl(d, avg_first, res_first, avg_next, prev_b);
            a.store(&mut out.row_mut(2 * y)[x..]);
            b.store(&mut out.row_mut(2 * y + 1)[x..]);
            prev_b = b;
            avg_first = avg_next;
            res_first = D::I32Vec::load(d, &in_res.row(y + 1)[x..]);
        }

        let avg_last = if has_tail {
            D::I32Vec::load(d, &in_avg.row(h)[x..])
        } else if let Some(mc) = in_next_avg {
            D::I32Vec::load(d, &mc.row(0)[x..])
        } else {
            avg_first
        };
        let (a, b) = unsqueeze_impl(d, avg_first, res_first, avg_last, prev_b);
        a.store(&mut out.row_mut(2 * h - 2)[x..]);
        b.store(&mut out.row_mut(2 * h - 1)[x..]);

        if has_tail {
            avg_last.store(&mut out.row_mut(2 * h)[x..]);
        }
    }

    let remainder_cols = w - x_limit;
    // We need `lanes > N` to convince the compiler that this function does not recurse
    if lanes > 8 && remainder_cols >= 8 {
        return vsqueeze_impl(
            d.maybe_downgrade_256bit(),
            x_limit,
            in_avg,
            in_res,
            in_next_avg,
            out_prev,
            out,
        );
    }
    if lanes > 4 && remainder_cols >= 4 {
        return vsqueeze_impl(
            d.maybe_downgrade_128bit(),
            x_limit,
            in_avg,
            in_res,
            in_next_avg,
            out_prev,
            out,
        );
    }

    vsqueeze_scalar(x_limit, in_avg, in_res, in_next_avg, out_prev, out)
}

#[inline(always)]
fn vsqueeze_scalar(
    x_start: usize,
    in_avg: &ImageRect<'_, i32>,
    in_res: &ImageRect<'_, i32>,
    in_next_avg: &Option<ImageRect<'_, i32>>,
    out_prev: &Option<AtomicRef<'_, ModularChannel>>,
    out: &mut Image<i32>,
) {
    let (w, h) = in_res.size();

    let has_tail = out.size().1 & 1 == 1;
    if has_tail {
        debug_assert!(in_avg.size().1 == h + 1);
        debug_assert!(out.size().1 == 2 * h + 1);
    }

    {
        let prev_b_row = match out_prev {
            None => in_avg.row(0),
            Some(mc) => mc.data.row(mc.data.size().1 - 1),
        };
        let avg_row = in_avg.row(0);
        let res_row = in_res.row(0);
        let avg_row_next = if !has_tail && (h == 1) {
            match in_next_avg {
                None => in_avg.row(0),
                Some(mc) => mc.row(0),
            }
        } else {
            in_avg.row(1)
        };
        for x in x_start..w {
            let (a, b) = unsqueeze_scalar(avg_row[x], res_row[x], avg_row_next[x], prev_b_row[x]);
            out.row_mut(0)[x] = a;
            out.row_mut(1)[x] = b;
        }
    }
    for y in 1..h {
        let avg_row = in_avg.row(y);
        let res_row = in_res.row(y);
        let avg_row_next = if has_tail || y < h - 1 {
            in_avg.row(y + 1)
        } else {
            match in_next_avg {
                None => avg_row,
                Some(mc) => mc.row(0),
            }
        };
        for x in x_start..w {
            let (a, b) = unsqueeze_scalar(
                avg_row[x],
                res_row[x],
                avg_row_next[x],
                out.row(2 * y - 1)[x],
            );
            out.row_mut(2 * y)[x] = a;
            out.row_mut(2 * y + 1)[x] = b;
        }
    }
    if has_tail {
        out.row_mut(2 * h)[x_start..].copy_from_slice(&in_avg.row(h)[x_start..]);
    }
}

simd_function!(
    vsqueeze,
    d: D,
    pub fn vsqueeze_fwd(
        in_avg: &ImageRect<'_, i32>,
        in_res: &ImageRect<'_, i32>,
        in_next_avg: &Option<ImageRect<'_, i32>>,
        out_prev: &Option<AtomicRef<'_, ModularChannel>>,
        out: &mut Image<i32>,
    ) {
        vsqueeze_impl(d, 0, in_avg, in_res, in_next_avg, out_prev, out)
    }
);

#[inline(always)]
pub fn do_vsqueeze_step(
    in_avg: &ImageRect<'_, i32>,
    in_res: &ImageRect<'_, i32>,
    in_next_avg: &Option<ImageRect<'_, i32>>,
    out_prev: &Option<AtomicRef<'_, ModularChannel>>,
    buffers: &mut [&mut ModularChannel],
) {
    trace!("vsqueeze step in_avg: {in_avg:?} in_res: {in_res:?} in_next_avg: {in_next_avg:?}");
    let out = &mut buffers.first_mut().unwrap().data;
    // Shortcut: guarantees that there at least 1 output row
    if out.size().1 == 0 || out.size().0 == 0 {
        return;
    }
    // Another shortcut: when there is one output row
    if in_res.size().1 == 0 {
        out.row_mut(0).copy_from_slice(in_avg.row(0));
        return;
    }
    // Otherwise: 2 or more rows

    vsqueeze(in_avg, in_res, in_next_avg, out_prev, out);
}

use super::super::{ModularBufferInfo, ModularGridKind};
use crate::headers::frame_header::FrameHeader;
use crate::image::Rect;

#[inline(always)]
fn convolve_2d_simd<D: SimdDescriptor>(
    d: D,
    n: &[D::F32Vec; 25],
) -> (D::I32Vec, D::I32Vec, D::I32Vec, D::I32Vec) {
    let w_7 = D::F32Vec::splat(d, 7.0 / 65536.0);
    let w_93 = D::F32Vec::splat(d, 93.0 / 65536.0);
    let w_1188 = D::F32Vec::splat(d, 1188.0 / 65536.0);
    let w_2842 = D::F32Vec::splat(d, 2842.0 / 65536.0);
    let w_6175 = D::F32Vec::splat(d, 6175.0 / 65536.0);
    let w_12905 = D::F32Vec::splat(d, 12905.0 / 65536.0);
    let w_25198 = D::F32Vec::splat(d, 25198.0 / 65536.0);

    let mut sum_0_0_a = D::F32Vec::zero(d);
    let mut sum_0_0_b = D::F32Vec::zero(d);
    let mut sum_0_0_c = D::F32Vec::zero(d);
    let mut sum_0_0_d = D::F32Vec::zero(d);

    sum_0_0_a = n[1].mul_add(w_7, sum_0_0_a);
    sum_0_0_a = n[2].mul_add(w_93, sum_0_0_a);
    sum_0_0_a = n[5].mul_add(w_7, sum_0_0_a);
    sum_0_0_a = n[6].mul_add(w_6175, sum_0_0_a);

    sum_0_0_b = n[7].mul_add(w_12905, sum_0_0_b);
    sum_0_0_b = n[8].mul_add(w_1188, sum_0_0_b);
    sum_0_0_b = n[10].mul_add(w_93, sum_0_0_b);

    sum_0_0_c = n[11].mul_add(w_12905, sum_0_0_c);
    sum_0_0_c = n[12].mul_add(w_25198, sum_0_0_c);
    sum_0_0_c = n[13].mul_add(w_2842, sum_0_0_c);

    sum_0_0_d = n[16].mul_add(w_1188, sum_0_0_d);
    sum_0_0_d = n[17].mul_add(w_2842, sum_0_0_d);
    sum_0_0_d = n[18].mul_add(w_93, sum_0_0_d);

    let sum_0_0 = (sum_0_0_a + sum_0_0_b) + (sum_0_0_c + sum_0_0_d);

    let mut sum_0_1_a = D::F32Vec::zero(d);
    let mut sum_0_1_b = D::F32Vec::zero(d);
    let mut sum_0_1_c = D::F32Vec::zero(d);
    let mut sum_0_1_d = D::F32Vec::zero(d);

    sum_0_1_a = n[2].mul_add(w_93, sum_0_1_a);
    sum_0_1_a = n[3].mul_add(w_7, sum_0_1_a);
    sum_0_1_a = n[6].mul_add(w_1188, sum_0_1_a);
    sum_0_1_a = n[7].mul_add(w_12905, sum_0_1_a);

    sum_0_1_b = n[8].mul_add(w_6175, sum_0_1_b);
    sum_0_1_b = n[9].mul_add(w_7, sum_0_1_b);
    sum_0_1_b = n[11].mul_add(w_2842, sum_0_1_b);

    sum_0_1_c = n[12].mul_add(w_25198, sum_0_1_c);
    sum_0_1_c = n[13].mul_add(w_12905, sum_0_1_c);
    sum_0_1_c = n[14].mul_add(w_93, sum_0_1_c);

    sum_0_1_d = n[16].mul_add(w_93, sum_0_1_d);
    sum_0_1_d = n[17].mul_add(w_2842, sum_0_1_d);
    sum_0_1_d = n[18].mul_add(w_1188, sum_0_1_d);

    let sum_0_1 = (sum_0_1_a + sum_0_1_b) + (sum_0_1_c + sum_0_1_d);

    let mut sum_1_0_a = D::F32Vec::zero(d);
    let mut sum_1_0_b = D::F32Vec::zero(d);
    let mut sum_1_0_c = D::F32Vec::zero(d);
    let mut sum_1_0_d = D::F32Vec::zero(d);

    sum_1_0_a = n[6].mul_add(w_1188, sum_1_0_a);
    sum_1_0_a = n[7].mul_add(w_2842, sum_1_0_a);
    sum_1_0_a = n[8].mul_add(w_93, sum_1_0_a);
    sum_1_0_a = n[10].mul_add(w_93, sum_1_0_a);

    sum_1_0_b = n[11].mul_add(w_12905, sum_1_0_b);
    sum_1_0_b = n[12].mul_add(w_25198, sum_1_0_b);
    sum_1_0_b = n[13].mul_add(w_2842, sum_1_0_b);

    sum_1_0_c = n[15].mul_add(w_7, sum_1_0_c);
    sum_1_0_c = n[16].mul_add(w_6175, sum_1_0_c);
    sum_1_0_c = n[17].mul_add(w_12905, sum_1_0_c);

    sum_1_0_d = n[18].mul_add(w_1188, sum_1_0_d);
    sum_1_0_d = n[21].mul_add(w_7, sum_1_0_d);
    sum_1_0_d = n[22].mul_add(w_93, sum_1_0_d);

    let sum_1_0 = (sum_1_0_a + sum_1_0_b) + (sum_1_0_c + sum_1_0_d);

    let mut sum_1_1_a = D::F32Vec::zero(d);
    let mut sum_1_1_b = D::F32Vec::zero(d);
    let mut sum_1_1_c = D::F32Vec::zero(d);
    let mut sum_1_1_d = D::F32Vec::zero(d);

    sum_1_1_a = n[6].mul_add(w_93, sum_1_1_a);
    sum_1_1_a = n[7].mul_add(w_2842, sum_1_1_a);
    sum_1_1_a = n[8].mul_add(w_1188, sum_1_1_a);
    sum_1_1_a = n[11].mul_add(w_2842, sum_1_1_a);

    sum_1_1_b = n[12].mul_add(w_25198, sum_1_1_b);
    sum_1_1_b = n[13].mul_add(w_12905, sum_1_1_b);
    sum_1_1_b = n[14].mul_add(w_93, sum_1_1_b);

    sum_1_1_c = n[16].mul_add(w_1188, sum_1_1_c);
    sum_1_1_c = n[17].mul_add(w_12905, sum_1_1_c);
    sum_1_1_c = n[18].mul_add(w_6175, sum_1_1_c);

    sum_1_1_d = n[19].mul_add(w_7, sum_1_1_d);
    sum_1_1_d = n[22].mul_add(w_93, sum_1_1_d);
    sum_1_1_d = n[23].mul_add(w_7, sum_1_1_d);

    let sum_1_1 = (sum_1_1_a + sum_1_1_b) + (sum_1_1_c + sum_1_1_d);

    let half = D::F32Vec::splat(d, 0.5);
    let out_0_0 = (sum_0_0 + half.copysign(sum_0_0)).as_i32();
    let out_0_1 = (sum_0_1 + half.copysign(sum_0_1)).as_i32();
    let out_1_0 = (sum_1_0 + half.copysign(sum_1_0)).as_i32();
    let out_1_1 = (sum_1_1 + half.copysign(sum_1_1)).as_i32();

    (out_0_0, out_0_1, out_1_0, out_1_1)
}

#[inline(always)]
fn convolve_1d_simd<D: SimdDescriptor>(d: D, n: &[D::F32Vec; 15]) -> (D::I32Vec, D::I32Vec) {
    let w_116 = D::F32Vec::splat(d, 116.0 / 65536.0);
    let w_474 = D::F32Vec::splat(d, 474.0 / 65536.0);
    let w_3145 = D::F32Vec::splat(d, 3145.0 / 65536.0);
    let w_6787 = D::F32Vec::splat(d, 6787.0 / 65536.0);
    let w_14093 = D::F32Vec::splat(d, 14093.0 / 65536.0);
    let w_27370 = D::F32Vec::splat(d, 27370.0 / 65536.0);

    let mut sum_even_a = D::F32Vec::zero(d);
    let mut sum_even_b = D::F32Vec::zero(d);
    let mut sum_even_c = D::F32Vec::zero(d);
    let mut sum_even_d = D::F32Vec::zero(d);

    sum_even_a = n[1].mul_add(w_3145, sum_even_a);
    sum_even_a = n[2].mul_add(w_6787, sum_even_a);
    sum_even_a = n[3].mul_add(w_474, sum_even_a);

    sum_even_b = n[5].mul_add(w_116, sum_even_b);
    sum_even_b = n[6].mul_add(w_14093, sum_even_b);

    sum_even_c = n[7].mul_add(w_27370, sum_even_c);
    sum_even_c = n[8].mul_add(w_3145, sum_even_c);

    sum_even_d = n[11].mul_add(w_3145, sum_even_d);
    sum_even_d = n[12].mul_add(w_6787, sum_even_d);
    sum_even_d = n[13].mul_add(w_474, sum_even_d);

    let sum_even = (sum_even_a + sum_even_b) + (sum_even_c + sum_even_d);

    let mut sum_odd_a = D::F32Vec::zero(d);
    let mut sum_odd_b = D::F32Vec::zero(d);
    let mut sum_odd_c = D::F32Vec::zero(d);
    let mut sum_odd_d = D::F32Vec::zero(d);

    sum_odd_a = n[1].mul_add(w_474, sum_odd_a);
    sum_odd_a = n[2].mul_add(w_6787, sum_odd_a);
    sum_odd_a = n[3].mul_add(w_3145, sum_odd_a);

    sum_odd_b = n[6].mul_add(w_3145, sum_odd_b);
    sum_odd_b = n[7].mul_add(w_27370, sum_odd_b);

    sum_odd_c = n[8].mul_add(w_14093, sum_odd_c);
    sum_odd_c = n[9].mul_add(w_116, sum_odd_c);

    sum_odd_d = n[11].mul_add(w_474, sum_odd_d);
    sum_odd_d = n[12].mul_add(w_6787, sum_odd_d);
    sum_odd_d = n[13].mul_add(w_3145, sum_odd_d);

    let sum_odd = (sum_odd_a + sum_odd_b) + (sum_odd_c + sum_odd_d);

    let half = D::F32Vec::splat(d, 0.5);
    let out_even = (sum_even + half.copysign(sum_even)).as_i32();
    let out_odd = (sum_odd + half.copysign(sum_odd)).as_i32();

    (out_even, out_odd)
}

fn init_buffers(buf: &mut [Vec<f32>; 5], ibuf: &mut Vec<i32>, len: usize) {
    for b in buf {
        b.resize(len, 0.0);
    }
    ibuf.resize(len, 0);
}

fn load_row_to_scratch(
    row_buf: &mut [i32],
    input: &ModularBufferInfo,
    frame_header: &FrameHeader,
    yg: isize,
    xoff: usize,
) {
    let (w, h) = input.info.size;
    let max_len = row_buf.len();
    let clamped_y = if h == 1 {
        0
    } else if yg < 0 {
        (-yg - 1) as usize
    } else if yg as usize >= h {
        2 * h - 1 - yg as usize
    } else {
        yg as usize
    };

    if input.grid_kind == ModularGridKind::None {
        let grid_data = input.buffer_grid[0].data.borrow();
        let chan = grid_data.as_ref().unwrap();
        let row = chan.data.row(clamped_y);

        let left_clamp = (2 - xoff as isize).max(0) as usize;
        let right_clamp_start = (w as isize + 2 - xoff as isize).min(max_len as isize) as usize;

        if left_clamp < right_clamp_start {
            let src_start = (xoff as isize + left_clamp as isize - 2) as usize;
            let len = right_clamp_start - left_clamp;
            row_buf[left_clamp..right_clamp_start]
                .copy_from_slice(&row[src_start..src_start + len]);
        }

        if left_clamp > 0 {
            row_buf[..left_clamp].fill(row[0]);
        }

        if right_clamp_start < max_len {
            row_buf[right_clamp_start..].fill(row[w - 1]);
        }

        return;
    }

    let shift = input.info.shift.unwrap_or((0, 0));
    let grid_dim = input.grid_kind.grid_dim(frame_header, shift);
    let grid_w = grid_dim.0;
    let gy = clamped_y / grid_dim.1;
    let ly = clamped_y % grid_dim.1;

    let global_x_start = xoff as isize - 2;
    let global_x_end = global_x_start + max_len as isize;

    let left_clamp = (-global_x_start).max(0) as usize;
    let right_clamp = (global_x_end - w as isize).max(0) as usize;

    let clamped_x_start = global_x_start.max(0) as usize;
    let clamped_x_end = global_x_end.min(w as isize) as usize;

    if clamped_x_start < clamped_x_end {
        let gx_start = clamped_x_start / grid_w;
        let gx_end = (clamped_x_end - 1) / grid_w;

        for gx in gx_start..=gx_end {
            let tile_x_start = gx * grid_w;
            let tile_w = (w - tile_x_start).min(grid_w);
            let intersect_start = clamped_x_start.max(tile_x_start);
            let intersect_end = clamped_x_end.min(tile_x_start + tile_w);

            if intersect_start < intersect_end {
                let grid_idx = input.get_grid_idx(input.grid_kind, (gx, gy));
                let grid_data = input.buffer_grid[grid_idx].data.borrow();
                // Note that smooth-unsqueezing depends on some grid positions that regular
                // unsqueezing does not, so we might not have all grid positions available.
                if let Some(chan) = grid_data.as_ref() {
                    let row = chan.data.row(ly);
                    let dest_start = left_clamp + (intersect_start - clamped_x_start);
                    let dest_end = dest_start + (intersect_end - intersect_start);
                    let src_start = intersect_start - tile_x_start;
                    let src_end = intersect_end - tile_x_start;
                    row_buf[dest_start..dest_end].copy_from_slice(&row[src_start..src_end]);
                }
            }
        }
    }

    if left_clamp > 0 {
        let left_val = {
            let grid_idx = input.get_grid_idx(input.grid_kind, (0, gy));
            let grid_data = input.buffer_grid[grid_idx].data.borrow();
            if let Some(chan) = grid_data.as_ref() {
                chan.data.row(ly)[0]
            } else {
                0
            }
        };
        row_buf[..left_clamp].fill(left_val);
    }

    if right_clamp > 0 {
        let right_val = {
            let gx_right = (w - 1) / grid_w;
            let lx_right = (w - 1) % grid_w;
            let grid_idx = input.get_grid_idx(input.grid_kind, (gx_right, gy));
            let grid_data = input.buffer_grid[grid_idx].data.borrow();
            if let Some(chan) = grid_data.as_ref() {
                chan.data.row(ly)[lx_right]
            } else {
                0
            }
        };
        row_buf[max_len - right_clamp..].fill(right_val);
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
fn smooth_2d_unsqueeze_simd_impl<D: SimdDescriptor>(
    d: D,
    // TODO(veluca): modify this to *not* take a full ModularBufferInfo, but to let the caller
    // extract appropriate buffers.
    input: &ModularBufferInfo,
    frame_header: &FrameHeader,
    rect: Rect,
    output: &mut Image<i32>,
    (buffer, ibuf): &mut ([Vec<f32>; 5], Vec<i32>),
) {
    let (in_xs, in_ys) = (rect.size.0 / 2, rect.size.1 / 2);
    let (col_offset, row_offset) = (rect.origin.0 / 2, rect.origin.1 / 2);
    let lanes = D::I32Vec::LEN;
    let (xs, ys) = output.size();

    if in_xs == 0 || in_ys == 0 {
        return;
    }
    init_buffers(buffer, ibuf, in_xs + 2 * lanes + 8);

    for (dy, buf) in buffer.iter_mut().enumerate().take(4) {
        let yg = (row_offset + dy) as isize - 2;
        load_row_to_scratch(ibuf, input, frame_header, yg, col_offset);
        make_float(d, ibuf, buf);
    }

    // Loop invariant: at the start of the loop, buffer[0..4] contains the first 4 rows needed.
    // We populate the fifth row at the start of the loop.
    for iy_center in 0..ys.div_ceil(2) {
        let yg = (row_offset + iy_center) as isize + 2;
        load_row_to_scratch(ibuf, input, frame_header, yg, col_offset);
        make_float(d, ibuf, &mut buffer[4]);

        const { assert!(IMAGE_OFFSET.1 > 0) };
        let offset = output.offset();
        let yout = 2 * iy_center;
        let [output_row_0, output_row_1] = output.distinct_full_rows_mut([
            yout + offset.1,
            if yout + 1 < ys {
                yout + offset.1 + 1
            } else {
                0
            },
        ]);

        let output_row_0 = &mut output_row_0[offset.0..offset.0 + xs];
        let output_row_1 = &mut output_row_1[offset.1..offset.1 + xs];

        let row_iters = buffer[0]
            .windows(lanes + 4)
            .zip(buffer[1].windows(lanes + 4))
            .zip(buffer[2].windows(lanes + 4))
            .zip(buffer[3].windows(lanes + 4))
            .zip(buffer[4].windows(lanes + 4))
            .step_by(lanes)
            .zip(output_row_0.chunks_mut(2 * lanes))
            .zip(output_row_1.chunks_mut(2 * lanes));
        for ((((((r0, r1), r2), r3), r4), out0), out1) in row_iters {
            let mut n = [D::F32Vec::zero(d); 25];
            n[0] = D::F32Vec::load(d, r0);
            n[1] = D::F32Vec::load(d, &r0[1..]);
            n[2] = D::F32Vec::load(d, &r0[2..]);
            n[3] = D::F32Vec::load(d, &r0[3..]);
            n[4] = D::F32Vec::load(d, &r0[4..]);

            n[5] = D::F32Vec::load(d, r1);
            n[6] = D::F32Vec::load(d, &r1[1..]);
            n[7] = D::F32Vec::load(d, &r1[2..]);
            n[8] = D::F32Vec::load(d, &r1[3..]);
            n[9] = D::F32Vec::load(d, &r1[4..]);

            n[10] = D::F32Vec::load(d, r2);
            n[11] = D::F32Vec::load(d, &r2[1..]);
            n[12] = D::F32Vec::load(d, &r2[2..]);
            n[13] = D::F32Vec::load(d, &r2[3..]);
            n[14] = D::F32Vec::load(d, &r2[4..]);

            n[15] = D::F32Vec::load(d, r3);
            n[16] = D::F32Vec::load(d, &r3[1..]);
            n[17] = D::F32Vec::load(d, &r3[2..]);
            n[18] = D::F32Vec::load(d, &r3[3..]);
            n[19] = D::F32Vec::load(d, &r3[4..]);

            n[20] = D::F32Vec::load(d, r4);
            n[21] = D::F32Vec::load(d, &r4[1..]);
            n[22] = D::F32Vec::load(d, &r4[2..]);
            n[23] = D::F32Vec::load(d, &r4[3..]);
            n[24] = D::F32Vec::load(d, &r4[4..]);

            let (out_0_0, out_0_1, out_1_0, out_1_1) = convolve_2d_simd(d, &n);

            if out0.len() == 2 * lanes {
                D::I32Vec::store_interleaved_2(out_0_0, out_0_1, out0);
                D::I32Vec::store_interleaved_2(out_1_0, out_1_1, out1);
            } else {
                const { assert!(D::I32Vec::LEN <= 16) };
                let mut temp_out_0 = [0i32; 32];
                let mut temp_out_1 = [0i32; 32];
                D::I32Vec::store_interleaved_2(out_0_0, out_0_1, &mut temp_out_0);
                D::I32Vec::store_interleaved_2(out_1_0, out_1_1, &mut temp_out_1);
                let remaining = out0.len();
                out0.copy_from_slice(&temp_out_0[..remaining]);
                out1.copy_from_slice(&temp_out_1[..remaining]);
            }
        }
        // This rotation preserves the loop invariant.
        buffer.rotate_left(1);
    }
}

simd_function!(
    smooth_2d_unsqueeze,
    d: D,
    pub fn smooth_2d_unsqueeze_simd_dispatch(
        input: &ModularBufferInfo,
        frame_header: &FrameHeader,
        rect: Rect,
        output: &mut Image<i32>,
        buffer: &mut ([Vec<f32>; 5], Vec<i32>)
    ) {
        smooth_2d_unsqueeze_simd_impl(d, input, frame_header, rect, output, buffer);
    }
);

#[inline(always)]
fn smooth_h_unsqueeze_simd_impl<D: SimdDescriptor>(
    d: D,
    input: &ModularBufferInfo,
    frame_header: &FrameHeader,
    rect: Rect,
    output: &mut Image<i32>,
    (buffer, ibuf): &mut ([Vec<f32>; 5], Vec<i32>),
) {
    let (in_xs, in_ys) = (rect.size.0 / 2, rect.size.1);
    let (col_offset, row_offset) = (rect.origin.0 / 2, rect.origin.1);
    let lanes = D::I32Vec::LEN;
    let (_, ys) = output.size();

    if in_xs == 0 || in_ys == 0 {
        return;
    }
    init_buffers(buffer, ibuf, in_xs + 2 * lanes + 8);

    for (dy, buf) in buffer.iter_mut().enumerate().take(2) {
        let yg = (row_offset + dy) as isize - 1;
        load_row_to_scratch(ibuf, input, frame_header, yg, col_offset);
        make_float(d, ibuf, buf);
    }

    // Loop invariant: at the start of the loop, buffer[0..2] contains the first 2 rows needed.
    // We populate the third row at the start of the loop.
    for iy_center in 0..ys {
        let yg = (row_offset + iy_center) as isize + 1;
        load_row_to_scratch(ibuf, input, frame_header, yg, col_offset);
        make_float(d, ibuf, &mut buffer[2]);

        let output_row = output.row_mut(iy_center);

        let row_iters = buffer[0]
            .windows(lanes + 4)
            .zip(buffer[1].windows(lanes + 4))
            .zip(buffer[2].windows(lanes + 4))
            .step_by(lanes)
            .zip(output_row.chunks_mut(2 * lanes));
        for (((r0, r1), r2), out) in row_iters {
            let mut n = [D::F32Vec::zero(d); 15];
            n[0] = D::F32Vec::load(d, r0);
            n[1] = D::F32Vec::load(d, &r0[1..]);
            n[2] = D::F32Vec::load(d, &r0[2..]);
            n[3] = D::F32Vec::load(d, &r0[3..]);
            n[4] = D::F32Vec::load(d, &r0[4..]);

            n[5] = D::F32Vec::load(d, r1);
            n[6] = D::F32Vec::load(d, &r1[1..]);
            n[7] = D::F32Vec::load(d, &r1[2..]);
            n[8] = D::F32Vec::load(d, &r1[3..]);
            n[9] = D::F32Vec::load(d, &r1[4..]);

            n[10] = D::F32Vec::load(d, r2);
            n[11] = D::F32Vec::load(d, &r2[1..]);
            n[12] = D::F32Vec::load(d, &r2[2..]);
            n[13] = D::F32Vec::load(d, &r2[3..]);
            n[14] = D::F32Vec::load(d, &r2[4..]);

            let (out_even, out_odd) = convolve_1d_simd(d, &n);

            if out.len() == 2 * lanes {
                D::I32Vec::store_interleaved_2(out_even, out_odd, out);
            } else {
                const { assert!(D::I32Vec::LEN <= 16) };
                let mut temp_out = [0i32; 32];
                D::I32Vec::store_interleaved_2(out_even, out_odd, &mut temp_out);
                let remaining = out.len();
                out.copy_from_slice(&temp_out[..remaining]);
            }
        }
        // This rotation preserves the loop invariant.
        buffer.rotate_left(1);
    }
}

simd_function!(
    smooth_h_unsqueeze,
    d: D,
    pub fn smooth_h_unsqueeze_simd_dispatch(
        input: &ModularBufferInfo,
        frame_header: &FrameHeader,
        rect: Rect,
        output: &mut Image<i32>,
        buffer: &mut ([Vec<f32>; 5], Vec<i32>)
    ) {
        smooth_h_unsqueeze_simd_impl(d, input, frame_header, rect, output, buffer);
    }
);

#[inline(always)]
fn smooth_v_unsqueeze_simd_impl<D: SimdDescriptor>(
    d: D,
    input: &ModularBufferInfo,
    frame_header: &FrameHeader,
    rect: Rect,
    output: &mut Image<i32>,
    (buffer, ibuf): &mut ([Vec<f32>; 5], Vec<i32>),
) {
    let (in_xs, in_ys) = (rect.size.0, rect.size.1 / 2);
    let (col_offset, row_offset) = (rect.origin.0, rect.origin.1 / 2);
    let lanes = D::I32Vec::LEN;
    let (xs, ys) = output.size();

    if in_xs == 0 || in_ys == 0 {
        return;
    }
    init_buffers(buffer, ibuf, in_xs + 2 * lanes + 8);

    for (dy, buf) in buffer.iter_mut().enumerate().take(4) {
        let yg = (row_offset + dy) as isize - 2;
        load_row_to_scratch(ibuf, input, frame_header, yg, col_offset);
        make_float(d, ibuf, buf);
    }

    // Loop invariant: at the start of the loop, buffer[0..4] contains the first 4 rows needed.
    // We populate the fifth row at the start of the loop.
    for iy_center in 0..ys.div_ceil(2) {
        let yg = (row_offset + iy_center) as isize + 2;
        load_row_to_scratch(ibuf, input, frame_header, yg, col_offset);
        make_float(d, ibuf, &mut buffer[4]);

        const { assert!(IMAGE_OFFSET.1 > 0) };
        let offset = output.offset();
        let yout = 2 * iy_center;
        let [output_row_0, output_row_1] = output.distinct_full_rows_mut([
            yout + offset.1,
            if yout + 1 < ys {
                yout + offset.1 + 1
            } else {
                0
            },
        ]);

        let output_row_0 = &mut output_row_0[offset.0..offset.0 + xs];
        let output_row_1 = &mut output_row_1[offset.1..offset.1 + xs];

        let row_iters = buffer[0][1..]
            .windows(lanes + 2)
            .zip(buffer[1][1..].windows(lanes + 2))
            .zip(buffer[2][1..].windows(lanes + 2))
            .zip(buffer[3][1..].windows(lanes + 2))
            .zip(buffer[4][1..].windows(lanes + 2))
            .step_by(lanes)
            .zip(output_row_0.chunks_mut(lanes))
            .zip(output_row_1.chunks_mut(lanes));
        for ((((((r0, r1), r2), r3), r4), out0), out1) in row_iters {
            let mut n = [D::F32Vec::zero(d); 15];
            n[0] = D::F32Vec::load(d, r0);
            n[1] = D::F32Vec::load(d, r1);
            n[2] = D::F32Vec::load(d, r2);
            n[3] = D::F32Vec::load(d, r3);
            n[4] = D::F32Vec::load(d, r4);

            n[5] = D::F32Vec::load(d, &r0[1..]);
            n[6] = D::F32Vec::load(d, &r1[1..]);
            n[7] = D::F32Vec::load(d, &r2[1..]);
            n[8] = D::F32Vec::load(d, &r3[1..]);
            n[9] = D::F32Vec::load(d, &r4[1..]);

            n[10] = D::F32Vec::load(d, &r0[2..]);
            n[11] = D::F32Vec::load(d, &r1[2..]);
            n[12] = D::F32Vec::load(d, &r2[2..]);
            n[13] = D::F32Vec::load(d, &r3[2..]);
            n[14] = D::F32Vec::load(d, &r4[2..]);

            let (out_py0, out_py1) = convolve_1d_simd(d, &n);

            if out0.len() == lanes {
                out_py0.store(out0);
                out_py1.store(out1);
            } else {
                const { assert!(D::I32Vec::LEN <= 16) };
                let mut temp_out_0 = [0i32; 16];
                let mut temp_out_1 = [0i32; 16];
                out_py0.store(&mut temp_out_0);
                out_py1.store(&mut temp_out_1);
                let remaining = out0.len();
                out0.copy_from_slice(&temp_out_0[..remaining]);
                out1.copy_from_slice(&temp_out_1[..remaining]);
            }
        }
        // This rotation preserves the loop invariant.
        buffer.rotate_left(1);
    }
}

simd_function!(
    smooth_v_unsqueeze,
    d: D,
    pub fn smooth_v_unsqueeze_simd_dispatch(
        input: &ModularBufferInfo,
        frame_header: &FrameHeader,
        rect: Rect,
        output: &mut Image<i32>,
        buffer: &mut ([Vec<f32>; 5], Vec<i32>)
    ) {
        smooth_v_unsqueeze_simd_impl(d, input, frame_header, rect, output, buffer);
    }
);
