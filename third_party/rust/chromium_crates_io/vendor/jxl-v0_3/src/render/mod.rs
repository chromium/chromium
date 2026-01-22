// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use internal::{RenderPipelineShared, RunInOutStage, RunInPlaceStage};
use std::any::Any;

use crate::{
    api::JxlOutputBuffer,
    error::Result,
    image::{Image, ImageDataType},
    render::buffer_splitter::BufferSplitter,
};

pub mod buffer_splitter;
mod builder;
mod channels;
mod internal;
mod low_memory_pipeline;
mod save;
mod simd_utils;
#[cfg(test)]
mod simple_pipeline;
pub mod stages;
#[cfg(test)]
mod test;

// Interesting border amounts:
// - 0 (lossless)
// - 1 (420 JPEG recompression)
// - 4 (Gaborish + EPF 1 + EPF 2)
// - 7 (Gaborish + EPF 0 + EPF 1 + EPF 2)
// - 9 (Gaborish + EPF 0/1/2 + upsampling)
// Note: adding 420 does *not* increase this value, because on chroma channels we get
// 9.div_ceil(2)+1 = 6 pixels of border, below the 9 for luma.
const MAX_BORDER: usize = 9;

pub(crate) use builder::RenderPipelineBuilder;
pub(crate) use channels::{Channels, ChannelsMut};
pub(crate) use low_memory_pipeline::LowMemoryRenderPipeline;
#[cfg(test)]
pub(crate) use simple_pipeline::SimpleRenderPipeline;

/// Modifies channels in-place.
pub trait RenderPipelineInPlaceStage: Any + std::fmt::Display {
    type Type: ImageDataType;

    fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        // one for each channel
        row: &mut [&mut [Self::Type]],
        state: Option<&mut dyn Any>,
    );

    fn init_local_state(&self, _thread_index: usize) -> Result<Option<Box<dyn Any>>> {
        Ok(None)
    }

    fn uses_channel(&self, c: usize) -> bool;
}

/// Modifies data and writes it to a new buffer, of possibly different type.
///
/// BORDER.0 and BORDER.1 represent the amount of padding required on the input side.
/// SHIFT.0 and SHIFT.1 represent the base 2 log of the number of rows/columns produced
/// for each row/column of input.
///
/// For each channel:
///  - the input slice contains 1 + BORDER.1 * 2 slices, each of length
///    xsize + BORDER.0 * 2, i.e. covering one input row and up to BORDER pixels of
///    padding on either side.
///  - the output slice contains 1 << SHIFT.1 slices, each of length xsize << SHIFT.0, the
///    corresponding output pixels.
pub trait RenderPipelineInOutStage: Any + std::fmt::Display {
    type InputT: ImageDataType;
    type OutputT: ImageDataType;

    const BORDER: (u8, u8);
    const SHIFT: (u8, u8);

    fn process_row_chunk(
        &self,
        position: (usize, usize),
        xsize: usize,
        // channel, row, column
        input_rows: &Channels<Self::InputT>,
        // channel, row, column
        output_rows: &mut ChannelsMut<Self::OutputT>,
        state: Option<&mut dyn Any>,
    );

    fn init_local_state(&self, _thread_index: usize) -> Result<Option<Box<dyn Any>>> {
        Ok(None)
    }

    fn uses_channel(&self, c: usize) -> bool;
}

// TODO(veluca): find a way to reduce the generated code due to having two builders, to integrate
// SIMD dispatch in the pipeline, and to test consistency across instruction sets in the pipeline.
pub(crate) trait RenderPipeline: Sized {
    type Buffer: 'static;

    fn new_from_shared(shared: RenderPipelineShared<Self::Buffer>) -> Result<Self>;

    /// Obtains a buffer suitable for storing the input in channel `channel`.
    /// This *might* be a buffer that was used to store that channel for that group in a previous
    /// pass, a new buffer, or a re-used buffer from i.e. previously decoded frames.
    fn get_buffer<T: ImageDataType>(&mut self, channel: usize) -> Result<Image<T>>;

    /// Gives back the buffer for a channel and group to the render pipeline, marking that
    /// `num_passes` additional passes (wrt. the previous call to this method for the same channel
    /// and group, or 0 if no previous call happend) were rendered into the input buffer.
    fn set_buffer_for_group<T: ImageDataType>(
        &mut self,
        channel: usize,
        group_id: usize,
        num_passes: usize,
        buf: Image<T>,
        buffer_splitter: &mut BufferSplitter,
    ) -> Result<()>;

    /// Checks whether the provided buffer sizes are correct.
    fn check_buffer_sizes(&self, buffers: &mut [Option<JxlOutputBuffer>]) -> Result<()>;

    /// Renders any data outside the frame that would not be rendered by calls to
    /// set_buffer_for_group. Can be called multiple times - it is up to the pipeline
    /// implementation to ensure rendering only happens once.
    fn render_outside_frame(&mut self, buffer_splitter: &mut BufferSplitter) -> Result<()>;

    fn box_inout_stage<S: RenderPipelineInOutStage>(
        stage: S,
    ) -> Box<dyn RunInOutStage<Self::Buffer>>;

    fn box_inplace_stage<S: RenderPipelineInPlaceStage>(
        stage: S,
    ) -> Box<dyn RunInPlaceStage<Self::Buffer>>;
}
