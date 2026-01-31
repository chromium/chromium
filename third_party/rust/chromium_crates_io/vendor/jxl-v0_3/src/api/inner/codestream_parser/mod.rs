// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    collections::{HashSet, VecDeque},
    io::IoSliceMut,
};

use sections::SectionState;

#[cfg(test)]
use crate::api::FrameCallback;
use crate::{
    api::{
        JxlBasicInfo, JxlBitstreamInput, JxlColorProfile, JxlDecoderOptions, JxlOutputBuffer,
        JxlPixelFormat,
        inner::{box_parser::BoxParser, process::SmallBuffer},
    },
    error::{Error, Result},
    frame::{DecoderState, Frame, Section},
    headers::{Animation, FileHeader, frame_header::FrameHeader, toc::IncrementalTocReader},
    icc::IncrementalIccReader,
};

mod non_section;
mod sections;

struct SectionBuffer {
    len: usize,
    data: Vec<u8>,
    section: Section,
}

pub(super) struct CodestreamParser {
    // TODO(veluca): this would probably be cleaner with some kind of state enum.
    pub(super) file_header: Option<FileHeader>,
    icc_parser: Option<IncrementalIccReader>,
    // These fields are populated once image information is available.
    decoder_state: Option<DecoderState>,
    pub(super) basic_info: Option<JxlBasicInfo>,
    pub(super) animation: Option<Animation>,
    pub(super) embedded_color_profile: Option<JxlColorProfile>,
    pub(super) output_color_profile: Option<JxlColorProfile>,
    pub(super) pixel_format: Option<JxlPixelFormat>,

    // These fields are populated when starting to decode a frame, and cleared once
    // the frame is done.
    frame_header: Option<FrameHeader>,
    toc_parser: Option<IncrementalTocReader>,
    pub(super) frame: Option<Frame>,

    // Buffers.
    non_section_buf: SmallBuffer,
    non_section_bit_offset: u8,
    sections: VecDeque<SectionBuffer>,
    ready_section_data: usize,
    skip_sections: bool,
    // True when we need to process frames without copying them to output buffers, e.g. reference frames
    process_without_output: bool,
    // True once the preview frame has been processed (if there is one)
    preview_done: bool,
    // Saved file header for recreating decoder state after preview frame
    saved_file_header: Option<crate::headers::FileHeader>,

    section_state: SectionState,

    // Or only section if in single section special case.
    lf_global_section: Option<SectionBuffer>,
    lf_sections: Vec<SectionBuffer>,
    hf_global_section: Option<SectionBuffer>,
    // indexed by group, then by pass.
    hf_sections: Vec<Vec<Option<SectionBuffer>>>,
    // group indices that *might* have new renderable data.
    candidate_hf_sections: HashSet<usize>,

    pub(super) has_more_frames: bool,

    header_needed_bytes: Option<u64>,

    #[cfg(test)]
    pub frame_callback: Option<Box<FrameCallback>>,
    #[cfg(test)]
    pub decoded_frames: usize,
}

impl CodestreamParser {
    pub(super) fn new() -> Self {
        Self {
            file_header: None,
            icc_parser: None,
            decoder_state: None,
            basic_info: None,
            animation: None,
            embedded_color_profile: None,
            output_color_profile: None,
            pixel_format: None,
            frame_header: None,
            toc_parser: None,
            frame: None,
            non_section_buf: SmallBuffer::new(4096),
            non_section_bit_offset: 0,
            sections: VecDeque::new(),
            ready_section_data: 0,
            skip_sections: false,
            process_without_output: false,
            preview_done: false,
            saved_file_header: None,
            section_state: SectionState::new(0, 0),
            lf_global_section: None,
            lf_sections: vec![],
            hf_global_section: None,
            hf_sections: vec![],
            candidate_hf_sections: HashSet::new(),
            has_more_frames: true,
            header_needed_bytes: None,
            #[cfg(test)]
            frame_callback: None,
            #[cfg(test)]
            decoded_frames: 0,
        }
    }

    fn has_visible_frame(&self) -> bool {
        if let Some(frame) = &self.frame {
            frame.header().is_visible()
        } else {
            false
        }
    }

    /// Returns the number of passes that are fully completed across all groups.
    pub(super) fn num_completed_passes(&self) -> usize {
        self.section_state.num_completed_passes()
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.decoder_state
            .as_mut()
            .unwrap()
            .set_use_simple_pipeline(u);
    }

