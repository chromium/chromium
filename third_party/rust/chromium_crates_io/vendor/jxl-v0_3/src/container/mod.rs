// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// Originally written for jxl-oxide.

pub mod box_header;
pub mod parse;

use box_header::*;
pub use parse::ParseEvent;
use parse::*;

/// Container format parser.
#[derive(Debug, Default)]
pub struct ContainerParser {
    state: DetectState,
    jxlp_index_state: JxlpIndexState,
    previous_consumed_bytes: usize,
}

#[derive(Debug, Default)]
enum DetectState {
    #[default]
    WaitingSignature,
    WaitingBoxHeader,
    WaitingJxlpIndex(ContainerBoxHeader),
    InAuxBox {
        #[allow(unused)]
        header: ContainerBoxHeader,
        bytes_left: Option<usize>,
    },
    InCodestream {
        kind: BitstreamKind,
        bytes_left: Option<usize>,
    },
}

/// Structure of the decoded bitstream.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum BitstreamKind {
    /// Decoder can't determine structure of the bitstream.
    Unknown,
    /// Bitstream is a direct JPEG XL codestream without box structure.
    BareCodestream,
    /// Bitstream is a JPEG XL container with box structure.
    Container,
    /// Bitstream is not a valid JPEG XL image.
    Invalid,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Default)]
enum JxlpIndexState {
    #[default]
    Initial,
    SingleJxlc,
    Jxlp(u32),
    JxlpFinished,
}

impl ContainerParser {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn kind(&self) -> BitstreamKind {
        match self.state {
            DetectState::WaitingSignature => BitstreamKind::Unknown,
            DetectState::WaitingBoxHeader
            | DetectState::WaitingJxlpIndex(..)
            | DetectState::InAuxBox { .. } => BitstreamKind::Container,
            DetectState::InCodestream { kind, .. } => kind,
        }
    }

    /// Parses input buffer and generates parser events.
    ///
    /// The parser might not fully consume the buffer. Use [`previous_consumed_bytes`] to get how
    /// many bytes are consumed. Bytes not consumed by the parser should be processed again.
    ///
    /// [`previous_consumed_bytes`]: ContainerParser::previous_consumed_bytes
    pub fn process_bytes<'inner, 'buf>(
        &'inner mut self,
        input: &'buf [u8],
    ) -> ParseEvents<'inner, 'buf> {
        ParseEvents::new(self, input)
    }

    /// Get how many bytes are consumed by the previous call to [`process_bytes`].
    ///
    /// Bytes not consumed by the parser should be processed again.
    ///
    /// [`process_bytes`]: ContainerParser::process_bytes
    pub fn previous_consumed_bytes(&self) -> usize {
        self.previous_consumed_bytes
    }
}

#[cfg(test)]
impl ContainerParser {
    pub(crate) fn collect_codestream(input: &[u8]) -> crate::error::Result<Vec<u8>> {
        let mut parser = Self::new();
        let mut codestream = Vec::new();
        for event in parser.process_bytes(input) {
            match event? {
                ParseEvent::BitstreamKind(_) => {}
                ParseEvent::Codestream(buf) => {
                    codestream.extend_from_slice(buf);
                }
            }
        }
        Ok(codestream)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use test_log::test;

    #[rustfmt::skip]
    const HEADER: &[u8] = &[
        0x00, 0x00, 0x00, 0x0c, b'J', b'X', b'L', b' ', 0x0d, 0x0a, 0x87, 0x0a, 0x00, 0x00, 0x00, 0x14,
        b'f', b't', b'y', b'p', b'j', b'x', b'l', b' ', 0x00, 0x00, 0x00, 0x00, b'j', b'x', b'l', b' ',
    ];

    #[test]
    fn parse_partial() {
        arbtest::arbtest(|u| {
            // Prepare arbitrary container format data with two jxlp boxes.
            let total_len = u.arbitrary_len::<u8>()?;
            let mut codestream0 = vec![0u8; total_len / 2];
            u.fill_buffer(&mut codestream0)?;
            let mut codestream1 = vec![0u8; total_len - codestream0.len()];
            u.fill_buffer(&mut codestream1)?;

            let mut container = HEADER.to_vec();
            container.extend_from_slice(&(12 + codestream0.len() as u32).to_be_bytes());
            container.extend_from_slice(b"jxlp\x00\x00\x00\x00");
            container.extend_from_slice(&codestream0);

            container.extend_from_slice(&(12 + codestream1.len() as u32).to_be_bytes());
            container.extend_from_slice(b"jxlp\x80\x00\x00\x01");
            container.extend_from_slice(&codestream1);

            let mut expected = codestream0;
            expected.extend(codestream1);

            // Create a list of arbitrary splits.
            let mut tests = Vec::new();
            u.arbitrary_loop(Some(1), Some(10), |u| {
                let split_at_idx = u.choose_index(container.len())?;
                tests.push(container.split_at(split_at_idx));
                Ok(std::ops::ControlFlow::Continue(()))
            })?;

            // Test if split index doesn't affect final codestream.
            for (first, second) in tests {
                let mut codestream = Vec::new();
                let mut parser = ContainerParser::new();

                for event in parser.process_bytes(first) {
                    let event = event.unwrap();
                    if let ParseEvent::Codestream(data) = event {
                        codestream.extend_from_slice(data);
                    }
                }

                let consumed = parser.previous_consumed_bytes();
                let mut second_chunk = first[consumed..].to_vec();
                second_chunk.extend_from_slice(second);

                for event in parser.process_bytes(&second_chunk) {
                    let event = event.unwrap();
                    if let ParseEvent::Codestream(data) = event {
                        codestream.extend_from_slice(data);
                    }
                }

                assert_eq!(codestream, expected);
            }

            Ok(())
        });
    }
}
