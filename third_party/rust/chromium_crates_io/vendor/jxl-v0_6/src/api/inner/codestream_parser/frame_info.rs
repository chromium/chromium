// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    collections::{HashSet, VecDeque},
    io::IoSliceMut,
};

use crate::{
    api::{
        JxlColorProfile, JxlDecoderOptions, JxlOutputBuffer, JxlParallelRunner, JxlPixelFormat,
        inner::{
            CodestreamParser,
            box_parser::CodestreamInput,
            codestream_parser::{ProcessMode, check_size_limit, validate_output_buffers},
            process::SmallBuffer,
        },
    },
    bit_reader::BitReader,
    error::{Error, Result},
    frame::{DecoderState, Frame, HfMetaSplitter, LfImageSplitter, Section},
    headers::{
        FileHeader,
        encodings::UnconditionalCoder,
        frame_header::{Encoding, FrameHeader, FrameType},
        toc::{IncrementalTocReader, Toc},
    },
    util::NewWithCapacity,
};

struct SectionBuffer {
    len: usize,
    data: Vec<u8>,
    section: Section,
}

#[derive(Debug)]
struct SectionState {
    lf_global_done: bool,
    remaining_lf: usize,
    hf_global_done: bool,
    completed_passes: Vec<u8>,
    lf_global_flush_len: usize,
}

impl SectionState {
    fn new(num_lf_groups: usize, num_groups: usize) -> Self {
        Self {
            lf_global_done: false,
            remaining_lf: num_lf_groups,
            hf_global_done: false,
            completed_passes: vec![0; num_groups],
            lf_global_flush_len: 0,
        }
    }
}

pub struct FrameInfo {
    frame_header: Option<FrameHeader>,
    toc_parser: Option<IncrementalTocReader>,
    frame: Option<Frame>,
    // Keeps track of whether pixels have been modified.
    pixels_dirty: bool,

    // Section information.
    sections: VecDeque<SectionBuffer>,
    section_size: usize,
    ready_section_data: usize,
    section_state: SectionState,

    // Or only section if in single section special case.
    lf_global_section: Option<SectionBuffer>,
    lf_sections: Vec<SectionBuffer>,
    hf_global_section: Option<SectionBuffer>,
    // indexed by group, then by pass.
    hf_sections: Vec<Vec<Option<SectionBuffer>>>,
    // group indices that *might* have new renderable data.
    candidate_hf_sections: HashSet<usize>,

    #[cfg(test)]
    pub use_simple_pipeline: bool,
}

impl FrameInfo {
    pub fn new() -> Self {
        Self {
            frame_header: None,
            toc_parser: None,
            frame: None,
            sections: VecDeque::new(),
            section_size: 0,
            ready_section_data: 0,
            section_state: SectionState::new(0, 0),
            lf_global_section: None,
            lf_sections: vec![],
            hf_global_section: None,
            hf_sections: vec![],
            candidate_hf_sections: HashSet::new(),
            pixels_dirty: false,
            #[cfg(test)]
            use_simple_pipeline: false,
        }
    }

    pub fn clear(&mut self, clear_frame: bool) {
        self.frame_header = None;
        self.toc_parser = None;
        self.ready_section_data = 0;

        if clear_frame {
            self.frame = None;
        }

        // Clear sections
        self.sections.clear();
        self.section_state = SectionState::new(0, 0);
        self.lf_global_section = None;
        self.lf_sections.clear();
        self.hf_global_section = None;
        self.hf_sections.clear();
        self.candidate_hf_sections.clear();
    }

    pub fn current_frame_header(&self) -> Option<&FrameHeader> {
        self.frame_header
            .as_ref()
            .or(self.frame.as_ref().map(|x| x.header()))
    }

