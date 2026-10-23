// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use jxl_simd::{F32SimdVec, I32SimdVec, SimdDescriptor};

use crate::image::ImageDataType;
use crate::render::low_memory_pipeline::row_buffers::RowBuffer;
use crate::util::{SmallVec, StackOnly, mirror};

pub trait VecLoad<D: SimdDescriptor>: Sized + Copy {
    type Vec: Copy;
    const LEN: usize;
    fn load(d: D, slice: &[Self]) -> Self::Vec;
}

pub trait VecStore<D: SimdDescriptor>: VecLoad<D> {
    fn store(d: D, vec: Self::Vec, slice: &mut [Self]);
}

impl<D: SimdDescriptor> VecLoad<D> for f32 {
    type Vec = D::F32Vec;
    const LEN: usize = D::F32Vec::LEN;

    #[inline(always)]
    fn load(d: D, slice: &[f32]) -> D::F32Vec {
        D::F32Vec::load(d, slice)
    }
}

impl<D: SimdDescriptor> VecStore<D> for f32 {
    #[inline(always)]
    fn store(_d: D, vec: D::F32Vec, slice: &mut [f32]) {
        vec.store(slice);
    }
}

impl<D: SimdDescriptor> VecLoad<D> for i32 {
    type Vec = D::I32Vec;
    const LEN: usize = D::I32Vec::LEN;

    #[inline(always)]
    fn load(d: D, slice: &[i32]) -> D::I32Vec {
        D::I32Vec::load(d, slice)
    }
}

impl<D: SimdDescriptor> VecLoad<D> for i16 {
    type Vec = D::I32Vec;
    const LEN: usize = D::I32Vec::LEN;

    #[inline(always)]
    fn load(d: D, slice: &[i16]) -> D::I32Vec {
        D::I32Vec::load_from_i16(d, slice)
    }
}

impl<D: SimdDescriptor> VecLoad<D> for u16 {
    type Vec = D::I32Vec;
    const LEN: usize = D::I32Vec::LEN;

    #[inline(always)]
    fn load(d: D, slice: &[u16]) -> D::I32Vec {
        D::I32Vec::load_from_u16(d, slice)
    }
}

pub trait StoreInterleaved<D: SimdDescriptor, const NUM: usize>: VecStore<D> {
    fn store_interleaved(d: D, vals: [Self::Vec; NUM], dest: &mut [Self]);
}

impl<D: SimdDescriptor> StoreInterleaved<D, 2> for f32 {
    #[inline(always)]
    fn store_interleaved(_d: D, vals: [D::F32Vec; 2], dest: &mut [f32]) {
        D::F32Vec::store_interleaved_2(vals[0], vals[1], dest);
    }
}

impl<D: SimdDescriptor> StoreInterleaved<D, 4> for f32 {
    #[inline(always)]
    fn store_interleaved(_d: D, vals: [D::F32Vec; 4], dest: &mut [f32]) {
        D::F32Vec::store_interleaved_4(vals[0], vals[1], vals[2], vals[3], dest);
    }
}

impl<D: SimdDescriptor> StoreInterleaved<D, 8> for f32 {
    #[inline(always)]
    fn store_interleaved(_d: D, vals: [D::F32Vec; 8], dest: &mut [f32]) {
        D::F32Vec::store_interleaved_8(
            vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], dest,
        );
    }
}

const MAX_SIMD_LANES: usize = 16;

pub struct Channels<'a, T> {
    channel_buffers: SmallVec<&'a [T], 8, StackOnly>,
    x_offset: usize,
    center_y: usize,
    num_rows: usize,
    image_height: usize,
    stride: usize,
}

