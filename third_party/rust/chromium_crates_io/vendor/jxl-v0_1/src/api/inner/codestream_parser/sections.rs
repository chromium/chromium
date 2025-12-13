// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{JxlDecoderOptions, JxlOutputBuffer},
    bit_reader::BitReader,
    error::Result,
    frame::Section,
};

use super::CodestreamParser;

pub(super) struct SectionState {
    lf_global_done: bool,
    remaining_lf: usize,
    hf_global_done: bool,
    completed_passes: Vec<u8>,
}

impl SectionState {
    pub(super) fn new(num_lf_groups: usize, num_groups: usize) -> Self {
        Self {
            lf_global_done: false,
            remaining_lf: num_lf_groups,
            hf_global_done: false,
            completed_passes: vec![0; num_groups],
        }
    }

    /// Returns the number of passes that are fully completed across all groups.
    /// A pass is fully completed when all groups have decoded that pass.
    pub(super) fn num_completed_passes(&self) -> usize {
        self.completed_passes.iter().copied().min().unwrap_or(0) as usize
    }
}

// No guarantees on the order of calls to f, or the order of retained elements in vec.
fn retain_by_value<T>(vec: &mut Vec<T>, mut f: impl FnMut(T) -> Option<T>) {
    for pos in (0..vec.len()).rev() {
        let element_to_test = vec.swap_remove(pos);
        if let Some(v) = f(element_to_test) {
            vec.push(v);
        }
    }
}

impl CodestreamParser {
    pub(super) fn process_sections(
        &mut self,
        decode_options: &JxlDecoderOptions,
        output_buffers: &mut Option<&mut [JxlOutputBuffer<'_>]>,
    ) -> Result<Option<usize>> {
        // Dequeue ready sections.
        while self
            .sections
            .front()
            .is_some_and(|s| s.len <= self.ready_section_data)
        {
            let s = self.sections.pop_front().unwrap();
            self.ready_section_data -= s.len;
            self.available_sections.push(s);
        }
        if self.available_sections.is_empty() {
            return Ok(Some(
                self.sections.front().unwrap().len - self.ready_section_data,
            ));
        }
        let frame = self.frame.as_mut().unwrap();
        let frame_header = frame.header();
        let pixel_format = self.pixel_format.as_ref().unwrap();
        if frame_header.num_groups() == 1 && frame_header.passes.num_passes == 1 {
            // Single-group special case.
            assert_eq!(self.available_sections.len(), 1);
            assert!(self.sections.is_empty());
            let mut br = BitReader::new(&self.available_sections[0].data);
            frame.decode_lf_global(&mut br)?;
            frame.decode_lf_group(0, &mut br)?;
            frame.decode_hf_global(&mut br)?;
            frame.prepare_render_pipeline(
                self.pixel_format.as_ref().unwrap(),
                decode_options.cms.as_deref(),
            )?;
            frame.finalize_lf()?;
            frame.decode_and_render_hf_groups(
                output_buffers,
                pixel_format,
                vec![(0, vec![(0, br)])],
            )?;
            self.available_sections.clear();
        } else {
            let mut lf_global_section = None;
            let mut lf_sections = vec![];
            let mut hf_global_section = None;
            let mut sorted_sections_for_each_group = Vec::with_capacity(frame_header.num_groups());
            for _ in 0..frame_header.num_groups() {
                sorted_sections_for_each_group.push(vec![]);
            }

            loop {
                let initial_sz = self.available_sections.len();
                retain_by_value(&mut self.available_sections, |sec| match sec.section {
                    Section::LfGlobal => {
                        lf_global_section = Some(sec);
                        self.section_state.lf_global_done = true;
                        None
                    }
                    Section::Lf { .. } => {
                        if !self.section_state.lf_global_done {
                            Some(sec)
                        } else {
                            lf_sections.push(sec);
                            self.section_state.remaining_lf -= 1;
                            None
                        }
                    }
                    Section::HfGlobal => {
                        if self.section_state.remaining_lf != 0 {
                            Some(sec)
                        } else {
                            hf_global_section = Some(sec);
                            self.section_state.hf_global_done = true;
                            None
                        }
                    }
                    Section::Hf { group, pass } => {
                        if !self.section_state.hf_global_done
                            || self.section_state.completed_passes[group] != pass as u8
                        {
                            Some(sec)
                        } else {
                            sorted_sections_for_each_group[group].push(sec);
                            self.section_state.completed_passes[group] += 1;
                            None
                        }
                    }
                });
                if self.available_sections.len() == initial_sz {
                    break;
                }
            }

            if let Some(lf_global) = lf_global_section {
                frame.decode_lf_global(&mut BitReader::new(&lf_global.data))?;
            }

            for lf_section in lf_sections {
                let Section::Lf { group } = lf_section.section else {
                    unreachable!()
                };
                frame.decode_lf_group(group, &mut BitReader::new(&lf_section.data))?;
            }

            if let Some(hf_global) = hf_global_section {
                frame.decode_hf_global(&mut BitReader::new(&hf_global.data))?;
                frame.prepare_render_pipeline(
                    self.pixel_format.as_ref().unwrap(),
                    decode_options.cms.as_deref(),
                )?;
                frame.finalize_lf()?;
            }

            let groups = sorted_sections_for_each_group
                .iter()
                .enumerate()
                .map(|(g, grp)| {
                    (
                        g,
                        grp.iter()
                            .map(|sec| {
                                let Section::Hf { group, pass } = sec.section else {
                                    unreachable!()
                                };
                                assert_eq!(group, g);
                                (pass, BitReader::new(&sec.data))
                            })
                            .collect(),
                    )
                })
                .collect();

            frame.decode_and_render_hf_groups(output_buffers, pixel_format, groups)?;
        }

        // Frame is not yet complete.
        if !self.sections.is_empty() {
            return Ok(None);
        }
        assert!(self.available_sections.is_empty());

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
                new_state.xyb_output_linear = decode_options.xyb_output_linear;
                new_state.render_spotcolors = decode_options.render_spot_colors;
                new_state.enable_output = decode_options.enable_output;
                self.decoder_state = Some(new_state);
            }
        } else {
            self.has_more_frames = false;
        }
        Ok(None)
    }
}