    pub fn parse_frame_header(
        &mut self,
        is_preview: bool,
        decode_options: &JxlDecoderOptions,
        file_header: &FileHeader,
        br: &mut BitReader,
        bits: &mut usize,
    ) -> Result<()> {
        // For preview frames, use the preview dimensions instead of main image dimensions
        let nonserialized = if is_preview {
            file_header
                .preview_frame_header_nonserialized()
                .unwrap_or_else(|| file_header.frame_header_nonserialized())
        } else {
            file_header.frame_header_nonserialized()
        };

        let mut frame_header = FrameHeader::read_unconditional(&(), br, &nonserialized)?;
        frame_header.postprocess(&nonserialized);
        check_size_limit(
            decode_options.sample_limit,
            frame_header.size(),
            frame_header.num_extra_channels as usize,
        )?;

        self.frame_header = Some(frame_header);
        *bits = br.total_bits_read();
        Ok(())
    }

    pub fn parse_toc(&mut self, br: &mut BitReader, bits: &mut usize) -> Result<Toc> {
        if self.toc_parser.is_none() {
            let num_toc_entries = self.frame_header.as_ref().unwrap().num_toc_entries();
            self.toc_parser = Some(IncrementalTocReader::new(num_toc_entries as u32, br)?);
        }

        let toc_parser = self.toc_parser.as_mut().unwrap();
        *bits = br.total_bits_read();
        while !toc_parser.is_complete() {
            match toc_parser.read_step(br) {
                Ok(()) => *bits = br.total_bits_read(),
                Err(Error::OutOfBounds(c)) => {
                    // Estimate >= 16 bits per remaining entry to read.
                    return Err(Error::OutOfBounds(
                        c + toc_parser.remaining_entries() as usize * 2,
                    ));
                }
                Err(e) => return Err(e),
            }
        }
        br.jump_to_byte_boundary()?;
        *bits = br.total_bits_read();
        Ok(self.toc_parser.take().unwrap().finalize())
    }

    #[allow(clippy::too_many_arguments)]
    pub(super) fn make_frame(
        &mut self,
        cbuf: &mut SmallBuffer,
        toc: Toc,
        file_header: &FileHeader,
        decode_options: &JxlDecoderOptions,
        pixel_format: &JxlPixelFormat,
        output_profile: &JxlColorProfile,
        process_mode: ProcessMode,
    ) -> Result<()> {
        self.section_size = toc.entries.iter().map(|x| *x as usize).sum();
        self.ready_section_data = 0;

        self.lf_global_section = None;
        self.lf_sections.clear();
        self.hf_global_section = None;
        self.candidate_hf_sections.clear();
        self.hf_sections.clear();

        if !matches!(process_mode, ProcessMode::Skip(_)) {
            // If we have a previous frame, compute the decoder state from there.
            // Otherwise, compute a new decoder state.
            // We finalize the previous frame here to allow progressive rendering
            // to work properly if a flush is requested while we parse a frame
            // header.
            let decoder_state = self
                .frame
                .take()
                .map(|x| x.finalize())
                .transpose()?
                .flatten()
                .unwrap_or_else(|| DecoderState::new(file_header.clone(), decode_options));
            let mut frame =
                Frame::from_header_and_toc(self.frame_header.take().unwrap(), toc, decoder_state)?;

            let num_groups = frame.header().num_groups();
            let num_passes = frame.header().passes.num_passes as usize;
            let mut hf_sections = Vec::new_with_capacity(num_groups)?;
            for _ in 0..num_groups {
                let mut row = Vec::new_with_capacity(num_passes)?;
                for _ in 0..num_passes {
                    row.push(None);
                }
                hf_sections.push(row);
            }
            self.hf_sections = hf_sections;

            let mut sections: Vec<_> = frame
                .toc()
                .entries
                .iter()
                .map(|x| SectionBuffer {
                    len: *x as usize,
                    data: vec![],
                    section: Section::LfGlobal, // will be fixed later
                })
                .collect();

            let order = if frame.toc().permuted {
                frame.toc().permutation.0.clone()
            } else {
                (0..sections.len() as u32).collect()
            };

            if sections.len() > 1 {
                let base_sections = [Section::LfGlobal, Section::HfGlobal];
                let lf_sections =
                    (0..frame.header().num_lf_groups()).map(|x| Section::Lf { group: x });
                let hf_sections = (0..frame.header().passes.num_passes).flat_map(|p| {
                    (0..frame.header().num_groups()).map(move |g| Section::Hf {
                        group: g,
                        pass: p as usize,
                    })
                });

                for section in base_sections
                    .into_iter()
                    .chain(lf_sections)
                    .chain(hf_sections)
                {
                    sections[order[frame.get_section_idx(section)] as usize].section = section;
                }
            }
            self.sections = sections.into_iter().collect();

            // Move data from the pre-section buffer into the sections.
            // Only allocate as much of each section as the pre-section buffer
            // can fill; the TOC-declared section length is untrusted and may
            // be much larger than the actual input.
            for buf in self.sections.iter_mut() {
                if cbuf.is_empty() {
                    break;
                }
                let target = buf.len.min(cbuf.len());
                let mut data = Vec::new();
                data.try_reserve_exact(target)?;
                data.resize(target, 0);
                buf.data = data;
                let n = cbuf.take(&mut [IoSliceMut::new(&mut buf.data)]);
                self.ready_section_data += n;
            }

            self.section_state =
                SectionState::new(frame.header().num_lf_groups(), frame.header().num_groups());
            frame.prepare_render_pipeline(pixel_format, output_profile)?;
            self.frame = Some(frame);
        } else {
            let num = cbuf.len().min(self.section_size);
            cbuf.consume(num);
            self.ready_section_data += num;
        }

        Ok(())
    }

