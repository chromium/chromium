// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::ModularBufferInfo;
use crate::error::Result;
use crate::frame::DataStatus;
use crate::frame::modular::ChannelInfo;
use crate::headers::bit_depth::BitDepth;
use crate::image::{BufferRecycler, Image, ImageRect, OwnedRawImage};
use crate::util::sync::atomic::{AtomicUsize, Ordering};
use crate::util::sync::{Mutex, RwLock};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ModularStorage {
    I16,
    I32,
}

impl ModularStorage {
    #[inline(always)]
    pub const fn sample_size(self) -> usize {
        match self {
            ModularStorage::I16 => 2,
            ModularStorage::I32 => 4,
        }
    }
}

// All the information on a specific buffer needed by Modular decoding.
#[derive(Debug)]
pub(super) struct ModularChannel {
    // Actual pixel buffer.
    pub(super) data: OwnedRawImage,
    // Shift of the channel (None if this is a meta-channel).
    pub(super) shift: Option<(usize, usize)>,
    pub(super) bit_depth: BitDepth,
}

impl ModularChannel {
    pub fn new(size: (usize, usize), storage: ModularStorage, bit_depth: BitDepth) -> Result<Self> {
        Self::new_with_shift(size, storage, Some((0, 0)), bit_depth)
    }

    pub fn new_with_shift(
        size: (usize, usize),
        storage: ModularStorage,
        shift: Option<(usize, usize)>,
        bit_depth: BitDepth,
    ) -> Result<Self> {
        let sample_size = storage.sample_size();
        Ok(ModularChannel {
            data: OwnedRawImage::new((size.0 * sample_size, size.1))?,
            shift,
            bit_depth,
        })
    }

    fn try_clone(&self) -> Result<Self> {
        Ok(ModularChannel {
            data: self.data.try_clone()?,
            shift: self.shift,
            bit_depth: self.bit_depth,
        })
    }

    pub fn channel_info(&self, storage: ModularStorage) -> ChannelInfo {
        ChannelInfo {
            output_channel_idx: None,
            size: self.size(storage),
            shift: self.shift,
            bit_depth: self.bit_depth,
            followed_by_palette: false,
        }
    }

    #[inline(always)]
    pub fn size(&self, storage: ModularStorage) -> (usize, usize) {
        let sample_size = storage.sample_size();
        (
            self.data.byte_size().0 / sample_size,
            self.data.byte_size().1,
        )
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub(super) struct NeededBorders {
    pub topbottom: bool,
    pub leftright: bool,
}

impl NeededBorders {
    pub const NONE: Self = Self {
        topbottom: false,
        leftright: false,
    };
    pub const TOPBOTTOM: Self = Self {
        topbottom: true,
        leftright: false,
    };
    pub const LEFTRIGHT: Self = Self {
        topbottom: false,
        leftright: true,
    };

    pub fn is_empty(&self) -> bool {
        !self.topbottom && !self.leftright
    }
}

#[derive(Debug)]
pub(super) struct ModularBuffer {
    pub(super) data: RwLock<Option<ModularChannel>>,
    // 2px horizontal borders (top 2 rows and bottom 2 rows, size: (width, 4)).
    pub(super) topbottom: RwLock<Option<OwnedRawImage>>,
    // 2px vertical borders (left 2 cols and right 2 cols, size: (4, height)).
    pub(super) leftright: RwLock<Option<OwnedRawImage>>,
    // Holds additional information such as the weighted predictor's error channel's last row for
    // the transform chunk that produced this buffer.
    pub(super) auxiliary_data: RwLock<Option<Image<i32>>>,
    pub(super) needed_borders: NeededBorders,
    // Number of times this buffer will be used, *including* when it is used for output.
    pub(super) remaining_uses: AtomicUsize,
    // Number of remaining final transform chunks that depend on this buffer (for data and/or borders).
    pub(super) remaining_final_uses: AtomicUsize,
    // Number of full-buffer uses for final renders.
    pub(super) full_uses_count_final: usize,
    // Transform steps that use the image data in this buffer for final renders.
    pub(super) used_by_transforms_final: Vec<usize>,
    // Transform steps that depend on this buffer for the current rendering pass.
    pub(super) used_by_transforms_current: Mutex<Vec<usize>>,
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
            data: RwLock::new(None),
            topbottom: RwLock::new(None),
            leftright: RwLock::new(None),
            auxiliary_data: RwLock::new(None),
            needed_borders: NeededBorders::NONE,
            remaining_uses: AtomicUsize::new(0),
            remaining_final_uses: AtomicUsize::new(0),
            full_uses_count_final: 0,
            used_by_transforms_final: vec![],
            used_by_transforms_current: Mutex::new(vec![]),
            size,
            data_status: DataStatus::Zero,
            produced_by_step: None,
        }
    }