    /// Rewinds for animation loop replay, keeping pixel_format setting.
    pub(super) fn rewind(&mut self) -> Option<JxlPixelFormat> {
        let pixel_format = self.pixel_format.take();
        *self = Self::new();
        self.pixel_format = pixel_format.clone();
        pixel_format
    }

    pub(super) fn process(
        &mut self,
        box_parser: &mut BoxParser,
        input: &mut dyn JxlBitstreamInput,
        decode_options: &JxlDecoderOptions,
        mut output_buffers: Option<&mut [JxlOutputBuffer]>,
    ) -> Result<()> {
        if let Some(output_buffers) = &output_buffers {
            let px = self.pixel_format.as_ref().unwrap();
            let expected_len = std::iter::once(&px.color_data_format)
                .chain(px.extra_channel_format.iter())
                .filter(|x| x.is_some())
                .count();
            if output_buffers.len() != expected_len {
                return Err(Error::WrongBufferCount(output_buffers.len(), expected_len));
            }
        }
        // If we have sections to read, read into sections; otherwise, read into the local buffer.
        loop {
            if !self.sections.is_empty() {
                let regular_frame = self.has_visible_frame();
                // Only skip sections if we don't need the frame data. Frames that can be
                // referenced must be decoded because they serve as sources for patches,
                // blending, or frame extension in subsequent frames.
                let can_be_referenced = self
                    .frame
                    .as_ref()
                    .is_some_and(|f| f.header().can_be_referenced);
                if !self.process_without_output && output_buffers.is_none() && !can_be_referenced {
                    self.skip_sections = true;
                }

                if !self.skip_sections {
                    // This is just an estimate as there could be box bytes in the middle.
                    let mut readable_section_data = (self.non_section_buf.len()
                        + input.available_bytes()?
                        + self.ready_section_data)
                        .max(1);
                    // Ensure enough section buffers are available for reading available data.
                    for buf in self.sections.iter_mut() {
                        if buf.data.is_empty() {
                            buf.data.resize(buf.len, 0);
                        }
                        readable_section_data =
                            readable_section_data.saturating_sub(buf.data.len());
                        if readable_section_data == 0 {
                            break;
                        }
                    }
                    // Read sections up to the end of the current box.
                    let mut available_codestream = match box_parser.get_more_codestream(input) {
                        Err(Error::OutOfBounds(_)) => 0,
                        Ok(c) => c as usize,
                        Err(e) => return Err(e),
                    };
                    let mut section_buffers = vec![];
                    let mut ready = self.ready_section_data;
                    for buf in self.sections.iter_mut() {
                        if buf.data.is_empty() {
                            break;
                        }
                        let len = buf.data.len();
                        if len > ready {
                            let readable = (available_codestream + ready).min(len);
                            section_buffers.push(IoSliceMut::new(&mut buf.data[ready..readable]));
                            available_codestream =
                                available_codestream.saturating_sub(readable - ready);
                            if available_codestream == 0 {
                                break;
                            }
                        }
                        ready = ready.saturating_sub(len);
                    }
                    let mut buffers = &mut section_buffers[..];
                    loop {
                        let num = if !box_parser.box_buffer.is_empty() {
                            box_parser.box_buffer.take(buffers)
                        } else {
                            input.read(buffers)?
                        };
                        self.ready_section_data += num;
                        box_parser.consume_codestream(num as u64);
                        IoSliceMut::advance_slices(&mut buffers, num);
                        if num == 0 || buffers.is_empty() {
                            break;
                        }
                    }
                    match self.process_sections(decode_options, &mut output_buffers) {
                        Ok(None) => Ok(()),
                        Ok(Some(missing)) => Err(Error::OutOfBounds(missing)),
                        Err(Error::OutOfBounds(_)) => Err(Error::SectionTooShort),
                        Err(err) => Err(err),
                    }?;
                } else {
                    let total_size = self.sections.iter().map(|x| x.len).sum::<usize>();
                    loop {
                        let to_skip = total_size - self.ready_section_data;
                        if to_skip == 0 {
                            break;
                        }
                        let available_codestream = box_parser.get_more_codestream(input)? as usize;
                        let to_skip = to_skip.min(available_codestream);
                        let skipped = if !box_parser.box_buffer.is_empty() {
                            box_parser.box_buffer.consume(to_skip)
                        } else {
                            input.skip(to_skip)?
                        };
                        box_parser.consume_codestream(skipped as u64);
                        self.ready_section_data += skipped;
                        if skipped == 0 {
                            break;
                        }
                    }
                    if self.ready_section_data < total_size {
                        return Err(Error::OutOfBounds(total_size - self.ready_section_data));
                    } else {
                        self.sections.clear();
                        // Finalize the skipped frame, mirroring what process_sections does
                        let frame = self
                            .frame
                            .take()
                            .expect("frame must be set when skip_sections is true");
                        if let Some(decoder_state) = frame.finalize()? {
                            self.decoder_state = Some(decoder_state);
                        } else {
                            self.has_more_frames = false;
                        }
                        self.skip_sections = false;
                    }
                }
                if self.sections.is_empty() {
                    // Go back to parsing a new frame header, if any.
                    // Only return if this was a regular visible frame that was actually decoded
                    // (not a frame we were skipping like a preview frame)
                    let was_skipping = self.process_without_output;
                    self.process_without_output = false;
                    if regular_frame && !was_skipping {
                        return Ok(());
                    }
                    continue;
                }
            } else {
                // Trying to read a frame or a file header.
                assert!(self.frame.is_none());
                assert!(self.has_more_frames);

                // Loop to handle incremental parsing (e.g. large ICC profiles) that may need
                // multiple buffer refills to complete.
                loop {
                    let available_codestream = match box_parser.get_more_codestream(input) {
                        Err(Error::OutOfBounds(_)) => 0,
                        Ok(c) => c as usize,
                        Err(e) => return Err(e),
                    };
                    let c = self.non_section_buf.refill(
                        |buf| {
                            if !box_parser.box_buffer.is_empty() {
                                Ok(box_parser.box_buffer.take(buf))
                            } else {
                                input.read(buf)
                            }
                        },
                        Some(available_codestream),
                    )? as u64;
                    box_parser.consume_codestream(c);

                    // If we know that non-section parsing will require more bytes than what
                    // we added to the codestream, don't even try to parse non-section data.
                    if let Some(needed) = self.header_needed_bytes.as_mut() {
                        *needed = needed.saturating_sub(c);
                        if *needed > 0 {
                            if !self.non_section_buf.can_read_more() {
                                self.non_section_buf.enlarge();
                            }
                            // Check if input still has data - if so, refill and retry
                            if input.available_bytes().unwrap_or(0) > 0 {
                                continue;
                            } else {
                                return Err(Error::OutOfBounds(*needed as usize));
                            }
                        }
                    }

                    let range = self.non_section_buf.range();

                    match self.process_non_section(decode_options) {
                        Ok(()) => {
                            self.header_needed_bytes = None;
                            break;
                        }
                        Err(Error::OutOfBounds(n)) => {
                            let new_range = self.non_section_buf.range();
                            // If non-section parsing consumed no bytes, and the non-section buffer
                            // cannot accept more bytes, enlarge the buffer to allow to make progress.
                            if new_range == range && !self.non_section_buf.can_read_more() {
                                self.non_section_buf.enlarge();
                            }
                            self.header_needed_bytes = Some(n as u64);
                            // Check if input still has data - if so, refill and retry
                            if input.available_bytes().unwrap_or(0) > 0 {
                                continue;
                            } else {
                                return Err(Error::OutOfBounds(n));
                            }
                        }
                        Err(e) => return Err(e),
                    }
                }

                if self.decoder_state.is_some() && self.frame_header.is_none() {
                    // Return to caller if we found image info.
                    return Ok(());
                }
                if self.frame.is_some() {
                    // Check if this is a preview frame that should be skipped
                    let is_preview_frame = !self.preview_done
                        && self
                            .basic_info
                            .as_ref()
                            .is_some_and(|info| info.preview_size.is_some());
                    if is_preview_frame {
                        self.preview_done = true;
                        if decode_options.skip_preview {
                            self.process_without_output = true;
                            continue;
                        }
                    }

                    if self.has_visible_frame() {
                        // Return to caller if we found visible frame info.
                        return Ok(());
                    } else {
                        self.process_without_output = true;
                        continue;
                    }
                }
            }
        }
    }
}
