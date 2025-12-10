// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::io::{BufRead, BufReader, Error, IoSliceMut, Read, Seek, SeekFrom};

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
            self.read(&mut [IoSliceMut::new(&mut skip_buf[..num])])?;
            bytes -= num;
            skipped += num;
        }
        Ok(skipped)
    }

    /// Un-consumes read bytes. This will only be called at the end of a file stream,
    /// to un-read potentially over-read bytes. If ensuring that data is not read past
    /// the file end is not required, this method can safely be implemented as a no-op.
    /// The provided implementation does nothing.
    fn unconsume(&mut self, _count: usize) -> Result<(), Error> {
        Ok(())
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
        self.seek(SeekFrom::Current(bytes as i64))
            .map(|x| x.saturating_sub(cur) as usize)
    }

    fn unconsume(&mut self, count: usize) -> Result<(), Error> {
        self.seek_relative(-(count as i64))
    }
}
