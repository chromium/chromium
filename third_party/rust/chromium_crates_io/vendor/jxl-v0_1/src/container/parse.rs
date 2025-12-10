// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// Originally written for jxl-oxide.

use super::{BitstreamKind, ContainerParser, DetectState, JxlpIndexState, box_header::*};
use crate::{
    api::{CODESTREAM_SIGNATURE, CONTAINER_SIGNATURE},
    error::{Error, Result},
    util::tracing_wrappers::*,
};

/// Iterator that reads over a buffer and emits parser events.
pub struct ParseEvents<'inner, 'buf> {
    inner: &'inner mut ContainerParser,
    remaining_input: &'buf [u8],
    finished: bool,
}

impl<'inner, 'buf> ParseEvents<'inner, 'buf> {
    pub(super) fn new(parser: &'inner mut ContainerParser, input: &'buf [u8]) -> Self {
        parser.previous_consumed_bytes = 0;
        Self {
            inner: parser,
            remaining_input: input,
            finished: false,
        }
    }

    fn emit_single(&mut self) -> Result<Option<ParseEvent<'buf>>> {
        let state = &mut self.inner.state;
        let jxlp_index_state = &mut self.inner.jxlp_index_state;
        let buf = &mut self.remaining_input;

        loop {
            if buf.is_empty() {
                self.finished = true;
                return Ok(None);
            }

            match state {
                DetectState::WaitingSignature => {
                    if buf.starts_with(&CODESTREAM_SIGNATURE) {
                        info!("Codestream signature found");
                        *state = DetectState::InCodestream {
                            kind: BitstreamKind::BareCodestream,
                            bytes_left: None,
                        };
                        return Ok(Some(ParseEvent::BitstreamKind(
                            BitstreamKind::BareCodestream,
                        )));
                    } else if buf.starts_with(&CONTAINER_SIGNATURE) {
                        info!("Container signature found");
                        *state = DetectState::WaitingBoxHeader;
                        *buf = &buf[CONTAINER_SIGNATURE.len()..];
                        return Ok(Some(ParseEvent::BitstreamKind(BitstreamKind::Container)));
                    } else if !CODESTREAM_SIGNATURE.starts_with(buf)
                        && !CONTAINER_SIGNATURE.starts_with(buf)
                    {
                        warn!(?buf, "Invalid signature");
                        *state = DetectState::InCodestream {
                            kind: BitstreamKind::Invalid,
                            bytes_left: None,
                        };
                        return Ok(Some(ParseEvent::BitstreamKind(BitstreamKind::Invalid)));
                    } else {
                        return Ok(None);
                    }
                }
                DetectState::WaitingBoxHeader => match ContainerBoxHeader::parse(buf)? {
                    HeaderParseResult::Done {
                        header,
                        header_size,
                    } => {
                        *buf = &buf[header_size..];
                        let tbox = header.box_type();
                        if tbox == ContainerBoxType::CODESTREAM {
                            match jxlp_index_state {
                                JxlpIndexState::Initial => {
                                    *jxlp_index_state = JxlpIndexState::SingleJxlc;
                                }
                                JxlpIndexState::SingleJxlc => {
                                    warn!("Duplicate jxlc box found");
                                    return Err(Error::InvalidBox);
                                }
                                JxlpIndexState::Jxlp(_) | JxlpIndexState::JxlpFinished => {
                                    warn!("Found jxlc box instead of jxlp box");
                                    return Err(Error::InvalidBox);
                                }
                            }

                            *state = DetectState::InCodestream {
                                kind: BitstreamKind::Container,
                                bytes_left: header.box_size().map(|x| x as usize),
                            };
                        } else if tbox == ContainerBoxType::PARTIAL_CODESTREAM {
                            if let Some(box_size) = header.box_size()
                                && box_size < 4
                            {
                                return Err(Error::InvalidBox);
                            }

                            match jxlp_index_state {
                                JxlpIndexState::Initial => {
                                    *jxlp_index_state = JxlpIndexState::Jxlp(0);
                                }
                                JxlpIndexState::Jxlp(index) => {
                                    *index += 1;
                                }
                                JxlpIndexState::SingleJxlc => {
                                    warn!("jxlp box found after jxlc box");
                                    return Err(Error::InvalidBox);
                                }
                                JxlpIndexState::JxlpFinished => {
                                    warn!("found another jxlp box after the final one");
                                    return Err(Error::InvalidBox);
                                }
                            }

                            *state = DetectState::WaitingJxlpIndex(header);
                        } else {
                            let bytes_left = header.box_size().map(|x| x as usize);
                            *state = DetectState::InAuxBox { header, bytes_left };
                        }
                    }
                    HeaderParseResult::NeedMoreData => return Ok(None),
                },
                DetectState::WaitingJxlpIndex(header) => {
                    let &[b0, b1, b2, b3, ..] = &**buf else {
                        return Ok(None);
                    };

                    let index = u32::from_be_bytes([b0, b1, b2, b3]);
                    *buf = &buf[4..];
                    let is_last = index & 0x80000000 != 0;
                    let index = index & 0x7fffffff;

                    match *jxlp_index_state {
                        JxlpIndexState::Jxlp(expected_index) if expected_index == index => {
                            if is_last {
                                *jxlp_index_state = JxlpIndexState::JxlpFinished;
                            }
                        }
                        #[allow(unused_variables)]
                        JxlpIndexState::Jxlp(expected_index) => {
                            warn!(
                                expected_index,
                                actual_index = index,
                                "Out-of-order jxlp box found",
                            );
                            return Err(Error::InvalidBox);
                        }
                        state => {
                            unreachable!("invalid jxlp index state in WaitingJxlpIndex: {state:?}");
                        }
                    }

                    *state = DetectState::InCodestream {
                        kind: BitstreamKind::Container,
                        bytes_left: header.box_size().map(|x| x as usize - 4),
                    };
                }
                DetectState::InCodestream {
                    bytes_left: None, ..
                } => {
                    let payload = *buf;
                    *buf = &[];
                    return Ok(Some(ParseEvent::Codestream(payload)));
                }
                DetectState::InCodestream {
                    bytes_left: Some(bytes_left),
                    ..
                } => {
                    let payload = if buf.len() >= *bytes_left {
                        let (payload, remaining) = buf.split_at(*bytes_left);
                        *state = DetectState::WaitingBoxHeader;
                        *buf = remaining;
                        payload
                    } else {
                        let payload = *buf;
                        *bytes_left -= buf.len();
                        *buf = &[];
                        payload
                    };
                    return Ok(Some(ParseEvent::Codestream(payload)));
                }
                DetectState::InAuxBox {
                    header: _,
                    bytes_left: None,
                } => {
                    let _payload = *buf;
                    *buf = &[];
                    // FIXME: emit auxiliary box event
                }
                DetectState::InAuxBox {
                    header: _,
                    bytes_left: Some(bytes_left),
                } => {
                    let _payload = if buf.len() >= *bytes_left {
                        let (payload, remaining) = buf.split_at(*bytes_left);
                        *state = DetectState::WaitingBoxHeader;
                        *buf = remaining;
                        payload
                    } else {
                        let payload = *buf;
                        *bytes_left -= buf.len();
                        *buf = &[];
                        payload
                    };
                    // FIXME: emit auxiliary box event
                }
            }
        }
    }
}

