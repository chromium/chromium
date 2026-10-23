// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::io::{BufRead, BufReader, Error, IoSliceMut, Read, Seek, SeekFrom};
use std::ops::{Deref, Range};

use crate::bit_reader::BitReader;
use crate::error::Result as JxlResult;

pub trait JxlBitstreamInput {
    /// Returns an estimate bound of the total number of bytes that can be read via `read`.
    /// Returning a too-low estimate here can impede parallelism. Returning a too-high
    /// estimate can increase memory usage.
    fn available_bytes(&mut self) -> Result<usize, Error>;

    /// Fills in `bufs` with more bytes, returning the number of bytes written.
    /// Buffers are filled in order and to completion.
    fn read(&mut self, bufs: &mut [IoSliceMut]) -> Result<usize, Error>;

    /// Skips up to `bytes` bytes of input. The provided implementation just uses `read`, but in
    /// some cases this can be implemented faster.
    /// Returns the number of bytes that were skipped. If this returns 0, it is assumed that no
    /// more input is available.
    fn skip(&mut self, bytes: usize) -> Result<usize, Error> {
        let mut bytes = bytes;
        const BUF_SIZE: usize = 1024;
        let mut skip_buf = [0; BUF_SIZE];
        let mut skipped = 0;
        while bytes > 0 {
            let num = bytes.min(BUF_SIZE);
            let n = self.read(&mut [IoSliceMut::new(&mut skip_buf[..num])])?;
            if n == 0 {
                break;
            }
            bytes -= n;
            skipped += n;
        }
        Ok(skipped)
    }
}

impl JxlBitstreamInput for &[u8] {
    fn available_bytes(&mut self) -> Result<usize, Error> {
        Ok(self.len())
    }

    fn read(&mut self, bufs: &mut [IoSliceMut]) -> Result<usize, Error> {
        self.read_vectored(bufs)
    }

    fn skip(&mut self, bytes: usize) -> Result<usize, Error> {
        let num = bytes.min(self.len());
        self.consume(num);
        Ok(num)
    }
}

impl<R: Read + Seek> JxlBitstreamInput for BufReader<R> {
    fn available_bytes(&mut self) -> Result<usize, Error> {
        let pos = self.stream_position()?;
        let end = self.seek(SeekFrom::End(0))?;
        self.seek(SeekFrom::Start(pos))?;
        Ok(end.saturating_sub(pos) as usize)
    }

    fn read(&mut self, bufs: &mut [IoSliceMut]) -> Result<usize, Error> {
        self.read_vectored(bufs)
    }

    fn skip(&mut self, bytes: usize) -> Result<usize, Error> {
        let cur = self.stream_position()?;
        if let Ok(offset) = i64::try_from(bytes) {
            self.seek(SeekFrom::Current(offset))
                .map(|x| x.saturating_sub(cur) as usize)
        } else {
            self.seek(SeekFrom::End(0))
                .map(|x| x.saturating_sub(cur) as usize)
        }
    }
}

/// A small buffer, that guarantees to never use more than twice the maximum
/// amount of bytes that were simultaneously present in it.
/// This is done by moving the data in the buffer back to the beginning
/// when the start of the populated range goes past half of its length.
pub(super) struct SmallBuffer {
    buf: Vec<u8>,
    initial_size: usize,
    range: Range<usize>,
    consumed: u64,
    bit_offset: u8,
}

impl SmallBuffer {
    pub(super) fn refill(
        &mut self,
        mut get_input: impl FnMut(&mut [IoSliceMut]) -> JxlResult<usize>,
    ) -> JxlResult<usize> {
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
            initial_size,
            range: 0..0,
            consumed: 0,
            bit_offset: 0,
        }
    }

    pub(super) fn reset(&mut self) {
        self.buf.truncate(self.initial_size);
        self.range = 0..0;
        self.consumed = 0;
        self.bit_offset = 0;
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
        mut fun: impl FnMut(&mut BitReader, &mut usize) -> JxlResult<T>,
    ) -> JxlResult<T> {
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