    pub fn has_buffer(&self) -> bool {
        self.data.try_read().unwrap().is_some()
    }

    pub fn has_borders(&self) -> bool {
        self.topbottom.try_read().unwrap().is_some() || self.leftright.try_read().unwrap().is_some()
    }

    pub fn extract_needed_borders(
        &self,
        storage: ModularStorage,
        recycler: &BufferRecycler,
    ) -> Result<()> {
        if self.needed_borders.is_empty() {
            return Ok(());
        }
        let data_guard = self.data.try_read().unwrap();
        let Some(chan) = data_guard.as_ref() else {
            return Ok(());
        };
        let (w, h) = chan.size(storage);
        if w == 0 || h == 0 {
            return Ok(());
        }

        let sample_size = storage.sample_size();

        if self.needed_borders.topbottom {
            let mut topbottom = recycler.get_raw_buffer((w * sample_size, 4))?;
            let r0 = chan.data.row(0);
            let r1 = if h > 1 { chan.data.row(1) } else { r0 };
            let rb0 = if h > 1 { chan.data.row(h - 2) } else { r0 };
            let rb1 = chan.data.row(h - 1);
            topbottom.row_mut(0).copy_from_slice(r0);
            topbottom.row_mut(1).copy_from_slice(r1);
            topbottom.row_mut(2).copy_from_slice(rb0);
            topbottom.row_mut(3).copy_from_slice(rb1);
            *self.topbottom.try_write().unwrap() = Some(topbottom);
        }

        if self.needed_borders.leftright {
            let leftright = match storage {
                ModularStorage::I16 => {
                    let in_rect = ImageRect::<i16>::from_raw(chan.data.as_rect());
                    let mut leftright = recycler.get_buffer::<i16>((4, h))?;
                    for y in 0..h {
                        let r = in_rect.row(y);
                        let out = leftright.row_mut(y);
                        out[0] = r[0];
                        out[1] = if w > 1 { r[1] } else { r[0] };
                        out[2] = if w > 1 { r[w - 2] } else { r[0] };
                        out[3] = r[w - 1];
                    }
                    leftright.into_raw()
                }
                ModularStorage::I32 => {
                    let in_rect = ImageRect::<i32>::from_raw(chan.data.as_rect());
                    let mut leftright = recycler.get_buffer::<i32>((4, h))?;
                    for y in 0..h {
                        let r = in_rect.row(y);
                        let out = leftright.row_mut(y);
                        out[0] = r[0];
                        out[1] = if w > 1 { r[1] } else { r[0] };
                        out[2] = if w > 1 { r[w - 2] } else { r[0] };
                        out[3] = r[w - 1];
                    }
                    leftright.into_raw()
                }
            };
            *self.leftright.try_write().unwrap() = Some(leftright);
        }

        Ok(())
    }

    pub fn make_buffer(
        &self,
        info: &ChannelInfo,
        storage: ModularStorage,
        recycler: &BufferRecycler,
    ) -> Result<ModularChannel> {
        let sample_size = storage.sample_size();
        let data = recycler.get_raw_buffer((self.size.0 * sample_size, self.size.1))?;
        Ok(ModularChannel {
            data,
            shift: info.shift,
            bit_depth: info.bit_depth,
        })
    }

    pub fn ensure_buffer(
        &self,
        info: &ChannelInfo,
        storage: ModularStorage,
        recycler: &BufferRecycler,
    ) -> Result<()> {
        if !self.has_buffer() {
            let buf = self.make_buffer(info, storage, recycler)?;
            *self.data.try_write().unwrap() = Some(buf);
        }
        Ok(())
    }