    #[cfg(test)]
    pub fn frame(&mut self) -> Option<&mut Frame> {
        self.frame.as_mut()
    }

    pub fn dequeue_ready_sections(&mut self) {
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
    }

    pub fn skip_sections(
        &mut self,
        input: &mut CodestreamInput,
        buf: &mut SmallBuffer,
    ) -> Result<()> {
        let total_size = self.section_size;
        let need_skip = total_size - self.ready_section_data;
        let skipped = input.skip(need_skip)?;
        buf.mark_consumed(skipped as u64);
        self.ready_section_data += skipped;
        if self.ready_section_data < total_size {
            Err(Error::OutOfBounds(total_size - self.ready_section_data))
        } else {
            self.sections.clear();
            Ok(())
        }
    }

    pub fn fill_sections(
        &mut self,
        input: &mut CodestreamInput,
        buf: &mut SmallBuffer,
    ) -> Result<()> {
        // TODO(veluca): consider re-using `section_buffers` between different iterations.
        let mut total = 0;
        loop {
            let available_codestream = input.available_bytes();
            let mut available_codestream = match (available_codestream, total) {
                (Ok(a), _) => a.max(1),
                (Err(Error::OutOfBounds(_)), t) if t > 0 => return Ok(()),
                (Err(e), _) => return Err(e),
            };
            let mut readable_section_data = self.ready_section_data + available_codestream;
            // Ensure enough section buffer space is available for reading the
            // available data. Section lengths come from the untrusted TOC, so
            // grow buffers only as far as the data we can actually read; a
            // truncated stream must not trigger a huge upfront allocation.
            for buf in self.sections.iter_mut() {
                if buf.data.len() < buf.len && buf.data.len() < readable_section_data {
                    let target = buf.len.min(readable_section_data);
                    if target == buf.len {
                        buf.data.try_reserve_exact(target - buf.data.len())?;
                    } else {
                        buf.data.try_reserve(target - buf.data.len())?;
                    }
                    buf.data.resize(target, 0);
                }
                readable_section_data = readable_section_data.saturating_sub(buf.data.len());
                if readable_section_data == 0 {
                    break;
                }
            }
            // Prepare buffers to read into.
            let mut section_buffers = vec![];
            let mut ready = self.ready_section_data;
            for buf in self.sections.iter_mut() {
                if buf.data.is_empty() && buf.len != 0 {
                    break;
                }
                let len = buf.data.len();
                if len > ready {
                    let readable = (available_codestream + ready).min(len);
                    section_buffers.push(IoSliceMut::new(&mut buf.data[ready..readable]));
                    available_codestream = available_codestream.saturating_sub(readable - ready);
                    if available_codestream == 0 {
                        break;
                    }
                }
                ready = ready.saturating_sub(len);
            }
            let num = input.read(&mut section_buffers[..])?;
            if num == 0 {
                break;
            }
            self.ready_section_data += num;
            total += num;
            buf.mark_consumed(num as u64);
            self.dequeue_ready_sections();
        }
        Ok(())
    }

