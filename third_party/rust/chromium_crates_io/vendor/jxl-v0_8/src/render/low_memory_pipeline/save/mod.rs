// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::row_buffers::RowBuffer;
use crate::api::{Endianness, JxlDataFormat};
use crate::error::Result;
use crate::headers::Orientation;
use crate::render::buffer_splitter::OutputChannelRef;
use crate::render::save::{SaveChannelType, SaveStage};
use crate::util::ChannelVec;

mod identity;

// Placeholder slow implementation.
impl SaveStage {
    // Takes as input only those channels that are *actually* saved.
    #[allow(clippy::too_many_arguments)]
    pub fn save_lowmem(
        &self,
        data: &[&RowBuffer],
        scratch: &mut [RowBuffer; 4],
        buffers: &mut [Option<OutputChannelRef>],
        group_size: (usize, usize),
        frame_y: usize,
        group_origin: (usize, usize),
        full_image_size: (usize, usize),
        frame_origin: (isize, isize),
    ) -> Result<()> {
        let Some(buf) = buffers[self.output_buffer_index].as_mut() else {
            return Ok(());
        };

        let group_y = frame_y - group_origin.1;

        let relative_full_image_start = (
            -frame_origin.0 - (group_origin.0 as isize),
            -frame_origin.1 - (group_origin.1 as isize),
        );

        let relative_full_image_end = (
            relative_full_image_start.0 + full_image_size.0 as isize,
            relative_full_image_start.1 + full_image_size.1 as isize,
        );

        let save_start = (
            relative_full_image_start.0.max(0) as usize,
            relative_full_image_start.1.max(0) as usize,
        );

        let save_end = (
            relative_full_image_end.0.clamp(0, group_size.0 as isize) as usize,
            relative_full_image_end.1.clamp(0, group_size.1 as isize) as usize,
        );

        // If the visible area were empty, we'd have gotten None for the buffer.
        assert!(save_start.0 < save_end.0);
        assert!(save_start.1 < save_end.1);

        if !(save_start.1..save_end.1).contains(&group_y) {
            // The current row is outside the visible area - skip rendering it.
            return Ok(());
        }

        let relative_y = group_y - save_start.1;
        let save_size = (save_end.0 - save_start.0, save_end.1 - save_start.1);

        let output_y = match self.orientation {
            Orientation::Identity => Some(relative_y),
            Orientation::FlipVertical => Some(save_size.1 - 1 - relative_y),
            _ => None,
        };

        if let Some(output_y) = output_y
            && identity::store_fused(
                self,
                data,
                frame_y,
                group_origin.0,
                save_start.0..save_end.0,
                buf,
                output_y,
            )
        {
            return Ok(());
        }

        let conv_start = save_start.0;
        let conv_len = save_end.0 - conv_start;
        let mut save_buffers: ChannelVec<&RowBuffer> = ChannelVec::new();
        let [s0, s1, s2, s3] = scratch;
        let mut scratch_iter = [s0, s1, s2, s3].into_iter();

        for (i, (&in_buf, &ch_ty)) in data.iter().zip(self.channel_types.iter()).enumerate() {
            let ch = self.channels[i];
            if ch_ty == SaveChannelType::F32
                && matches!(self.data_format, JxlDataFormat::F32 { .. })
            {
                save_buffers.push(in_buf);
                continue;
            }
            let s_buf = scratch_iter.next().unwrap();
            match (ch_ty, self.data_format) {
                (SaveChannelType::F32, JxlDataFormat::U8 { bit_depth }) => {
                    let max = ((1u32 << bit_depth) - 1) as f32;
                    let src = &in_buf.get_row::<f32>(frame_y)
                        [RowBuffer::x0_offset::<f32>() + conv_start..];
                    let dst = &mut s_buf.get_row_mut::<u8>(0)
                        [RowBuffer::x0_offset::<u8>() + conv_start..];
                    identity::f32_to_u8(
                        src,
                        dst,
                        max,
                        (group_origin.0 + conv_start, frame_y),
                        ch,
                        conv_len,
                    );
                }
                (SaveChannelType::F32, JxlDataFormat::U16 { bit_depth, .. }) => {
                    let max = ((1u32 << bit_depth) - 1) as f32;
                    let src = &in_buf.get_row::<f32>(frame_y)
                        [RowBuffer::x0_offset::<f32>() + conv_start..];
                    let dst = &mut s_buf.get_row_mut::<u16>(0)
                        [RowBuffer::x0_offset::<u16>() + conv_start..];
                    identity::f32_to_u16(src, dst, max, conv_len);
                }
                (SaveChannelType::F32, JxlDataFormat::F16 { .. }) => {
                    let src = &in_buf.get_row::<f32>(frame_y)
                        [RowBuffer::x0_offset::<f32>() + conv_start..];
                    let dst = &mut s_buf.get_row_mut::<u16>(0)
                        [RowBuffer::x0_offset::<u16>() + conv_start..];
                    identity::f32_to_f16(src, dst, conv_len);
                }
                (
                    SaveChannelType::I16 { bit_depth: in_bd },
                    JxlDataFormat::U8 { bit_depth: out_bd },
                ) => {
                    let max = (1i32 << out_bd) - 1;
                    let scale = max / ((1i32 << in_bd) - 1);
                    let src = &in_buf.get_row::<i16>(frame_y)
                        [RowBuffer::x0_offset::<i16>() + conv_start..];
                    let dst = &mut s_buf.get_row_mut::<u8>(0)
                        [RowBuffer::x0_offset::<u8>() + conv_start..];
                    identity::i16_to_u8(src, dst, scale, max, conv_len);
                }
                (SaveChannelType::I16 { bit_depth: in_bd }, JxlDataFormat::F16 { .. }) => {
                    let scale = 1.0 / ((1u64 << in_bd) - 1) as f32;
                    let src = &in_buf.get_row::<i16>(frame_y)
                        [RowBuffer::x0_offset::<i16>() + conv_start..];
                    let dst = &mut s_buf.get_row_mut::<u16>(0)
                        [RowBuffer::x0_offset::<u16>() + conv_start..];
                    identity::i16_to_f16(src, dst, scale, conv_len);
                }
                (
                    SaveChannelType::I32 { bit_depth: in_bd },
                    JxlDataFormat::U8 { bit_depth: out_bd },
                ) => {
                    let max = (1i32 << out_bd) - 1;
                    let scale = max / ((1i32 << in_bd) - 1);
                    let src = &in_buf.get_row::<i32>(frame_y)
                        [RowBuffer::x0_offset::<i32>() + conv_start..];
                    let dst = &mut s_buf.get_row_mut::<u8>(0)
                        [RowBuffer::x0_offset::<u8>() + conv_start..];
                    identity::i32_to_u8(src, dst, scale, max, conv_len);
                }
                _ => unreachable!(),
            }
            save_buffers.push(&*s_buf);
        }

        if self.fill_opaque_alpha {
            let s_buf = scratch_iter.next().unwrap();
            match self.data_format {
                JxlDataFormat::U8 { bit_depth } => {
                    let off = RowBuffer::x0_offset::<u8>();
                    s_buf.get_row_mut::<u8>(0)[off + conv_start..off + save_end.0]
                        .fill(((1u16 << bit_depth) - 1) as u8);
                }
                JxlDataFormat::U16 { bit_depth, .. } => {
                    let off = RowBuffer::x0_offset::<u16>();
                    s_buf.get_row_mut::<u16>(0)[off + conv_start..off + save_end.0]
                        .fill(((1u32 << bit_depth) - 1) as u16);
                }
                JxlDataFormat::F16 { .. } => {
                    let off = RowBuffer::x0_offset::<u16>();
                    s_buf.get_row_mut::<u16>(0)[off + conv_start..off + save_end.0].fill(0x3c00);
                }
                JxlDataFormat::F32 { .. } => {
                    let off = RowBuffer::x0_offset::<f32>();
                    s_buf.get_row_mut::<f32>(0)[off + conv_start..off + save_end.0].fill(1.0);
                }
            }
            save_buffers.push(&*s_buf);
        }
        let data = &save_buffers[..];

        if let Some(output_y) = output_y
            && identity::store(
                data,
                frame_y,
                save_start.0..save_end.0,
                buf,
                output_y,
                self.data_format,
            )
        {
            return Ok(());
        }

        let nc = self.output_channels();
        debug_assert_eq!(nc, data.len());
        let (x0, y0) = self.orientation.display_pixel((0, relative_y), save_size);
        let x0 = x0 as isize;
        let y0 = y0 as isize;
        // Compute the per-pixel step directly from the orientation rather
        // than via `display_pixel((1, ..))`, which would underflow when
        // `save_size.0 == 1` and the orientation flips x.
        let (dx, dy) = self.orientation.display_row_step();

        macro_rules! write_oriented {
            ($ty:ty, $bps:literal, $endianness:expr) => {{
                let off = RowBuffer::x0_offset::<$ty>() + conv_start;
                let mut src_rows = [&[] as &[$ty]; 4];
                for (c, d) in data.iter().enumerate() {
                    src_rows[c] = &d.get_row::<$ty>(frame_y)[off..off + conv_len];
                }
                let to_bytes = {
                    #[inline(always)]
                    |px: $ty| -> [u8; $bps] {
                        if $endianness == Endianness::LittleEndian {
                            px.to_le_bytes()
                        } else {
                            px.to_be_bytes()
                        }
                    }
                };
                macro_rules! write_nc {
                    ($nc:literal) => {{
                        const PIXEL_BYTES: usize = $nc * $bps;
                        if dy == 0 {
                            let out_row = buf.row_mut(y0 as usize);
                            for i in 0..conv_len {
                                let x = (x0 + dx * i as isize) as usize;
                                let dst = &mut out_row[x * PIXEL_BYTES..][..PIXEL_BYTES];
                                for (c, chunk) in
                                    dst.as_chunks_mut::<$bps>().0.iter_mut().enumerate()
                                {
                                    *chunk = to_bytes(src_rows[c][i]);
                                }
                            }
                        } else {
                            let byte_x = (x0 as usize) * PIXEL_BYTES;
                            for i in 0..conv_len {
                                let y = (y0 + dy * i as isize) as usize;
                                let dst = &mut buf.row_mut(y)[byte_x..byte_x + PIXEL_BYTES];
                                for (c, chunk) in
                                    dst.as_chunks_mut::<$bps>().0.iter_mut().enumerate()
                                {
                                    *chunk = to_bytes(src_rows[c][i]);
                                }
                            }
                        }
                    }};
                }
                match nc {
                    1 => write_nc!(1),
                    2 => write_nc!(2),
                    3 => write_nc!(3),
                    4 => write_nc!(4),
                    _ => unreachable!(),
                }
            }};
        }

        match self.data_format {
            JxlDataFormat::U8 { .. } => {
                write_oriented!(u8, 1, Endianness::LittleEndian);
            }
            JxlDataFormat::U16 { endianness, .. } | JxlDataFormat::F16 { endianness, .. } => {
                write_oriented!(u16, 2, endianness);
            }
            JxlDataFormat::F32 { endianness, .. } => {
                write_oriented!(f32, 4, endianness);
            }
        }
        Ok(())
    }
}
