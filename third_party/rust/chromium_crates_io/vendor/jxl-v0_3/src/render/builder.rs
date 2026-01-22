// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::{JxlColorType, JxlDataFormat};
use crate::error::{Error, Result};
use crate::headers::Orientation;
use crate::render::internal::ChannelInfo;
use crate::render::save::SaveStage;
use crate::util::{ShiftRightCeil, tracing_wrappers::*};

use super::internal::{RenderPipelineShared, Stage};
use super::stages::ExtendToImageDimensionsStage;
use super::{RenderPipeline, RenderPipelineInOutStage, RenderPipelineInPlaceStage};

pub(crate) struct RenderPipelineBuilder<Pipeline: RenderPipeline> {
    shared: RenderPipelineShared<Pipeline::Buffer>,
}

impl<Pipeline: RenderPipeline> RenderPipelineBuilder<Pipeline> {
    #[instrument(level = "debug")]
    pub(super) fn new_with_chunk_size(
        num_channels: usize,
        size: (usize, usize),
        downsampling_shift: usize,
        mut log_group_size: usize,
        num_passes: usize,
        chunk_size: usize,
    ) -> Self {
        info!("creating render pipeline");
        assert!(chunk_size <= u16::MAX as usize);
        assert_ne!(chunk_size, 0);
        // The number of pixels that a group encompasses in the final, upsampled image along one
        // dimension is effectively multiplied by the upsampling factor.
        log_group_size += downsampling_shift;
        Self {
            shared: RenderPipelineShared {
                channel_info: vec![vec![
                    ChannelInfo {
                        ty: None,
                        downsample: (0, 0)
                    };
                    num_channels
                ]],
                input_size: size,
                log_group_size,
                group_count: (size.0.shrc(log_group_size), size.1.shrc(log_group_size)),
                stages: vec![],
                group_chan_ready_passes: vec![
                    vec![0; num_channels];
                    size.0.shrc(log_group_size)
                        * size.1.shrc(log_group_size)
                ],
                num_passes,
                chunk_size,
                extend_stage_index: None,
            },
        }
    }

    pub(super) fn add_stage_internal(mut self, stage: Stage<Pipeline::Buffer>) -> Result<Self> {
        let input_type = stage.input_type();
        let output_type = stage.output_type();
        let shift = stage.shift();
        let border = stage.border();
        let is_extend = matches!(stage, Stage::Extend(_));
        let current_info = self.shared.channel_info.last().unwrap().clone();
        debug!(
            last_stage_channel_info = ?current_info,
            extend_stage_index= ?self.shared.extend_stage_index,
            "adding stage '{stage}'",
        );
        let mut after_info = vec![];
        for (c, info) in current_info.iter().enumerate() {
            if !stage.uses_channel(c) {
                after_info.push(ChannelInfo {
                    ty: info.ty,
                    downsample: (0, 0),
                });
            } else {
                if let Some(ty) = info.ty
                    && ty != input_type
                {
                    return Err(Error::PipelineChannelTypeMismatch(
                        stage.to_string(),
                        c,
                        input_type,
                        ty,
                    ));
                }
                after_info.push(ChannelInfo {
                    ty: Some(output_type.unwrap_or(input_type)),
                    downsample: shift,
                });
            }
        }
        if self.shared.extend_stage_index.is_some()
            && (shift != (0, 0) || border != (0, 0) || is_extend)
        {
            return Err(Error::PipelineInvalidStageAfterExtend(stage.to_string()));
        }
        if is_extend {
            self.shared.extend_stage_index = Some(self.shared.stages.len());
        }
        debug!(
            new_channel_info = ?after_info,
            extend_stage_index= ?self.shared.extend_stage_index,
            "added stage '{stage}'",
        );
        self.shared.channel_info.push(after_info);
        self.shared.stages.push(stage);
        Ok(self)
    }

    pub fn new(
        num_channels: usize,
        size: (usize, usize),
        downsampling_shift: usize,
        log_group_size: usize,
        num_passes: usize,
    ) -> Self {
        Self::new_with_chunk_size(
            num_channels,
            size,
            downsampling_shift,
            log_group_size,
            num_passes,
            1 << (log_group_size + downsampling_shift),
        )
    }

