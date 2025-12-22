// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::error::{Error, Result};

use crate::api::{
    JxlBitstreamInput, JxlSignatureType, check_signature_internal, inner::process::SmallBuffer,
};

#[derive(Clone)]
enum ParseState {
    SignatureNeeded,
    BoxNeeded,
    CodestreamBox(u64),
    SkippableBox(u64),
}

enum CodestreamBoxType {
    None,
    Jxlc,
    Jxlp(u32),
    LastJxlp,
}

pub(super) struct BoxParser {
    pub(super) box_buffer: SmallBuffer,
    state: ParseState,
    box_type: CodestreamBoxType,
}

impl BoxParser {
    pub(super) fn new() -> Self {
        BoxParser {
            box_buffer: SmallBuffer::new(128),
            state: ParseState::SignatureNeeded,
            box_type: CodestreamBoxType::None,
        }
    }

    // Reads input until the next byte of codestream is available.
    // This function might over-read bytes. Thus, the contents of self.box_buffer should always be
    // read after this function call.
    // Returns the number of codestream bytes that will be available to be read after this call,
    // including any bytes in self.box_buffer.
    // Might return `u64::MAX`, indicating that the rest of the file is codestream.
    pub(super) fn get_more_codestream(&mut self, input: &mut dyn JxlBitstreamInput) -> Result<u64> {
        loop {
            match self.state.clone() {
                ParseState::SignatureNeeded => {
                    self.box_buffer.refill(|b| input.read(b), None)?;
                    match check_signature_internal(&self.box_buffer)? {
                        None => return Err(Error::InvalidSignature),
                        Some(JxlSignatureType::Codestream) => {
                            self.state = ParseState::CodestreamBox(u64::MAX);
                            return Ok(u64::MAX);
                        }
                        Some(JxlSignatureType::Container) => {
                            self.box_buffer
                                .consume(JxlSignatureType::Container.signature().len());
                            self.state = ParseState::BoxNeeded;
                        }
                    }
                }
                ParseState::CodestreamBox(b) => {
                    return Ok(b);
                }
                ParseState::SkippableBox(mut s) => {
                    let num = s.min(usize::MAX as u64) as usize;
                    let skipped = if !self.box_buffer.is_empty() {
                        self.box_buffer.consume(num)
                    } else {
                        input.skip(num)?
                    };
                    if skipped == 0 {
                        return Err(Error::OutOfBounds(num));
                    }
                    s -= skipped as u64;
                    if s == 0 {
                        self.state = ParseState::BoxNeeded;
                    } else {
                        self.state = ParseState::SkippableBox(s);
                    }
                }
                ParseState::BoxNeeded => {
                    self.box_buffer.refill(|b| input.read(b), None)?;
                    let min_len = match &self.box_buffer[..] {
                        [0, 0, 0, 1, ..] => 16,
                        _ => 8,
                    };
                    if self.box_buffer.len() <= min_len {
                        return Err(Error::OutOfBounds(min_len - self.box_buffer.len()));
                    }
                    let ty: [_; 4] = self.box_buffer[4..8].try_into().unwrap();
                    let extra_len = if &ty == b"jxlp" { 4 } else { 0 };
                    if self.box_buffer.len() <= min_len + extra_len {
                        return Err(Error::OutOfBounds(
                            min_len + extra_len - self.box_buffer.len(),
                        ));
                    }
                    let box_len = match &self.box_buffer[..] {
                        [0, 0, 0, 1, ..] => {
                            u64::from_be_bytes(self.box_buffer[8..16].try_into().unwrap())
                        }
                        _ => u32::from_be_bytes(self.box_buffer[0..4].try_into().unwrap()) as u64,
                    };
                    // Per JXL spec: jxlc box with length 0 has special meaning "extends to EOF"
                    let content_len = if box_len == 0 && (&ty == b"jxlp" || &ty == b"jxlc") {
                        u64::MAX
                    } else {
                        if box_len <= (min_len + extra_len) as u64 {
                            return Err(Error::InvalidBox);
                        }
                        box_len - min_len as u64 - extra_len as u64
                    };
                    match &ty {
                        b"jxlc" => {
                            if matches!(
                                self.box_type,
                                CodestreamBoxType::Jxlp(..) | CodestreamBoxType::LastJxlp
                            ) {
                                return Err(Error::InvalidBox);
                            }
                            self.box_type = CodestreamBoxType::Jxlc;
                            self.state = ParseState::CodestreamBox(content_len);
                        }
                        b"jxlp" => {
                            let index = u32::from_be_bytes(
                                self.box_buffer[min_len..min_len + 4].try_into().unwrap(),
                            );
                            let wanted_idx = match self.box_type {
                                CodestreamBoxType::Jxlc | CodestreamBoxType::LastJxlp => {
                                    return Err(Error::InvalidBox);
                                }
                                CodestreamBoxType::None => 0,
                                CodestreamBoxType::Jxlp(i) => i + 1,
                            };
                            let last = index & 0x80000000 != 0;
                            let idx = index & 0x7fffffff;
                            if idx != wanted_idx {
                                return Err(Error::InvalidBox);
                            }
                            self.box_type = if last {
                                CodestreamBoxType::LastJxlp
                            } else {
                                CodestreamBoxType::Jxlp(idx)
                            };
                            self.state = ParseState::CodestreamBox(content_len);
                        }
                        _ => {
                            self.state = ParseState::SkippableBox(content_len);
                        }
                    }
                    self.box_buffer.consume(min_len + extra_len);
                }
            }
        }
    }

    pub(super) fn consume_codestream(&mut self, amount: u64) {
        if let ParseState::CodestreamBox(cb) = &mut self.state {
            *cb = cb.checked_sub(amount).unwrap();
            if *cb == 0 {
                self.state = ParseState::BoxNeeded;
            }
        } else if amount != 0 {
            unreachable!()
        }
    }
}
