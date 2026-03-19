// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::needless_range_loop)]

use std::any::Any;

use row_buffers::RowBuffer;

use crate::api::JxlOutputBuffer;
use crate::error::Result;
use crate::image::{DataTypeTag, Image, ImageDataType, OwnedRawImage, Rect};
use crate::render::MAX_BORDER;
use crate::render::buffer_splitter::{BufferSplitter, SaveStageBufferInfo};
use crate::render::internal::Stage;
use crate::render::low_memory_pipeline::group_scheduler::InputBuffer;
use crate::util::{ShiftRightCeil, tracing_wrappers::*};

use super::RenderPipeline;
use super::internal::{RenderPipelineShared, RunInOutStage, RunInPlaceStage};

mod group_scheduler;
mod helpers;
mod render_group;
pub(crate) mod row_buffers;
mod run_stage;
mod save;

pub struct LowMemoryRenderPipeline {
    shared: RenderPipelineShared<RowBuffer>,
    input_buffers: Vec<InputBuffer>,
    row_buffers: Vec<Vec<RowBuffer>>,
    save_buffer_info: Vec<Option<SaveStageBufferInfo>>,
    // The input buffer that each channel of each stage should use.
    // This is indexed both by stage index (0 corresponds to input data, 1 to stage[0], etc) and by
    // index *of those channels that are used*.
    stage_input_buffer_index: Vec<Vec<(usize, usize)>>,
    // Tracks whether we already rendered the padding around the core frame (if any).
    padding_was_rendered: bool,
    // The amount of pixels that each stage needs to *output* around the current group to
    // run future stages correctly.
    stage_output_border_pixels: Vec<(usize, usize)>,
    // The amount of pixels that we need to read (for every channel) in non-edge groups to run all
    // stages correctly.
    input_border_pixels: Vec<(usize, usize)>,
    // Size of the border, in image (i.e. non-downsampled) pixels.
    border_size: (usize, usize),
    // For every stage, the downsampling level of *any* channel that the stage uses at that point.
    // Note that this must be equal across all the used channels.
    downsampling_for_stage: Vec<(usize, usize)>,
    // Local states of each stage, if any.
    local_states: Vec<Option<Box<dyn Any>>>,
    // Pre-filled opaque alpha buffers for stages that need fill_opaque_alpha.
    // Indexed by stage index; None if stage doesn't need alpha fill.
    opaque_alpha_buffers: Vec<Option<RowBuffer>>,
    // Sorted indices to call get_distinct_indices.
    sorted_buffer_indices: Vec<Vec<(usize, usize, usize)>>,
    // For each channel and the 3 kinds of buffers (center / topbottom / leftright), buffers that
    // could be reused to store group data for that channel.
    // Indexed by [3*channel] = center, [3*channel+1] = topbottom, [3*channel+2] = leftright.
    scratch_channel_buffers: Vec<Vec<OwnedRawImage>>,
}

impl RenderPipeline for LowMemoryRenderPipeline {
    type Buffer = RowBuffer;

