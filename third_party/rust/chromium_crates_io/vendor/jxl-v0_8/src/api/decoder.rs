// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::box_parser::{BoxParser, CodestreamInput};
use super::codestream_parser::{CodestreamParser, FrameStartInfo};
use super::{
    Event, JxlAuxBox, JxlAuxBoxType, JxlBasicInfo, JxlBitstreamInput, JxlColorProfile,
    JxlDecoderOptions, JxlFrameHeader, JxlOutputBuffer, JxlParallelRunner, JxlParallelRunnerFun,
    JxlPixelFormat,
};
use crate::error::{Error, Result};

struct SequentialRunner;

impl JxlParallelRunner for SequentialRunner {
    fn run(&mut self, _num: usize, _fun: &JxlParallelRunnerFun) -> Result<()> {
        unreachable!("jxl-rs should only use run_ordered!")
    }

    fn num_threads(&self) -> usize {
        1
    }
}

/// Information about a single visible frame discovered while decoding.
#[derive(Debug, Clone)]
pub struct VisibleFrameInfo {
    /// Zero-based index among visible frames.
    pub index: usize,
    /// Duration in milliseconds (0 for still images or the last frame).
    pub duration_ms: f64,
    /// Duration in raw ticks from the animation header.
    pub duration_ticks: u32,
    /// Byte offset of this frame's header in the input file.
    pub file_offset: u64,
    /// Whether this is the last frame in the codestream.
    pub is_last: bool,
    /// Whether this frame is a seek-keyframe for visible-frame playback.
    pub is_keyframe: bool,
    /// Precomputed seek inputs for this visible frame.
    pub seek_target: VisibleFrameSeekTarget,
    /// Frame name, if any.
    pub name: String,
}

/// Precomputed seek inputs for a target visible frame.
#[derive(Debug, Clone, Copy)]
pub struct VisibleFrameSeekTarget {
    /// Start of the target frame itself.
    pub(crate) target: FrameStartInfo,
    /// For each reference frame (0-3) and LF frame (4-7) needed at the target frame: the index of
    /// the frame stored there, and the start of the earliest frame required to reconstruct it.
    pub(crate) slots: [Option<(usize, FrameStartInfo)>; 8],
}

/// JPEG XL decoder.
///
/// Internal buffers and scratch allocations are retained across frames and across calls to
/// [`Self::reset`] so that they can be reused without reallocating. To release this memory,
/// drop the `JxlDecoder`.
pub struct JxlDecoder {
    options: JxlDecoderOptions,
    box_parser: BoxParser,
    codestream_parser: CodestreamParser,
}

impl JxlDecoder {
    /// Creates a new decoder with the given options.
    pub fn new(options: JxlDecoderOptions) -> Self {
        let box_parser = BoxParser::with_aux_boxes(options.request_aux_boxes.iter().copied());
        JxlDecoder {
            options,
            box_parser,
            codestream_parser: CodestreamParser::new(),
        }
    }

    /// Resets the decoder to its initial state so it can be reused to decode a new file with the
    /// same [`JxlDecoderOptions`].
    ///
    /// Internal buffers and scratch allocations are retained across resets to avoid repeated
    /// allocations when decoding multiple images.
    pub fn reset(&mut self) {
        self.box_parser.reset();
        self.codestream_parser.reset();
    }

    #[cfg(test)]
    pub(crate) fn file_header(&self) -> Option<&crate::headers::FileHeader> {
        if self.codestream_parser.image_info.is_complete() {
            Some(self.codestream_parser.image_info.file_header())
        } else {
            None
        }
    }

    #[cfg(test)]
    pub(crate) fn raw_frame_header(&self) -> Option<&crate::headers::frame_header::FrameHeader> {
        self.codestream_parser.frame_info.current_frame_header()
    }

    #[cfg(test)]
    pub(crate) fn frames_decoded(&self) -> usize {
        self.codestream_parser.frames_decoded
    }

    #[cfg(test)]
    pub(crate) fn toc(&self) -> Option<&crate::headers::toc::Toc> {
        self.codestream_parser.frame_info.current_toc()
    }

    /// Obtains the image's basic information, if available.
    pub fn basic_info(&self) -> Option<&JxlBasicInfo> {
        if self.codestream_parser.image_info.is_complete() {
            Some(self.codestream_parser.image_info.basic_info())
        } else {
            None
        }
    }

