// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{JxlDecoderOptions, JxlOutputBuffer},
    bit_reader::BitReader,
    error::Result,
    frame::Section,
    headers::frame_header::{Encoding, FrameType},
};

use super::CodestreamParser;

#[derive(Debug)]
pub(super) struct SectionState {
    lf_global_done: bool,
    remaining_lf: usize,
    hf_global_done: bool,
    completed_passes: Vec<u8>,
    lf_global_flush_len: usize,
}

impl SectionState {
    pub(super) fn new(num_lf_groups: usize, num_groups: usize) -> Self {
        Self {
            lf_global_done: false,
            remaining_lf: num_lf_groups,
            hf_global_done: false,
            completed_passes: vec![0; num_groups],
            lf_global_flush_len: 0,
        }
    }

    /// Returns the number of passes that are fully completed across all groups.
    /// A pass is fully completed when all groups have decoded that pass.
    pub(super) fn num_completed_passes(&self) -> usize {
        self.completed_passes.iter().copied().min().unwrap_or(0) as usize
    }
}

impl CodestreamParser {
    pub(super) fn process_sections(
        &mut self,
        decode_options: &JxlDecoderOptions,
        output_buffers: &mut Option<&mut [JxlOutputBuffer<'_>]>,
        do_flush: bool,
    ) -> Result<Option<usize>> {
        let frame = self.frame.as_mut().unwrap();

        let output_profile = self
            .output_color_profile
            .as_ref()
            .expect("output_color_profile should be set before pipeline preparation");

        let frame_header = frame.header();

        // Dequeue ready sections.
        while self
            .sections
            .front()
            .is_some_and(|s| s.len <= self.ready_section_data)
        {
            let s = self.sections.pop_front().unwrap();
            self.ready_section_data -= s.len;

            match s.section {
                Section::LfGlobal => {
                    self.lf_global_section = Some(s);
                }
                Section::HfGlobal => {
                    self.hf_global_section = Some(s);
                }
                Section::Lf { .. } => {
                    self.lf_sections.push(s);
                }
                Section::Hf { group, pass } => {
                    self.hf_sections[group][pass] = Some(s);
                    self.candidate_hf_sections.insert(group);
                }
            }
        }

        let mut processed_section = false;
        let mut called_render_hf = false;
        let pixel_format = self.pixel_format.as_ref().unwrap();

        let complete_lf_global;
        let (lf_global, lf_global_is_complete) = if let Some(d) = self.lf_global_section.take() {
            complete_lf_global = d;
            (
                Some(&complete_lf_global.data[..complete_lf_global.len]),
                true,
            )
        } else if do_flush
            && self
                .sections
                .front()
                .is_some_and(|s| s.section == Section::LfGlobal)
            && 2 * self.ready_section_data > 3 * self.section_state.lf_global_flush_len
            && frame_header.encoding == Encoding::Modular
            && matches!(
                frame_header.frame_type,
                FrameType::RegularFrame | FrameType::LFFrame
            )
        {
            self.section_state.lf_global_flush_len = self.ready_section_data;
            (
                Some(&self.sections[0].data[..self.ready_section_data]),
                false,
            )
        } else {
            (None, false)
        };

        'process: {
            if frame_header.num_groups() == 1 && frame_header.passes.num_passes == 1 {
                // Single-group special case.
                let Some(buf) = lf_global else {
                    break 'process;
                };
                assert!(self.sections.is_empty() || !lf_global_is_complete);
                let mut br = BitReader::new(buf);
                let res = (|| -> Result<()> {
                    frame.decode_lf_global(&mut br, !lf_global_is_complete)?;
                    frame.decode_lf_group(0, &mut br)?;
                    frame.decode_hf_global(&mut br)?;
                    frame.finalize_lf()?;
                    frame.decode_and_render_hf_groups(
                        output_buffers,
                        pixel_format,
                        vec![(0, vec![(0, br)])],
                        do_flush,
                        output_profile,
                    )?;
                    called_render_hf = true;
                    Ok(())
                })();
                match res {
                    Ok(_) => {
                        processed_section = true;
                    }
                    Err(_) if !lf_global_is_complete => {
                        // Ignore errors if we are doing partial parsing.
                    }
                    Err(e) => return Err(e),
                }
            } else {
                if let Some(buf) = lf_global {
                    match frame.decode_lf_global(&mut BitReader::new(buf), !lf_global_is_complete) {
                        Ok(_) => {
                            self.section_state.lf_global_done = true;
                            processed_section = true;
                        }
                        Err(_) if !lf_global_is_complete => {
                            // Ignore errors if we are doing partial parsing.
                        }
                        Err(e) => return Err(e),
                    }
                }

                if !self.section_state.lf_global_done {
                    break 'process;
                }

                for lf_section in self.lf_sections.drain(..) {
                    let Section::Lf { group } = lf_section.section else {
                        unreachable!()
                    };
                    frame.decode_lf_group(group, &mut BitReader::new(&lf_section.data))?;
                    processed_section = true;
                    self.section_state.remaining_lf -= 1;
                }

                if self.section_state.remaining_lf != 0 {
                    break 'process;
                }

                if let Some(hf_global) = self.hf_global_section.take() {
                    frame.decode_hf_global(&mut BitReader::new(&hf_global.data))?;
                    frame.finalize_lf()?;
                    self.section_state.hf_global_done = true;
                    processed_section = true;
                }

                if !self.section_state.hf_global_done {
                    break 'process;
                }

                let mut group_readers = vec![];
                let mut processed_groups = vec![];

                let mut check_group = |g: usize| {
                    let mut sections = vec![];
                    for (pass, grp) in self.hf_sections[g]
                        .iter()
                        .enumerate()
                        .skip(self.section_state.completed_passes[g] as usize)
                    {
                        let Some(s) = &grp else {
                            break;
                        };
                        self.section_state.completed_passes[g] += 1;
                        sections.push((pass, BitReader::new(&s.data)));
                    }
                    if !sections.is_empty() {
                        group_readers.push((g, sections));
                        processed_groups.push(g);
                    }
                };

                if self.candidate_hf_sections.len() * 4 < self.hf_sections.len() {
                    for g in self.candidate_hf_sections.drain() {
                        check_group(g)
                    }
                    // Processing sections in order is more efficient because it lets us flush
                    // the pipeline faster.
                    group_readers.sort_by_key(|x| x.0);
                } else {
                    for g in 0..self.hf_sections.len() {
                        if self.candidate_hf_sections.contains(&g) {
                            check_group(g);
                        }
                    }
                    self.candidate_hf_sections.clear();
                }

                frame.decode_and_render_hf_groups(
                    output_buffers,
                    pixel_format,
                    group_readers,
                    do_flush,
                    output_profile,
                )?;
                called_render_hf = true;

                for g in processed_groups.into_iter() {
                    for i in 0..self.section_state.completed_passes[g] {
                        self.hf_sections[g][i as usize] = None;
                    }
                    processed_section = true;
                }
            }
        }

        if do_flush && !called_render_hf && frame.can_do_early_rendering() {
            frame.decode_and_render_hf_groups(
                output_buffers,
                pixel_format,
                vec![],
                do_flush,
                output_profile,
            )?;
        }

        if !processed_section {
            let data_for_next_section =
                self.sections.front().unwrap().len - self.ready_section_data;
            return Ok(Some(data_for_next_section));
        }

        // Frame is not yet complete.
        if !self.sections.is_empty() {
            return Ok(None);
        }

        #[cfg(test)]
        {
            self.frame_callback.as_mut().map_or(Ok(()), |cb| {
                cb(self.frame.as_ref().unwrap(), self.decoded_frames)
            })?;
            self.decoded_frames += 1;
        }

        // Check if this might be a preview frame (skipped frame with preview enabled)
        let has_preview = self
            .basic_info
            .as_ref()
            .is_some_and(|info| info.preview_size.is_some());
        let might_be_preview = self.process_without_output && has_preview;

        let decoder_state = self.frame.take().unwrap().finalize()?;
        if let Some(state) = decoder_state {
            self.decoder_state = Some(state);
        } else if might_be_preview {
            // Preview frame has is_last=true but the main frame follows.
            // Recreate decoder state from saved file header for the main frame.
            if let Some(fh) = self.saved_file_header.take() {
                let mut new_state = crate::frame::DecoderState::new(fh);
                new_state.render_spotcolors = decode_options.render_spot_colors;
                self.decoder_state = Some(new_state);
            }
        } else {
            self.has_more_frames = false;
        }
        Ok(None)
    }
}
