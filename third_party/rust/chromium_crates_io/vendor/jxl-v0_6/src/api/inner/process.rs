// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    io::IoSliceMut,
    ops::{Deref, Range},
};

use crate::{
    api::{
        JxlBitstreamInput, JxlDecoderInner, JxlOutputBuffer, ProcessingResult,
        inner::box_parser::CodestreamInput,
    },
    bit_reader::BitReader,
};
use crate::{
    api::{JxlParallelRunner, JxlParallelRunnerFun},
    error::Result,
};

/// A small buffer, that guarantees to never use more than twice the maximum
/// amount of bytes that were simultaneously present in it.
/// This is done by moving the data in the buffer back to the beginning
/// when the start of the populated range goes past half of its length.
pub(super) struct SmallBuffer {
    buf: Vec<u8>,
    range: Range<usize>,
    consumed: u64,
    bit_offset: u8,
}

impl SmallBuffer {
    pub(super) fn refill(
        &mut self,
        mut get_input: impl FnMut(&mut [IoSliceMut]) -> Result<usize>,
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
            let num = get_input(&mut [IoSliceMut::new(&mut self.buf[self.range.end..])])?;
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
            self.consume(len);
            num += len;
        }
        num
    }

    pub(super) fn consume(&mut self, amount: usize) {
        assert!(
            amount <= self.range.len(),
            "consuming {amount} with {} available!",
            self.range.len()
        );
        self.range.start += amount;
        self.consumed += amount as u64;
    }

    pub(super) fn mark_consumed(&mut self, amount: u64) {
        self.consumed += amount;
    }

    pub(super) fn consumed(&self) -> u64 {
        self.consumed
    }

    pub(super) fn new(initial_size: usize) -> Self {
        Self {
            buf: vec![0; initial_size],
            range: 0..0,
            consumed: 0,
            bit_offset: 0,
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

    pub(super) fn with_br<T>(
        &mut self,
        mut fun: impl FnMut(&mut BitReader, &mut usize) -> Result<T>,
    ) -> Result<T> {
        let mut br = BitReader::new(self);
        br.skip_bits(self.bit_offset as usize)?;
        let mut bits = br.total_bits_read();
        let ret = fun(&mut br, &mut bits);
        self.consume(bits / 8);
        self.bit_offset = (bits % 8) as u8;
        ret
    }
}

impl Deref for SmallBuffer {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        &self.buf[self.range.clone()]
    }
}

pub(crate) struct SequentialRunner;

impl JxlParallelRunner for SequentialRunner {
    fn run(&mut self, num: usize, fun: &JxlParallelRunnerFun) -> Result<()> {
        for i in 0..num {
            fun(i)?
        }
        Ok(())
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
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<ProcessingResult<(), ()>> {
        ProcessingResult::new(self.codestream_parser.process(
            &mut CodestreamInput::new(&mut self.box_parser, input),
            &self.options,
            buffers,
            parallel_runner.unwrap_or(&mut SequentialRunner),
        ))
    }

    /// Draws all the pixels we have data for. Returns `true` if any new pixels
    /// were written to `buffers` since the previous call to `flush_pixels`;
    /// returns `false` if no new rendering has happened, in which case the
    /// contents of `buffers` are unchanged from the caller's perspective.
    pub fn flush_pixels(
        &mut self,
        buffers: &mut [JxlOutputBuffer],
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<bool> {
        let Some(profile) = self.codestream_parser.output_color_profile.as_ref() else {
            return Ok(false);
        };
        let Some(pixel_format) = self.codestream_parser.pixel_format.as_ref() else {
            return Ok(false);
        };
        match self.codestream_parser.frame_info.do_flush(
            buffers,
            profile,
            pixel_format,
            parallel_runner.unwrap_or(&mut SequentialRunner),
        ) {
            Ok(()) | Err(crate::error::Error::OutOfBounds(_)) => {
                Ok(self.codestream_parser.get_and_clear_pixels_dirty())
            }
            Err(e) => Err(e),
        }
    }
}
