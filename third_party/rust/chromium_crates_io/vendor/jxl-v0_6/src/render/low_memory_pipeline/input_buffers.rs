// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::sync::{
    RwLock,
    atomic::{AtomicBool, AtomicU8, AtomicUsize, Ordering},
};

use crate::error::Result;
use crate::image::OwnedRawImage;
use crate::render::internal::RenderPipelineShared;
use crate::render::low_memory_pipeline::row_buffers::RowBuffer;
use crate::util::NewWithCapacity;

pub(super) struct InputBuffer {
    // One buffer per channel.
    pub(super) data: Vec<RwLock<Option<OwnedRawImage>>>,
    // Storage for left/right borders. Includes corners.
    pub(super) leftright: Vec<RwLock<Option<OwnedRawImage>>>,
    // Storage for top/bottom borders. Includes corners.
    pub(super) topbottom: Vec<RwLock<Option<OwnedRawImage>>>,
    // Number of ready channels in the current pass.
    ready_channels: AtomicUsize,
    is_ready: AtomicBool,
}

impl InputBuffer {
    // Returns the number of ready channels.
    pub(super) fn set_buffer(&self, chan: usize, buf: OwnedRawImage) -> usize {
        assert!(!self.is_ready.load(Ordering::Relaxed));
        assert!(
            self.data[chan].try_write().unwrap().is_none(),
            "chan: {chan}"
        );
        *self.data[chan].try_write().unwrap() = Some(buf);
        self.ready_channels.fetch_add(1, Ordering::AcqRel) + 1
    }

    pub(super) fn new(num_channels: usize) -> Self {
        let b = || (0..num_channels).map(|_| RwLock::new(None)).collect();
        Self {
            data: b(),
            leftright: b(),
            topbottom: b(),
            ready_channels: AtomicUsize::new(0),
            is_ready: AtomicBool::new(false),
        }
    }
}

pub(super) struct InputBuffers {
    buffers: Vec<InputBuffer>,
    // We use the cell that would map to the buffer itself to keep
    // track of the number of complete groups in its neighbourhood.
    remaining_flags: Vec<AtomicU8>,
    size: (usize, usize),
}

impl InputBuffers {
    fn increase_borders(flags: &[AtomicU8], (gx, gy): (usize, usize), (xs, _): (usize, usize)) {
        let stride = xs * 2 + 1;
        let center = (gy * 2 + 1) * stride + gx * 2 + 1;
        flags[center - stride - 1].fetch_add(1, Ordering::Relaxed);
        flags[center - stride].fetch_add(1, Ordering::Relaxed);
        flags[center - stride + 1].fetch_add(1, Ordering::Relaxed);
        flags[center - 1].fetch_add(1, Ordering::Relaxed);
        flags[center + 1].fetch_add(1, Ordering::Relaxed);
        flags[center + stride - 1].fetch_add(1, Ordering::Relaxed);
        flags[center + stride].fetch_add(1, Ordering::Relaxed);
        flags[center + stride + 1].fetch_add(1, Ordering::Relaxed);
    }

    pub fn mark_not_ready(&mut self, group: usize) {
        if !self.buffers[group].is_ready.swap(false, Ordering::Relaxed) {
            return;
        }
        let gx = group % self.size.0;
        let gy = group / self.size.0;
        Self::increase_borders(&self.remaining_flags, (gx, gy), self.size);
    }

    pub fn mark_ready(&self, group: usize) -> [bool; 9] {
        assert!(!self.buffers[group].is_ready.swap(true, Ordering::Relaxed));
        let gx = group % self.size.0;
        let gy = group / self.size.0;
        let stride = self.size.0 * 2 + 1;
        let center = (gy * 2 + 1) * stride + gx * 2 + 1;
        [
            self.remaining_flags[center - stride - 1].fetch_sub(1, Ordering::AcqRel) == 1,
            self.remaining_flags[center - stride].fetch_sub(1, Ordering::AcqRel) == 1,
            self.remaining_flags[center - stride + 1].fetch_sub(1, Ordering::AcqRel) == 1,
            self.remaining_flags[center - 1].fetch_sub(1, Ordering::AcqRel) == 1,
            true,
            self.remaining_flags[center + 1].fetch_sub(1, Ordering::AcqRel) == 1,
            self.remaining_flags[center + stride - 1].fetch_sub(1, Ordering::AcqRel) == 1,
            self.remaining_flags[center + stride].fetch_sub(1, Ordering::AcqRel) == 1,
            self.remaining_flags[center + stride + 1].fetch_sub(1, Ordering::AcqRel) == 1,
        ]
    }

