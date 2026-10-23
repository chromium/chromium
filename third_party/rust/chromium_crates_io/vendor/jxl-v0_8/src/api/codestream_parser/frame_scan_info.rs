// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::codestream_parser::{CodestreamParser, stored_slot};
use crate::api::{BoxParserCheckpoint, VisibleFrameInfo, VisibleFrameSeekTarget};
use crate::frame::DecoderState;
use crate::headers::Animation;
use crate::headers::frame_header::FrameHeader;

#[derive(Debug, Clone, Copy)]
pub(crate) struct FrameStartInfo {
    pub(crate) box_parser_checkpoint: BoxParserCheckpoint,
    pub(crate) frame_counters: (usize, usize),
    pub(crate) frame_index: usize,
}

pub(super) struct FrameScanInfo {
    /// Collected visible frame info entries.
    scanned_frames: Vec<VisibleFrameInfo>,
    /// Zero-based visible frame index counter.
    visible_frame_index: usize,
    /// Non-visible frames since the last visible frame.
    nonvisible_frame_index: usize,
    /// Index of the next non-preview frame (visible or non-visible), in parse order.
    next_frame_index: usize,
    /// For each reference slot and LF slot (see `stored_slot`), the frame stored there so far,
    /// and the earliest frame required to reconstruct it.
    stored_frames: [Option<(usize, FrameStartInfo)>; 8],
    /// Box parser state where the current frame header parse started.
    /// Set when we begin parsing a frame header.
    current_frame_box_parser_checkpoint: Option<BoxParserCheckpoint>,
}

impl FrameScanInfo {
    pub fn new() -> Self {
        Self {
            scanned_frames: Vec::new(),
            visible_frame_index: 0,
            nonvisible_frame_index: 0,
            next_frame_index: 0,
            stored_frames: [None; 8],
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

        let current_frame_index = self.next_frame_index;
        self.next_frame_index += 1;
        let is_visible = header.is_visible();
        let target = FrameStartInfo {
            box_parser_checkpoint,
            frame_counters: (self.visible_frame_index, self.nonvisible_frame_index),
            frame_index: current_frame_index,
        };
        if is_visible {
            self.nonvisible_frame_index = 0;
        } else {
            self.nonvisible_frame_index += 1;
        }

        // Track frame dependencies through reference and LF slots. For blending we know
        // exactly which slots are used. For patches we conservatively assume any
        // reference slot may be used.
        let mut used_slots = [false; 8];
        if header.needs_blending() {
            for blending_info in header
                .ec_blending_info
                .iter()
                .chain(std::iter::once(&header.blending_info))
            {
                used_slots[blending_info.source as usize] = true;
            }
        }
        if header.has_patches() {
            used_slots[..DecoderState::MAX_STORED_FRAMES].fill(true);
        }
        if header.has_lf_frame() {
            used_slots[DecoderState::MAX_STORED_FRAMES + header.lf_level as usize] = true;
        }

        let decode_start = (0..8)
            .filter(|&s| used_slots[s])
            .filter_map(|s| Some(self.stored_frames[s]?.1))
            .min_by_key(|s| s.frame_index)
            .unwrap_or(target);

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

            // A seek to this frame also restores everything else that is stored (except what this
            // frame overwrites without reading), since the frames after it may read it.
            let mut slots = self.stored_frames;
            if let Some(slot) = stored_slot(header)
                && !used_slots[slot]
            {
                slots[slot] = None;
            }
            let is_keyframe = slots
                .iter()
                .flatten()
                .all(|(_, start)| start.frame_counters.0 == target.frame_counters.0);

            self.scanned_frames.push(VisibleFrameInfo {
                index: self.visible_frame_index,
                duration_ms,
                duration_ticks,
                file_offset: box_parser_checkpoint.file_position,
                is_last: header.is_last,
                is_keyframe,
                seek_target: VisibleFrameSeekTarget { target, slots },
                name: header.name.clone(),
            });

            self.visible_frame_index += 1;
        }

        if let Some(slot) = stored_slot(header) {
            self.stored_frames[slot] = Some((current_frame_index, decode_start));
        }
    }
}

impl CodestreamParser {
    pub fn scanned_frames(&self) -> &[VisibleFrameInfo] {
        &self.frame_scan_info.scanned_frames
    }
}
