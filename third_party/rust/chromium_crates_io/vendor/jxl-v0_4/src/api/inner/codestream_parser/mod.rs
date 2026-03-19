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
        JxlBasicInfo, JxlBitstreamInput, JxlColorEncoding, JxlColorProfile, JxlDataFormat,
        JxlDecoderOptions, JxlOutputBuffer, JxlPixelFormat, VisibleFrameInfo,
        VisibleFrameSeekTarget,
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

#[derive(Clone, Copy)]
struct FrameStartInfo {
    file_offset: usize,
    remaining_in_box: u64,
    visible_count_before: usize,
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
    xyb_encoded: bool,
    is_gray: bool,
    pub(super) output_color_profile_set_by_user: bool,

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
    /// Number of visible frames still to skip before returning to the caller.
    /// Set via `start_new_frame` when seeking to a non-keyframe.
    visible_frames_to_skip: usize,
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

    // --- Frame info tracking (for frame scanning) ---
    /// Collected visible frame info entries.
    pub(super) scanned_frames: Vec<VisibleFrameInfo>,
    /// Zero-based visible frame index counter.
    visible_frame_index: usize,
    /// File offsets and visibility info for every non-preview frame (visible
    /// and non-visible), in parse order.
    frame_starts: Vec<FrameStartInfo>,
    /// For each reference slot, earliest frame index required to reconstruct
    /// the current contents of that slot.
    reference_slot_decode_start: [Option<usize>; DecoderState::MAX_STORED_FRAMES],
    /// For each LF slot, earliest frame index required to reconstruct the
    /// current contents of that slot.
    lf_slot_decode_start: [Option<usize>; DecoderState::NUM_LF_FRAMES],
    /// File byte offset where the current frame header parse started.
    /// Set when we begin parsing a frame header.
    current_frame_file_offset: usize,
    /// Remaining codestream bytes in the current box at frame start.
    /// Captured alongside `current_frame_file_offset`.
    current_frame_remaining_in_box: u64,

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
            xyb_encoded: false,
            is_gray: false,
            output_color_profile_set_by_user: false,
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
            visible_frames_to_skip: 0,
            saved_file_header: None,
            section_state: SectionState::new(0, 0),
            lf_global_section: None,
            lf_sections: vec![],
            hf_global_section: None,
            hf_sections: vec![],
            candidate_hf_sections: HashSet::new(),
            has_more_frames: true,
            header_needed_bytes: None,
            scanned_frames: Vec::new(),
            visible_frame_index: 0,
            frame_starts: Vec::new(),
            reference_slot_decode_start: [None; DecoderState::MAX_STORED_FRAMES],
            lf_slot_decode_start: [None; DecoderState::NUM_LF_FRAMES],
            current_frame_file_offset: 0,
            current_frame_remaining_in_box: u64::MAX,
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