    #[instrument(skip_all, err)]
    pub fn add_save_stage(
        self,
        channels: &[usize],
        orientation: Orientation,
        output_buffer_index: usize,
        color_type: JxlColorType,
        data_format: JxlDataFormat,
        fill_opaque_alpha: bool,
    ) -> Result<Self> {
        let stage = SaveStage::new(
            channels,
            orientation,
            output_buffer_index,
            color_type,
            data_format,
            fill_opaque_alpha,
        );
        self.add_stage_internal(Stage::Save(stage))
    }

    #[instrument(skip_all, err)]
    pub fn add_extend_stage(self, extend: ExtendToImageDimensionsStage) -> Result<Self> {
        self.add_stage_internal(Stage::Extend(extend))
    }

    #[instrument(skip_all, err)]
    pub fn add_inplace_stage<S: RenderPipelineInPlaceStage>(self, stage: S) -> Result<Self> {
        self.add_stage_internal(Stage::InPlace(Pipeline::box_inplace_stage(stage)))
    }

    #[instrument(skip_all, err)]
    pub fn add_inout_stage<S: RenderPipelineInOutStage>(self, stage: S) -> Result<Self> {
        self.add_stage_internal(Stage::InOut(Pipeline::box_inout_stage(stage)))
    }

    #[instrument(skip_all, err)]
    pub fn build(mut self) -> Result<Box<Pipeline>> {
        let channel_info = &mut self.shared.channel_info;
        let num_channels = channel_info[0].len();
        let mut cur_downsamples = vec![(0u8, 0u8); num_channels];
        for (s, stage) in self.shared.stages.iter().enumerate().rev() {
            let [current_info, next_info, ..] = &mut channel_info[s..] else {
                unreachable!()
            };
            let mut save_downsample = None;
            for chan in 0..num_channels {
                let cur_chan = &mut current_info[chan];
                let next_chan = &mut next_info[chan];
                let uses_channel = stage.uses_channel(chan);
                let input_type = stage.input_type();

                if cur_chan.ty.is_none() {
                    cur_chan.ty = if uses_channel {
                        Some(input_type)
                    } else {
                        next_chan.ty
                    }
                }
                // Arithmetic overflows here should be very uncommon, so custom error variants
                // are probably unwarranted.
                let cur_downsample = &mut cur_downsamples[chan];
                if matches!(stage, Stage::Save(_))
                    && save_downsample.is_some_and(|x| x != *cur_downsample)
                {
                    save_downsample = Some(*cur_downsample);
                    return Err(Error::SaveDifferentDownsample(
                        save_downsample.unwrap(),
                        *cur_downsample,
                    ));
                }
                let next_downsample = &mut next_chan.downsample;
                let next_total_downsample = *cur_downsample;
                cur_downsample.0 = cur_downsample
                    .0
                    .checked_add(next_downsample.0)
                    .ok_or(Error::ArithmeticOverflow)?;
                cur_downsample.1 = cur_downsample
                    .1
                    .checked_add(next_downsample.1)
                    .ok_or(Error::ArithmeticOverflow)?;
                *next_downsample = next_total_downsample;
            }
        }
        for (chan, cur_downsample) in cur_downsamples.iter().enumerate() {
            channel_info[0][chan].downsample = *cur_downsample;
        }
        #[cfg(feature = "tracing")]
        {
            for (s, (current_info, stage)) in channel_info
                .iter()
                .zip(self.shared.stages.iter())
                .enumerate()
            {
                debug!("final channel info before stage {s} '{stage}': {current_info:?}");
            }
            debug!(
                "final channel info after all stages {:?}",
                channel_info.last().unwrap()
            );
        }

        // Ensure all channels have been used, so that we know the types of all buffers at all
        // stages.
        for (c, chinfo) in channel_info.iter().flat_map(|x| x.iter().enumerate()) {
            if chinfo.ty.is_none() {
                return Err(Error::PipelineChannelUnused(c));
            }
        }

        Ok(Box::new(Pipeline::new_from_shared(self.shared)?))
    }
}
