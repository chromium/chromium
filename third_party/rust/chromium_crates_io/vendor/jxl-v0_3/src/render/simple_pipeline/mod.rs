// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::JxlOutputBuffer,
    error::Result,
    image::{Image, ImageDataType},
    render::{buffer_splitter::BufferSplitter, internal::ChannelInfo},
    util::{ShiftRightCeil, tracing_wrappers::*},
};

use super::{
    RenderPipeline, RenderPipelineInOutStage, RenderPipelineInPlaceStage,
    internal::{RenderPipelineShared, Stage},
};

mod extend;
mod run_stage;
mod save;

/// A RenderPipeline that waits for all input of a pass to be ready before doing any rendering, and
/// prioritizes simplicity over memory usage and computational efficiency.
/// Eventually meant to be used only for verification purposes.
pub struct SimpleRenderPipeline {
    shared: RenderPipelineShared<Image<f64>>,
    input_buffers: Vec<Image<f64>>,
    completed_passes: usize,
}

impl SimpleRenderPipeline {
    #[instrument(skip_all, err)]
    fn do_render(&mut self, buffer_splitter: &mut BufferSplitter) -> Result<()> {
        let ready_passes = self
            .shared
            .group_chan_ready_passes
            .iter()
            .flat_map(|x| x.iter())
            .copied()
            .min()
            .unwrap();
        if ready_passes <= self.completed_passes {
            debug!(
                "no more ready passes ({} completed, {ready_passes} ready)",
                self.completed_passes
            );
            return Ok(());
        }
        debug!(
            "new ready passes ({} completed, {ready_passes} ready)",
            self.completed_passes
        );

        let mut current_buffers = clone_images(&self.input_buffers)?;

        let mut current_size = self.shared.input_size;

        for (i, stage) in self.shared.stages.iter().enumerate() {
            debug!("running stage {i}: {stage}");
            let mut output_buffers = clone_images(&current_buffers)?;
            if stage.shift() != (0, 0) || stage.new_size(current_size) != current_size {
                // Replace buffers of different sizes.
                current_size = stage.new_size(current_size);
                for (c, info) in self.shared.channel_info[i + 1].iter().enumerate() {
                    if stage.uses_channel(c) {
                        let xsize = current_size.0.shrc(info.downsample.0);
                        let ysize = current_size.1.shrc(info.downsample.1);
                        debug!("reallocating channel {c} to new size {xsize}x{ysize}");
                        output_buffers[c] = Image::new((xsize, ysize))?;
                    }
                }
            }
            match stage {
                Stage::InOut(stage) => {
                    let input_buf: Vec<_> = current_buffers
                        .iter()
                        .enumerate()
                        .filter(|x| stage.uses_channel(x.0))
                        .map(|x| x.1)
                        .collect();
                    let mut output_buf = vec![];
                    for (c, buf) in output_buffers.iter_mut().enumerate() {
                        if stage.uses_channel(c) {
                            let mut tmp = Image::new((0, 0)).unwrap();
                            std::mem::swap(&mut tmp, buf);
                            output_buf.push(tmp);
                        }
                    }
                    let mut state = stage.init_local_state(0)?;
                    stage.run_stage_on(
                        self.shared.chunk_size,
                        &input_buf,
                        &mut output_buf,
                        state.as_deref_mut(),
                    );
                    let repl_iter = (0..self.shared.num_channels())
                        .filter(|c| stage.uses_channel(*c))
                        .zip(output_buf.into_iter());
                    for (c, chan) in repl_iter {
                        output_buffers[c] = chan;
                    }
                }
                Stage::InPlace(stage) => {
                    let mut output_buf: Vec<_> = output_buffers
                        .iter_mut()
                        .enumerate()
                        .filter(|x| stage.uses_channel(x.0))
                        .map(|x| x.1)
                        .collect();
                    let mut state = stage.init_local_state(0)?;
                    stage.run_stage_on(
                        self.shared.chunk_size,
                        &mut output_buf,
                        state.as_deref_mut(),
                    );
                }
                Stage::Extend(e) => {
                    e.extend_simple(
                        self.shared.chunk_size,
                        &current_buffers,
                        &mut output_buffers,
                    );
                }
                Stage::Save(stage) => {
                    stage.save_simple(&output_buffers, buffer_splitter.get_full_buffers())?;
                }
            }
            current_buffers = output_buffers;
        }

        self.completed_passes = ready_passes;
        Ok(())
    }
}

