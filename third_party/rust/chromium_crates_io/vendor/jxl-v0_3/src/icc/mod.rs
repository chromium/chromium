// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::io::Cursor;

use byteorder::{BigEndian, ReadBytesExt, WriteBytesExt};

use crate::bit_reader::*;
use crate::entropy_coding::decode::Histograms;
use crate::entropy_coding::decode::SymbolReader;
use crate::error::{Error, Result};
use crate::headers::encodings::*;
use crate::util::NewWithCapacity;
use crate::util::tracing_wrappers::warn;

mod header;
mod stream;
mod tag;

use header::read_header;
use stream::{IccStream, read_varint_from_reader};
use tag::{read_single_command, read_tag_list};

const ICC_CONTEXTS: usize = 41;
const ICC_HEADER_SIZE: u64 = 128;

fn read_icc_inner(stream: &mut IccStream) -> Result<Vec<u8>, Error> {
    let output_size = stream.read_varint()?;
    let commands_size = stream.read_varint()?;
    if stream.bytes_read().saturating_add(commands_size) > stream.len() {
        return Err(Error::InvalidIccStream);
    }

    // Simple check to avoid allocating too large buffer.
    if output_size > (1 << 28) {
        return Err(Error::IccTooLarge);
    }

    if output_size + 65536 < stream.len() {
        return Err(Error::IccTooLarge);
    }

    // Extract command stream first.
    let commands = stream.read_to_vec_exact(commands_size as usize)?;
    let mut commands_stream = Cursor::new(commands);
    // `stream` contains data stream from here.
    let data_stream = stream;

    // Decode ICC profile header.
    let mut decoded_profile = read_header(data_stream, output_size)?;
    if output_size <= ICC_HEADER_SIZE {
        return Ok(decoded_profile);
    }

    // Convert to slice writer to prevent buffer from growing.
    // `read_header` above returns buffer with capacity of `output_size`, so this doesn't realloc.
    debug_assert_eq!(decoded_profile.capacity(), output_size as usize);
    decoded_profile.resize(output_size as usize, 0);
    let mut decoded_profile_writer = Cursor::new(&mut *decoded_profile);
    decoded_profile_writer.set_position(ICC_HEADER_SIZE);

    // Decode tag list.
    let v = read_varint_from_reader(&mut commands_stream)?;
    if let Some(num_tags) = v.checked_sub(1) {
        if (output_size - ICC_HEADER_SIZE) / 12 < num_tags {
            warn!(output_size, num_tags, "num_tags too large");
            return Err(Error::InvalidIccStream);
        }

        let num_tags = num_tags as u32;
        decoded_profile_writer
            .write_u32::<BigEndian>(num_tags)
            .map_err(|_| Error::InvalidIccStream)?;

        read_tag_list(
            data_stream,
            &mut commands_stream,
            &mut decoded_profile_writer,
            num_tags,
            output_size,
        )?;
    }

    // Decode tag data.
    // Will not enter the loop if end of stream was reached while decoding tag list.
    while let Ok(command) = commands_stream.read_u8() {
        read_single_command(
            data_stream,
            &mut commands_stream,
            &mut decoded_profile_writer,
            command,
        )?;
    }

    // Validate output size.
    let actual_len = decoded_profile_writer.position();
    if actual_len != output_size {
        warn!(output_size, actual_len, "ICC profile size mismatch");
        return Err(Error::InvalidIccStream);
    }

    Ok(decoded_profile)
}

/// Struct to incrementally decode an ICC profile.
pub struct IncrementalIccReader {
    histograms: Histograms,
    reader: SymbolReader,
    out_buf: Vec<u8>,
    len: usize,
    // [prev, prev_prev]
    prev_bytes: [u8; 2],
}

impl IncrementalIccReader {
    pub fn new(br: &mut BitReader) -> Result<Self> {
        let len = u64::read_unconditional(&(), br, &Empty {})?;
        if len > 1u64 << 20 {
            return Err(Error::IccTooLarge);
        }

        let len = len as usize;

        let histograms = Histograms::decode(ICC_CONTEXTS, br, true)?;
        let reader = SymbolReader::new(&histograms, br, None)?;
        Ok(Self {
            histograms,
            reader,
            len,
            out_buf: Vec::new_with_capacity(len)?,
            prev_bytes: [0, 0],
        })
    }

    fn get_icc_ctx(&self) -> u32 {
        if self.out_buf.len() <= ICC_HEADER_SIZE as usize {
            return 0;
        }

        let [b1, b2] = self.prev_bytes;

        let p1 = match b1 {
            b'a'..=b'z' | b'A'..=b'Z' => 0,
            b'0'..=b'9' | b'.' | b',' => 1,
            0..=1 => 2 + b1 as u32,
            2..=15 => 4,
            241..=254 => 5,
            255 => 6,
            _ => 7,
        };
        let p2 = match b2 {
            b'a'..=b'z' | b'A'..=b'Z' => 0,
            b'0'..=b'9' | b'.' | b',' => 1,
            0..=15 => 2,
            241..=255 => 3,
            _ => 4,
        };

        1 + p1 + 8 * p2
    }

    pub fn num_coded_bytes(&self) -> usize {
        self.len
    }

    pub fn remaining(&self) -> usize {
        assert!(self.num_coded_bytes() >= self.out_buf.len());
        self.num_coded_bytes() - self.out_buf.len()
    }

    pub fn read_one(&mut self, br: &mut BitReader) -> Result<()> {
        let ctx = self.get_icc_ctx() as usize;
        let checkpoint = self.reader.checkpoint::<1>();
        let sym = self.reader.read_unsigned(&self.histograms, br, ctx);

        if let Err(err) = br.check_for_error() {
            self.reader.restore(checkpoint);
            return Err(err);
        }
        if sym >= 256 {
            warn!(sym, "Invalid symbol in ICC stream");
            return Err(Error::InvalidIccStream);
        }

        let b = sym as u8;
        self.out_buf.push(b);
        self.prev_bytes = [b, self.prev_bytes[0]];
        Ok(())
    }

    pub fn read_all(&mut self, br: &mut BitReader) -> Result<()> {
        for _ in self.out_buf.len()..self.num_coded_bytes() {
            self.read_one(br)?;
        }
        Ok(())
    }

    pub fn finalize(self, br: &mut BitReader) -> Result<Vec<u8>> {
        assert_eq!(self.num_coded_bytes(), self.out_buf.len());
        self.reader.check_final_state(&self.histograms, br)?;
        let mut stream = IccStream::new(self.out_buf);
        let profile = read_icc_inner(&mut stream)?;
        stream.finalize()?;
        Ok(profile)
    }
}
