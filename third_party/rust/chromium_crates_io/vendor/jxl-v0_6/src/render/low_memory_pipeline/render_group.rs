// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::sync::{RwLock, RwLockReadGuard};
use std::ops::Range;

use crate::{
    error::Result,
    image::{DataTypeTag, OwnedRawImage, Rect},
    render::{
        buffer_splitter::OutputChannelRef,
        internal::{ChannelInfo, Stage},
        low_memory_pipeline::{
            LowMemoryRenderPipelinePerThread, helpers::get_distinct_indices, run_stage::ExtraInfo,
        },
    },
    util::{ChannelVec, ShiftRightCeil, mirror, tracing_wrappers::*},
};

use super::{LowMemoryRenderPipeline, row_buffers::RowBuffer};

fn apply_x_padding(
    input_type: DataTypeTag,
    row: &mut [u8],
    to_pad: Range<isize>,
    valid_pixels: Range<isize>,
) {
    let x0_offset = RowBuffer::x0_byte_offset() as isize;
    let num_valid = valid_pixels.clone().count();
    let sz = input_type.size();
    match sz {
        1 => {
            for x in to_pad {
                let sx = mirror(x - valid_pixels.start, num_valid) as isize + valid_pixels.start;
                let from = (x0_offset + sx) as usize;
                let to = (x0_offset + x) as usize;
                row[to] = row[from];
            }
        }
        2 => {
            for x in to_pad {
                let sx = mirror(x - valid_pixels.start, num_valid) as isize + valid_pixels.start;
                let from = (x0_offset + sx * 2) as usize;
                let to = (x0_offset + x * 2) as usize;
                row[to] = row[from];
                row[to + 1] = row[from + 1];
            }
        }
        4 => {
            for x in to_pad {
                let sx = mirror(x - valid_pixels.start, num_valid) as isize + valid_pixels.start;
                let from = (x0_offset + sx * 4) as usize;
                let to = (x0_offset + x * 4) as usize;
                row[to] = row[from];
                row[to + 1] = row[from + 1];
                row[to + 2] = row[from + 2];
                row[to + 3] = row[from + 3];
            }
        }
        _ => {
            unimplemented!("only 1, 2 or 4 byte data types supported");
        }
    }
}

struct BufferFiller<'a> {
    c: usize,
    ty: DataTypeTag,
    group_y0: usize,
    group_ysize: usize,
    top_y_offset: usize,
    bot_y_offset: usize,
    images: [Option<RwLockReadGuard<'a, Option<OwnedRawImage>>>; 9],
    copy_byte_offset_initial: usize,
    src_byte_offset_left_topbottom: usize,
    src_byte_offset_left_center: usize,
    to_copy_left: usize,
    copy_start: usize,
    copy_end: usize,
    to_copy_main: usize,
    to_copy_right: usize,
    padding_range: Option<(isize, isize)>,
}

