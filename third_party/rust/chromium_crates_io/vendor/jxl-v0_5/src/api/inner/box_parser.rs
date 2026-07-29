// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::collections::BTreeMap;
use std::io::Read;
use std::{collections::HashMap, io::IoSliceMut};

use crate::error::{Error, Result};

use crate::api::{
    JxlBitstreamInput, JxlSignatureType, check_signature_internal, inner::process::SmallBuffer,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ParseState {
    SignatureNeeded,
    // We need at least this many bytes in the buffer to determine what to do
    // with the next box.
    BoxNeeded(usize),
    // The next bytes are raw codestream. None = no limit.
    Codestream(Option<u64>),
    // Skip the next bytes.
    Skip(Option<u64>),
    // The next bytes should be buffered in a jxlp box.
    OOOJxlp(u32, Option<u64>),
    // After the last codestream box, no more container bytes: no further
    // codestream in file.
    Complete,
}

// Relevant box types.
#[derive(Debug, Clone, Copy)]
enum CodestreamBoxType {
    None,
    Jxlc,
    // (index, is_last)
    Jxlp(u32, bool),
}

struct OOOJxlpBox {
    data: Vec<u8>,
    consumed: usize,
    is_last: bool,
}

#[derive(Debug, Clone, Copy)]
pub struct BoxParserCheckpoint {
    box_type: CodestreamBoxType,
    pub(super) file_position: u64,
    codestream_left: Option<u64>,
    is_valid_checkpoint: bool,
    pub(crate) consumed_codestream: u64,
}

pub(super) struct BoxParser {
    local_buffer: SmallBuffer,
    version: Option<u32>,
    ooo_jxlp_buffer: HashMap<u32, OOOJxlpBox>,
    state: ParseState,
    latest_codestream_box: CodestreamBoxType,
    // Map from codestream position at the start of the box
    // to box info
    codestream_pos_to_box: BTreeMap<u64, BoxParserCheckpoint>,
    allow_checkpoint: bool,
}

impl BoxParser {
    pub(super) fn new() -> Self {
        BoxParser {
            local_buffer: SmallBuffer::new(128),
            state: ParseState::SignatureNeeded,
            latest_codestream_box: CodestreamBoxType::None,
            ooo_jxlp_buffer: HashMap::new(),
            version: None,
            codestream_pos_to_box: BTreeMap::new(),
            allow_checkpoint: true,
        }
    }

    pub(super) fn reset_to_checkpoint(&mut self, box_checkpoint: BoxParserCheckpoint) {
        self.local_buffer = SmallBuffer::new(128);
        self.ooo_jxlp_buffer.clear();
        self.latest_codestream_box = box_checkpoint.box_type;
        self.state = ParseState::Codestream(box_checkpoint.codestream_left);
        // Do not allow creating new checkpoints after a reset.
        self.allow_checkpoint = false;
    }

    pub(super) fn state_checkpoint(
        &self,
        codestream_pos: u64,
    ) -> Result<Option<BoxParserCheckpoint>> {
        if !self.allow_checkpoint {
            Ok(None)
        } else {
            // last element with key <= codestream_pos.
            let (&start, &(mut b)) = self
                .codestream_pos_to_box
                .range(..=codestream_pos)
                .last()
                .unwrap();
            if !b.is_valid_checkpoint {
                // Disallow files that have frame starts in boxes that are not
                // valid checkpoints.
                return Err(Error::InvalidBox);
            }
            let overflow = codestream_pos - start;
            b.file_position += overflow;
            b.consumed_codestream = codestream_pos;
            Ok(Some(b))
        }
    }

    pub(super) fn total_bytes_consumed(&self, codestream_bytes_consumed: u64) -> u64 {
        let (start, b) = self.codestream_pos_to_box.last_key_value().unwrap();
        b.file_position + (codestream_bytes_consumed - *start)
    }

    fn add_checkpoint(&mut self) {
        if !self.allow_checkpoint {
            return;
        }
        // For a codestream box to be a valid checkpoint, it needs to be decoded
        // in-order (i.e. all other logically-preceding jxlp boxes have been seen already)
        // and we must not have seen any future jxlp boxes (which would be stored in the
        // ooo jxlp buffer).
        let (codestream_left, is_valid_checkpoint) = match self.state {
            ParseState::Codestream(c) => (c, self.ooo_jxlp_buffer.is_empty()),
            ParseState::OOOJxlp(_, c) => (c, false),
            _ => (None, false),
        };
        let codestream_position = self
            .codestream_pos_to_box
            .last_key_value()
            .map(|(p, c)| *p + c.codestream_left.unwrap())
            .unwrap_or(0);
        self.codestream_pos_to_box.insert(
            codestream_position,
            BoxParserCheckpoint {
                file_position: self.local_buffer.consumed(),
                box_type: self.latest_codestream_box,
                codestream_left,
                is_valid_checkpoint,
                consumed_codestream: 0,
            },
        );
    }

    fn injected_jxlp(&mut self) -> Option<&mut OOOJxlpBox> {
        if matches!(self.state, ParseState::BoxNeeded(_) | ParseState::Complete)
            && let CodestreamBoxType::Jxlp(j, _) = self.latest_codestream_box
            && let Some(b) = self.ooo_jxlp_buffer.get_mut(&j)
        {
            Some(b)
        } else {
            None
        }
    }

    fn check_ooo_jxlp_done(&mut self) {
        let CodestreamBoxType::Jxlp(j, is_last) = self.latest_codestream_box else {
            unreachable!()
        };
        let Some(b) = self.ooo_jxlp_buffer.get_mut(&j) else {
            unreachable!()
        };
        if b.consumed == b.data.len() {
            self.ooo_jxlp_buffer.remove(&j);
            if is_last {
                self.state = ParseState::Complete;
            } else if let Some(b) = self.ooo_jxlp_buffer.get(&(j + 1)) {
                self.latest_codestream_box = CodestreamBoxType::Jxlp(j + 1, b.is_last);
            }
        }
    }

    fn mark_codestream_read(&mut self, n: usize) {
        let ParseState::Codestream(c) = &mut self.state else {
            unreachable!()
        };
        if c.is_none() {
            return;
        }
        let new_size = c.unwrap() - n as u64;
        if new_size == 0 {
            self.state = if matches!(
                self.latest_codestream_box,
                CodestreamBoxType::Jxlc | CodestreamBoxType::Jxlp(_, true)
            ) {
                ParseState::Complete
            } else {
                if let CodestreamBoxType::Jxlp(j, _) = self.latest_codestream_box
                    && let Some(b) = self.ooo_jxlp_buffer.get(&(j + 1))
                {
                    self.latest_codestream_box = CodestreamBoxType::Jxlp(j + 1, b.is_last);
                    self.add_checkpoint();
                }
                ParseState::BoxNeeded(8)
            }
        } else {
            *c = Some(new_size);
        }
    }

    fn available_bytes_inner(&mut self, input: &mut dyn JxlBitstreamInput) -> Result<usize> {
        Ok(self.local_buffer.len() + input.available_bytes()?)
    }

    fn read_inner(
        &mut self,
        input: &mut dyn JxlBitstreamInput,
        mut buf: &mut [IoSliceMut],
    ) -> Result<usize> {
        let mut n = 0;
        if !self.local_buffer.is_empty() {
            let mut ll = &self.local_buffer[..];
            n = ll.read_vectored(buf)?;
            IoSliceMut::advance_slices(&mut buf, n);
            self.local_buffer.consume(n);
        }
        let read = input.read(buf)?;
        self.local_buffer.mark_consumed(read as u64);
        Ok(n + read)
    }

    fn skip_inner(
        &mut self,
        input: &mut dyn JxlBitstreamInput,
        mut amount: usize,
    ) -> Result<usize> {
        let mut n = 0;
        if !self.local_buffer.is_empty() {
            let ll = self.local_buffer.len();
            n = ll.min(amount);
            amount -= n;
            self.local_buffer.consume(n);
        }
        let skip = input.skip(amount)?;
        self.local_buffer.mark_consumed(skip as u64);
        Ok(n + skip)
    }

    fn read_until_at_least(
        &mut self,
        input: &mut dyn JxlBitstreamInput,
        min_size: usize,
    ) -> Result<()> {
        loop {
            let n = self.local_buffer.refill(|b| Ok(input.read(b)?))?;
            if n == 0 || self.local_buffer.len() >= min_size {
                break;
            }
        }
        if self.local_buffer.len() < min_size {
            return Err(Error::OutOfBounds(min_size - self.local_buffer.len()));
        }
        Ok(())
    }

    fn advance_to_codestream(&mut self, input: &mut dyn JxlBitstreamInput) -> Result<()> {
        loop {
            match self.state {
                ParseState::Codestream(Some(0)) => self.state = ParseState::BoxNeeded(8),
                ParseState::Complete | ParseState::Codestream(_) => return Ok(()),
                ParseState::SignatureNeeded => {
                    let codestream_signature_len = JxlSignatureType::Codestream.signature().len();
                    self.read_until_at_least(input, codestream_signature_len)?;
                    match check_signature_internal(&self.local_buffer)? {
                        None => return Err(Error::InvalidSignature),
                        Some(JxlSignatureType::Codestream) => {
                            self.state = ParseState::Codestream(None);
                            self.latest_codestream_box = CodestreamBoxType::Jxlc;
                            self.add_checkpoint();
                            return Ok(());
                        }
                        Some(JxlSignatureType::Container) => {
                            let l = JxlSignatureType::Container.signature().len();
                            self.local_buffer.consume(l);
                            self.state = ParseState::BoxNeeded(8);
                        }
                    }
                }
                ParseState::Skip(count) => {
                    if count == Some(0) {
                        self.state = ParseState::BoxNeeded(8);
                        continue;
                    }
                    let to_skip = count.unwrap_or(u64::MAX).min(usize::MAX as u64) as usize;
                    let n = self.skip_inner(input, to_skip)? as u64;
                    if n == 0 {
                        return Err(Error::OutOfBounds(to_skip));
                    }
                    self.state = ParseState::Skip(count.map(|x| x - n));
                }
                ParseState::OOOJxlp(id, count) => {
                    if count == Some(0) {
                        self.state = ParseState::BoxNeeded(8);
                        continue;
                    }
                    // Temporarily take the buffer out of the jxlp state to make the borrow checker happy.
                    let mut buf = self.ooo_jxlp_buffer.remove(&id).unwrap();
                    let mut total = 0;
                    loop {
                        let space = buf.data.len().max(1024).min(input.available_bytes()?) as u64;
                        let space = count.map(|x| (x - total).min(space)).unwrap_or(space) as usize;
                        if space == 0 {
                            break;
                        }
                        buf.data.try_reserve(space)?;
                        let cur = buf.data.len();
                        buf.data.resize(cur + space, 0);
                        let n =
                            self.read_inner(input, &mut [IoSliceMut::new(&mut buf.data[cur..])])?;
                        if n == 0 {
                            break;
                        }
                        total += n as u64;
                    }
                    self.ooo_jxlp_buffer.insert(id, buf);
                    if total == 0 {
                        return Err(Error::OutOfBounds(
                            count.map(|x| x.min(usize::MAX as u64)).unwrap_or(1) as usize,
                        ));
                    }
                    self.state = ParseState::OOOJxlp(id, count.map(|x| x - total));
                }
                ParseState::BoxNeeded(min_size) => self.parse_box(input, min_size)?,
            }
        }
    }

    fn parse_box(&mut self, input: &mut dyn JxlBitstreamInput, required_size: usize) -> Result<()> {
        self.read_until_at_least(input, required_size)?;

        let min_len = match &self.local_buffer[..] {
            [0, 0, 0, 1, ..] => 16,
            _ => 8,
        };

        let ty: [_; 4] = self.local_buffer[4..8].try_into().unwrap();
        if &ty != b"ftyp" && self.version.is_none() {
            // ftyp should be the first box.
            return Err(Error::InvalidBox);
        }

        let extra_len = match &ty {
            b"jxlp" => 4,
            b"ftyp" => 8,
            _ => 0,
        };

        if self.local_buffer.len() < extra_len + min_len {
            self.state = ParseState::BoxNeeded(extra_len + min_len);
            return Ok(());
        }

        let box_len = match &self.local_buffer[..] {
            [0, 0, 0, 1, ..] => u64::from_be_bytes(self.local_buffer[8..16].try_into().unwrap()),
            _ => u32::from_be_bytes(self.local_buffer[0..4].try_into().unwrap()) as u64,
        };

        self.local_buffer.consume(min_len);

        let content_len = if box_len == 0 {
            None
        } else if let Some(d) = box_len.checked_sub((extra_len + min_len) as u64) {
            Some(d)
        } else {
            return Err(Error::InvalidBox);
        };

        match &ty {
            b"ftyp" => {
                if self.version.is_some() {
                    return Err(Error::InvalidBox);
                }
                if &self.local_buffer[0..4] != b"jxl " {
                    return Err(Error::InvalidBox);
                }
                let ver = u32::from_be_bytes(self.local_buffer[4..8].try_into().unwrap());
                if ver > 1 {
                    return Err(Error::InvalidBox);
                }
                self.version = Some(ver);
                self.local_buffer.consume(8);
                self.state = ParseState::Skip(content_len);
            }
            b"jxlc" => {
                if matches!(self.latest_codestream_box, CodestreamBoxType::Jxlp(..)) {
                    return Err(Error::InvalidBox);
                }
                self.latest_codestream_box = CodestreamBoxType::Jxlc;
                self.state = ParseState::Codestream(content_len);
                self.add_checkpoint();
            }
            b"jxlp" => {
                let index = u32::from_be_bytes(self.local_buffer[..4].try_into().unwrap());
                self.local_buffer.consume(4);
                let last = index & 0x80000000 != 0;
                let idx = index & 0x7fffffff;
                let wanted_idx = match self.latest_codestream_box {
                    CodestreamBoxType::Jxlc | CodestreamBoxType::Jxlp(_, true) => {
                        return Err(Error::InvalidBox);
                    }
                    CodestreamBoxType::None => 0,
                    CodestreamBoxType::Jxlp(i, _) => i + 1,
                };
                if idx < wanted_idx {
                    return Err(Error::InvalidBox);
                }
                if idx > wanted_idx {
                    // Out-of-order jxlp
                    if self.version != Some(1) {
                        return Err(Error::InvalidBox);
                    }
                    if self.ooo_jxlp_buffer.contains_key(&idx) {
                        return Err(Error::InvalidBox);
                    }
                    if content_len.is_none() {
                        return Err(Error::InvalidBox);
                    }
                    self.state = ParseState::OOOJxlp(idx, content_len);
                    self.ooo_jxlp_buffer.insert(
                        idx,
                        OOOJxlpBox {
                            data: vec![],
                            consumed: 0,
                            is_last: last,
                        },
                    );
                    // We create a known-invalid checkpoint here to ensure
                    // that the checks work properly, and to count codestream
                    // length correctly.
                    self.add_checkpoint();
                    return Ok(());
                }
                // In-order jxlp
                self.latest_codestream_box = CodestreamBoxType::Jxlp(idx, last);
                self.state = ParseState::Codestream(content_len);
                self.add_checkpoint();
            }
            _ => self.state = ParseState::Skip(content_len),
        }
        Ok(())
    }
}

pub(super) struct CodestreamInput<'a> {
    box_parser: &'a mut BoxParser,
    input: &'a mut dyn JxlBitstreamInput,
}