impl<'a, T> Channels<'a, T> {
    #[inline]
    pub fn from_row_buffers(
        buffers: &[&'a RowBuffer],
        x_offset: usize,
        center_y: usize,
        image_height: usize,
        border_x: usize,
    ) -> Self
    where
        T: ImageDataType,
    {
        debug_assert!(buffers.len() <= 8);
        debug_assert!(x_offset >= border_x);
        let mut channel_buffers = SmallVec::new();
        let (num_rows, stride) = if let Some(first) = buffers.first() {
            let num_rows = first.num_rows();
            let stride = first.stride_elements::<T>();
            debug_assert!(num_rows.is_power_of_two());
            for b in buffers {
                debug_assert_eq!(b.num_rows(), num_rows);
                debug_assert_eq!(b.stride_elements::<T>(), stride);
                let slice = b.as_slice::<T>();
                debug_assert_eq!(slice.len(), num_rows * stride);
                channel_buffers.push(slice);
            }
            (num_rows, stride)
        } else {
            (0, 0)
        };
        Self {
            channel_buffers,
            x_offset,
            center_y,
            num_rows,
            image_height,
            stride,
        }
    }

    /// Creates a compile-time sized `ChannelsView` precomputing mirrored row offsets for
    /// `CHANS` channels, `ROWS` vertical rows (`ROWS = 2 * border_y + 1`), and horizontal
    /// radius `RADIUS`.
    #[inline(always)]
    pub fn view<const CHANS: usize, const ROWS: usize, const RADIUS: usize>(
        &self,
    ) -> ChannelsView<'a, T, CHANS, ROWS, RADIUS> {
        const { assert!(CHANS > 0) };
        const { assert!(ROWS % 2 == 1) };
        const { assert!(RADIUS <= isize::MAX as usize / 2) };
        let border_y = ROWS / 2;
        assert_eq!(self.channel_buffers.len(), CHANS);
        assert!(self.num_rows.is_power_of_two());
        assert!(self.image_height > 0 && self.image_height <= isize::MAX as usize / 2);
        assert!(self.x_offset >= RADIUS);
        // [check]
        assert!(
            self.x_offset
                .checked_add(RADIUS)
                .and_then(|x| x.checked_add(MAX_SIMD_LANES))
                .is_some_and(|max_x| max_x <= self.stride)
        );
        let total_len = self.num_rows.checked_mul(self.stride).unwrap();
        let mut row_offsets = [0usize; ROWS];
        for (i, slot) in row_offsets.iter_mut().enumerate() {
            let y = (self.center_y as isize).wrapping_add(i as isize - border_y as isize);
            let row = mirror(y, self.image_height) & (self.num_rows - 1);
            // Since `row < self.num_rows` and `self.num_rows * self.stride == total_len` did not
            // overflow, `row * self.stride + self.stride <= total_len` cannot overflow.
            // Thus, [2] follows.
            *slot = row * self.stride;
        }
        let channel_buffers: [&'a [T]; CHANS] = (&self.channel_buffers[..]).try_into().unwrap();
        for buf in &channel_buffers {
            // Verifies [3]
            assert_eq!(buf.len(), total_len);
        }
        // Safety note: `self.x_offset >= RADIUS` and for every `r < ROWS` and `c < CHANS`,
        // `row_offsets[r] + self.x_offset + RADIUS + MAX_SIMD_LANES <= [1] row * stride + stride
        // <= [2] total_len == [3] channel_buffers[c].len()`.
        // [1] follows from `row_offsets[r] = row * stride` combined with [check].
        ChannelsView {
            channel_buffers,
            row_offsets,
            x_offset: self.x_offset,
        }
    }
}

pub struct ChannelsView<'a, T, const CHANS: usize, const ROWS: usize, const RADIUS: usize> {
    // Safety invariant: For every `c < CHANS` and `r < ROWS`, `x_offset >= RADIUS` and
    // `row_offsets[r] + x_offset + RADIUS + MAX_SIMD_LANES <= channel_buffers[c].len()`
    // holds without arithmetic overflow.
    channel_buffers: [&'a [T]; CHANS],
    row_offsets: [usize; ROWS],
    x_offset: usize,
}

impl<'a, T, const CHANS: usize, const ROWS: usize, const RADIUS: usize>
    ChannelsView<'a, T, CHANS, ROWS, RADIUS>
{
    /// Loads a SIMD vector from channel `CHAN` at vertical offset `dy` (`-(ROWS/2)..=(ROWS/2)`)
    /// and horizontal offset `dx` (`-RADIUS..=RADIUS`) relative to the current chunk offset.
    #[inline(always)]
    pub fn load<D: SimdDescriptor, const CHAN: usize>(&self, d: D, dy: isize, dx: isize) -> T::Vec
    where
        T: VecLoad<D>,
    {
        const { assert!(CHAN < CHANS) };
        const { assert!(ROWS % 2 == 1) };
        const { assert!(RADIUS <= isize::MAX as usize / 2) };
        const { assert!(T::LEN <= MAX_SIMD_LANES) };
        let row_idx = (ROWS / 2).wrapping_add_signed(dy);
        assert!(row_idx < ROWS);
        let col_off = RADIUS.wrapping_add_signed(dx);
        assert!(col_off <= 2 * RADIUS);
        // [off]
        let offset = (self.row_offsets[row_idx] + self.x_offset).wrapping_add_signed(dx);
        let buf = self.channel_buffers[CHAN];
        debug_assert!(offset + T::LEN <= buf.len());
        // SAFETY: `slice::get_unchecked` requires `offset <= offset + T::LEN <= buf.len()`.
        // By the `ChannelsView` safety invariant (`self.x_offset >= RADIUS` and
        // `row_offsets[row_idx] + x_offset + RADIUS + MAX_SIMD_LANES <= buf.len()` without
        // overflow), along with `CHAN < CHANS`, `row_idx < ROWS`, `|dx| <= RADIUS` (verified via
        // `col_off <= 2 * RADIUS`), and `T::LEN <= MAX_SIMD_LANES`, `wrapping_add_signed(dx)` in
        // [off] cannot underflow and `offset <= offset + T::LEN <= buf.len()` holds without
        // overflow.
        let slice = unsafe { buf.get_unchecked(offset..offset + T::LEN) };
        T::load(d, slice)
    }

    /// Narrows the view to a single channel `CHAN`.
    #[inline(always)]
    pub fn select_channel<const CHAN: usize>(&self) -> ChannelsView<'a, T, 1, ROWS, RADIUS> {
        const { assert!(CHAN < CHANS) };
        // Safety note: invariant inherited directly from `self` since `CHAN < CHANS`.
        ChannelsView {
            channel_buffers: [self.channel_buffers[CHAN]],
            row_offsets: self.row_offsets,
            x_offset: self.x_offset,
        }
    }
}

