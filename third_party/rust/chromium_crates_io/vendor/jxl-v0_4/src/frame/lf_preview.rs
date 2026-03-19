// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{JxlColorProfile, JxlColorType, JxlDataFormat, JxlOutputBuffer, JxlPixelFormat},
    error::Result,
    frame::Frame,
    headers::{Orientation, frame_header::FrameType},
    image::{DataTypeTag, Rect},
    render::{
        Channels, ChannelsMut, RenderPipelineInOutStage, RenderPipelineInPlaceStage,
        buffer_splitter::{BufferSplitter, SaveStageBufferInfo},
        low_memory_pipeline::row_buffers::RowBuffer,
        save::SaveStage,
        stages::{
            ConvertF32ToF16Stage, ConvertF32ToU8Stage, ConvertF32ToU16Stage, FromLinearStage,
            OutputColorInfo, TransferFunction, Upsample8x, XybStage,
        },
    },
    util::{f16, mirror},
};

impl Frame {
    #[allow(clippy::too_many_arguments)]
    fn render_lf_frame_rect(
        &mut self,
        color_type: JxlColorType,
        data_format: JxlDataFormat,
        rect: Rect,
        upsampled_rect: Rect,
        orientation: Orientation,
        output_buffers: &mut [Option<JxlOutputBuffer<'_>>],
        full_size: (usize, usize),
        output_color_info: &OutputColorInfo,
        output_tf: &TransferFunction,
    ) -> Result<()> {
        let save_stage = SaveStage::new(
            if color_type.has_alpha() {
                &[0, 1, 2, 3]
            } else {
                &[0, 1, 2]
            },
            orientation,
            0,
            color_type,
            data_format,
            color_type.has_alpha(),
        );
        let len = rect.size.0;
        let ulen = len * 8;
        enum DataFormatConverter {
            U8(ConvertF32ToU8Stage),
            U16(ConvertF32ToU16Stage),
            F16(ConvertF32ToF16Stage),
            None,
        }
        let (converter, constant_alpha) = match data_format {
            JxlDataFormat::U8 { bit_depth } => (
                DataFormatConverter::U8(ConvertF32ToU8Stage::new(0, bit_depth)),
                RowBuffer::new_filled(DataTypeTag::U8, ulen, &(1u8 << bit_depth).to_ne_bytes())?,
            ),
            JxlDataFormat::U16 { bit_depth, .. } => (
                DataFormatConverter::U16(ConvertF32ToU16Stage::new(0, bit_depth)),
                RowBuffer::new_filled(DataTypeTag::U16, ulen, &(1u16 << bit_depth).to_ne_bytes())?,
            ),
            JxlDataFormat::F16 { .. } => (
                DataFormatConverter::F16(ConvertF32ToF16Stage::new(0)),
                RowBuffer::new_filled(
                    DataTypeTag::F16,
                    ulen,
                    &(f16::from_f32(1.0).to_bits().to_ne_bytes()),
                )?,
            ),
            JxlDataFormat::F32 { .. } => (
                DataFormatConverter::None,
                RowBuffer::new_filled(DataTypeTag::F32, ulen, &1.0f32.to_ne_bytes())?,
            ),
        };

        let upsample_stage = Upsample8x::new(&self.decoder_state.file_header.transform_data, 0);
        let mut upsample_state = upsample_stage.init_local_state(0)?.unwrap();

        let xyb_stage = XybStage::new(0, output_color_info.clone());

        let from_linear_stage = FromLinearStage::new(0, output_tf.clone());

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

        let mut output_rows = [
            RowBuffer::new(data_format.data_type(), 0, 0, 0, ulen)?,
            RowBuffer::new(data_format.data_type(), 0, 0, 0, ulen)?,
            RowBuffer::new(data_format.data_type(), 0, 0, 0, ulen)?,
        ];

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
                let off = RowBuffer::x0_offset::<f32>() - 2;
                let input_rows_refs = [
                    &lf_rows[c].get_row::<f32>(y + LF_ROW_OFFSET - 2)[off..],
                    &lf_rows[c].get_row::<f32>(y + LF_ROW_OFFSET - 1)[off..],
                    &lf_rows[c].get_row::<f32>(y + LF_ROW_OFFSET)[off..],
                    &lf_rows[c].get_row::<f32>(y + LF_ROW_OFFSET + 1)[off..],
                    &lf_rows[c].get_row::<f32>(y + LF_ROW_OFFSET + 2)[off..],
                ]
                .into_iter()
                .collect();
                let input_channels = Channels::new(input_rows_refs, 1, 5);

                let output_rows_refs =
                    upsampled_rows[c].get_rows_mut(y * 8..y * 8 + 8, RowBuffer::x0_offset::<f32>());
                let mut output_channels = ChannelsMut::new(output_rows_refs, 1, 8);

                upsample_stage.process_row_chunk(
                    (0, 0),
                    len,
                    &input_channels,
                    &mut output_channels,
                    Some(upsample_state.as_mut()),
                );
            }

            // un-XYB, convert and save.
            for uy in y * 8..y * 8 + 8 {
                // XYB
                let [x, y, b] = &mut upsampled_rows;
                let off = RowBuffer::x0_offset::<f32>();
                let mut rows = [
                    &mut x.get_row_mut(uy)[off..],
                    &mut y.get_row_mut(uy)[off..],
                    &mut b.get_row_mut(uy)[off..],
                ];
                xyb_stage.process_row_chunk((0, 0), ulen, &mut rows, None);
                from_linear_stage.process_row_chunk((0, 0), ulen, &mut rows, None);

                macro_rules! convert {
                    ($s: expr, $t: ty) => {
                        for c in 0..3 {
                            let input_rows_refs = std::iter::once(
                                &upsampled_rows[c].get_row(uy)[RowBuffer::x0_offset::<f32>()..],
                            )
                            .collect();
                            let input_channels = Channels::new(input_rows_refs, 1, 1);
                            let output_rows_refs = output_rows[c]
                                .get_rows_mut(uy..uy + 1, RowBuffer::x0_offset::<$t>());
                            let mut output_channels = ChannelsMut::new(output_rows_refs, 1, 1);
                            $s.process_row_chunk(
                                (0, 0),
                                ulen,
                                &input_channels,
                                &mut output_channels,
                                None,
                            );
                        }
                    };
                }

                // Convert
                let save_input = match &converter {
                    DataFormatConverter::U8(s) => {
                        convert!(s, u8);
                        &output_rows
                    }
                    DataFormatConverter::U16(s) => {
                        convert!(s, u16);
                        &output_rows
                    }
                    DataFormatConverter::F16(s) => {
                        convert!(s, f16);
                        &output_rows
                    }
                    DataFormatConverter::None => &upsampled_rows,
                };

                let input_no_alpha = [&save_input[0], &save_input[1], &save_input[2]];
                let input_alpha = [
                    &save_input[0],
                    &save_input[1],
                    &save_input[2],
                    &constant_alpha,
                ];

                save_stage.save_lowmem(
                    if color_type.has_alpha() {
                        &input_alpha
                    } else {
                        &input_no_alpha
                    },
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
        changed_regions: Option<&[Rect]>,
        output_profile: &JxlColorProfile,
    ) -> Result<()> {
        if self.header.needs_blending() {
            return Ok(());
        }
        if !((self.header.has_lf_frame() && self.header.frame_type == FrameType::RegularFrame)
            || (self.header.frame_type == FrameType::LFFrame && self.header.lf_level == 1))
        {
            return Ok(());
        }

        let output_color_info = OutputColorInfo::from_header(&self.decoder_state.file_header)?;

        let Some(output_tf) = output_profile.transfer_function().map(|tf| {
            TransferFunction::from_api_tf(
                tf,
                output_color_info.intensity_target,
                output_color_info.luminances,
            )
        }) else {
            return Ok(());
        };

        if output_tf.is_linear() {
            return Ok(());
        }

        let image_metadata = &self.decoder_state.file_header.image_metadata;
        if !image_metadata.xyb_encoded || !image_metadata.extra_channel_info.is_empty() {
            // We only render LF frames for XYB VarDCT images with no extra channels.
            // TODO(veluca): we might want to relax this to "no alpha".
            return Ok(());
        }
        let color_type = pixel_format.color_type;
        let data_format = pixel_format.color_data_format.unwrap();
        if pixel_format.color_data_format.is_none()
            || output_buffers.is_empty()
            || !matches!(
                color_type,
                JxlColorType::Rgb | JxlColorType::Rgba | JxlColorType::Bgr | JxlColorType::Bgra,
            )
        {
            // We only render color data, and only to 3- or 4- channel output buffers.
            return Ok(());
        }
        // We already have a fully-rendered frame and we are not requesting to re-render
        // specific regions.
        if self.decoder_state.lf_frame_was_rendered && changed_regions.is_none() {
            return Ok(());
        }
        if changed_regions.is_none() {
            self.decoder_state.lf_frame_was_rendered = true;
        }

        let sz = &self.decoder_state.file_header.size;
        let xsize = sz.xsize() as usize;
        let ysize = sz.ysize() as usize;

        let mut regions_storage;

        let regions = if let Some(regions) = changed_regions {
            regions
        } else {
            regions_storage = vec![];
            for i in (0..xsize.div_ceil(8)).step_by(256) {
                let x0 = i;
                let x1 = (i + 256).min(xsize.div_ceil(8));
                regions_storage.push(Rect {
                    origin: (x0, 0),
                    size: (x1 - x0, ysize.div_ceil(8)),
                });
            }
            &regions_storage[..]
        };

        let orientation = image_metadata.orientation;
        let info = SaveStageBufferInfo {
            downsample: (0, 0),
            orientation,
            byte_size: data_format.bytes_per_sample() * color_type.samples_per_pixel(),
            after_extend: false,
        };
        let info = [Some(info)];
        let mut bufs = [Some(JxlOutputBuffer::reborrow(&mut output_buffers[0]))];
        let mut bufs = BufferSplitter::new(&mut bufs);
        for r in regions {
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
            )?;
        }

        Ok(())
    }
}