    // Returns whether we modified the pixels.
    #[allow(clippy::too_many_arguments)]
    fn process_single_section(
        frame: &mut Frame,
        buf: &[u8],
        is_complete: bool,
        output_buffers: &mut Option<&mut [JxlOutputBuffer<'_>]>,
        output_profile: &JxlColorProfile,
        pixel_format: &JxlPixelFormat,
        do_flush: bool,
        parallel_runner: &mut dyn JxlParallelRunner,
    ) -> Result<bool> {
        let mut br = BitReader::new(buf);
        frame.decode_lf_global(&mut br, !is_complete)?;
        {
            let lf_splitter = frame.lf_image.as_mut().map(LfImageSplitter::new);
            let hf_meta_splitter = frame.hf_meta.as_mut().map(HfMetaSplitter::new);
            Frame::decode_lf_group(
                &frame.header,
                &frame.decoder_state,
                frame.lf_global.as_ref().unwrap(),
                0,
                &mut br,
                lf_splitter.as_ref(),
                hf_meta_splitter.as_ref(),
            )?;
        }
        frame.post_decode_lf_group(0);
        frame.decode_hf_global(&mut br)?;
        frame.finalize_lf(parallel_runner)?;
        frame.decode_and_render_hf_groups(
            output_buffers,
            pixel_format,
            vec![(0, vec![(0, br)])],
            do_flush,
            output_profile,
            parallel_runner,
        )
    }

    // Returns:
    // Ok(None) if the frame is complete
    // Ok(Some(len)) if we need `len` bytes to make progress
    // Err(_) if there was an error.
    pub fn process_sections(
        &mut self,
        output_buffers: &mut Option<&mut [JxlOutputBuffer<'_>]>,
        output_profile: &JxlColorProfile,
        pixel_format: &JxlPixelFormat,
        parallel_runner: &mut dyn JxlParallelRunner,
    ) -> Result<Option<usize>> {
        let data_for_next_section = self
            .sections
            .front()
            .map(|x| x.len - self.ready_section_data);

        let frame = self.frame.as_mut().unwrap();
        let frame_header = frame.header();
        if frame_header.num_groups() == 1 && frame_header.passes.num_passes == 1 {
            // Single-group special case.
            let Some(buf) = self.lf_global_section.take() else {
                return Ok(data_for_next_section);
            };
            assert!(self.sections.is_empty());
            self.pixels_dirty |= Self::process_single_section(
                frame,
                &buf.data,
                true,
                output_buffers,
                output_profile,
                pixel_format,
                false,
                parallel_runner,
            )?;
            return Ok(None);
        }

        if let Some(buf) = self.lf_global_section.take() {
            frame.decode_lf_global(&mut BitReader::new(&buf.data), false)?;
            self.section_state.lf_global_done = true;
        }

        if !self.section_state.lf_global_done {
            return Ok(data_for_next_section);
        }

        {
            let lf_splitter = frame.lf_image.as_mut().map(LfImageSplitter::new);
            let hf_meta_splitter = frame.hf_meta.as_mut().map(HfMetaSplitter::new);
            let header = &frame.header;
            let decoder_state = &frame.decoder_state;
            let lf_global = frame.lf_global.as_ref().unwrap();

            parallel_runner.run(self.lf_sections.len(), &|i: usize| -> Result<()> {
                let lf_section = &self.lf_sections[i];
                let Section::Lf { group } = &lf_section.section else {
                    unreachable!()
                };
                Frame::decode_lf_group(
                    header,
                    decoder_state,
                    lf_global,
                    *group,
                    &mut BitReader::new(&lf_section.data),
                    lf_splitter.as_ref(),
                    hf_meta_splitter.as_ref(),
                )?;
                Ok(())
            })?;
        }

        for lf_section in self.lf_sections.drain(..) {
            let Section::Lf { group } = lf_section.section else {
                unreachable!()
            };
            frame.post_decode_lf_group(group);
            self.section_state.remaining_lf -= 1;
        }

        if self.section_state.remaining_lf != 0 {
            return Ok(data_for_next_section);
        }

        if let Some(hf_global) = self.hf_global_section.take() {
            frame.decode_hf_global(&mut BitReader::new(&hf_global.data))?;
            frame.finalize_lf(parallel_runner)?;
            self.section_state.hf_global_done = true;
        }

        if !self.section_state.hf_global_done {
            return Ok(data_for_next_section);
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

        self.pixels_dirty |= frame.decode_and_render_hf_groups(
            output_buffers,
            pixel_format,
            group_readers,
            false,
            output_profile,
            parallel_runner,
        )?;

        for g in processed_groups.into_iter() {
            for i in 0..self.section_state.completed_passes[g] {
                self.hf_sections[g][i as usize] = None;
            }
        }

        Ok(data_for_next_section)
    }

    pub fn do_flush(
        &mut self,
        output_buffers: &mut [JxlOutputBuffer<'_>],
        output_profile: &JxlColorProfile,
        pixel_format: &JxlPixelFormat,
        parallel_runner: &mut dyn JxlParallelRunner,
    ) -> Result<()> {
        let Some(frame) = self.frame.as_mut() else {
            return Ok(());
        };
        validate_output_buffers(output_buffers, Some(pixel_format))?;
        let frame_header = frame.header();

        let has_partial_lf = self
            .sections
            .front()
            .is_some_and(|s| s.section == Section::LfGlobal)
            && 2 * self.ready_section_data > 3 * self.section_state.lf_global_flush_len
            && frame_header.encoding == Encoding::Modular
            && matches!(
                frame_header.frame_type,
                FrameType::RegularFrame | FrameType::LFFrame
            );

        if has_partial_lf {
            self.section_state.lf_global_flush_len = self.ready_section_data;
            let section = &self.sections[0].data[..self.ready_section_data];
            // These are partial renders, so we ignore any errors.
            if frame_header.num_groups() == 1 && frame_header.passes.num_passes == 1 {
                if let Ok(dirty) = Self::process_single_section(
                    frame,
                    section,
                    false,
                    &mut Some(output_buffers),
                    output_profile,
                    pixel_format,
                    true,
                    parallel_runner,
                ) {
                    self.pixels_dirty |= dirty;
                    return Ok(());
                }
            } else {
                let _ = frame.decode_lf_global(&mut BitReader::new(section), true);
            }
        }

        if frame.can_do_early_rendering() || self.section_state.hf_global_done {
            self.pixels_dirty |= frame.decode_and_render_hf_groups(
                &mut Some(output_buffers),
                pixel_format,
                vec![],
                true,
                output_profile,
                parallel_runner,
            )?;
        }
        Ok(())
    }
}

impl CodestreamParser {
    pub fn get_and_clear_pixels_dirty(&mut self) -> bool {
        let r = self.frame_info.pixels_dirty;
        self.frame_info.pixels_dirty = false;
        r
    }
}