impl<'a> BufferFiller<'a> {
    fn new(
        rp: &'a LowMemoryRenderPipeline,
        c: usize,
        (x0, xsize): (usize, usize),
        (gx, gy): (usize, usize),
        vyrange: Range<usize>,
    ) -> Option<Self> {
        if !rp.shared.channel_is_used[c] || vyrange.is_empty() {
            return None;
        }
        let ChannelInfo {
            ty,
            downsample: (dx, dy),
        } = rp.shared.channel_info[0][c];
        let ty = ty.expect("Channel info should be populated at this point");

        let scaled_y_border = rp.input_border_pixels[c].1 << dy;
        let stage_vy_start = vyrange.start as isize - scaled_y_border as isize;
        let stage_vy_end = (vyrange.end as isize - 1) + scaled_y_border as isize;
        let min_y = stage_vy_start >> dy;
        let max_y = stage_vy_end >> dy;
        let max_valid_y = rp.shared.input_size.1.shrc(dy) as isize - 1;

        let yrange = if max_y < 0 || min_y > max_valid_y {
            0..0
        } else {
            let start = min_y.max(0) as usize;
            let end = (max_y.min(max_valid_y) as usize) + 1;
            start..end
        };

        if yrange.is_empty() {
            return None;
        }

        let x0 = x0 >> dx;
        let xsize = xsize >> dx;
        let group_ysize = 1 << (rp.shared.log_group_size - dy as usize);
        let group_xsize = 1 << (rp.shared.log_group_size - dx as usize);

        let (bx, by) = rp.border_size;

        let group_y0 = gy * group_ysize;
        let group_x0 = gx << (rp.shared.log_group_size - dx as usize);
        let group_x1 = group_x0 + group_xsize;

        let top_y_offset = (by >> dy) * 4;
        let bot_y_offset = group_y0 + group_ysize;

        let has_top = yrange.start < group_y0;
        let has_center = yrange.start < group_y0 + group_ysize && yrange.end > group_y0;
        let has_bot = yrange.end > group_y0 + group_ysize;

        let copy_x0 = x0.saturating_sub(rp.input_border_pixels[c].0);
        let copy_x1 =
            (x0 + xsize + rp.input_border_pixels[c].0).min(rp.shared.input_size.0.shrc(dx));

        debug_assert!(copy_x1 >= group_x0);

        let copy_byte_offset_initial = RowBuffer::x0_byte_offset() - (x0 - copy_x0) * ty.size();

        let gw = rp.shared.group_count.0;

        let mut images: [Option<RwLockReadGuard<'_, Option<OwnedRawImage>>>; 9] =
            std::array::from_fn(|_| None);

        let has_left = copy_x0 < group_x0;
        let has_right = copy_x1 > group_x1;

        let to_copy_left = if has_left {
            (group_x0 - copy_x0) * ty.size()
        } else {
            0
        };

        let src_byte_offset_left_topbottom = group_xsize * ty.size() - to_copy_left;
        let src_byte_offset_left_center = 4 * (bx >> dx) * ty.size() - to_copy_left;

        fn make_ref(
            g: &RwLock<Option<OwnedRawImage>>,
        ) -> Option<RwLockReadGuard<'_, Option<OwnedRawImage>>> {
            Some(g.try_read().unwrap())
        }

        if has_top {
            let base_gid = (gy - 1) * gw + gx;
            if has_left {
                images[0] = make_ref(&rp.input_buffers.get(base_gid - 1).topbottom[c]);
            }
            images[1] = make_ref(&rp.input_buffers.get(base_gid).topbottom[c]);
            if has_right {
                images[2] = make_ref(&rp.input_buffers.get(base_gid + 1).topbottom[c]);
            }
        }

        if has_center {
            let base_gid = gy * gw + gx;
            if has_left {
                images[3] = make_ref(&rp.input_buffers.get(base_gid - 1).leftright[c]);
            }
            images[4] = make_ref(&rp.input_buffers.get(base_gid).data[c]);
            if has_right {
                images[5] = make_ref(&rp.input_buffers.get(base_gid + 1).leftright[c]);
            }
        }

        if has_bot {
            let base_gid = (gy + 1) * gw + gx;
            if has_left {
                images[6] = make_ref(&rp.input_buffers.get(base_gid - 1).topbottom[c]);
            }
            images[7] = make_ref(&rp.input_buffers.get(base_gid).topbottom[c]);
            if has_right {
                images[8] = make_ref(&rp.input_buffers.get(base_gid + 1).topbottom[c]);
            }
        }

        let copy_start = copy_x0.saturating_sub(group_x0) * ty.size();
        let copy_end = (copy_x1.min(group_x1) - group_x0) * ty.size();
        let to_copy_main = copy_end - copy_start;

        let (to_copy_right, padding_range) = if has_right {
            let gid = gy * gw + gx;
            let next_group_xsize = rp.shared.group_size(gid + 1).0.shrc(dx);
            let border_x = (copy_x1 - group_x1).min(next_group_xsize);
            let to_copy_right = border_x * ty.size();
            let pad = if border_x + group_x1 < copy_x1 {
                let pad_from = (xsize + border_x) as isize;
                let pad_to = (xsize + copy_x1 - group_x1) as isize;
                Some((pad_from, pad_to))
            } else {
                None
            };
            (to_copy_right, pad)
        } else {
            (0, None)
        };

        Some(Self {
            c,
            ty,
            group_y0,
            group_ysize,
            top_y_offset,
            bot_y_offset,
            images,
            copy_byte_offset_initial,
            src_byte_offset_left_topbottom,
            src_byte_offset_left_center,
            to_copy_left,
            copy_start,
            copy_end,
            to_copy_main,
            to_copy_right,
            padding_range,
        })
    }

