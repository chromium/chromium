// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::any::Any;
use std::fmt::Display;
use std::sync::Arc;

use super::save::SaveStage;
use super::stages::ExtendToImageDimensionsStage;
use super::{RenderPipelineInOutStage, RenderPipelineInPlaceStage};
use crate::error::Result;
use crate::image::{BufferRecycler, DataTypeTag, ImageDataType};
use crate::render::{ErasedLocalState, StageSpecialCase};
use crate::util::ShiftRightCeil;
use crate::util::sync::atomic::AtomicBool;

pub enum Stage<Buffer> {
    InPlace(Box<dyn RunInPlaceStage<Buffer>>),
    InOut(Box<dyn RunInOutStage<Buffer>>),
    Save(SaveStage),
    Extend(ExtendToImageDimensionsStage),
}

impl<Buffer: 'static> Stage<Buffer> {
    pub(super) fn init_local_state(&self) -> Result<Option<Box<ErasedLocalState>>> {
        match self {
            Stage::InPlace(s) => s.init_local_state(),
            Stage::InOut(s) => s.init_local_state(),
            _ => Ok(None),
        }
    }

    pub(super) fn shift(&self) -> (u8, u8) {
        match self {
            Stage::InOut(s) => s.shift(),
            _ => (0, 0),
        }
    }

    pub(super) fn border(&self) -> (u8, u8) {
        match self {
            Stage::InOut(s) => s.border(),
            _ => (0, 0),
        }
    }

    #[cfg(test)]
    pub(super) fn new_size(&self, size: (usize, usize)) -> (usize, usize) {
        match self {
            Stage::Extend(e) => e.image_size,
            _ => size,
        }
    }

    pub(super) fn uses_channel(&self, c: usize) -> bool {
        match self {
            Stage::Extend(_) => true,
            Stage::InPlace(s) => s.uses_channel(c),
            Stage::InOut(s) => s.uses_channel(c),
            Stage::Save(s) => s.uses_channel(c),
        }
    }
    pub(super) fn input_type(&self) -> DataTypeTag {
        match self {
            Stage::Extend(_) => DataTypeTag::F32,
            Stage::InPlace(s) => s.ty(),
            Stage::InOut(s) => s.input_type(),
            Stage::Save(s) => s.input_type(),
        }
    }
    pub(super) fn output_type(&self) -> Option<DataTypeTag> {
        match self {
            Stage::InOut(s) => Some(s.output_type()),
            _ => None,
        }
    }
    pub(super) fn is_special_case(&self) -> Option<StageSpecialCase> {
        match self {
            Stage::InOut(s) => s.is_special_case(),
            Stage::InPlace(s) => s.is_special_case(),
            _ => None,
        }
    }
}

impl<Buffer> Display for Stage<Buffer> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Stage::InOut(s) => write!(f, "{}", s),
            Stage::InPlace(s) => write!(f, "{}", s),
            Stage::Save(s) => write!(f, "{}", s),
            Stage::Extend(e) => write!(f, "{}", e),
        }
    }
}

#[derive(Clone, Debug)]
pub struct ChannelInfo {
    pub ty: Option<DataTypeTag>,
    pub downsample: (u8, u8),
}

pub struct RenderPipelineShared<Buffer> {
    pub channel_info: Vec<Vec<ChannelInfo>>,
    pub input_size: (usize, usize),
    pub log_group_size: usize,
    pub group_count: (usize, usize),
    pub group_chan_complete: Vec<Vec<AtomicBool>>,
    pub chunk_size: usize,
    pub stages: Vec<Stage<Buffer>>,
    pub extend_stage_index: Option<usize>,
    pub channel_is_used: Vec<bool>,
    pub buffer_recycler: Arc<BufferRecycler>,
}

impl<Buffer> RenderPipelineShared<Buffer> {
    pub fn group_position(&self, group_id: usize) -> (usize, usize) {
        (group_id % self.group_count.0, group_id / self.group_count.0)
    }

    pub fn group_offset(&self, group_id: usize) -> (usize, usize) {
        let group = self.group_position(group_id);
        (
            group.0 << self.log_group_size,
            group.1 << self.log_group_size,
        )
    }