impl<'a> CodestreamInput<'a> {
    pub(super) fn new(box_parser: &'a mut BoxParser, input: &'a mut dyn JxlBitstreamInput) -> Self {
        Self { box_parser, input }
    }

    pub(super) fn box_parser(&self) -> &BoxParser {
        self.box_parser
    }

    // The methods below have the same semantics as the methods in JxlBitstreamInput.

    pub(super) fn available_bytes(&mut self) -> Result<usize> {
        if let Some(b) = self.box_parser.injected_jxlp() {
            return Ok(b.data.len() - b.consumed);
        }

        self.box_parser.advance_to_codestream(self.input)?;
        let ParseState::Codestream(r) = self.box_parser.state else {
            return Ok(0);
        };
        let available = self.box_parser.available_bytes_inner(self.input)?;
        Ok(r.map(|x| x.min(available as u64) as usize)
            .unwrap_or(available))
    }

    pub(super) fn read(&mut self, buf: &mut [IoSliceMut]) -> Result<usize> {
        if let Some(b) = self.box_parser.injected_jxlp() {
            let mut r = &b.data[b.consumed..];
            let n = r.read_vectored(buf)?;
            b.consumed += n;
            self.box_parser.check_ooo_jxlp_done();
            return Ok(n);
        }

        self.box_parser.advance_to_codestream(self.input)?;
        let ParseState::Codestream(r) = self.box_parser.state else {
            return Ok(0);
        };
        let limit = r.unwrap_or(u64::MAX);
        // If the input buffers have more space than the remaining codestream in the current
        // box, split the read to ensure we don't over-read.
        let n = if buf.iter().map(|x| x.len() as u64).sum::<u64>() <= limit {
            self.box_parser.read_inner(self.input, buf)?
        } else {
            let mut cumsum = 0;
            let mut upper = 0;
            while cumsum + buf[upper].len() as u64 <= limit {
                cumsum += buf[upper].len() as u64;
                upper += 1;
            }
            let n = self.box_parser.read_inner(self.input, &mut buf[..upper])?;
            let left = (limit - n as u64) as usize;
            n + self
                .box_parser
                .read_inner(self.input, &mut [IoSliceMut::new(&mut buf[upper][..left])])?
        };
        self.box_parser.mark_codestream_read(n);
        Ok(n)
    }

