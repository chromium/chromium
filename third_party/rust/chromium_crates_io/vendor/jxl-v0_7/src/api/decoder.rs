// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::marker::PhantomData;

use states::*;

use super::{
    BoxParserCheckpoint, JxlAuxBox, JxlAuxBoxType, JxlBasicInfo, JxlBitstreamInput,
    JxlColorProfile, JxlDecoderInner, JxlDecoderOptions, JxlFrameHeader, JxlOutputBuffer,
    JxlParallelRunner, JxlPixelFormat, ProcessingResult,
};
use crate::error::Result;
#[cfg(test)]
use crate::{frame::Frame, headers::FileHeader};

pub mod states {
    pub trait JxlState {}
    pub struct Initialized;
    pub struct WithImageInfo;
    pub struct WithFrameInfo;
    pub struct InTrailingBox;
    impl JxlState for Initialized {}
    impl JxlState for WithImageInfo {}
    impl JxlState for WithFrameInfo {}
    impl JxlState for InTrailingBox {}
}

// Q: do we plan to add support for box decoding?
// If we do, one way is to take a callback &[u8; 4] -> Box<dyn Write>.

/// High level API using the typestate pattern to forbid invalid usage.
pub struct JxlDecoder<State: JxlState> {
    inner: Box<JxlDecoderInner>,
    _state: PhantomData<State>,
}

#[cfg(test)]
pub type FrameCallback = dyn FnMut(&FileHeader, &Frame, usize) -> Result<()>;

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
    ///
    /// This is equivalent to `seek_target.visible_frames_to_skip == 0`.
    pub is_keyframe: bool,
    /// Precomputed seek inputs for this visible frame.
    pub seek_target: VisibleFrameSeekTarget,
    /// Frame name, if any.
    pub name: String,
}

/// Computed seek inputs for a target visible frame.
#[derive(Debug, Clone, Copy)]
pub struct VisibleFrameSeekTarget {
    /// File byte offset to start feeding input from.
    pub decode_start_file_offset: u64,
    /// State of the box parser at the file offset we want to seek to.
    /// Pass this to [`JxlDecoder::start_new_frame`].
    pub box_parser_checkpoint: BoxParserCheckpoint,
    /// Number of visible frames to skip after seek-start before decoding the
    /// requested target frame.
    pub visible_frames_to_skip: usize,
}

impl<S: JxlState> JxlDecoder<S> {
    fn wrap_inner(inner: Box<JxlDecoderInner>) -> Self {
        Self {
            inner,
            _state: PhantomData,
        }
    }

    /// Sets a callback that processes all frames by calling `callback(frame, frame_index)`.
    #[cfg(test)]
    pub fn set_frame_callback(&mut self, callback: Box<FrameCallback>) {
        self.inner.set_frame_callback(callback);
    }

    /// Returns visible frame info entries collected so far.
    ///
    /// When `JxlDecoderOptions::scan_frames_only` is enabled this is the
    /// primary output of decoding.
    pub fn scanned_frames(&self) -> &[VisibleFrameInfo] {
        self.inner.scanned_frames()
    }

    pub fn aux_boxes(&self, box_type: JxlAuxBoxType) -> &[JxlAuxBox] {
        self.inner.aux_boxes(box_type)
    }

    fn map_inner_processing_result<SuccessState: JxlState>(
        self,
        inner_result: ProcessingResult<(), ()>,
    ) -> ProcessingResult<JxlDecoder<SuccessState>, Self> {
        match inner_result {
            ProcessingResult::Complete { .. } => ProcessingResult::Complete {
                result: JxlDecoder::wrap_inner(self.inner),
            },
            ProcessingResult::NeedsMoreInput { size_hint, .. } => {
                ProcessingResult::NeedsMoreInput {
                    size_hint,
                    fallback: self,
                }
            }
        }
    }
}

impl JxlDecoder<Initialized> {
    pub fn new(options: JxlDecoderOptions) -> Self {
        Self::wrap_inner(Box::new(JxlDecoderInner::new(options)))
    }

    pub fn process(
        mut self,
        input: &mut impl JxlBitstreamInput,
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<ProcessingResult<JxlDecoder<WithImageInfo>, Self>> {
        let inner_result = self.inner.process(input, None, parallel_runner)?;
        Ok(self.map_inner_processing_result(inner_result))
    }
}

impl JxlDecoder<WithImageInfo> {
    /// Obtains the image's basic information.
    pub fn basic_info(&self) -> &JxlBasicInfo {
        self.inner.basic_info().unwrap()
    }

    /// Retrieves the file's color profile.
    pub fn embedded_color_profile(&self) -> &JxlColorProfile {
        self.inner.embedded_color_profile().unwrap()
    }

    /// Retrieves the current output color profile.
    pub fn output_color_profile(&self) -> &JxlColorProfile {
        self.inner.output_color_profile().unwrap()
    }

    /// Retrieves the current pixel format for output buffers.
    pub fn current_pixel_format(&self) -> &JxlPixelFormat {
        self.inner.current_pixel_format().unwrap()
    }

    /// Specifies pixel format for output buffers.
    ///
    /// Setting this may also change output color profile in some cases, if the profile was not set
    /// manually before.
    ///
    /// The pixel format can only be changed before the first frame header is
    /// decoded, i.e. right after basic info becomes available; afterwards
    /// this returns an error (except when the format does not change).
    pub fn set_pixel_format(&mut self, pixel_format: JxlPixelFormat) -> Result<()> {
        self.inner.set_pixel_format(pixel_format)
    }

