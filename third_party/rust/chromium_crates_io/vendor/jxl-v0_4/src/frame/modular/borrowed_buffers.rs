// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::ops::DerefMut;

use crate::{
    error::Result,
    frame::modular::{IMAGE_OFFSET, IMAGE_PADDING},
    image::Image,
    util::AtomicRefMut,
};

use super::{ModularBufferInfo, ModularChannel};

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
        let mut data = b.data.borrow_mut();
        if data.is_none() {
            *data = Some(ModularChannel {
                data: Image::new_with_padding(b.size, IMAGE_OFFSET, IMAGE_PADDING)?,
                auxiliary_data: None,
                shift: buf.info.shift,
                bit_depth: buf.info.bit_depth,
            });
        }

        // Skip zero-sized *tiles*.
        //
        // Note that some bitstreams can contain channels with one dimension being 0 (e.g. palette
        // meta-channel with 0 colors has size (0, 3)). Those must still participate in channel
        // numbering (but carry no entropy-coded pixels), so we only skip when both dimensions are 0.
        if b.size.0 == 0 && b.size.1 == 0 {
            continue;
        }

        bufs.push(AtomicRefMut::map(data, |x| x.as_mut().unwrap()));
    }
    f(bufs.iter_mut().map(|x| x.deref_mut()).collect())
}