    fn fill(&self, data: &mut LowMemoryRenderPipelinePerThread, y: usize) {
        let (row_idx, input_y) = if y < self.group_y0 {
            (0, y + self.top_y_offset - self.group_y0)
        } else if y >= self.group_y0 + self.group_ysize {
            (2, y - self.bot_y_offset)
        } else {
            (1, y - self.group_y0)
        };

        let base = row_idx * 3;
        let output_row = data.row_buffers[0][self.c].get_row_mut::<u8>(y);
        let mut copy_byte_offset = self.copy_byte_offset_initial;

        if let Some(left_buf_guard) = &self.images[base] {
            let left_buf = left_buf_guard.as_ref().unwrap();
            let input_row = left_buf.row(input_y);
            let src_byte_offset = if row_idx != 1 {
                self.src_byte_offset_left_topbottom
            } else {
                self.src_byte_offset_left_center
            };
            output_row[copy_byte_offset..copy_byte_offset + self.to_copy_left]
                .copy_from_slice(&input_row[src_byte_offset..src_byte_offset + self.to_copy_left]);
            copy_byte_offset += self.to_copy_left;
        }

        let center_buf = self.images[base + 1].as_ref().unwrap().as_ref().unwrap();
        let input_row = center_buf.row(input_y);
        output_row[copy_byte_offset..copy_byte_offset + self.to_copy_main]
            .copy_from_slice(&input_row[self.copy_start..self.copy_end]);
        copy_byte_offset += self.to_copy_main;

        if let Some(right_buf_guard) = &self.images[base + 2] {
            let right_buf = right_buf_guard.as_ref().unwrap();
            let input_row = right_buf.row(input_y);
            output_row[copy_byte_offset..copy_byte_offset + self.to_copy_right]
                .copy_from_slice(&input_row[..self.to_copy_right]);
            if let Some((pad_from, pad_to)) = self.padding_range {
                apply_x_padding(self.ty, output_row, pad_from..pad_to, 0..pad_from);
            }
        }
    }
}