    // Gives out a copy of the buffer + auxiliary buffer, marking the buffer as used.
    // If this was the last usage of the buffer, does not actually copy the buffer.
    pub fn get_buffer(
        &self,
        can_consume: bool,
        recycler: &BufferRecycler,
    ) -> Result<ModularChannel> {
        if !can_consume || DISABLE_MODULAR_BUFFER_DEALLOCATION_FOR_DEBUG {
            return ModularChannel::try_clone(self.data.try_read().unwrap().as_ref().unwrap());
        }
        let mut ret = None;
        let _ = self.remaining_uses.fetch_update(
            Ordering::Release,
            Ordering::Acquire,
            |remaining_pre| {
                let remaining = remaining_pre.checked_sub(1).unwrap();
                if ret.is_none() {
                    if remaining == 0 {
                        ret = Some(Ok(self.data.try_write().unwrap().take().unwrap()))
                    } else {
                        ret = self
                            .data
                            .try_read()
                            .unwrap()
                            .as_ref()
                            .map(ModularChannel::try_clone);
                    }
                } else if remaining == 0 {
                    let old_data = self.data.try_write().unwrap().take();
                    if let Some(chan) = old_data {
                        recycler.recycle_raw_buffer(chan.data);
                    }
                }
                Some(remaining)
            },
        );
        ret.unwrap()
    }

    #[inline]
    pub fn can_consume(&self, is_final: bool) -> bool {
        (self.produced_by_step.is_some() && self.data_status == DataStatus::Partial) || is_final
    }

    pub fn mark_used(&self, can_consume: bool, recycler: &BufferRecycler) {
        if !can_consume || DISABLE_MODULAR_BUFFER_DEALLOCATION_FOR_DEBUG {
            return;
        }
        let _ = self.remaining_uses.fetch_update(
            Ordering::Release,
            Ordering::Acquire,
            |remaining_pre: usize| {
                let remaining = remaining_pre.checked_sub(1).unwrap();
                if remaining == 0 {
                    let old_data = self.data.try_write().unwrap().take();
                    if let Some(chan) = old_data {
                        recycler.recycle_raw_buffer(chan.data);
                    }
                }
                Some(remaining)
            },
        );
    }

    pub fn mark_final_use_done(&self, recycler: &BufferRecycler) {
        if DISABLE_MODULAR_BUFFER_DEALLOCATION_FOR_DEBUG {
            return;
        }
        let prev = self.remaining_final_uses.fetch_sub(1, Ordering::AcqRel);
        assert_ne!(prev, 0);
        if prev == 1 {
            let tb = self.topbottom.try_write().unwrap().take();
            let lr = self.leftright.try_write().unwrap().take();
            let _ = self.auxiliary_data.try_write().unwrap().take();
            let d = self.data.try_write().unwrap().take();
            if let Some(chan) = d {
                recycler.recycle_raw_buffer(chan.data);
            }
            if let Some(tb_img) = tb {
                recycler.recycle_raw_buffer(tb_img);
            }
            if let Some(lr_img) = lr {
                recycler.recycle_raw_buffer(lr_img);
            }
        }
    }
}

pub fn with_buffers<T>(
    buffers: &[ModularBufferInfo],
    indices: &[usize],
    grid: usize,
    recycler: &BufferRecycler,
    f: impl FnOnce(Vec<&mut ModularChannel>) -> Result<T>,
) -> Result<T> {
    let mut guards = vec![];
    for i in indices {
        // Allocate buffers if they are not present.
        let buf = &buffers[*i];
        let b = &buf.buffer_grid[grid];
        b.ensure_buffer(&buf.info, buf.storage, recycler)?;

        // Skip zero-sized *tiles*.
        //
        // Note that some bitstreams can contain channels with one dimension being 0 (e.g. palette
        // meta-channel with 0 colors has size (0, 3)). Those must still participate in channel
        // numbering (but carry no entropy-coded pixels), so we only skip when both dimensions are 0.
        if b.size.0 == 0 && b.size.1 == 0 {
            continue;
        }

        guards.push(b.data.try_write().unwrap());
    }
    let bufs: Vec<&mut ModularChannel> = guards.iter_mut().map(|g| g.as_mut().unwrap()).collect();
    f(bufs)
}