fn clone_images<T: ImageDataType>(images: &[Image<T>]) -> Result<Vec<Image<T>>> {
    images.iter().map(|x| x.try_clone()).collect()
}

impl RenderPipeline for SimpleRenderPipeline {
    type Buffer = Image<f64>;

    fn new_from_shared(shared: RenderPipelineShared<Self::Buffer>) -> Result<Self> {
        let input_buffers = shared.channel_info[0]
            .iter()
            .map(|x| {
                let xsize = shared.input_size.0.shrc(x.downsample.0);
                let ysize = shared.input_size.1.shrc(x.downsample.1);
                Image::new((xsize, ysize))
            })
            .collect::<Result<Vec<_>>>()?;

        Ok(Self {
            shared,
            input_buffers,
            completed_passes: 0,
        })
    }

    #[instrument(skip_all, err)]
    fn get_buffer<T: ImageDataType>(&mut self, channel: usize) -> Result<Image<T>> {
        let sz = self.shared.group_size_for_channel(channel, T::DATA_TYPE_ID);
        Image::<T>::new(sz)
    }

    fn set_buffer_for_group<T: ImageDataType>(
        &mut self,
        channel: usize,
        group_id: usize,
        num_passes: usize,
        buf: Image<T>,
        buffer_splitter: &mut BufferSplitter,
    ) -> Result<()> {
        debug!(
            "filling data for group {}, channel {}, using type {:?}",
            group_id,
            channel,
            T::DATA_TYPE_ID,
        );
        let sz = self.shared.group_size_for_channel(channel, T::DATA_TYPE_ID);
        let goffset = self.shared.group_offset(group_id);
        let ChannelInfo { ty, downsample } = self.shared.channel_info[0][channel];
        let off = (goffset.0 >> downsample.0, goffset.1 >> downsample.1);
        debug!(?sz, input_buffers_sz=?self.input_buffers[channel].size(), offset=?off, ?downsample, ?goffset);
        let ty = ty.unwrap();
        assert_eq!(ty, T::DATA_TYPE_ID);
        let total_sz = self.input_buffers[channel].size();
        for y in 0..sz.1.min(total_sz.1 - off.1) {
            let row_in = buf.row(y);
            let row_out = self.input_buffers[channel].row_mut(y + off.1);
            for x in 0..sz.0.min(total_sz.0 - off.0) {
                row_out[x + off.0] = row_in[x].to_f64();
            }
        }
        self.shared.group_chan_ready_passes[group_id][channel] += num_passes;

        self.do_render(buffer_splitter)
    }

    fn check_buffer_sizes(&self, _buffers: &mut [Option<JxlOutputBuffer>]) -> Result<()> {
        // This will be checked during rendering.
        Ok(())
    }

    fn render_outside_frame(&mut self, _buffer_splitter: &mut BufferSplitter) -> Result<()> {
        // Nothing to do in the simple pipeline.
        Ok(())
    }

    fn box_inout_stage<S: RenderPipelineInOutStage>(
        stage: S,
    ) -> Box<dyn super::RunInOutStage<Self::Buffer>> {
        Box::new(stage)
    }

    fn box_inplace_stage<S: RenderPipelineInPlaceStage>(
        stage: S,
    ) -> Box<dyn super::RunInPlaceStage<Self::Buffer>> {
        Box::new(stage)
    }
}