    /// Retrieves the file's color profile, if available.
    pub fn embedded_color_profile(&self) -> Option<&JxlColorProfile> {
        if self.codestream_parser.image_info.is_complete() {
            Some(self.codestream_parser.image_info.embedded_color_profile())
        } else {
            None
        }
    }

    /// Retrieves the current output color profile, if available.
    pub fn output_color_profile(&self) -> Option<&JxlColorProfile> {
        self.codestream_parser.output_color_profile.as_ref()
    }

    /// Retrieves the current pixel format for output buffers, if available.
    pub fn current_pixel_format(&self) -> Option<&JxlPixelFormat> {
        self.codestream_parser.pixel_format.as_ref()
    }

    /// Specifies the pixel format for output buffers.
    ///
    /// Setting this may also change the output color profile if it was not set
    /// manually before.
    ///
    /// Frame render pipelines are built for the pixel format that is current
    /// when the frame's TOC is parsed, so the format can only be changed after
    /// [`Event::BasicInfo`](crate::api::Event::BasicInfo) and before the first
    /// frame header is decoded.
    pub fn set_pixel_format(&mut self, pixel_format: JxlPixelFormat) -> Result<()> {
        // TODO(veluca): return an error if we are asking for both planar and
        // interleaved-in-color alpha.
        if !self.codestream_parser.image_info.is_complete() {
            return Err(Error::ApiUsageError(
                "cannot set pixel format before BasicInfo",
            ));
        }
        let frame_header_was_decoded = self
            .codestream_parser
            .frame_info
            .current_frame_header()
            .is_some()
            || !self.codestream_parser.scanned_frames().is_empty();
        if frame_header_was_decoded
            && self.codestream_parser.pixel_format.as_ref() != Some(&pixel_format)
        {
            return Err(Error::ApiUsageError(
                "cannot change pixel format after first frame header",
            ));
        }
        self.codestream_parser.pixel_format = Some(pixel_format);
        self.codestream_parser.update_default_output_options();
        Ok(())
    }

    /// Retrieves the current frame's header, if between
    /// [`Event::FrameHeader`](crate::api::Event::FrameHeader) and
    /// [`Event::FrameComplete`](crate::api::Event::FrameComplete).
    pub fn frame_header(&self) -> Option<JxlFrameHeader> {
        if !self.codestream_parser.has_visible_frame() {
            return None;
        }
        let frame_header = self.codestream_parser.frame_info.current_frame_header()?;
        // The render pipeline always adds ExtendToImageDimensionsStage which extends
        // frames to the full image size. So the output size is always the image size,
        // not the frame's upsampled size.
        let size = self.codestream_parser.image_info.basic_info().size;
        Some(JxlFrameHeader {
            name: frame_header.name.clone(),
            duration: self
                .codestream_parser
                .image_info
                .file_header()
                .image_metadata
                .animation
                .as_ref()
                .map(|anim| frame_header.duration(anim)),
            size,
        })
    }

    /// Returns visible frame info entries collected during parsing.
    ///
    /// When `JxlDecoderOptions::scan_frames_only` is enabled this is the primary output of decoding.
    pub fn scanned_frames(&self) -> &[VisibleFrameInfo] {
        self.codestream_parser.scanned_frames()
    }

    /// Returns information about the trailing box that extends to the end of stream.
    ///
    /// The raw buffer inside the returned [`JxlAuxBox`] is part of the box data, and must be
    /// prepended to the remaining input to get the complete data.
    pub fn trailing_box(&self) -> Option<&JxlAuxBox> {
        self.box_parser.trailing_box()
    }

    /// Resets frame-level state to prepare for decoding a new frame, and returns the file byte
    /// offset from which raw file input must be provided next.
    ///
    /// After seeking the first time, scanned frame information will no longer be updated,
    /// since frames may be decoded out of order and not all frames may be visited.
    ///
    /// Stored frames that the decoder already has are kept; this seeks to the earliest frame
    /// needed to reconstruct any missing ones (or directly to the target frame if none are
    /// missing), skipping intermediate frames that are not stored.
    pub fn start_new_frame(&mut self, seek_target: VisibleFrameSeekTarget) -> Result<u64> {
        if !self.codestream_parser.image_info.is_complete() {
            return Err(Error::ApiUsageError("cannot seek before BasicInfo"));
        }
        let checkpoint = self.codestream_parser.start_new_frame(&seek_target);
        self.box_parser.reset_to_checkpoint(checkpoint);
        Ok(checkpoint.file_position)
    }