pub struct ChannelsMut<'a, T> {
    channel_buffers: SmallVec<&'a mut [T], 8, StackOnly>,
    x_offset: usize,
    first_y: usize,
    num_rows: usize,
    stride: usize,
}

impl<'a, T> ChannelsMut<'a, T> {
    #[inline]
    pub fn from_row_buffers(
        buffers: &'a mut [RowBuffer],
        x_offset: usize,
        first_y: usize,
        num_output_rows: usize,
    ) -> Self
    where
        T: ImageDataType,
    {
        debug_assert!(buffers.len() <= 8);
        let mut channel_buffers = SmallVec::new();
        let (num_rows, stride) = if let Some(first) = buffers.first() {
            let num_rows = first.num_rows();
            let stride = first.stride_elements::<T>();
            debug_assert!(num_rows.is_power_of_two());
            debug_assert!(num_output_rows <= num_rows);
            for b in buffers {
                debug_assert_eq!(b.num_rows(), num_rows);
                debug_assert_eq!(b.stride_elements::<T>(), stride);
                let slice = b.as_mut_slice::<T>();
                debug_assert_eq!(slice.len(), num_rows * stride);
                channel_buffers.push(slice);
            }
            (num_rows, stride)
        } else {
            (0, 0)
        };
        Self {
            channel_buffers,
            x_offset,
            first_y,
            num_rows,
            stride,
        }
    }

    /// Creates a compile-time sized `ChannelsMutView` precomputing row offsets for
    /// `CHANS` channels and `ROWS` output rows with horizontal scale factor `SCALE`.
    #[inline(always)]
    pub fn view<const CHANS: usize, const ROWS: usize, const SCALE: usize>(
        &mut self,
    ) -> ChannelsMutView<'_, 'a, T, CHANS, ROWS, SCALE> {
        const { assert!(CHANS > 0) };
        const { assert!(ROWS > 0) };
        const { assert!(SCALE > 0) };
        assert_eq!(self.channel_buffers.len(), CHANS);
        assert!(self.num_rows.is_power_of_two());
        assert!(ROWS <= self.num_rows);
        // [check]
        assert!(
            MAX_SIMD_LANES
                .checked_mul(SCALE)
                .and_then(|w| self.x_offset.checked_add(w))
                .is_some_and(|max_x| max_x <= self.stride)
        );
        let total_len = self.num_rows.checked_mul(self.stride).unwrap();
        let mut row_offsets = [0usize; ROWS];
        for (dy, slot) in row_offsets.iter_mut().enumerate() {
            let row = self.first_y.wrapping_add(dy) & (self.num_rows - 1);
            // Since `row < self.num_rows` and `self.num_rows * self.stride == total_len` did not
            // overflow, `row * self.stride + self.stride <= total_len` cannot overflow.
            // Thus [2] is true.
            *slot = row * self.stride;
        }
        for buf in &self.channel_buffers[..] {
            // Verifies [3] below.
            assert_eq!(buf.len(), total_len);
        }
        let channel_buffers: &mut [&'a mut [T]; CHANS] =
            (&mut self.channel_buffers[..]).try_into().unwrap();
        // Safety note: For every `r < ROWS` and `c < CHANS`,
        // `row_offsets[r] + self.x_offset + MAX_SIMD_LANES * SCALE <= [1]
        //  row * stride + stride <= [2] total_len == [3] channel_buffers[c].len()`.
        // [1] follows from `row_offsets[r] = row * stride` combined with [check].
        ChannelsMutView {
            channel_buffers,
            row_offsets,
            x_offset: self.x_offset,
        }
    }
}

pub struct ChannelsMutView<'b, 'a, T, const CHANS: usize, const ROWS: usize, const SCALE: usize = 1>
{
    // Safety invariant: For every `c < CHANS` and `r < ROWS`,
    // `row_offsets[r] + x_offset + MAX_SIMD_LANES * SCALE <= channel_buffers[c].len()`
    // holds without arithmetic overflow.
    channel_buffers: &'b mut [&'a mut [T]; CHANS],
    row_offsets: [usize; ROWS],
    x_offset: usize,
}