    pub fn group_size(&self, group_id: usize) -> (usize, usize) {
        let goffset = self.group_offset(group_id);
        (
            self.input_size
                .0
                .min(goffset.0 + (1 << self.log_group_size))
                - goffset.0,
            self.input_size
                .1
                .min(goffset.1 + (1 << self.log_group_size))
                - goffset.1,
        )
    }

    pub fn group_size_for_channel(
        &self,
        channel: usize,
        requested_data_type: DataTypeTag,
    ) -> (usize, usize) {
        let ChannelInfo { downsample, ty } = self.channel_info[0][channel];
        // Channels that no stage consumes have no type at all. Callers may still ask for a
        // scratch buffer for them (e.g. VarDCT always decodes three colour channels, but a
        // grayscale pipeline only ever reads the first one); the data written there is
        // discarded, so any type is acceptable.
        if let Some(ty) = ty
            && ty != requested_data_type
        {
            panic!(
                "Invalid pipeline usage: incorrect channel type, requested {requested_data_type:?}, but pipeline wants {ty:?}"
            );
        }
        // 420 JPEGs are padded to 16 pixels, not to 8.
        (
            (1 << self.log_group_size)
                .min(self.input_size.0)
                .shrc(downsample.0 as usize)
                .next_multiple_of(16),
            (1 << self.log_group_size)
                .min(self.input_size.1)
                .shrc(downsample.1 as usize)
                .next_multiple_of(16),
        )
    }

    pub fn num_channels(&self) -> usize {
        self.channel_is_used.len()
    }

    pub fn num_used_channels(&self) -> usize {
        self.channel_is_used.iter().filter(|x| **x).count()
    }
}

pub trait PipelineBuffer {
    type InPlaceExtraInfo;
    type InOutExtraInfo;
}

pub trait InPlaceStage: Any + Display + Send + Sync {
    fn init_local_state(&self) -> Result<Option<Box<ErasedLocalState>>>;
    fn uses_channel(&self, c: usize) -> bool;
    fn ty(&self) -> DataTypeTag;
    fn is_special_case(&self) -> Option<StageSpecialCase>;
}

pub trait RunInPlaceStage<Buffer: PipelineBuffer>: InPlaceStage {
    fn run_stage_on(
        &self,
        info: Buffer::InPlaceExtraInfo,
        buffers: &mut [&mut Buffer],
        state: Option<&mut ErasedLocalState>,
    );
}

impl<T: RenderPipelineInPlaceStage> InPlaceStage for T {
    fn init_local_state(&self) -> Result<Option<Box<ErasedLocalState>>> {
        self.init_local_state()
    }
    fn uses_channel(&self, c: usize) -> bool {
        self.uses_channel(c)
    }
    fn ty(&self) -> DataTypeTag {
        T::Type::DATA_TYPE_ID
    }
    fn is_special_case(&self) -> Option<StageSpecialCase> {
        self.is_special_case()
    }
}

pub trait InOutStage: Any + Display + Send + Sync {
    fn init_local_state(&self) -> Result<Option<Box<ErasedLocalState>>>;
    fn shift(&self) -> (u8, u8);
    fn border(&self) -> (u8, u8);
    fn uses_channel(&self, c: usize) -> bool;
    fn input_type(&self) -> DataTypeTag;
    fn output_type(&self) -> DataTypeTag;
    fn is_special_case(&self) -> Option<StageSpecialCase>;
}

impl<T: RenderPipelineInOutStage> InOutStage for T {
    fn init_local_state(&self) -> Result<Option<Box<ErasedLocalState>>> {
        self.init_local_state()
    }
    fn uses_channel(&self, c: usize) -> bool {
        self.uses_channel(c)
    }
    fn shift(&self) -> (u8, u8) {
        T::SHIFT
    }
    fn border(&self) -> (u8, u8) {
        T::BORDER
    }
    fn input_type(&self) -> DataTypeTag {
        T::InputT::DATA_TYPE_ID
    }
    fn output_type(&self) -> DataTypeTag {
        T::OutputT::DATA_TYPE_ID
    }
    fn is_special_case(&self) -> Option<StageSpecialCase> {
        self.is_special_case()
    }
}

pub trait RunInOutStage<Buffer: PipelineBuffer>: InOutStage {
    fn run_stage_on(
        &self,
        info: Buffer::InOutExtraInfo,
        input_buffers: &[&Buffer],
        output_buffers: &mut [Buffer],
        state: Option<&mut ErasedLocalState>,
    );
}
