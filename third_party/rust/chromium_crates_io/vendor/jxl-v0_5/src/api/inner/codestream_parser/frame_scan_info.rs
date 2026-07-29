// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{BoxParserCheckpoint, VisibleFrameInfo, VisibleFrameSeekTarget, inner::CodestreamParser},
    frame::DecoderState,
    headers::{Animation, frame_header::FrameHeader},
};

#[derive(Clone, Copy)]
struct FrameStartInfo {
    box_parser_checkpoint: BoxParserCheckpoint,
    visible_count_before: usize,
}

pub(super) struct FrameScanInfo {
    /// Collected visible frame info entries.
    scanned_frames: Vec<VisibleFrameInfo>,
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
    /// Box parser state where the current frame header parse started.
    /// Set when we begin parsing a frame header.
    current_frame_box_parser_checkpoint: Option<BoxParserCheckpoint>,
}

impl FrameScanInfo {
    pub fn new() -> Self {
        Self {
            scanned_frames: Vec::new(),
            visible_frame_index: 0,
            frame_starts: Vec::new(),
            reference_slot_decode_start: [None; DecoderState::MAX_STORED_FRAMES],
            lf_slot_decode_start: [None; DecoderState::NUM_LF_FRAMES],
            current_frame_box_parser_checkpoint: None,
        }
    }

    pub(super) fn set_current_frame_checkpoint(&mut self, checkpoint: Option<BoxParserCheckpoint>) {
        self.current_frame_box_parser_checkpoint = checkpoint;
    }

    /// Record frame info for the just-parsed frame.
    /// Called after process_non_section() creates a Frame, for frame scanning.
    pub(super) fn record(&mut self, header: &FrameHeader, animation: &Option<Animation>) {
        let Some(box_parser_checkpoint) = self.current_frame_box_parser_checkpoint else {
            return;
        };

        let current_frame_index = self.frame_starts.len();
        let is_visible = header.is_visible();
        self.frame_starts.push(FrameStartInfo {
            box_parser_checkpoint,
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
            let duration_ms = if let Some(anim) = animation {
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
                decode_start_file_offset: decode_start.box_parser_checkpoint.file_position,
                box_parser_checkpoint: decode_start.box_parser_checkpoint,
                visible_frames_to_skip: self
                    .visible_frame_index
                    .saturating_sub(decode_start.visible_count_before),
            };
            let is_keyframe = seek_target.visible_frames_to_skip == 0;

            self.scanned_frames.push(VisibleFrameInfo {
                index: self.visible_frame_index,
                duration_ms,
                duration_ticks,
                file_offset: box_parser_checkpoint.file_position,
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
}

impl CodestreamParser {
    pub fn scanned_frames(&self) -> &[VisibleFrameInfo] {
        &self.frame_scan_info.scanned_frames
    }
}