    pub fn get(&self, group: usize) -> &InputBuffer {
        &self.buffers[group]
    }

    pub fn mark_done(
        &self,
        group: usize,
        shared: &RenderPipelineShared<RowBuffer>,
        store_buf: impl Fn(usize, usize, OwnedRawImage),
    ) {
        let gx = group % self.size.0;
        let gy = group / self.size.0;
        let gxm1 = gx.saturating_sub(1);
        let gym1 = gy.saturating_sub(1);
        let gxp1 = (gx + 1).min(shared.group_count.0 - 1);
        let gyp1 = (gy + 1).min(shared.group_count.1 - 1);
        let gw = shared.group_count.0;

        let all_finalized = (0..shared.num_channels())
            .filter(|&c| shared.channel_is_used[c])
            .all(|c| shared.group_chan_complete[group][c].load(Ordering::Relaxed));

        {
            let data = &self.buffers[group].data;
            let mut preserved_count = 0;
            for c in 0..data.len() {
                if !shared.channel_is_used[c] {
                    continue;
                }
                let is_finalized = shared.group_chan_complete[group][c].load(Ordering::Relaxed);
                let preserve = is_finalized && !all_finalized;
                if !preserve {
                    if let Some(b) = std::mem::take(&mut *data[c].try_write().unwrap()) {
                        store_buf(c, 0, b);
                    }
                } else {
                    preserved_count += 1;
                }
            }
            self.buffers[group]
                .ready_channels
                .store(preserved_count, Ordering::Relaxed);
        }

        // Clear border buffers that will not be used again.
        // This is certainly the case if *all* the groups in the 3x3 group area around
        // the current group are complete.
        if shared.group_chan_complete[group]
            .iter()
            .all(|x| x.load(Ordering::Relaxed))
        {
            for g in [
                gym1 * gw + gxm1,
                gym1 * gw + gx,
                gym1 * gw + gxp1,
                gy * gw + gxm1,
                gy * gw + gx,
                gy * gw + gxp1,
                gyp1 * gw + gxm1,
                gyp1 * gw + gx,
                gyp1 * gw + gxp1,
            ] {
                let gx = g % self.size.0;
                let gy = g / self.size.0;
                let idx = (gy * 2 + 1) * (2 * self.size.0 + 1) + gx * 2 + 1;
                if self.remaining_flags[idx].fetch_add(1, Ordering::AcqRel) != 8 {
                    continue;
                }
                for c in 0..self.buffers[g].data.len() {
                    if let Some(b) =
                        std::mem::take(&mut *self.buffers[g].topbottom[c].try_write().unwrap())
                    {
                        store_buf(c, 1, b);
                    }
                    if let Some(b) =
                        std::mem::take(&mut *self.buffers[g].leftright[c].try_write().unwrap())
                    {
                        store_buf(c, 2, b);
                    }
                }
            }
        }
    }

    pub fn new(nc: usize, size: (usize, usize)) -> Result<Self> {
        let bufs = size.0 * size.1;
        let mut buffers = Vec::new_with_capacity(bufs)?;
        for _ in 0..bufs {
            buffers.push(InputBuffer::new(nc));
        }

        let grids = (2 * size.0 + 1) * (2 * size.1 + 1);
        let mut remaining_flags = Vec::new_with_capacity(grids)?;
        for _ in 0..grids {
            remaining_flags.push(AtomicU8::new(0));
        }

        for gy in 0..size.1 {
            for gx in 0..size.0 {
                Self::increase_borders(&remaining_flags, (gx, gy), size);
            }
        }

        Ok(Self {
            buffers,
            remaining_flags,
            size,
        })
    }
}
