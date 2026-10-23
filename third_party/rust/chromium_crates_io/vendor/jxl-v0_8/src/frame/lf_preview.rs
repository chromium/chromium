// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::{
    JxlColorProfile, JxlColorType, JxlDataFormat, JxlOutputBuffer, JxlParallelRunner,
    JxlPixelFormat,
};
use crate::color::tf::TransferFunction;
use crate::error::Result;
use crate::frame::Frame;
use crate::headers::Orientation;
use crate::headers::frame_header::FrameType;
use crate::image::{DataTypeTag, Rect};
use crate::render::buffer_splitter::{BufferSplitter, OutputChannelRef, SaveStageBufferInfo};
use crate::render::low_memory_pipeline::row_buffers::RowBuffer;
use crate::render::save::SaveStage;
use crate::render::stages::{OutputColorInfo, Upsample8x, XybColorConvertStage};
use crate::render::{Channels, ChannelsMut, RenderPipelineInOutStage, RenderPipelineInPlaceStage};
use crate::util::mirror;

impl Frame {
    #[allow(clippy::too_many_arguments)]
    fn render_lf_frame_rect(
        &self,
        color_type: JxlColorType,
        data_format: JxlDataFormat,
        rect: Rect,
        upsampled_rect: Rect,
        orientation: Orientation,
        output_buffers: &mut [Option<OutputChannelRef>],
        full_size: (usize, usize),
        output_color_info: &OutputColorInfo,
        output_tf: &TransferFunction,
    ) -> Result<()> {
        let save_stage = SaveStage::new(
            &[0, 1, 2],
            orientation,
            0,
            color_type,
            data_format,
            color_type.has_alpha(),
        );
        let len = rect.size.0;
        let ulen = len * 8;
        let mut scratch = [
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, ulen)?,
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, ulen)?,
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, ulen)?,
            RowBuffer::new(DataTypeTag::F32, 0, 0, 0, ulen)?,
        ];

        let upsample_stage = Upsample8x::new(&self.decoder_state.file_header.transform_data, 0);

        let mut stage_color_info = output_color_info.clone();
        stage_color_info.tf = output_tf.clone();
        let xyb_stage = XybColorConvertStage::new(0, stage_color_info);

        let mut lf_rows = [
            RowBuffer::new(DataTypeTag::F32, 2, 0, 0, len)?,
            RowBuffer::new(DataTypeTag::F32, 2, 0, 0, len)?,
            RowBuffer::new(DataTypeTag::F32, 2, 0, 0, len)?,
        ];

        // Converted to RGB in place.
        let mut upsampled_rows = [
            RowBuffer::new(DataTypeTag::F32, 0, 3, 3, ulen)?,
            RowBuffer::new(DataTypeTag::F32, 0, 3, 3, ulen)?,
            RowBuffer::new(DataTypeTag::F32, 0, 3, 3, ulen)?,
        ];

        // At this point, we already verified that lf_frame or lf_frame_data are present.
        let src = if self.header.frame_type == FrameType::RegularFrame {
            self.decoder_state.lf_frames[0].as_ref().unwrap()
        } else {
            self.lf_frame_data.as_ref().unwrap()
        };

        const LF_ROW_OFFSET: usize = 8;

        let x0 = rect.origin.0;
        let x1 = rect.end().0;

        let y0 = rect.origin.1 as isize - 2;
        let y1 = rect.end().1 as isize + 2;

        let lf_size = src[0].size();

        for yy in y0..y1 {
            let sy = mirror(yy, lf_size.1);

            // Fill in input.
            for c in 0..3 {
                let bufy = (yy + LF_ROW_OFFSET as isize) as usize;
                let row = lf_rows[c].get_row_mut::<f32>(bufy);
                let srow = src[c].row(sy);
                let off = RowBuffer::x0_offset::<f32>();
                row[off..off + len].copy_from_slice(&srow[x0..x1]);
                row[off - 1] = srow[mirror(x0 as isize - 1, lf_size.0)];
                row[off - 2] = srow[mirror(x0 as isize - 2, lf_size.0)];
                row[off + len] = srow[mirror(x1 as isize, lf_size.0)];
                row[off + len + 1] = srow[mirror(x1 as isize + 1, lf_size.0)];
            }

            if yy < y0 + 4 {
                continue;
            }

            let y = yy as usize - 2;

            // Upsample.
            for c in 0..3 {
                let x0_off = RowBuffer::x0_offset::<f32>();
                let lf_row_ref = &lf_rows[c];
                let input_channels = Channels::from_row_buffers(
                    std::slice::from_ref(&lf_row_ref),
                    x0_off,
                    y + LF_ROW_OFFSET,
                    lf_size.1 + LF_ROW_OFFSET + 2,
                    2,
                );
                let mut output_channels = ChannelsMut::from_row_buffers(
                    std::slice::from_mut(&mut upsampled_rows[c]),
                    x0_off,
                    y * 8,
                    8,
                );

                upsample_stage.process_row_chunk(
                    (0, 0),
                    len,
                    &input_channels,
                    &mut output_channels,
                    None,
                    false,
                );
            }

            // un-XYB and save.
            for uy in y * 8..y * 8 + 8 {
                // XYB
                let [x, y, b] = &mut upsampled_rows;
                let off = RowBuffer::x0_offset::<f32>();
                let mut rows = [
                    &mut x.get_row_mut(uy)[off..],
                    &mut y.get_row_mut(uy)[off..],
                    &mut b.get_row_mut(uy)[off..],
                ];
                xyb_stage.process_row_chunk((0, 0), ulen, &mut rows, None, false);

                let input_channels = match color_type {
                    JxlColorType::Bgr | JxlColorType::Bgra => {
                        [&upsampled_rows[2], &upsampled_rows[1], &upsampled_rows[0]]
                    }
                    _ => [&upsampled_rows[0], &upsampled_rows[1], &upsampled_rows[2]],
                };

                save_stage.save_lowmem(
                    &input_channels,
                    &mut scratch,
                    output_buffers,
                    upsampled_rect.size,
                    uy,
                    upsampled_rect.origin,
                    full_size,
                    (0, 0),
                )?;
            }
        }

        Ok(())
    }

    pub fn maybe_preview_lf_frame(
        &mut self,
        pixel_format: &JxlPixelFormat,
        output_buffers: &mut [JxlOutputBuffer<'_>],
        output_profile: &JxlColorProfile,
        parallel_runner: &mut dyn JxlParallelRunner,
    ) -> Result<bool> {
        if self.header.needs_blending() {
            return Ok(false);
        }
        if !((self.header.has_lf_frame() && self.header.frame_type == FrameType::RegularFrame)
            || (self.header.frame_type == FrameType::LFFrame && self.header.lf_level == 1))
        {
            return Ok(false);
        }

        let output_color_info = OutputColorInfo::from_header(&self.decoder_state.file_header)?;

        let Some(output_tf) = output_profile.transfer_function().map(|tf| {
            TransferFunction::from_api_tf(
                tf,
                output_color_info.intensity_target,
                output_color_info.luminances,
            )
        }) else {
            return Ok(false);
        };

        let image_metadata = &self.decoder_state.file_header.image_metadata;
        if !image_metadata.xyb_encoded || !image_metadata.extra_channel_info.is_empty() {
            // We only render LF frames for XYB VarDCT images with no extra channels.
            // TODO(veluca): we might want to relax this to "no alpha".
            return Ok(false);
        }
        let color_type = pixel_format.color_type;
        let Some(data_format) = pixel_format.color_data_format else {
            return Ok(false);
        };
        if output_buffers.is_empty()
            || !matches!(
                color_type,
                JxlColorType::Rgb | JxlColorType::Rgba | JxlColorType::Bgr | JxlColorType::Bgra,
            )
        {
            // We only render color data, and only to 3- or 4- channel output buffers.
            return Ok(false);
        }
        let sz = &self.decoder_state.file_header.size;
        let xsize = sz.xsize() as usize;
        let ysize = sz.ysize() as usize;

        let groups = std::mem::take(&mut self.lf_preview_dirty_groups);
        if groups.is_empty() {
            return Ok(false);
        }
        self.decoder_state.lf_frame_was_rendered = true;
        let regions_storage: Vec<Rect> = groups
            .into_iter()
            .map(|g| self.header.group_rect(g))
            .collect();
        let regions = &regions_storage[..];

        let orientation = image_metadata.orientation;
        let info = SaveStageBufferInfo {
            downsample: (0, 0),
            orientation,
            byte_size: data_format.bytes_per_sample() * color_type.samples_per_pixel(),
            after_extend: false,
        };
        let info = [Some(info)];
        let mut bufs = [Some(JxlOutputBuffer::reborrow(&mut output_buffers[0]))];
        let bufs = BufferSplitter::new(&mut bufs);
        parallel_runner.run_ordered(regions.len(), None, &|i| {
            let r = &regions[i];
            let upsampled_rect = Rect {
                size: (r.size.0 * 8, r.size.1 * 8),
                origin: (r.origin.0 * 8, r.origin.1 * 8),
            };
            let upsampled_rect = upsampled_rect.clip((xsize, ysize));
            let mut bufs = bufs.get_local_buffers(
                &info,
                upsampled_rect,
                false,
                (xsize, ysize),
                (xsize, ysize),
                (0, 0),
            );
            self.render_lf_frame_rect(
                color_type,
                data_format,
                *r,
                upsampled_rect,
                orientation,
                &mut bufs,
                (xsize, ysize),
                &output_color_info,
                &output_tf,
            )
        })?;

        Ok(!regions.is_empty())
    }
}