    /// Returns the total length of the JPEG XL file, once decoding is finished.
    /// This is needed because the decoder might over-consume bytes from the provided input stream
    /// in some cases.
    pub fn file_length(&self) -> Option<u64> {
        self.codestream_parser.file_length
    }

    /// Returns extracted boxes matching the given type.
    pub fn aux_boxes(&self, box_type: JxlAuxBoxType) -> &[JxlAuxBox] {
        self.box_parser.aux_boxes(box_type)
    }

    /// Signals that no more input bytes will be provided to the decoder in subsequent
    /// calls to process().
    ///
    /// Calling this is only necessary if additional boxes were requested (via
    /// [`JxlDecoderOptions::request_aux_boxes`]) - if no additional boxes are requested,
    /// or if the decoder determines that they cannot be present in the file after the
    /// end of the codestream, the decoder will transition to [`Event::Complete`] after
    /// the last frame.
    pub fn close_input(&mut self) {
        self.box_parser.close_input();
    }

    /// Process more of the input file.
    /// This function will return when reaching the next decoding stage (i.e. finished decoding
    /// file/frame header, finished decoding a frame, or finished the entire decode).
    ///
    /// Output `buffers` may be provided any time after [`Event::BasicInfo`]; they will only be
    /// written to when decoding a visible frame's sections (between [`Event::FrameHeader`] and
    /// [`Event::FrameComplete`]).
    ///
    /// If called when decoding a frame with `None` for `buffers`, the frame will still be read,
    /// but pixel data will not be produced. Note that reference frames that may be needed by
    /// later frames are still decoded internally even if `buffers` is `None`.
    /// For actually skipping frames without decoding reference data, first do a scan pass with
    /// `JxlDecoderOptions::scan_frames_only = true`, inspect [`Self::scanned_frames()`],
    /// and use [`Self::start_new_frame()`] to seek directly to the desired frame's keyframe.
    ///
    /// Note: the data in `buffers` should have alignment that is compatible with the requested
    /// pixel format. This means that, if we are asking for 2-byte or 4-byte output (i.e. u16/f16
    /// and f32 respectively), each row in the provided buffers must be aligned to 2 or 4 bytes
    /// respectively. If that is not the case, the library may panic.
    #[inline(never)]
    pub fn process(
        &mut self,
        input: &mut dyn JxlBitstreamInput,
        buffers: Option<&mut [JxlOutputBuffer]>,
        parallel_runner: Option<&mut (dyn JxlParallelRunner + '_)>,
    ) -> Result<Event> {
        self.codestream_parser.process(
            &mut CodestreamInput::new(&mut self.box_parser, input),
            &self.options,
            buffers,
            parallel_runner.unwrap_or(&mut SequentialRunner),
        )
    }

    /// Draws all the pixels we have data for.
    ///
    /// Returns `true` if any new pixels were written to `buffers` since the previous call to
    /// `flush_pixels`; returns `false` if no new rendering has happened.
    ///
    /// See [`Self::process`] for alignment requirements on `buffers`.
    pub fn flush_pixels(
        &mut self,
        buffers: &mut [JxlOutputBuffer],
        parallel_runner: Option<&mut (dyn JxlParallelRunner + '_)>,
    ) -> Result<bool> {
        let Some(profile) = self.codestream_parser.output_color_profile.as_ref() else {
            return Ok(false);
        };
        let Some(pixel_format) = self.codestream_parser.pixel_format.as_ref() else {
            return Ok(false);
        };
        match self.codestream_parser.frame_info.do_flush(
            buffers,
            profile,
            pixel_format,
            parallel_runner.unwrap_or(&mut SequentialRunner),
        ) {
            Ok(()) | Err(Error::OutOfBounds(_)) => {
                Ok(self.codestream_parser.get_and_clear_pixels_dirty())
            }
            Err(e) => Err(e),
        }
    }
}