    pub(super) fn skip(&mut self, amount: usize) -> Result<usize> {
        if let Some(b) = self.box_parser.injected_jxlp() {
            let n = (b.data.len() - b.consumed).min(amount);
            b.consumed += n;
            self.box_parser.check_ooo_jxlp_done();
            return Ok(n);
        }

        self.box_parser.advance_to_codestream(self.input)?;
        let ParseState::Codestream(r) = self.box_parser.state else {
            return Ok(0);
        };
        let n = self.box_parser.skip_inner(
            self.input,
            r.unwrap_or(u64::MAX).min(amount as u64) as usize,
        )?;
        self.box_parser.mark_codestream_read(n);
        Ok(n)
    }
}

#[cfg(test)]
mod tests {
    use std::io::IoSliceMut;

    use crate::api::inner::box_parser::CodestreamInput;

    use super::BoxParser;

    /// Regression: a zero-length skippable box must not leave the parser stuck at
    /// `SkippableBox(0)` when more container input is available.
    #[test]
    fn zero_length_skippable_box_does_not_hang() {
        let data = include_bytes!("../../../tests/testdata/zero_length_skippable_box.jxl");
        let mut parser = BoxParser::new();
        let mut input = data.as_slice();

        {
            let mut input = CodestreamInput::new(&mut parser, &mut input);
            let mut buf = vec![0; 1024];
            while matches!(input.read(&mut [IoSliceMut::new(&mut buf)]), Ok(x) if x > 0) {}
        }

        // All input should be consumed.
        assert!(input.is_empty());
    }
}