    fn new_from_shared(shared: RenderPipelineShared<Self::Buffer>) -> Result<Self> {
        let mut input_buffers = vec![];
        let nc = shared.num_channels();
        for _ in 0..shared.group_chan_complete.len() {
            input_buffers.push(InputBuffer::new(nc));
        }
        let mut previous_inout: Vec<_> = (0..nc).map(|x| (0usize, x)).collect();
        let mut stage_input_buffer_index = vec![];
        let mut next_border_and_cur_downsample = vec![vec![]];

        for ci in shared.channel_info[0].iter() {
            next_border_and_cur_downsample[0].push((0, ci.downsample));
        }

        // For each stage, compute in which stage its input was buffered (the previous InOut
        // stage). Also, compute for each InOut stage and channel the border with which the stage
        // output is used; this will used to allocate buffers of the correct size.
        for (i, stage) in shared.stages.iter().enumerate() {
            stage_input_buffer_index.push(previous_inout.clone());
            next_border_and_cur_downsample.push(vec![]);
            if let Stage::InOut(p) = stage {
                for (chan, (ps, pc)) in previous_inout.iter_mut().enumerate() {
                    if !p.uses_channel(chan) {
                        continue;
                    }
                    next_border_and_cur_downsample[*ps][*pc].0 = p.border().1;
                    *ps = i + 1;
                    *pc = next_border_and_cur_downsample[i + 1].len();
                    next_border_and_cur_downsample[i + 1]
                        .push((0, shared.channel_info[i + 1][chan].downsample));
                }
            }
        }

        let mut initial_buffers = vec![];
        for chan in 0..nc {
            initial_buffers.push(RowBuffer::new(
                shared.channel_info[0][chan].ty.unwrap_or(DataTypeTag::U8),
                next_border_and_cur_downsample[0][chan].0 as usize,
                0,
                0,
                shared.chunk_size >> shared.channel_info[0][chan].downsample.0,
            )?);
        }
        let mut row_buffers = vec![initial_buffers];

        // Allocate buffers.
        for (i, stage) in shared.stages.iter().enumerate() {
            let mut stage_buffers = vec![];
            for (next_y_border, (dsx, _)) in next_border_and_cur_downsample[i + 1].iter() {
                stage_buffers.push(RowBuffer::new(
                    stage.output_type().unwrap(),
                    *next_y_border as usize,
                    stage.shift().1 as usize,
                    stage.shift().0 as usize,
                    shared.chunk_size >> *dsx,
                )?);
            }
            row_buffers.push(stage_buffers);
        }
        // Compute information to be used to compute sub-rects for "save" stages to operate on
        // rects.
        let mut save_buffer_info = vec![];
        'stage: for (i, (s, ci)) in shared
            .stages
            .iter()
            .zip(shared.channel_info.iter())
            .enumerate()
        {
            let Stage::Save(s) = s else {
                continue;
            };
            for (c, ci) in ci.iter().enumerate() {
                if s.uses_channel(c) {
                    let info = SaveStageBufferInfo {
                        downsample: ci.downsample,
                        orientation: s.orientation,
                        byte_size: s.data_format.bytes_per_sample() * s.output_channels(),
                        after_extend: shared.extend_stage_index.is_some_and(|e| i > e),
                    };
                    while save_buffer_info.len() <= s.output_buffer_index {
                        save_buffer_info.push(None);
                    }
                    save_buffer_info[s.output_buffer_index] = Some(info);
                    continue 'stage;
                }
            }
        }

        // Compute the amount of border pixels needed per channel, per stage.
        let mut border_pixels = vec![(0usize, 0usize); nc];
        let mut border_pixels_per_stage = vec![];
        for s in shared.stages.iter().rev() {
            let mut stage_max = (0, 0);
            for (c, bp) in border_pixels.iter_mut().enumerate() {
                if !s.uses_channel(c) {
                    continue;
                }
                stage_max.0 = stage_max.0.max(bp.0);
                stage_max.1 = stage_max.1.max(bp.1);

                bp.0 = bp.0.shrc(s.shift().0) + s.border().0 as usize;
                bp.1 = bp.1.shrc(s.shift().1) + s.border().1 as usize;
            }
            border_pixels_per_stage.push(stage_max);
        }
        border_pixels_per_stage.reverse();

        assert!(border_pixels_per_stage[0].0 <= MAX_BORDER);

        let downsampling_for_stage: Vec<_> = shared
            .stages
            .iter()
            .zip(shared.channel_info.iter())
            .map(|(s, ci)| {
                let dowsamplings: Vec<_> = (0..nc)
                    .filter_map(|c| {
                        if s.uses_channel(c) {
                            Some(ci[c].downsample)
                        } else {
                            None
                        }
                    })
                    .collect();
                for &d in dowsamplings.iter() {
                    assert_eq!(d, dowsamplings[0]);
                }
                (dowsamplings[0].0 as usize, dowsamplings[0].1 as usize)
            })
            .collect();

        // Create opaque alpha buffers for save stages that need fill_opaque_alpha
        let mut opaque_alpha_buffers = vec![];
        for (i, stage) in shared.stages.iter().enumerate() {
            if let Stage::Save(s) = stage {
                if s.fill_opaque_alpha {
                    let (dx, _dy) = downsampling_for_stage[i];
                    let row_len = shared.chunk_size >> dx;
                    let fill_pattern = s.data_format.opaque_alpha_bytes();
                    let buf =
                        RowBuffer::new_filled(s.data_format.data_type(), row_len, &fill_pattern)?;
                    opaque_alpha_buffers.push(Some(buf));
                } else {
                    opaque_alpha_buffers.push(None);
                }
            } else {
                opaque_alpha_buffers.push(None);
            }
        }

        let default_channels: Vec<usize> = (0..nc).collect();
        for (s, ibi) in stage_input_buffer_index.iter_mut().enumerate() {
            let mut filtered = vec![];
            // For SaveStage, use s.channels to get correct output ordering (e.g., BGRA).
            let channels = if let Stage::Save(save_stage) = &shared.stages[s] {
                save_stage.channels.as_slice()
            } else {
                default_channels.as_slice()
            };
            for &c in channels {
                if shared.stages[s].uses_channel(c) {
                    filtered.push(ibi[c]);
                }
            }
            *ibi = filtered;
        }

        let sorted_buffer_indices = (0..shared.stages.len())
            .map(|s| {
                let mut v: Vec<_> = stage_input_buffer_index[s]
                    .iter()
                    .enumerate()
                    .map(|(i, (outer, inner))| (*outer, *inner, i))
                    .collect();
                v.sort();
                v
            })
            .collect();

        let mut border_size = (0, 0);
        for c in 0..nc {
            border_size.0 = border_size
                .0
                .max(border_pixels[c].0 << shared.channel_info[0][c].downsample.0);
            border_size.1 = border_size
                .1
                .max(border_pixels[c].1 << shared.channel_info[0][c].downsample.1);
        }
        for s in 0..shared.stages.len() {
            border_size.0 = border_size
                .0
                .max(border_pixels_per_stage[s].0 << downsampling_for_stage[s].0);
            border_size.1 = border_size
                .1
                .max(border_pixels_per_stage[s].1 << downsampling_for_stage[s].1);
        }

        Ok(Self {
            input_buffers,
            stage_input_buffer_index,
            row_buffers,
            padding_was_rendered: false,
            save_buffer_info,
            stage_output_border_pixels: border_pixels_per_stage,
            border_size,
            input_border_pixels: border_pixels,
            local_states: shared
                .stages
                .iter()
                .map(|x| x.init_local_state(0)) // Thread index 0 for single-threaded execution
                .collect::<Result<_>>()?,
            shared,
            downsampling_for_stage,
            opaque_alpha_buffers,
            sorted_buffer_indices,
            scratch_channel_buffers: (0..nc * 3).map(|_| vec![]).collect(),
        })
    }

