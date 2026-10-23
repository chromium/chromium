// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::super::{RenderPipelineInOutStage, RenderPipelineInPlaceStage};
use super::row_buffers::RowBuffer;
use crate::render::internal::{PipelineBuffer, RunInOutStage};
use crate::render::{Channels, ChannelsMut, ErasedLocalState, RunInPlaceStage};
use crate::util::tracing_wrappers::*;
use crate::util::{ChannelVec, ShiftRightCeil};

pub struct ExtraInfo {
    // Number of *input* pixels to process (ignoring additional border pixels).
    pub(super) xsize: usize,
    // Additional border pixels requested in the output on each side, if not first/last xgroup.
    pub(super) out_extra_x: usize,
    pub(super) current_row: usize,
    pub(super) group_x0: usize,
    pub(super) start_of_row: bool,
    pub(super) end_of_row: bool,
    pub(super) image_height: usize,
    pub(super) previous_call_was_previous_row: bool,
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
            start_of_row,
            end_of_row,
            previous_call_was_previous_row,
        }: ExtraInfo,
        buffers: &mut [&mut RowBuffer],
        state: Option<&mut ErasedLocalState>,
    ) {
        let x0 = RowBuffer::x0_offset::<T::Type>();
        let xpre = if start_of_row { 0 } else { out_extra_x };
        let xstart = x0 - xpre;
        let xend = x0 + xsize + if end_of_row { 0 } else { out_extra_x };
        let mut rows: ChannelVec<_> = buffers
            .iter_mut()
            .map(|x| &mut x.get_row_mut::<T::Type>(current_row)[xstart..])
            .collect();

        self.process_row_chunk(
            (group_x0 - xpre, current_row),
            xend - xstart,
            &mut rows[..],
            state,
            previous_call_was_previous_row,
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
            start_of_row,
            end_of_row,
            previous_call_was_previous_row,
        }: ExtraInfo,
        input_buffers: &[&RowBuffer],
        output_buffers: &mut [RowBuffer],
        state: Option<&mut ErasedLocalState>,
    ) {
        let x0 = RowBuffer::x0_offset::<T::InputT>();
        let xpre = if start_of_row {
            0
        } else {
            out_extra_x.shrc(T::SHIFT.0)
        };
        let xstart = x0 - xpre;
        let xend = x0
            + xsize
            + if end_of_row {
                0
            } else {
                out_extra_x.shrc(T::SHIFT.0)
            };

        let input_rows = Channels::from_row_buffers(
            input_buffers,
            xstart,
            current_row,
            image_height,
            Self::BORDER.0 as usize,
        );

        let output_xstart = RowBuffer::x0_offset::<T::OutputT>() - (xpre << T::SHIFT.0);
        let mut output_rows = ChannelsMut::from_row_buffers(
            output_buffers,
            output_xstart,
            current_row << T::SHIFT.1,
            1 << T::SHIFT.1,
        );

        self.process_row_chunk(
            (group_x0 - xpre, current_row),
            xend - xstart,
            &input_rows,
            &mut output_rows,
            state,
            previous_call_was_previous_row,
        );
    }
}
