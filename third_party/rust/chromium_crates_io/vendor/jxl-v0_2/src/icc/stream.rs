// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::io::{Read, Write};

use byteorder::{ReadBytesExt, WriteBytesExt};

use crate::error::{Error, Result};
use crate::util::NewWithCapacity;
use crate::util::tracing_wrappers::{instrument, warn};

fn read_varint(mut read_one: impl FnMut() -> Result<u8>) -> Result<u64> {
    let mut value = 0u64;
    let mut shift = 0;
    while shift < 63 {
        let b = read_one()?;
        value |= ((b & 0x7f) as u64) << shift;
        if b & 0x80 == 0 {
            break;
        }
        shift += 7;
    }
    Ok(value)
}

pub(super) fn read_varint_from_reader(stream: &mut impl Read) -> Result<u64> {
    read_varint(|| stream.read_u8().map_err(|_| Error::IccEndOfStream))
}

pub(super) struct IccStream {
    command_stream: Vec<u8>,
    bytes_read: u64,
}

impl IccStream {
    pub(super) fn new(command_stream: Vec<u8>) -> Self {
        Self {
            command_stream,
            bytes_read: 0,
        }
    }

    pub fn len(&self) -> u64 {
        self.command_stream.len() as u64
    }

    pub fn bytes_read(&self) -> u64 {
        self.bytes_read
    }

    pub fn remaining_bytes(&self) -> u64 {
        self.len() - self.bytes_read
    }

    fn read_one(&mut self) -> Result<u8> {
        if self.remaining_bytes() == 0 {
            return Err(Error::IccEndOfStream);
        }
        self.bytes_read += 1;
        Ok(self.command_stream[self.bytes_read as usize - 1])
    }

    pub fn read_exact(&mut self, buf: &mut [u8]) -> Result<()> {
        if buf.len() > self.remaining_bytes() as usize {
            return Err(Error::IccEndOfStream);
        }

        for b in buf {
            *b = self.read_one()?;
        }

        Ok(())
    }

    pub fn read_to_vec_exact(&mut self, len: usize) -> Result<Vec<u8>> {
        if len > self.remaining_bytes() as usize {
            return Err(Error::IccEndOfStream);
        }

        let mut out = Vec::new_with_capacity(len)?;

        for _ in 0..len {
            out.push(self.read_one()?);
        }

        Ok(out)
    }

    pub fn read_varint(&mut self) -> Result<u64> {
        read_varint(|| self.read_one())
    }

    pub fn copy_bytes(&mut self, writer: &mut impl Write, len: usize) -> Result<()> {
        if len > self.remaining_bytes() as usize {
            return Err(Error::IccEndOfStream);
        }

        for _ in 0..len {
            let b = self.read_one()?;
            writer.write_u8(b).map_err(|_| Error::InvalidIccStream)?;
        }

        Ok(())
    }

    #[instrument(skip_all, err)]
    pub fn finalize(self) -> Result<()> {
        // Check if all bytes are read
        if self.bytes_read == self.command_stream.len() as u64 {
            Ok(())
        } else {
            warn!("ICC stream is not fully consumed");
            Err(Error::InvalidIccStream)
        }
    }
}
