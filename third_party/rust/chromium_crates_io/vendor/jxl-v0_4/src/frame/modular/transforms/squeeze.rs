// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use jxl_simd::{
    F32SimdVec, I32SimdVec, SimdDescriptor, SimdMask, U32SimdVec, shl, shr, simd_function,
};

use crate::{
    error::{Error, Result},
    frame::modular::{ChannelInfo, ModularChannel},
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

    let (w, h) = in_res.size();
    // Another shortcut: when output row has just 1px
    if w == 0 {
        for y in 0..h {
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
            debug_assert!(in_next_avg.is_none());
            in_avg.row(0)
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