impl<'b, 'a, T, const CHANS: usize, const ROWS: usize, const SCALE: usize>
    ChannelsMutView<'b, 'a, T, CHANS, ROWS, SCALE>
{
    /// Stores a SIMD vector to channel `CHAN` and output row `row` (`0..ROWS`).
    #[inline(always)]
    pub fn store<D: SimdDescriptor, const CHAN: usize>(&mut self, d: D, row: usize, val: T::Vec)
    where
        T: VecStore<D>,
    {
        const { assert!(CHAN < CHANS) };
        const { assert!(SCALE == 1) };
        const { assert!(T::LEN <= MAX_SIMD_LANES) };
        assert!(row < ROWS);
        let offset = self.row_offsets[row] + self.x_offset;
        let buf = &mut *self.channel_buffers[CHAN];
        debug_assert!(offset + T::LEN <= buf.len());
        // SAFETY: `slice::get_unchecked_mut` requires `offset <= offset + T::LEN <= buf.len()`.
        // By the `ChannelsMutView` safety invariant
        // (`row_offsets[row] + x_offset + MAX_SIMD_LANES * SCALE <= buf.len()` without overflow),
        // along with `CHAN < CHANS`, `row < ROWS`, `SCALE == 1`, and `T::LEN <= MAX_SIMD_LANES`,
        // `offset <= offset + T::LEN <= buf.len()` holds without overflow.
        let slice = unsafe { buf.get_unchecked_mut(offset..offset + T::LEN) };
        T::store(d, val, slice);
    }

    /// Stores `SCALE` interleaved SIMD vectors to channel `CHAN` and output row `row` (`0..ROWS`).
    #[inline(always)]
    pub fn store_interleaved<D: SimdDescriptor, const CHAN: usize>(
        &mut self,
        d: D,
        row: usize,
        vals: [T::Vec; SCALE],
    ) where
        T: StoreInterleaved<D, SCALE>,
    {
        const { assert!(CHAN < CHANS) };
        const { assert!(T::LEN <= MAX_SIMD_LANES) };
        assert!(row < ROWS);
        let offset = self.row_offsets[row] + self.x_offset;
        let buf = &mut *self.channel_buffers[CHAN];
        debug_assert!(offset + SCALE * T::LEN <= buf.len());
        // SAFETY: `slice::get_unchecked_mut` requires
        // `offset <= offset + SCALE * T::LEN <= buf.len()`. By the `ChannelsMutView` safety
        // invariant (`row_offsets[row] + x_offset + MAX_SIMD_LANES * SCALE <= buf.len()` without
        // overflow), along with `CHAN < CHANS`, `row < ROWS`, and `T::LEN <= MAX_SIMD_LANES`,
        // `offset <= offset + SCALE * T::LEN <= buf.len()` holds without overflow.
        let slice = unsafe { buf.get_unchecked_mut(offset..offset + SCALE * T::LEN) };
        T::store_interleaved(d, vals, slice);
    }
}

/// Iterates over SIMD-width chunks across `0..xsize`, validating all buffer bounds once up-front
/// so `ChannelsView::load` and `ChannelsMutView::store` / `store_interleaved` inside `body`
/// execute without per-access bounds checks.
#[inline(always)]
pub fn for_each_chunk<
    'a,
    'b,
    'c,
    D: SimdDescriptor,
    InT: VecLoad<D>,
    OutT: VecLoad<D>,
    const IN_CHANS: usize,
    const IN_ROWS: usize,
    const RADIUS: usize,
    const OUT_CHANS: usize,
    const OUT_ROWS: usize,
    const SCALE: usize,
>(
    _d: D,
    xsize: usize,
    in_view: ChannelsView<'a, InT, IN_CHANS, IN_ROWS, RADIUS>,
    out_view: ChannelsMutView<'b, 'c, OutT, OUT_CHANS, OUT_ROWS, SCALE>,
    mut body: impl FnMut(
        usize,
        &ChannelsView<'a, InT, IN_CHANS, IN_ROWS, RADIUS>,
        &mut ChannelsMutView<'_, 'c, OutT, OUT_CHANS, OUT_ROWS, SCALE>,
    ),
) {
    const { assert!(SCALE > 0) };
    const { assert!(InT::LEN > 0 && InT::LEN <= MAX_SIMD_LANES) };
    const { assert!(OutT::LEN == InT::LEN) };
    let len = InT::LEN;
    if xsize == 0 {
        return;
    }
    let num_chunks = xsize.div_ceil(len);
    assert!(num_chunks <= usize::MAX / len);
    // [in_check]
    let max_in_x = ((num_chunks - 1) * len)
        .checked_add(in_view.x_offset)
        .and_then(|x| x.checked_add(RADIUS))
        .and_then(|x| x.checked_add(MAX_SIMD_LANES))
        .unwrap();
    for buf in &in_view.channel_buffers {
        for &row_offset in &in_view.row_offsets {
            // [in_check_buf]
            assert!(
                row_offset
                    .checked_add(max_in_x)
                    .is_some_and(|end| end <= buf.len())
            );
        }
    }
    let max_out_width = MAX_SIMD_LANES.checked_mul(SCALE).unwrap();
    // [out_check]
    let max_out_x = (num_chunks - 1)
        .checked_mul(len)
        .and_then(|x| x.checked_mul(SCALE))
        .and_then(|w| out_view.x_offset.checked_add(w))
        .and_then(|x| x.checked_add(max_out_width))
        .unwrap();
    for buf in out_view.channel_buffers.iter() {
        for &row_offset in &out_view.row_offsets {
            // [out_check_buf]
            assert!(
                row_offset
                    .checked_add(max_out_x)
                    .is_some_and(|end| end <= buf.len())
            );
        }
    }
    let in_bufs = in_view.channel_buffers;
    let in_row_offsets = in_view.row_offsets;
    let out_bufs = out_view.channel_buffers;
    let out_row_offsets = out_view.row_offsets;
    for chunk_idx in 0..num_chunks {
        let x = chunk_idx * len;
        // This does not overflow because x_offset + (num_chunks - 1) * len
        // does not.
        let in_x = x + in_view.x_offset;
        // Safety note: `in_x >= in_view.x_offset >= RADIUS` [1] and
        // `in_x <= in_view.x_offset + (num_chunks - 1) * len` [2],
        // so `in_row_offsets[r] + in_x + RADIUS + MAX_SIMD_LANES <= [3]
        //     in_row_offsets[r] + max_in_x <= [4] in_bufs[c].len()`.
        // [1] follows from the safety invariant of `in_view` + the fact
        //     that its computation does not overflow (implied by [in_check]).
        // [2] follows from chunk_idx < num_chunks and lack of overflow.
        // [3] follows from in_x + RADIUS + MAX_SIMD_LANES <= max_in_x which
        //     follows from chunk_idx < num_chunks, from how they are computed
        //     and from lack of overflow.
        // [4] is checked in [in_check_buf].
        let inv = ChannelsView {
            channel_buffers: in_bufs,
            row_offsets: in_row_offsets,
            x_offset: in_x,
        };
        let out_x = x * SCALE + out_view.x_offset;
        // Safety note: `out_x <= out_view.x_offset + (num_chunks - 1) * len * SCALE` [1],
        // so `out_row_offsets[r] + out_x + MAX_SIMD_LANES * SCALE <= [2]
        //     out_row_offsets[r] + max_out_x <= [3] out_bufs[c].len()`.
        // [1] follows from chunk_idx < num_chunks and lack of overflow (checked by
        //     [out_check])
        // [2] follows from out_x + MAX_SIMD_LANES * SCALE <= max_out_x which
        //     follows from chunk_idx < num_chunks, from how they are computed
        //     and from lack of overflow.
        // [3] is checked in [out_check_buf].
        let mut outv = ChannelsMutView {
            channel_buffers: &mut *out_bufs,
            row_offsets: out_row_offsets,
            x_offset: out_x,
        };
        body(x, &inv, &mut outv);
    }
}