impl std::fmt::Debug for ParseEvents<'_, '_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ParseEvents")
            .field("inner", &self.inner)
            .field(
                "remaining_input",
                &format_args!("({} byte(s))", self.remaining_input.len()),
            )
            .field("finished", &self.finished)
            .finish()
    }
}

impl<'buf> Iterator for ParseEvents<'_, 'buf> {
    type Item = Result<ParseEvent<'buf>>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.finished {
            return None;
        }

        let initial_buf = self.remaining_input;
        let event = self.emit_single();

        if event.is_err() {
            self.finished = true;
        }

        self.inner.previous_consumed_bytes += initial_buf.len() - self.remaining_input.len();
        event.transpose()
    }
}

/// Parser event emitted by [`ParseEvents`].
pub enum ParseEvent<'buf> {
    /// Bitstream structure is detected.
    BitstreamKind(BitstreamKind),
    /// Codestream data is read.
    ///
    /// Returned data may be partial. Complete codestream can be obtained by concatenating all data
    /// of `Codestream` events.
    Codestream(&'buf [u8]),
}

impl std::fmt::Debug for ParseEvent<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::BitstreamKind(kind) => f.debug_tuple("BitstreamKind").field(kind).finish(),
            Self::Codestream(buf) => f
                .debug_tuple("Codestream")
                .field(&format_args!("{} byte(s)", buf.len()))
                .finish(),
        }
    }
}