    /// Record frame info for the just-parsed frame.
    /// Called after process_non_section() creates a Frame, for frame scanning.
    fn record_frame_info(&mut self) {
        let frame = match self.frame.as_ref() {
            Some(f) => f,
            None => return,
        };
        let header = frame.header();

        let current_frame_index = self.frame_starts.len();
        let is_visible = header.is_visible();
        self.frame_starts.push(FrameStartInfo {
            file_offset: self.current_frame_file_offset,
            remaining_in_box: self.current_frame_remaining_in_box,
            visible_count_before: self.visible_frame_index,
        });

        let mut decode_start_frame_index = current_frame_index;

        // Track frame dependencies through reference slots. For blending we know
        // exactly which slots are used. For patches we conservatively assume any
        // reference slot may be used.
        let mut used_reference_slots = [false; DecoderState::MAX_STORED_FRAMES];
        if header.needs_blending() {
            for blending_info in header
                .ec_blending_info
                .iter()
                .chain(std::iter::once(&header.blending_info))
            {
                let source = blending_info.source as usize;
                assert!(
                    source < DecoderState::MAX_STORED_FRAMES,
                    "invalid blending source slot {source}, max {}",
                    DecoderState::MAX_STORED_FRAMES - 1
                );
                used_reference_slots[source] = true;
            }
        }
        if header.has_patches() {
            used_reference_slots.fill(true);
        }

        for (slot, used) in used_reference_slots.iter().enumerate() {
            if *used && let Some(dep_start) = self.reference_slot_decode_start[slot] {
                decode_start_frame_index = decode_start_frame_index.min(dep_start);
            }
        }

        if header.has_lf_frame() {
            let lf_slot = header.lf_level as usize;
            assert!(
                lf_slot < DecoderState::NUM_LF_FRAMES,
                "invalid lf slot {lf_slot}, max {}",
                DecoderState::NUM_LF_FRAMES - 1
            );
            if let Some(dep_start) = self.lf_slot_decode_start[lf_slot] {
                decode_start_frame_index = decode_start_frame_index.min(dep_start);
            }
        }

        if is_visible {
            let duration_ticks = header.duration;
            let duration_ms = if let Some(ref anim) = self.animation {
                if anim.tps_numerator > 0 {
                    (duration_ticks as f64) * 1000.0 * (anim.tps_denominator as f64)
                        / (anim.tps_numerator as f64)
                } else {
                    0.0
                }
            } else {
                0.0
            };

            let decode_start = self.frame_starts[decode_start_frame_index];
            let seek_target = VisibleFrameSeekTarget {
                decode_start_file_offset: decode_start.file_offset,
                remaining_in_box: decode_start.remaining_in_box,
                visible_frames_to_skip: self
                    .visible_frame_index
                    .saturating_sub(decode_start.visible_count_before),
            };
            let is_keyframe = seek_target.visible_frames_to_skip == 0;

            self.scanned_frames.push(VisibleFrameInfo {
                index: self.visible_frame_index,
                duration_ms,
                duration_ticks,
                file_offset: self.current_frame_file_offset,
                is_last: header.is_last,
                is_keyframe,
                seek_target,
                name: header.name.clone(),
            });

            self.visible_frame_index += 1;
        }

        // Update slot dependency origins after processing this frame.
        if header.can_be_referenced {
            let slot = header.save_as_reference as usize;
            assert!(
                slot < DecoderState::MAX_STORED_FRAMES,
                "invalid save_as_reference slot {slot}, max {}",
                DecoderState::MAX_STORED_FRAMES - 1
            );
            self.reference_slot_decode_start[slot] = Some(decode_start_frame_index);
        }

        if header.lf_level != 0 {
            let slot = (header.lf_level - 1) as usize;
            assert!(
                slot < DecoderState::NUM_LF_FRAMES,
                "invalid lf save slot {slot}, max {}",
                DecoderState::NUM_LF_FRAMES - 1
            );
            self.lf_slot_decode_start[slot] = Some(decode_start_frame_index);
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

    /// Resets frame-level state for seeking to a new frame.
    ///
    /// Preserves: file_header, decoder_state (including reference frames),
    /// basic_info, animation, color profiles, pixel_format, xyb_encoded,
    /// is_gray, output_color_profile_set_by_user, preview_done.
    ///
    /// Clears: frame_header, toc_parser, frame, all section buffers,
    /// non_section_buf, and processing flags.
    pub(super) fn start_new_frame(&mut self, visible_frames_to_skip: usize) {
        self.frame_header = None;
        self.toc_parser = None;
        self.frame = None;
        self.non_section_buf = SmallBuffer::new(4096);
        self.non_section_bit_offset = 0;
        self.sections.clear();
        self.ready_section_data = 0;
        self.skip_sections = false;
        self.process_without_output = false;
        self.visible_frames_to_skip = visible_frames_to_skip;
        self.section_state = SectionState::new(0, 0);
        self.lf_global_section = None;
        self.lf_sections.clear();
        self.hf_global_section = None;
        self.hf_sections.clear();
        self.candidate_hf_sections.clear();
        self.has_more_frames = true;
        self.header_needed_bytes = None;
    }

    pub(super) fn process(
        &mut self,
        box_parser: &mut BoxParser,
        input: &mut dyn JxlBitstreamInput,
        decode_options: &JxlDecoderOptions,
        mut output_buffers: Option<&mut [JxlOutputBuffer]>,
        do_flush: bool,
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
                if decode_options.scan_frames_only
                    || (!self.process_without_output
                        && output_buffers.is_none()
                        && !can_be_referenced)
                {
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
                            let num = input.read(buffers)?;
                            box_parser.mark_file_consumed(num);
                            num
                        };
                        self.ready_section_data += num;
                        box_parser.consume_codestream(num as u64);
                        IoSliceMut::advance_slices(&mut buffers, num);
                        if num == 0 || buffers.is_empty() {
                            break;
                        }
                    }
                    match self.process_sections(decode_options, &mut output_buffers, do_flush) {
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
                            let skipped = input.skip(to_skip)?;
                            box_parser.mark_file_consumed(skipped);
                            skipped
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
                if !self.has_more_frames {
                    // If this is a flush request and the file is complete, we are done.
                    // Otherwise, this is an API usage error.
                    assert!(do_flush);
                    return Ok(());
                }

                // Capture frame-start metadata once before parsing the next
                // frame header. We do this after `get_more_codestream()` so we
                // are robust to the previous frame ending exactly at a box
                // boundary (BoxNeeded -> CodestreamBox transition).
                let mut capture_frame_start =
                    self.decoder_state.is_some() && self.frame_header.is_none();

                // Loop to handle incremental parsing (e.g. large ICC profiles) that may need
                // multiple buffer refills to complete.
                loop {
                    let available_codestream = match box_parser.get_more_codestream(input) {
                        Err(Error::OutOfBounds(_)) => 0,
                        Ok(c) => c,
                        Err(e) => return Err(e),
                    };

                    if capture_frame_start {
                        // total_file_consumed counts bytes read/skipped from
                        // raw input. non_section_buf and box_buffer contain
                        // unread bytes already accounted there.
                        self.current_frame_file_offset = (box_parser.total_file_consumed as usize)
                            .saturating_sub(self.non_section_buf.len())
                            .saturating_sub(box_parser.box_buffer.len());

                        // `available_codestream` includes bytes still in
                        // box_buffer and not yet in non_section_buf.
                        self.current_frame_remaining_in_box = if available_codestream > u64::MAX / 2
                        {
                            u64::MAX
                        } else {
                            available_codestream.saturating_add(self.non_section_buf.len() as u64)
                        };
                        capture_frame_start = false;
                    }

                    let c = self.non_section_buf.refill(
                        |buf| {
                            if !box_parser.box_buffer.is_empty() {
                                Ok(box_parser.box_buffer.take(buf))
                            } else {
                                let read = input.read(buf)?;
                                box_parser.mark_file_consumed(read);
                                Ok(read)
                            }
                        },
                        Some(available_codestream as usize),
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

                    // Record frame info for scanning (after preview check).
                    if !is_preview_frame {
                        self.record_frame_info();
                    }

                    if self.has_visible_frame() {
                        if self.visible_frames_to_skip > 0 {
                            // Skip this visible frame without returning to the
                            // caller; decrement the counter and continue
                            // processing internally.
                            self.visible_frames_to_skip -= 1;
                            self.process_without_output = true;
                            continue;
                        }
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

    pub(super) fn update_default_output_color_profile(&mut self) {
        // Only set default output_color_profile if not already configured by user
        if self.output_color_profile_set_by_user {
            return;
        }

        let embedded_color_profile = self.embedded_color_profile.as_ref().unwrap();
        let pixel_format = self.pixel_format.as_ref().unwrap();

        // Determine default output color profile following libjxl logic:
        // - For XYB: use embedded if can_output_to(), else:
        //   - if float samples are requested: linear sRGB,
        //   - else: sRGB
        // - For non-XYB: use embedded color profile
        let output_color_profile = if self.xyb_encoded {
            // Use embedded if we can output to it, otherwise fall back to sRGB
            let base_encoding = if embedded_color_profile.can_output_to() {
                match &embedded_color_profile {
                    JxlColorProfile::Simple(enc) => enc.clone(),
                    JxlColorProfile::Icc(_) => {
                        unreachable!("can_output_to returns false for ICC")
                    }
                }
            } else {
                let data_format = pixel_format
                    .color_data_format
                    .unwrap_or(JxlDataFormat::U8 { bit_depth: 8 });
                let is_float = matches!(
                    data_format,
                    JxlDataFormat::F32 { .. } | JxlDataFormat::F16 { .. }
                );
                if is_float {
                    JxlColorEncoding::linear_srgb(self.is_gray)
                } else {
                    JxlColorEncoding::srgb(self.is_gray)
                }
            };

            JxlColorProfile::Simple(base_encoding)
        } else {
            embedded_color_profile.clone()
        };
        self.output_color_profile = Some(output_color_profile);
    }
}