    #[instrument(skip_all, err)]
    fn get_buffer<T: ImageDataType>(&mut self, channel: usize) -> Result<Image<T>> {
        if let Some(b) = self.maybe_get_scratch_buffer(channel, 0) {
            return Ok(Image::from_raw(b));
        }
        let sz = self.shared.group_size_for_channel(channel, T::DATA_TYPE_ID);
        Image::<T>::new(sz)
    }

    fn set_buffer_for_group<T: ImageDataType>(
        &mut self,
        channel: usize,
        group_id: usize,
        complete: bool,
        buf: Image<T>,
        buffer_splitter: &mut BufferSplitter,
    ) -> Result<()> {
        if self.shared.channel_is_used[channel] {
            debug!(
                "filling data for group {}, channel {}, using type {:?}",
                group_id,
                channel,
                T::DATA_TYPE_ID,
            );
            self.input_buffers[group_id].set_buffer(channel, buf.into_raw());
            self.shared.group_chan_complete[group_id][channel] = complete;

            self.render_with_new_group(group_id, buffer_splitter)?;
        }
        Ok(())
    }

    fn check_buffer_sizes(&self, buffers: &mut [Option<JxlOutputBuffer>]) -> Result<()> {
        // Check that buffer sizes are correct.
        let mut size = self.shared.input_size;
        for (i, s) in self.shared.stages.iter().enumerate() {
            match s {
                Stage::Extend(e) => size = e.image_size,
                Stage::Save(s) => {
                    let (dx, dy) = self.downsampling_for_stage[i];
                    s.check_buffer_size(
                        (size.0 >> dx, size.1 >> dy),
                        buffers[s.output_buffer_index].as_ref(),
                    )?
                }
                _ => {}
            }
        }
        Ok(())
    }

