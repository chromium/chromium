// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::any::Any;

use crate::{
    render::{
        Channels, ChannelsMut, RunInPlaceStage,
        internal::{PipelineBuffer, RunInOutStage},
        low_memory_pipeline::{helpers::mirror, render_group::ChannelVec},
    },
    util::{ShiftRightCeil, SmallVec, tracing_wrappers::*},
};

use super::{
    super::{RenderPipelineInOutStage, RenderPipelineInPlaceStage},
    row_buffers::RowBuffer,
};

pub struct ExtraInfo {
    // Number of *input* pixels to process (ignoring additional border pixels).
    pub(super) xsize: usize,
    // Additional border pixels requested in the output on each side, if not first/last xgroup.
    pub(super) out_extra_x: usize,
    pub(super) current_row: usize,
    pub(super) group_x0: usize,
    pub(super) is_first_xgroup: bool,
    pub(super) is_last_xgroup: bool,
    pub(super) image_height: usize,
}

impl PipelineBuffer for RowBuffer {
    type InPlaceExtraInfo = ExtraInfo;
    type InOutExtraInfo = ExtraInfo;
}

impl<T: RenderPipelineInPlaceStage> RunInPlaceStage<RowBuffer> for T {
    #[instrument(skip_all)]
    fn run_stage_on(
        &self,
        ExtraInfo {
            xsize,
            current_row,
            group_x0,
            out_extra_x,
            image_height: _,
            is_first_xgroup,
            is_last_xgroup,
        }: ExtraInfo,
        buffers: &mut [&mut RowBuffer],
        state: Option<&mut dyn Any>,
    ) {
        let x0 = RowBuffer::x0_offset::<T::Type>();
        let xpre = if is_first_xgroup { 0 } else { out_extra_x };
        let xstart = x0 - xpre;
        let xend = x0 + xsize + if is_last_xgroup { 0 } else { out_extra_x };
        let mut rows: ChannelVec<_> = buffers
            .iter_mut()
            .map(|x| &mut x.get_row_mut::<T::Type>(current_row)[xstart..])
            .collect();

        self.process_row_chunk(
            (group_x0 - xpre, current_row),
            xend - xstart,
            &mut rows[..],
            state,
        );
    }
}

impl<T: RenderPipelineInOutStage> RunInOutStage<RowBuffer> for T {
    #[instrument(skip_all)]
    fn run_stage_on(
        &self,
        ExtraInfo {
            xsize,
            current_row,
            group_x0,
            out_extra_x,
            image_height,
            is_first_xgroup,
            is_last_xgroup,
        }: ExtraInfo,
        input_buffers: &[&RowBuffer],
        output_buffers: &mut [RowBuffer],
        state: Option<&mut dyn Any>,
    ) {
        let ibordery = Self::BORDER.1 as isize;
        let x0 = RowBuffer::x0_offset::<T::InputT>();
        let xpre = if is_first_xgroup {
            0
        } else {
            out_extra_x.shrc(T::SHIFT.0)
        };
        let xstart = x0 - xpre;
        let xend = x0
            + xsize
            + if is_last_xgroup {
                0
            } else {
                out_extra_x.shrc(T::SHIFT.0)
            };

        // Build flat input rows: all rows for all channels in one Vec
        let input_rows_per_channel = (2 * Self::BORDER.1 + 1) as usize;
        let num_channels = input_buffers.len();
        let mut input_row_data = SmallVec::new();
        for x in input_buffers.iter() {
            for iy in -ibordery..=ibordery {
                input_row_data.push(
                    &x.get_row::<T::InputT>(mirror(current_row as isize + iy, image_height))
                        [xstart - Self::BORDER.0 as usize..],
                );
            }
        }
        let input_rows = Channels::new(input_row_data, num_channels, input_rows_per_channel);

        // Build flat output rows: all rows for all channels in one Vec
        let output_rows_per_channel = 1 << T::SHIFT.1;
        let num_output_channels = output_buffers.len();
        let mut output_row_data = SmallVec::new();
        // optimize for the common case of a single output row per channel.
        if output_rows_per_channel == 1 {
            // Use OutputT's x0_offset, not InputT's - they differ for type conversions (e.g., f32→u8).
            // Must apply the same offset calculation as the else branch.
            let output_xstart = RowBuffer::x0_offset::<T::OutputT>() - (xpre << T::SHIFT.0);
            for x in output_buffers.iter_mut() {
                let row = x.get_row_mut::<T::OutputT>(current_row);
                output_row_data.push(&mut row[output_xstart..]);
            }
        } else {
            for x in output_buffers.iter_mut() {
                let rows = x.get_rows_mut::<T::OutputT>(
                    (current_row << T::SHIFT.1)..((current_row + 1) << T::SHIFT.1),
                    RowBuffer::x0_offset::<T::OutputT>() - (xpre << T::SHIFT.0),
                );
                output_row_data.extend_sv(rows);
            }
        }
        let mut output_rows = ChannelsMut::new(
            output_row_data,
            num_output_channels,
            output_rows_per_channel,
        );

        self.process_row_chunk(
            (group_x0 - xpre, current_row),
            xend - xstart,
            &input_rows,
            &mut output_rows,
            state,
        );
    }
}
