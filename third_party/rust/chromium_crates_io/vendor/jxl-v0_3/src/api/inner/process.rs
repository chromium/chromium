// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    io::IoSliceMut,
    ops::{Deref, Range},
};

use crate::error::Result;

use crate::api::{JxlBitstreamInput, JxlDecoderInner, JxlOutputBuffer, ProcessingResult};

// General implementation strategy:
// - Anything that is not a section is read into a small buffer.
// - As soon as we know section sizes, data is read directly into sections.
// When the start of the populated range in `buf` goes past half of its length,
// the data in the buffer is moved back to the beginning.

pub(super) struct SmallBuffer {
    buf: Vec<u8>,
    range: Range<usize>,
}

impl SmallBuffer {
    pub(super) fn refill(
        &mut self,
        mut get_input: impl FnMut(&mut [IoSliceMut]) -> Result<usize, std::io::Error>,
        max: Option<usize>,
    ) -> Result<usize> {
        let mut total = 0;
        loop {
            if self.range.start >= self.buf.len() / 2 {
                let start = self.range.start;
                let len = self.range.len();
                let (pre, post) = self.buf.split_at_mut(start);
                pre[0..len].copy_from_slice(&post[0..len]);
                self.range.start -= start;
                self.range.end -= start;
            }
            if self.range.len() >= self.buf.len() / 2 {
                break;
            }
            let stop = if let Some(max) = max {
                self.range
                    .end
                    .saturating_add(max.saturating_sub(total))
                    .min(self.buf.len())
            } else {
                self.buf.len()
            };
            let num = get_input(&mut [IoSliceMut::new(&mut self.buf[self.range.end..stop])])?;
            total += num;
            self.range.end += num;
            if num == 0 {
                break;
            }
        }
        Ok(total)
    }

    pub(super) fn take(&mut self, mut buffers: &mut [IoSliceMut]) -> usize {
        let mut num = 0;
        while !self.range.is_empty() {
            let Some((buf, rest)) = buffers.split_first_mut() else {
                break;
            };
            buffers = rest;
            let len = self.range.len().min(buf.len());
            // Only copy 'len' bytes, not the entire range, to avoid panic when buf is smaller than range
            buf[..len].copy_from_slice(&self.buf[self.range.start..self.range.start + len]);
            self.range.start += len;
            num += len;
        }
        num
    }

    pub(super) fn consume(&mut self, amount: usize) -> usize {
        let amount = amount.min(self.range.len());
        self.range.start += amount;
        amount
    }

    pub(super) fn new(initial_size: usize) -> Self {
        Self {
            buf: vec![0; initial_size],
            range: 0..0,
        }
    }

    pub(super) fn range(&self) -> Range<usize> {
        self.range.clone()
    }

    pub(super) fn enlarge(&mut self) {
        // Note: we need a *4 here because doubling the buffer size might still not allow refill() to make progress.
        self.buf.resize(self.buf.len() * 4, 0);
    }

    pub(super) fn can_read_more(&self) -> bool {
        self.buf.len() > self.len() * 2 && self.range.end < self.buf.len()
    }
}

impl Deref for SmallBuffer {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        &self.buf[self.range.clone()]
    }
}

impl JxlDecoderInner {
    /// Process more of the input file.
    /// This function will return when reaching the next decoding stage (i.e. finished decoding
    /// file/frame header, or finished decoding a frame).
    /// If called when decoding a frame with `None` for buffers, the frame will still be read,
    /// but pixel data will not be produced.
    #[inline(never)]
    pub fn process(
        &mut self,
        input: &mut dyn JxlBitstreamInput,
        buffers: Option<&mut [JxlOutputBuffer]>,
    ) -> Result<ProcessingResult<(), ()>> {
        ProcessingResult::new(self.codestream_parser.process(
            &mut self.box_parser,
            input,
            &self.options,
            buffers,
        ))
    }

    /// Draws all the pixels we have data for.
    pub fn flush_pixels(&mut self, _buffers: &mut [JxlOutputBuffer]) -> Result<()> {
        todo!()
    }
}
