// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    ops::DerefMut,
    sync::atomic::{AtomicUsize, Ordering},
};

use crate::{
    error::Result,
    frame::{
        DataStatus,
        modular::{ChannelInfo, IMAGE_OFFSET, IMAGE_PADDING},
    },
    headers::bit_depth::BitDepth,
    image::Image,
    util::{AtomicRefCell, AtomicRefMut},
};

use super::ModularBufferInfo;

// All the information on a specific buffer needed by Modular decoding.
#[derive(Debug)]
pub(super) struct ModularChannel {
    // Actual pixel buffer.
    pub(super) data: Image<i32>,
    // Holds additional information such as the weighted predictor's error channel's last row for
    // the transform chunk that produced this buffer.
    pub(super) auxiliary_data: Option<Image<i32>>,
    // Shift of the channel (None if this is a meta-channel).
    pub(super) shift: Option<(usize, usize)>,
    pub(super) bit_depth: BitDepth,
}

impl ModularChannel {
    pub fn new(size: (usize, usize), bit_depth: BitDepth) -> Result<Self> {
        Self::new_with_shift(size, Some((0, 0)), bit_depth)
    }

    pub fn new_with_shift(
        size: (usize, usize),
        shift: Option<(usize, usize)>,
        bit_depth: BitDepth,
    ) -> Result<Self> {
        Ok(ModularChannel {
            data: Image::new_with_padding(size, IMAGE_OFFSET, IMAGE_PADDING)?,
            auxiliary_data: None,
            shift,
            bit_depth,
        })
    }

    fn try_clone(&self) -> Result<Self> {
        Ok(ModularChannel {
            data: self.data.try_clone()?,
            auxiliary_data: self
                .auxiliary_data
                .as_ref()
                .map(Image::try_clone)
                .transpose()?,
            shift: self.shift,
            bit_depth: self.bit_depth,
        })
    }

    pub fn channel_info(&self) -> ChannelInfo {
        ChannelInfo {
            output_channel_idx: None,
            size: self.data.size(),
            shift: self.shift,
            bit_depth: self.bit_depth,
        }
    }
}

#[derive(Debug)]
pub(super) struct ModularBuffer {
    pub(super) data: AtomicRefCell<Option<ModularChannel>>,
    // Number of times this buffer will be used, *including* when it is used for output.
    pub(super) remaining_uses: AtomicUsize,
    // Transform steps that use the image data in this buffer for final renders.
    pub(super) used_by_transforms_final: Vec<usize>,
    // Transform steps that depend on this buffer for the current rendering pass.
    pub(super) used_by_transforms_current: Vec<usize>,
    // Transform step that will produce this channel (None if the channel is final).
    pub(super) produced_by_step: Option<usize>,
    pub(super) size: (usize, usize),
    // Status of the data in this buffer. Note that the distinction between "Zero"
    // and "partial" is only meaningful for section0 coded buffers.
    pub(super) data_status: DataStatus,
}

const DISABLE_MODULAR_BUFFER_DEALLOCATION_FOR_DEBUG: bool = false;

impl ModularBuffer {
    pub fn new(size: (usize, usize)) -> Self {
        ModularBuffer {
            data: AtomicRefCell::new(None),
            remaining_uses: AtomicUsize::new(0),
            used_by_transforms_final: vec![],
            used_by_transforms_current: vec![],
            size,
            data_status: DataStatus::Zero,
            produced_by_step: None,
        }
    }

    pub fn has_buffer(&self) -> bool {
        self.data.borrow().is_some()
    }

    pub fn make_buffer(&self, info: &ChannelInfo) -> Result<ModularChannel> {
        Ok(ModularChannel {
            data: Image::new_with_padding(self.size, IMAGE_OFFSET, IMAGE_PADDING)?,
            auxiliary_data: None,
            shift: info.shift,
            bit_depth: info.bit_depth,
        })
    }

    pub fn ensure_buffer(&self, info: &ChannelInfo) -> Result<()> {
        if !self.has_buffer() {
            let buf = self.make_buffer(info)?;
            *self.data.borrow_mut() = Some(buf);
        }
        Ok(())
    }

    // Gives out a copy of the buffer + auxiliary buffer, marking the buffer as used.
    // If this was the last usage of the buffer, does not actually copy the buffer.
    pub fn get_buffer(&self, can_consume: bool) -> Result<ModularChannel> {
        if !can_consume || DISABLE_MODULAR_BUFFER_DEALLOCATION_FOR_DEBUG {
            return ModularChannel::try_clone(self.data.borrow().as_ref().unwrap());
        }
        let mut ret = None;
        let _ = self.remaining_uses.fetch_update(
            Ordering::Release,
            Ordering::Acquire,
            |remaining_pre| {
                let remaining = remaining_pre.checked_sub(1).unwrap();
                if ret.is_none() {
                    if remaining == 0 {
                        ret = Some(Ok(self.data.borrow_mut().take().unwrap()))
                    } else {
                        ret = self.data.borrow().as_ref().map(ModularChannel::try_clone);
                    }
                } else if remaining == 0 {
                    *self.data.borrow_mut() = None;
                }
                Some(remaining)
            },
        );
        Ok(ret.transpose()?.unwrap())
    }

    pub fn mark_used(&self, can_consume: bool) {
        if !can_consume || DISABLE_MODULAR_BUFFER_DEALLOCATION_FOR_DEBUG {
            return;
        }
        let _ = self.remaining_uses.fetch_update(
            Ordering::Release,
            Ordering::Acquire,
            |remaining_pre: usize| {
                let remaining = remaining_pre.checked_sub(1).unwrap();
                if remaining == 0 {
                    *self.data.borrow_mut() = None;
                }
                Some(remaining)
            },
        );
    }
}

pub fn with_buffers<T>(
    buffers: &[ModularBufferInfo],
    indices: &[usize],
    grid: usize,
    f: impl FnOnce(Vec<&mut ModularChannel>) -> Result<T>,
) -> Result<T> {
    let mut bufs = vec![];
    for i in indices {
        // Allocate buffers if they are not present.
        let buf = &buffers[*i];
        let b = &buf.buffer_grid[grid];
        b.ensure_buffer(&buf.info)?;
        let data = b.data.borrow_mut();

        // Skip zero-sized *tiles*.
        //
        // Note that some bitstreams can contain channels with one dimension being 0 (e.g. palette
        // meta-channel with 0 colors has size (0, 3)). Those must still participate in channel
        // numbering (but carry no entropy-coded pixels), so we only skip when both dimensions are 0.
        // TODO(veluca): figure out if this is the best approach or we should instead pass through
        // empty buffers.
        if b.size.0 == 0 && b.size.1 == 0 {
            continue;
        }

        bufs.push(AtomicRefMut::map(data, |x| x.as_mut().unwrap()));
    }
    f(bufs.iter_mut().map(|x| x.deref_mut()).collect())
}
