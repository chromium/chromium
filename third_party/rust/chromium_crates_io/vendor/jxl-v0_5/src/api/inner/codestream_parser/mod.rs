// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{
        JxlColorProfile, JxlDecoderOptions, JxlOutputBuffer, JxlPixelFormat,
        inner::{
            box_parser::CodestreamInput,
            codestream_parser::{
                frame_info::FrameInfo, frame_scan_info::FrameScanInfo, image_info::ImageInfo,
            },
            process::SmallBuffer,
        },
    },
    error::{Error, Result},
};

#[cfg(test)]
use crate::api::FrameCallback;

mod frame_info;
mod frame_scan_info;
mod image_info;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ProcessMode {
    // Frame is processed regularly.
    Process,
    // Frame is an internal frame that is not shown to the user.
    SkipOutput,
    // Frame is skipped entirely; whether the user is notified
    // depends on the argument.
    Skip(bool),
}

impl ProcessMode {
    fn notify_user(&self) -> bool {
        *self == ProcessMode::Process || *self == ProcessMode::Skip(true)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ParserState {
    FileHeader,
    ColorEncoding,
    FrameHeader {
        is_preview: bool,
    },
    Toc {
        is_preview: bool,
    },
    Sections {
        is_preview: bool,
        process_mode: ProcessMode,
    },
    Finished,
}

fn check_size_limit(
    pixel_limit: Option<usize>,
    (xs, ys): (usize, usize),
    num_ec: usize,
) -> Result<()> {
    if let Some(limit) = pixel_limit {
        let xs = xs.max(16); // xsize is always at least 64 bytes.
        let total_pixels = xs.saturating_mul(ys).saturating_mul(3 + num_ec);
        if total_pixels >= limit {
            return Err(Error::ImageSizeTooLarge(xs, ys));
        }
    };
    Ok(())
}

fn validate_output_buffers(
    output_buffers: &[JxlOutputBuffer],
    pixel_format: Option<&JxlPixelFormat>,
) -> Result<()> {
    let px = pixel_format.unwrap();
    let expected_len = std::iter::once(&px.color_data_format)
        .chain(px.extra_channel_format.iter())
        .filter(|x| x.is_some())
        .count();
    if output_buffers.len() != expected_len {
        return Err(Error::WrongBufferCount(output_buffers.len(), expected_len));
    }
    Ok(())
}

pub(super) struct CodestreamParser {
    state: ParserState,

    // Image/frame information
    pub image_info: ImageInfo,
    pub frame_info: FrameInfo,
    frame_scan_info: FrameScanInfo,

    /// Total length of the file.
    pub file_length: Option<u64>,

    // Temporary buffer
    local_buffer: SmallBuffer,
    header_needed_bytes: Option<usize>,

    // Output information
    pub output_color_profile: Option<JxlColorProfile>,
    pub pixel_format: Option<JxlPixelFormat>,

    /// Number of visible frames still to skip before returning to the caller.
    /// Set via `start_new_frame` when seeking to a non-keyframe.
    visible_frames_to_skip: usize,

    #[cfg(test)]
    pub frame_callback: Option<Box<FrameCallback>>,
}

impl CodestreamParser {
    pub(super) fn new() -> Self {
        Self {
            image_info: ImageInfo::new(),
            state: ParserState::FileHeader,
            output_color_profile: None,
            pixel_format: None,
            local_buffer: SmallBuffer::new(4096),
            header_needed_bytes: None,
            frame_info: FrameInfo::new(),
            visible_frames_to_skip: 0,
            frame_scan_info: FrameScanInfo::new(),
            file_length: None,
            #[cfg(test)]
            frame_callback: None,
        }
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.frame_info.use_simple_pipeline = u;
    }

    pub(super) fn start_new_frame(
        &mut self,
        visible_frames_to_skip: usize,
        consumed_codestream: u64,
    ) {
        self.frame_info.clear(true);
        self.local_buffer = SmallBuffer::new(4096);
        self.local_buffer.mark_consumed(consumed_codestream);
        self.visible_frames_to_skip = visible_frames_to_skip;
        self.state = ParserState::FrameHeader { is_preview: false };
        self.header_needed_bytes = None;
    }

    pub(super) fn has_more_frames(&self) -> bool {
        self.state != ParserState::Finished
    }

    fn refill_and_parse<T>(
        &mut self,
        input: &mut CodestreamInput,
        mut do_parse: impl FnMut(&mut Self, &mut CodestreamInput) -> Result<T>,
    ) -> Result<T> {
        loop {
            let c = self.local_buffer.refill(|buf| input.read(buf))?;

            if let Some(mut need) = self.header_needed_bytes.take() {
                need = need.saturating_sub(c);
                if need > 0 {
                    self.header_needed_bytes = Some(need);
                    let enlarge = !self.local_buffer.can_read_more();
                    if enlarge {
                        self.local_buffer.enlarge();
                    }
                    if c > 0 || enlarge {
                        continue;
                    }
                    return Err(Error::OutOfBounds(need));
                }
            }

            let range = self.local_buffer.range();
            let res = do_parse(self, input);
            match res {
                Ok(val) => {
                    self.header_needed_bytes = None;
                    return Ok(val);
                }
                Err(Error::OutOfBounds(n)) => {
                    let new_range = self.local_buffer.range();
                    let enlarge = new_range == range && !self.local_buffer.can_read_more();
                    if enlarge {
                        self.local_buffer.enlarge();
                    }
                    self.header_needed_bytes = Some(n);
                    if c > 0 || enlarge {
                        continue;
                    }
                    return Err(Error::OutOfBounds(n));
                }
                Err(e) => return Err(e),
            }
        }
    }

    pub(super) fn process(
        &mut self,
        input: &mut CodestreamInput,
        decode_options: &JxlDecoderOptions,
        mut output_buffers: Option<&mut [JxlOutputBuffer]>,
    ) -> Result<()> {
        if let Some(output_buffers) = &output_buffers {
            validate_output_buffers(output_buffers, self.pixel_format.as_ref())?;
        }

        loop {
            match self.state {
                ParserState::FileHeader => {
                    self.refill_and_parse(input, |c, _| {
                        c.local_buffer.with_br(|br, bits| {
                            c.image_info.parse_file_header(decode_options, br, bits)
                        })
                    })?;
                    self.state = ParserState::ColorEncoding;
                }

                ParserState::ColorEncoding => {
                    self.refill_and_parse(input, |c, _| {
                        c.local_buffer
                            .with_br(|br, bits| c.image_info.parse_color_encoding(br, bits))
                    })?;
                    self.update_default_output_options();
                    self.state = ParserState::FrameHeader {
                        is_preview: self.image_info.has_preview(),
                    };
                    return Ok(());
                }

                ParserState::FrameHeader { is_preview } => {
                    self.refill_and_parse(input, |c, input| {
                        c.frame_scan_info.set_current_frame_checkpoint(
                            input
                                .box_parser()
                                .state_checkpoint(c.local_buffer.consumed())?,
                        );

                        c.local_buffer.with_br(|br, bits| {
                            c.frame_info.parse_frame_header(
                                is_preview,
                                decode_options,
                                c.image_info.file_header(),
                                br,
                                bits,
                            )
                        })
                    })?;
                    self.state = ParserState::Toc { is_preview };
                }

                ParserState::Toc { is_preview } => {
                    let toc = self.refill_and_parse(input, |c, _| {
                        c.local_buffer
                            .with_br(|br, bits| c.frame_info.parse_toc(br, bits))
                    })?;

                    // Decide how much to decode this frame.
                    let mut process_mode = ProcessMode::Process;

                    if is_preview && decode_options.skip_preview {
                        process_mode = ProcessMode::Skip(false);
                    }

                    if !self.frame_info.current_frame_header().unwrap().is_visible() {
                        process_mode = ProcessMode::SkipOutput;
                    } else if self.visible_frames_to_skip > 0 {
                        self.visible_frames_to_skip -= 1;
                        process_mode = ProcessMode::SkipOutput;
                    }

                    if decode_options.scan_frames_only && process_mode == ProcessMode::Process {
                        process_mode = ProcessMode::Skip(true);
                    }

                    self.state = ParserState::Sections {
                        is_preview,
                        process_mode,
                    };

                    // Record frame for seeking purposes
                    if !is_preview {
                        self.frame_scan_info.record(
                            self.frame_info.current_frame_header().unwrap(),
                            &self.image_info.file_header().image_metadata.animation,
                        );
                    }

                    self.frame_info.make_frame(
                        &mut self.local_buffer,
                        toc,
                        self.image_info.file_header(),
                        decode_options,
                        self.pixel_format.as_ref().unwrap(),
                        self.output_color_profile.as_ref().unwrap(),
                        process_mode,
                    )?;

                    if !matches!(process_mode, ProcessMode::Skip(..)) {
                        self.frame_info.dequeue_ready_sections();
                    }

                    if process_mode.notify_user() {
                        return Ok(());
                    }
                }

                ParserState::Sections {
                    is_preview,
                    process_mode,
                } => {
                    if matches!(process_mode, ProcessMode::Skip(..)) {
                        self.frame_info
                            .skip_sections(input, &mut self.local_buffer)?;
                    } else {
                        self.frame_info
                            .fill_sections(input, &mut self.local_buffer)?;

                        match self.frame_info.process_sections(
                            &mut output_buffers,
                            self.output_color_profile.as_ref().unwrap(),
                            self.pixel_format.as_ref().unwrap(),
                        ) {
                            Ok(None) => Ok(()),
                            Ok(Some(missing)) => Err(Error::OutOfBounds(missing)),
                            Err(Error::OutOfBounds(_)) => Err(Error::SectionTooShort),
                            Err(err) => Err(err),
                        }?;
                    }

                    #[cfg(test)]
                    {
                        let num_frames = self.scanned_frames().len();
                        if let Some(frame) = self.frame_info.frame() {
                            self.frame_callback.as_mut().map_or(Ok(()), |cb| {
                                cb(self.image_info.file_header(), frame, num_frames)
                            })?;
                        }
                    }

                    let is_last = self.frame_info.current_frame_header().unwrap().is_last;
                    self.frame_info.clear(is_last);
                    if !is_last {
                        self.state = ParserState::FrameHeader { is_preview };
                    } else if is_preview {
                        self.state = ParserState::FrameHeader { is_preview: false };
                    } else {
                        self.state = ParserState::Finished;
                        self.file_length = Some(
                            input
                                .box_parser()
                                .total_bytes_consumed(self.local_buffer.consumed()),
                        );
                    }

                    if process_mode.notify_user() {
                        return Ok(());
                    }
                }

                ParserState::Finished => {
                    panic!("API usage error: called process() on completed file")
                }
            };
        }
    }

    pub fn has_frame(&self) -> bool {
        matches!(self.state, ParserState::Sections { .. })
    }
}