#[cfg(test)]
mod tests {
    use jxl_simd::{I32SimdVec, SimdDescriptor, simd_function, test_all_instruction_sets};

    use super::*;
    use crate::image::DataTypeTag;

    simd_function!(
        run_radius1_sum_dispatch,
        d: D,
        fn run_radius1_sum(
            xsize: usize,
            in_rows: &Channels<f32>,
            out_rows: &mut ChannelsMut<f32>,
        ) {
            for_each_chunk(
                d,
                xsize,
                in_rows.view::<1, 3, 1>(),
                out_rows.view::<1, 1, 1>(),
                |_x, inv, outv| {
                    let top = inv.load::<_, 0>(d, -1, 0);
                    let left = inv.load::<_, 0>(d, 0, -1);
                    let center = inv.load::<_, 0>(d, 0, 0);
                    let right = inv.load::<_, 0>(d, 0, 1);
                    let bottom = inv.load::<_, 0>(d, 1, 0);
                    outv.store::<_, 0>(d, 0, top + left + center + right + bottom);
                },
            );
        }
    );

    simd_function!(
        run_interleaved_3ch_dispatch,
        d: D,
        fn run_interleaved_3ch(
            xsize: usize,
            in_rows: &Channels<f32>,
            out_rows: &mut ChannelsMut<f32>,
        ) {
            for_each_chunk(
                d,
                xsize,
                in_rows.view::<3, 1, 0>(),
                out_rows.view::<1, 2, 2>(),
                |_x, inv, outv| {
                    let c0 = inv.select_channel::<0>().load::<_, 0>(d, 0, 0);
                    let c1 = inv.select_channel::<1>().load::<_, 0>(d, 0, 0);
                    let c2 = inv.select_channel::<2>().load::<_, 0>(d, 0, 0);
                    outv.store_interleaved::<_, 0>(d, 0, [c0, c1]);
                    outv.store_interleaved::<_, 0>(d, 1, [c1, c2]);
                },
            );
        }
    );

    #[test]
    fn test_for_each_chunk_1ch_radius1() {
        let xsize = 20;
        let x0 = RowBuffer::x0_offset::<f32>();
        let mut in_buf = RowBuffer::new(DataTypeTag::F32, 1, 0, 0, xsize).unwrap();
        let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap();

        for r in 0..3 {
            let row = in_buf.get_row_mut::<f32>(r);
            for x in 0..(xsize + 2) {
                row[x0 - 1 + x] = (r * 100 + x) as f32;
            }
        }

        let in_refs = [&in_buf];
        let in_channels = Channels::<f32>::from_row_buffers(&in_refs, x0, 1, 3, 1);
        {
            let mut out_channels =
                ChannelsMut::<f32>::from_row_buffers(std::slice::from_mut(&mut out_buf), x0, 0, 1);
            run_radius1_sum_dispatch(xsize, &in_channels, &mut out_channels);
        }

        let out_row = &out_buf.get_row::<f32>(0)[x0..x0 + xsize];
        for (x, &actual) in out_row.iter().enumerate() {
            let top = (x + 1) as f32;
            let left = (100 + x) as f32;
            let center = (100 + x + 1) as f32;
            let right = (100 + x + 2) as f32;
            let bottom = (200 + x + 1) as f32;
            assert_eq!(actual, top + left + center + right + bottom);
        }
    }