    pub fn process(
        mut self,
        input: &mut impl JxlBitstreamInput,
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<ProcessingResult<JxlDecoder<WithFrameInfo>, Self>> {
        let inner_result = self.inner.process(input, None, parallel_runner)?;
        Ok(self.map_inner_processing_result(inner_result))
    }

    /// Feeds additional trailing data, potentially parsing more trailing auxiliary boxes.
    ///
    /// When [`has_more_frames`][Self::has_more_frames] returned `false` and there is more data
    /// available, call this method to parse auxiliary boxes.
    pub fn process_trailing_data(
        mut self,
        input: &mut impl JxlBitstreamInput,
    ) -> Result<ProcessingResult<JxlDecoder<InTrailingBox>, Self>> {
        let inner_result = self.inner.process_trailing_data(input)?;
        Ok(self.map_inner_processing_result(inner_result))
    }

    /// Draws all the pixels we have data for. This is useful for i.e. previewing LF frames.
    ///
    /// Returns `true` if any new pixels were written to `buffers` since the
    /// previous call to `flush_pixels`; `false` if nothing new was rendered.
    ///
    /// Note: see `process` for alignment requirements for the buffer data.
    pub fn flush_pixels(
        &mut self,
        buffers: &mut [JxlOutputBuffer<'_>],
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<bool> {
        self.inner.flush_pixels(buffers, parallel_runner)
    }

    pub fn has_more_frames(&self) -> bool {
        self.inner.has_more_frames()
    }

    /// Returns the total length of the JPEG XL file, once decoding is finished.
    /// This is needed because the decoder might over-consume bytes from the
    /// provided input stream in some cases.
    pub fn file_length(&self) -> Option<u64> {
        self.inner.file_length()
    }

    /// Resets frame-level state to prepare for decoding a new frame.
    ///
    /// After seeking the first time, scanned frame information will no longer
    /// be updated. If you seek before having completed decoding once, the scanned
    /// frames might be incomplete.
    ///
    /// After calling this, provide raw file input starting from
    /// `seek_target.decode_start_file_offset`.
    pub fn start_new_frame(&mut self, seek_target: VisibleFrameSeekTarget) {
        self.inner.start_new_frame(seek_target);
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.inner.set_use_simple_pipeline(u);
    }

    #[cfg(test)]
    pub(crate) fn disable_16bit_modular_buffers(&mut self) {
        self.inner.disable_16bit_modular_buffers();
    }
}

impl JxlDecoder<WithFrameInfo> {
    /// Skip the current frame without decoding pixels.
    ///
    /// This reads section data from the input to advance past the frame, but
    /// does not render pixels. Reference frames that may be needed by later
    /// frames are still decoded internally.
    ///
    /// For efficient frame seeking in animations, enable
    /// `JxlDecoderOptions::scan_frames_only` and use
    /// [`scanned_frames`](JxlDecoder::scanned_frames), then
    /// [`start_new_frame`](JxlDecoder::start_new_frame) to jump directly to a
    /// target frame.
    pub fn skip_frame(
        mut self,
        input: &mut impl JxlBitstreamInput,
    ) -> Result<ProcessingResult<JxlDecoder<WithImageInfo>, Self>> {
        let inner_result = self.inner.process(input, None, None)?;
        Ok(self.map_inner_processing_result(inner_result))
    }

    pub fn frame_header(&self) -> JxlFrameHeader {
        self.inner.frame_header().unwrap()
    }

    /// Draws all the pixels we have data for.
    ///
    /// Returns `true` if any new pixels were written to `buffers` since the
    /// previous call to `flush_pixels`; `false` if nothing new was rendered.
    ///
    /// Note: see `process` for alignment requirements for the buffer data.
    pub fn flush_pixels(
        &mut self,
        buffers: &mut [JxlOutputBuffer<'_>],
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<bool> {
        self.inner.flush_pixels(buffers, parallel_runner)
    }

    /// Guarantees to populate exactly the appropriate part of the buffers.
    /// Wants one buffer for each non-ignored pixel type, i.e. color channels and each extra channel.
    ///
    /// Note: the data in `buffers` should have alignment requirements that are compatible with the
    /// requested pixel format. This means that, if we are asking for 2-byte or 4-byte output (i.e.
    /// u16/f16 and f32 respectively), each row in the provided buffers must be aligned to 2 or 4
    /// bytes respectively. If that is not the case, the library may panic.
    pub fn process<In: JxlBitstreamInput>(
        mut self,
        input: &mut In,
        buffers: &mut [JxlOutputBuffer<'_>],
        parallel_runner: Option<&mut dyn JxlParallelRunner>,
    ) -> Result<ProcessingResult<JxlDecoder<WithImageInfo>, Self>> {
        let inner_result = self.inner.process(input, Some(buffers), parallel_runner)?;
        Ok(self.map_inner_processing_result(inner_result))
    }
}

impl JxlDecoder<InTrailingBox> {
    /// Returns information about the trailing box that extends to the end of stream.
    ///
    /// The raw buffer inside the returned `JxlAuxBox` is part of the box data, and must be
    /// prepended to the remaining input to get the complete data.
    pub fn trailing_box(&self) -> Option<&JxlAuxBox> {
        self.inner.trailing_box()
    }

    /// Feeds additional trailing data, potentially parsing more trailing auxiliary boxes.
    ///
    /// When [`trailing_box`][Self::trailing_box] retruned `None` and there is more data available,
    /// call this method to parse more auxiliary boxes.
    pub fn process_trailing_data(
        &mut self,
        input: &mut impl JxlBitstreamInput,
    ) -> Result<ProcessingResult<(), ()>> {
        self.inner.process_trailing_data(input)
    }

    pub fn start_new_frame(
        mut self,
        seek_target: VisibleFrameSeekTarget,
    ) -> JxlDecoder<WithImageInfo> {
        self.inner.start_new_frame(seek_target);
        JxlDecoder::wrap_inner(self.inner)
    }
}