impl LowMemoryRenderPipeline {
    // Renders *parts* of group's worth of data.
    // In particular, renders the sub-rectangle given in `image_area`, where (1, 1) refers to
    // the center of the group, and 0 and 2 include data from the neighbouring group (if any).
    #[instrument(skip(self, buffers))]
    pub(super) fn render_group(
        &self,
        data: &mut LowMemoryRenderPipelinePerThread,
        (gx, gy): (usize, usize),
        image_area: Rect,
        buffers: &mut [Option<OutputChannelRef>],
    ) -> Result<()> {
        let start_of_row = image_area.origin.0 == 0;
        let end_of_row = image_area.end().0 == self.shared.input_size.0;

        let Rect {
            origin: (x0, y0),
            size: (xsize, num_rows),
        } = image_area;

        let num_channels = self.shared.num_channels();
        let num_extra_rows = self.border_size.1;

        // This follows the same implementation strategy as the C++ code in libjxl.
        // We pretend that every stage has a vertical shift of 0, i.e. it is as tall
        // as the final image.
        // We call each such row a "virtual" row, because it may or may not correspond
        // to an actual row of the current processing stage; actual processing happens
        // when vy % (1<<vshift) == 0.

        // Rects are not guaranteed to start at a vertical position that is aligned
        // to the rows of subsampled channels/stages. Since border rows are computed
        // with floor semantics (see the parity adjustments in the loop below), the
        // first border row of a subsampled channel can start up to (1 << dy) - 1
        // virtual rows before y0 - scaled_y_border; extend the virtual row range
        // upwards so that it is still produced.
        let max_dy = (0..num_channels)
            .map(|c| self.shared.channel_info[0][c].downsample.1 as usize)
            .chain(self.downsampling_for_stage.iter().map(|d| d.1))
            .max()
            .unwrap_or(0);
        let vy0 = y0.saturating_sub(num_extra_rows + (1 << max_dy) - 1);
        let vy1 = image_area.end().1 + num_extra_rows;

        let fillers: ChannelVec<_> = (0..num_channels)
            .map(|c| BufferFiller::new(self, c, (x0, xsize), (gx, gy), y0..image_area.end().1))
            .collect();

        for vy in vy0..vy1 {
            let mut current_origin = (0, 0);
            let mut current_size = self.shared.input_size;

            // Step 1: read input channels.
            for c in 0..num_channels {
                let Some(filler) = &fillers[c] else {
                    continue;
                };
                // Same logic as below, but adapted to the input stage.
                let (_, dy) = self.shared.channel_info[0][c].downsample;
                let scaled_y_border = self.input_border_pixels[c].1 << dy;
                let stage_vy = vy as isize - num_extra_rows as isize + scaled_y_border as isize;
                if stage_vy % (1 << dy) != 0 {
                    continue;
                }
                let y = stage_vy >> dy;
                // The first needed row is computed with *floor* semantics (matching the
                // x direction and BufferFiller::new): if y0 - scaled_y_border is not
                // aligned to the channel's vertical subsampling, the subsampled row
                // containing it must still be filled, as downstream stages will read it.
                if y < (y0 as isize - scaled_y_border as isize) >> dy {
                    continue;
                }
                // Do not produce rows in out-of-bounds areas.
                if y < 0 || y >= self.shared.input_size.1.shrc(dy) as isize {
                    continue;
                }
                let y = y as usize;
                filler.fill(data, y);
            }
            // Step 2: go through stages one by one.
            for (i, stage) in self.shared.stages.iter().enumerate() {
                let (dx, dy) = self.downsampling_for_stage[i];
                // The logic below uses *virtual* y coordinates, so we need to convert the border
                // amount appropriately.
                let scaled_y_border = self.stage_output_border_pixels[i].1 << dy;
                // I knew the reason behind this formula at some point, but now I don't.
                let stage_vy = vy as isize - num_extra_rows as isize + scaled_y_border as isize;
                if stage_vy % (1 << dy) != 0 {
                    continue;
                }
                let y = stage_vy >> dy;
                if matches!(stage, Stage::Save(_)) {
                    // Save stages write to shared output buffers, so they must only
                    // process rows owned by this rect; keep ceil semantics for them.
                    if stage_vy - (y0 as isize) < -(scaled_y_border as isize) {
                        continue;
                    }
                } else if y < (y0 as isize - scaled_y_border as isize) >> dy {
                    // As for input channels, border rows of vertically subsampled
                    // stages are computed with floor semantics.
                    continue;
                }
                let shifted_ysize = self.shared.input_size.1.shrc(dy);
                // Do not produce rows in out-of-bounds areas.
                if y < 0 || y >= shifted_ysize as isize {
                    continue;
                }
                let y = y as usize;

                let out_extra_x = self.stage_output_border_pixels[i].0;
                let shifted_xsize = xsize.shrc(dx);

                match stage {
                    Stage::InPlace(s) => {
                        let mut buffers = get_distinct_indices(
                            &mut data.row_buffers,
                            &self.sorted_buffer_indices[i],
                        );
                        s.run_stage_on(
                            ExtraInfo {
                                xsize: shifted_xsize,
                                current_row: y,
                                group_x0: x0 >> dx,
                                out_extra_x,
                                start_of_row,
                                end_of_row,
                                image_height: shifted_ysize,
                            },
                            &mut buffers,
                            data.local_states[i].as_deref_mut(),
                        );
                    }
                    Stage::Save(s) => {
                        // Find buffers for channels that will be saved.
                        // Channel ordering is handled in stage_input_buffer_index construction.
                        let mut input_data: ChannelVec<_> = self.stage_input_buffer_index[i]
                            .iter()
                            .map(|(si, ci)| &data.row_buffers[*si][*ci])
                            .collect();
                        // Append opaque alpha buffer if fill_opaque_alpha is set
                        if let Some(ref alpha_buf) = self.opaque_alpha_buffers[i] {
                            input_data.push(alpha_buf);
                        }
                        s.save_lowmem(
                            &input_data,
                            &mut *buffers,
                            (xsize >> dx, num_rows >> dy),
                            y,
                            (x0 >> dx, y0 >> dy),
                            current_size,
                            current_origin,
                        )?;
                    }
                    Stage::Extend(s) => {
                        current_size = s.image_size;
                        current_origin = s.frame_origin;
                    }
                    Stage::InOut(s) => {
                        let borderx = s.border().0 as usize;
                        let bordery = s.border().1 as isize;
                        // Apply x padding.
                        if start_of_row && borderx != 0 {
                            for (si, ci) in self.stage_input_buffer_index[i].iter() {
                                for iy in -bordery..=bordery {
                                    let y = mirror(y as isize + iy, shifted_ysize);
                                    apply_x_padding(
                                        s.input_type(),
                                        data.row_buffers[*si][*ci].get_row_mut::<u8>(y),
                                        -(borderx as isize)..0,
                                        // Either xsize is the actual size of the image, or it is
                                        // much larger than borderx, so this works out either way.
                                        0..shifted_xsize as isize,
                                    );
                                }
                            }
                        }
                        if end_of_row && borderx != 0 {
                            for (si, ci) in self.stage_input_buffer_index[i].iter() {
                                for iy in -bordery..=bordery {
                                    let y = mirror(y as isize + iy, shifted_ysize);
                                    apply_x_padding(
                                        s.input_type(),
                                        data.row_buffers[*si][*ci].get_row_mut::<u8>(y),
                                        shifted_xsize as isize..(shifted_xsize + borderx) as isize,
                                        // borderx..0 is either data from the neighbouring group or
                                        // data that was filled in by the iteration above.
                                        -(borderx as isize)..shifted_xsize as isize,
                                    );
                                }
                            }
                        }
                        let (inb, outb) = data.row_buffers.split_at_mut(i + 1);
                        // Prepare pointers to input and output buffers.
                        let input_data: ChannelVec<_> = self.stage_input_buffer_index[i]
                            .iter()
                            .map(|(si, ci)| &inb[*si][*ci])
                            .collect();
                        s.run_stage_on(
                            ExtraInfo {
                                xsize: shifted_xsize,
                                current_row: y,
                                group_x0: x0 >> dx,
                                out_extra_x,
                                start_of_row,
                                end_of_row,
                                image_height: shifted_ysize,
                            },
                            &input_data,
                            &mut outb[0][..],
                            data.local_states[i].as_deref_mut(),
                        );
                    }
                }
            }
        }
        Ok(())
    }