    #[test]
    fn test_for_each_chunk_3ch_and_interleaved() {
        let xsize = 19;
        let x0 = RowBuffer::x0_offset::<f32>();
        let mut in_bufs = [
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
        ];
        let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 1, 1, xsize * 2).unwrap();

        for (c, buf) in in_bufs.iter_mut().enumerate() {
            let row = buf.get_row_mut::<f32>(0);
            for x in 0..xsize {
                row[x0 + x] = (c * 10 + x) as f32;
            }
        }

        let in_refs = [&in_bufs[0], &in_bufs[1], &in_bufs[2]];
        let in_channels = Channels::<f32>::from_row_buffers(&in_refs, x0, 0, 1, 0);
        {
            let mut out_channels =
                ChannelsMut::<f32>::from_row_buffers(std::slice::from_mut(&mut out_buf), x0, 0, 2);
            run_interleaved_3ch_dispatch(xsize, &in_channels, &mut out_channels);
        }

        let out_row0 = &out_buf.get_row::<f32>(0)[x0..x0 + xsize * 2];
        let out_row1 = &out_buf.get_row::<f32>(1)[x0..x0 + xsize * 2];
        for x in 0..xsize {
            assert_eq!(out_row0[2 * x], x as f32);
            assert_eq!(out_row0[2 * x + 1], (10 + x) as f32);
            assert_eq!(out_row1[2 * x], (10 + x) as f32);
            assert_eq!(out_row1[2 * x + 1], (20 + x) as f32);
        }
    }

    fn for_each_chunk_stencil_arb<D: SimdDescriptor>(d: D) {
        arbtest::arbtest(|u| {
            let xsize = u.int_in_range(0..=64)?;
            let image_height = u.int_in_range(1..=8)?;
            let center_y = u.int_in_range(0..=image_height - 1)?;

            let x0 = RowBuffer::x0_offset::<f32>();
            let mut in_buf = RowBuffer::new(DataTypeTag::F32, 1, 2, 0, xsize).unwrap();
            let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap();

            let num_rows = in_buf.num_rows();
            for r in 0..num_rows {
                let row = in_buf.get_row_mut::<f32>(r);
                for (i, elem) in row.iter_mut().enumerate() {
                    *elem = (r * 1000 + i) as f32;
                }
            }

            let in_refs = [&in_buf];
            let in_channels =
                Channels::<f32>::from_row_buffers(&in_refs, x0, center_y, image_height, 1);
            {
                let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                    std::slice::from_mut(&mut out_buf),
                    x0,
                    0,
                    1,
                );
                for_each_chunk(
                    d,
                    xsize,
                    in_channels.view::<1, 3, 1>(),
                    out_channels.view::<1, 1, 1>(),
                    |_x, inv, outv| {
                        let top = inv.load::<_, 0>(d, -1, 0);
                        let left = inv.load::<_, 0>(d, 0, -1);
                        let center = inv.load::<_, 0>(d, 0, 0);
                        let right = inv.load::<_, 0>(d, 0, 1);
                        let bottom = inv.load::<_, 0>(d, 1, 0);
                        outv.store::<_, 0>(d, 0, top + left + center + right + bottom);
                    },
                );
            }

            let out_row = &out_buf.get_row::<f32>(0)[x0..x0 + xsize];
            for (x, &actual) in out_row.iter().enumerate() {
                let row_top = mirror((center_y as isize) - 1, image_height) & (num_rows - 1);
                let row_mid = mirror(center_y as isize, image_height) & (num_rows - 1);
                let row_bot = mirror((center_y as isize) + 1, image_height) & (num_rows - 1);

                let top = in_buf.get_row::<f32>(row_top)[x0 + x];
                let left = in_buf.get_row::<f32>(row_mid)[x0 + x - 1];
                let center = in_buf.get_row::<f32>(row_mid)[x0 + x];
                let right = in_buf.get_row::<f32>(row_mid)[x0 + x + 1];
                let bottom = in_buf.get_row::<f32>(row_bot)[x0 + x];

                assert_eq!(actual, top + left + center + right + bottom);
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(for_each_chunk_stencil_arb);

    fn for_each_chunk_interleaved_2_arb<D: SimdDescriptor>(d: D) {
        arbtest::arbtest(|u| {
            let xsize = u.int_in_range(0..=64)?;
            let x0 = RowBuffer::x0_offset::<f32>();
            let mut in_bufs = [
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
            ];
            let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 1, xsize * 2).unwrap();

            for (c, buf) in in_bufs.iter_mut().enumerate() {
                let row = buf.get_row_mut::<f32>(0);
                for x in 0..xsize {
                    row[x0 + x] = (c * 1000 + x) as f32;
                }
            }

            let in_refs = [&in_bufs[0], &in_bufs[1]];
            let in_channels = Channels::<f32>::from_row_buffers(&in_refs, x0, 0, 1, 0);
            {
                let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                    std::slice::from_mut(&mut out_buf),
                    x0,
                    0,
                    1,
                );
                for_each_chunk(
                    d,
                    xsize,
                    in_channels.view::<2, 1, 0>(),
                    out_channels.view::<1, 1, 2>(),
                    |_x, inv, outv| {
                        let c0 = inv.select_channel::<0>().load::<_, 0>(d, 0, 0);
                        let c1 = inv.select_channel::<1>().load::<_, 0>(d, 0, 0);
                        outv.store_interleaved::<_, 0>(d, 0, [c0, c1]);
                    },
                );
            }

            let out_row = &out_buf.get_row::<f32>(0)[x0..x0 + xsize * 2];
            for x in 0..xsize {
                assert_eq!(out_row[2 * x], x as f32);
                assert_eq!(out_row[2 * x + 1], (1000 + x) as f32);
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(for_each_chunk_interleaved_2_arb);

    fn for_each_chunk_interleaved_4_arb<D: SimdDescriptor>(d: D) {
        arbtest::arbtest(|u| {
            let xsize = u.int_in_range(0..=64)?;
            let x0 = RowBuffer::x0_offset::<f32>();
            let mut in_bufs = [
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
            ];
            let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 2, xsize * 4).unwrap();

            for (c, buf) in in_bufs.iter_mut().enumerate() {
                let row = buf.get_row_mut::<f32>(0);
                for x in 0..xsize {
                    row[x0 + x] = (c * 1000 + x) as f32;
                }
            }

            let in_refs = [&in_bufs[0], &in_bufs[1], &in_bufs[2], &in_bufs[3]];
            let in_channels = Channels::<f32>::from_row_buffers(&in_refs, x0, 0, 1, 0);
            {
                let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                    std::slice::from_mut(&mut out_buf),
                    x0,
                    0,
                    1,
                );
                for_each_chunk(
                    d,
                    xsize,
                    in_channels.view::<4, 1, 0>(),
                    out_channels.view::<1, 1, 4>(),
                    |_x, inv, outv| {
                        let c0 = inv.select_channel::<0>().load::<_, 0>(d, 0, 0);
                        let c1 = inv.select_channel::<1>().load::<_, 0>(d, 0, 0);
                        let c2 = inv.select_channel::<2>().load::<_, 0>(d, 0, 0);
                        let c3 = inv.select_channel::<3>().load::<_, 0>(d, 0, 0);
                        outv.store_interleaved::<_, 0>(d, 0, [c0, c1, c2, c3]);
                    },
                );
            }

            let out_row = &out_buf.get_row::<f32>(0)[x0..x0 + xsize * 4];
            for x in 0..xsize {
                for c in 0..4 {
                    assert_eq!(out_row[4 * x + c], (c * 1000 + x) as f32);
                }
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(for_each_chunk_interleaved_4_arb);

    fn for_each_chunk_interleaved_8_arb<D: SimdDescriptor>(d: D) {
        arbtest::arbtest(|u| {
            let xsize = u.int_in_range(0..=64)?;
            let x0 = RowBuffer::x0_offset::<f32>();
            let mut in_bufs = [
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
                RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap(),
            ];
            let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 3, xsize * 8).unwrap();

            for (c, buf) in in_bufs.iter_mut().enumerate() {
                let row = buf.get_row_mut::<f32>(0);
                for x in 0..xsize {
                    row[x0 + x] = (c * 1000 + x) as f32;
                }
            }

            let in_refs = [
                &in_bufs[0],
                &in_bufs[1],
                &in_bufs[2],
                &in_bufs[3],
                &in_bufs[4],
                &in_bufs[5],
                &in_bufs[6],
                &in_bufs[7],
            ];
            let in_channels = Channels::<f32>::from_row_buffers(&in_refs, x0, 0, 1, 0);
            {
                let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                    std::slice::from_mut(&mut out_buf),
                    x0,
                    0,
                    1,
                );
                for_each_chunk(
                    d,
                    xsize,
                    in_channels.view::<8, 1, 0>(),
                    out_channels.view::<1, 1, 8>(),
                    |_x, inv, outv| {
                        let c0 = inv.select_channel::<0>().load::<_, 0>(d, 0, 0);
                        let c1 = inv.select_channel::<1>().load::<_, 0>(d, 0, 0);
                        let c2 = inv.select_channel::<2>().load::<_, 0>(d, 0, 0);
                        let c3 = inv.select_channel::<3>().load::<_, 0>(d, 0, 0);
                        let c4 = inv.select_channel::<4>().load::<_, 0>(d, 0, 0);
                        let c5 = inv.select_channel::<5>().load::<_, 0>(d, 0, 0);
                        let c6 = inv.select_channel::<6>().load::<_, 0>(d, 0, 0);
                        let c7 = inv.select_channel::<7>().load::<_, 0>(d, 0, 0);
                        outv.store_interleaved::<_, 0>(d, 0, [c0, c1, c2, c3, c4, c5, c6, c7]);
                    },
                );
            }

            let out_row = &out_buf.get_row::<f32>(0)[x0..x0 + xsize * 8];
            for x in 0..xsize {
                for c in 0..8 {
                    assert_eq!(out_row[8 * x + c], (c * 1000 + x) as f32);
                }
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(for_each_chunk_interleaved_8_arb);

    fn for_each_chunk_integer_types_arb<D: SimdDescriptor>(d: D) {
        arbtest::arbtest(|u| {
            let xsize = u.int_in_range(0..=64)?;
            let x0_f32 = RowBuffer::x0_offset::<f32>();

            // Test i16 (signed 16-bit) -> i32 vector
            {
                let x0_i16 = RowBuffer::x0_offset::<i16>();
                let mut in_buf = RowBuffer::new(DataTypeTag::I16, 0, 0, 0, xsize).unwrap();
                let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap();
                let row = in_buf.get_row_mut::<i16>(0);
                for x in 0..xsize {
                    row[x0_i16 + x] = (x as i16).wrapping_mul(137) - 1000;
                }
                let in_refs = [&in_buf];
                let in_channels = Channels::<i16>::from_row_buffers(&in_refs, x0_i16, 0, 1, 0);
                {
                    let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                        std::slice::from_mut(&mut out_buf),
                        x0_f32,
                        0,
                        1,
                    );
                    for_each_chunk(
                        d,
                        xsize,
                        in_channels.view::<1, 1, 0>(),
                        out_channels.view::<1, 1, 1>(),
                        |_x, inv, outv| {
                            let val: D::I32Vec = inv.load::<_, 0>(d, 0, 0);
                            outv.store::<_, 0>(d, 0, val.as_f32());
                        },
                    );
                }
                let in_row = &in_buf.get_row::<i16>(0)[x0_i16..x0_i16 + xsize];
                let out_row = &out_buf.get_row::<f32>(0)[x0_f32..x0_f32 + xsize];
                for (x, (&inp, &out)) in in_row.iter().zip(out_row).enumerate() {
                    assert_eq!(out, inp as f32, "mismatch at x={x} for i16");
                }
            }

            // Test u16 (unsigned 16-bit) -> i32 vector
            {
                let x0_u16 = RowBuffer::x0_offset::<u16>();
                let mut in_buf = RowBuffer::new(DataTypeTag::U16, 0, 0, 0, xsize).unwrap();
                let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap();
                let row = in_buf.get_row_mut::<u16>(0);
                for x in 0..xsize {
                    row[x0_u16 + x] = (x as u16).wrapping_mul(311) + 40000;
                }
                let in_refs = [&in_buf];
                let in_channels = Channels::<u16>::from_row_buffers(&in_refs, x0_u16, 0, 1, 0);
                {
                    let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                        std::slice::from_mut(&mut out_buf),
                        x0_f32,
                        0,
                        1,
                    );
                    for_each_chunk(
                        d,
                        xsize,
                        in_channels.view::<1, 1, 0>(),
                        out_channels.view::<1, 1, 1>(),
                        |_x, inv, outv| {
                            let val: D::I32Vec = inv.load::<_, 0>(d, 0, 0);
                            outv.store::<_, 0>(d, 0, val.as_f32());
                        },
                    );
                }
                let in_row = &in_buf.get_row::<u16>(0)[x0_u16..x0_u16 + xsize];
                let out_row = &out_buf.get_row::<f32>(0)[x0_f32..x0_f32 + xsize];
                for (x, (&inp, &out)) in in_row.iter().zip(out_row).enumerate() {
                    assert_eq!(out, inp as f32, "mismatch at x={x} for u16");
                }
            }

            // Test i32 (signed 32-bit) -> i32 vector
            {
                let x0_i32 = RowBuffer::x0_offset::<i32>();
                let mut in_buf = RowBuffer::new(DataTypeTag::I32, 0, 0, 0, xsize).unwrap();
                let mut out_buf = RowBuffer::new(DataTypeTag::F32, 0, 0, 0, xsize).unwrap();
                let row = in_buf.get_row_mut::<i32>(0);
                for x in 0..xsize {
                    row[x0_i32 + x] = (x as i32).wrapping_mul(12345) - 50000;
                }
                let in_refs = [&in_buf];
                let in_channels = Channels::<i32>::from_row_buffers(&in_refs, x0_i32, 0, 1, 0);
                {
                    let mut out_channels = ChannelsMut::<f32>::from_row_buffers(
                        std::slice::from_mut(&mut out_buf),
                        x0_f32,
                        0,
                        1,
                    );
                    for_each_chunk(
                        d,
                        xsize,
                        in_channels.view::<1, 1, 0>(),
                        out_channels.view::<1, 1, 1>(),
                        |_x, inv, outv| {
                            let val: D::I32Vec = inv.load::<_, 0>(d, 0, 0);
                            outv.store::<_, 0>(d, 0, val.as_f32());
                        },
                    );
                }
                let in_row = &in_buf.get_row::<i32>(0)[x0_i32..x0_i32 + xsize];
                let out_row = &out_buf.get_row::<f32>(0)[x0_f32..x0_f32 + xsize];
                for (x, (&inp, &out)) in in_row.iter().zip(out_row).enumerate() {
                    assert_eq!(out, inp as f32, "mismatch at x={x} for i32");
                }
            }

            Ok(())
        });
    }

    test_all_instruction_sets!(for_each_chunk_integer_types_arb);
}