    fn render_outside_frame(&mut self, buffer_splitter: &mut BufferSplitter) -> Result<()> {
        if self.shared.extend_stage_index.is_none() || self.padding_was_rendered {
            return Ok(());
        }
        self.padding_was_rendered = true;
        // TODO(veluca): consider pre-computing those strips at pipeline construction and making
        // smaller strips.
        let e = self.shared.extend_stage_index.unwrap();
        let Stage::Extend(e) = &self.shared.stages[e] else {
            unreachable!("extend stage is not an extend stage");
        };
        let frame_end = (
            e.frame_origin.0 + self.shared.input_size.0 as isize,
            e.frame_origin.1 + self.shared.input_size.1 as isize,
        );
        // Split the full image area in 4 strips: left and right of the frame, and above and below.
        // We divide each part further in strips of width self.shared.chunk_size.
        let mut strips = vec![];
        // Above (including left and right)
        if e.frame_origin.1 > 0 {
            let xend = e.image_size.0;
            let yend = (e.frame_origin.1 as usize).min(e.image_size.1);
            for x in (0..xend).step_by(self.shared.chunk_size) {
                let xe = (x + self.shared.chunk_size).min(xend);
                strips.push((x..xe, 0..yend));
            }
        }
        // Below
        if frame_end.1 < e.image_size.1 as isize {
            let ystart = frame_end.1.max(0) as usize;
            let yend = e.image_size.1;
            let xend = e.image_size.0;
            for x in (0..xend).step_by(self.shared.chunk_size) {
                let xe = (x + self.shared.chunk_size).min(xend);
                strips.push((x..xe, ystart..yend));
            }
        }
        // Left
        if e.frame_origin.0 > 0 {
            let ystart = e.frame_origin.1.max(0) as usize;
            let yend = (frame_end.1 as usize).min(e.image_size.1);
            let xend = (e.frame_origin.0 as usize).min(e.image_size.0);
            for x in (0..xend).step_by(self.shared.chunk_size) {
                let xe = (x + self.shared.chunk_size).min(xend);
                strips.push((x..xe, ystart..yend));
            }
        }
        // Right
        if frame_end.0 < e.image_size.0 as isize {
            let xstart = frame_end.0.max(0) as usize;
            let xend = e.image_size.0;
            let ystart = e.frame_origin.1.max(0) as usize;
            let yend = (frame_end.1 as usize).min(e.image_size.1);
            for x in (xstart..xend).step_by(self.shared.chunk_size) {
                let xe = (x + self.shared.chunk_size).min(xend);
                strips.push((x..xe, ystart..yend));
            }
        }
        let full_image_size = e.image_size;
        for (xrange, yrange) in strips {
            let rect_to_render = Rect {
                origin: (xrange.start, yrange.start),
                size: (xrange.clone().count(), yrange.clone().count()),
            };
            if rect_to_render.size.0 == 0 || rect_to_render.size.1 == 0 {
                continue;
            }
            let mut local_buffers = buffer_splitter.get_local_buffers(
                &self.save_buffer_info,
                rect_to_render,
                true,
                full_image_size,
                full_image_size,
                (0, 0),
            );
            self.render_outside_frame(xrange, yrange, &mut local_buffers)?;
        }
        Ok(())
    }

    fn mark_group_to_rerender(&mut self, g: usize) {
        self.input_buffers[g].is_ready = false;
    }

    fn box_inout_stage<S: super::RenderPipelineInOutStage>(
        stage: S,
    ) -> Box<dyn RunInOutStage<Self::Buffer>> {
        Box::new(stage)
    }

    fn box_inplace_stage<S: super::RenderPipelineInPlaceStage>(
        stage: S,
    ) -> Box<dyn RunInPlaceStage<Self::Buffer>> {
        Box::new(stage)
    }

    fn used_channel_mask(&self) -> &[bool] {
        &self.shared.channel_is_used
    }
}