    // Renders a chunk of data outside the current frame.
    #[instrument(skip(self, buffers))]
    pub(super) fn render_outside_frame_internal(
        &self,
        data: &mut LowMemoryRenderPipelinePerThread,
        xrange: Range<usize>,
        yrange: Range<usize>,
        buffers: &mut [Option<OutputChannelRef>],
    ) -> Result<()> {
        let num_channels = self.shared.num_channels();
        let x0 = xrange.start;
        let y0 = yrange.start;
        let xsize = xrange.clone().count();
        let ysize = yrange.clone().count();
        // Significantly simplified version of render_group.
        for y in yrange.clone() {
            let extend = self.shared.extend_stage_index.unwrap();
            // Step 1: get padding from extend stage.
            for c in 0..num_channels {
                let (si, ci) = self.stage_input_buffer_index[extend][c];
                let buffer = &mut data.row_buffers[si][ci];
                let Stage::Extend(extend) = &self.shared.stages[extend] else {
                    unreachable!("extend stage is not an extend stage");
                };
                let row = &mut buffer.get_row_mut(y)[RowBuffer::x0_offset::<f32>()..];
                extend.process_row_chunk((x0, y), xsize, c, row);
            }
            // Step 2: go through remaining stages one by one.
            for (i, stage) in self.shared.stages.iter().enumerate().skip(extend + 1) {
                assert_eq!(self.downsampling_for_stage[i], (0, 0));

                match stage {
                    Stage::InPlace(s) => {
                        let mut buffers = get_distinct_indices(
                            &mut data.row_buffers,
                            &self.sorted_buffer_indices[i],
                        );
                        s.run_stage_on(
                            ExtraInfo {
                                xsize,
                                current_row: y,
                                group_x0: x0,
                                out_extra_x: 0,
                                start_of_row: false,
                                end_of_row: false,
                                image_height: self.shared.input_size.1,
                            },
                            &mut buffers,
                            data.local_states[i].as_deref_mut(),
                        );
                    }
                    Stage::Save(s) => {
                        // Find buffers for channels that will be saved.
                        // Channel ordering is handled in stage_input_buffer_index construction.
                        let mut input_data: ChannelVec<_> = self.stage_input_buffer_index[i]
                            .iter()
                            .map(|(si, ci)| &data.row_buffers[*si][*ci])
                            .collect();
                        // Append opaque alpha buffer if fill_opaque_alpha is set
                        if let Some(ref alpha_buf) = self.opaque_alpha_buffers[i] {
                            input_data.push(alpha_buf);
                        }
                        s.save_lowmem(
                            &input_data,
                            &mut *buffers,
                            (xsize, ysize),
                            y,
                            (x0, y0),
                            (xrange.end, yrange.end), // this is not true, but works out correctly.
                            (0, 0),
                        )?;
                    }
                    Stage::Extend(_) => {
                        unreachable!("duplicate extend stage");
                    }
                    Stage::InOut(s) => {
                        assert_eq!(s.border(), (0, 0));
                        let (inb, outb) = data.row_buffers.split_at_mut(i + 1);
                        // Prepare pointers to input and output buffers.
                        let input_data: ChannelVec<_> = self.stage_input_buffer_index[i]
                            .iter()
                            .map(|(si, ci)| &inb[*si][*ci])
                            .collect();
                        s.run_stage_on(
                            ExtraInfo {
                                xsize,
                                current_row: y,
                                group_x0: x0,
                                out_extra_x: 0,
                                start_of_row: false,
                                end_of_row: false,
                                image_height: self.shared.input_size.1,
                            },
                            &input_data,
                            &mut outb[0][..],
                            data.local_states[i].as_deref_mut(),
                        );
                    }
                }
            }
        }
        Ok(())
    }
}
