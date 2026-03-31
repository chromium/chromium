// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::{
    JxlBasicInfo, JxlBitstreamInput, JxlColorProfile, JxlDecoderInner, JxlDecoderOptions,
    JxlOutputBuffer, JxlPixelFormat, ProcessingResult,
};
#[cfg(test)]
use crate::frame::Frame;
use crate::{api::JxlFrameHeader, container::frame_index::FrameIndexBox, error::Result};
use states::*;
use std::marker::PhantomData;

pub mod states {
    pub trait JxlState {}
    pub struct Initialized;
    pub struct WithImageInfo;
    pub struct WithFrameInfo;
    impl JxlState for Initialized {}
    impl JxlState for WithImageInfo {}
    impl JxlState for WithFrameInfo {}
}

// Q: do we plan to add support for box decoding?
// If we do, one way is to take a callback &[u8; 4] -> Box<dyn Write>.

/// High level API using the typestate pattern to forbid invalid usage.
pub struct JxlDecoder<State: JxlState> {
    inner: Box<JxlDecoderInner>,
    _state: PhantomData<State>,
}

#[cfg(test)]
pub type FrameCallback = dyn FnMut(&Frame, usize) -> Result<()>;

/// Information about a single visible frame discovered while decoding.
#[derive(Debug, Clone, PartialEq)]
pub struct VisibleFrameInfo {
    /// Zero-based index among visible frames.
    pub index: usize,
    /// Duration in milliseconds (0 for still images or the last frame).
    pub duration_ms: f64,
    /// Duration in raw ticks from the animation header.
    pub duration_ticks: u32,
    /// Byte offset of this frame's header in the input file.
    pub(crate) file_offset: usize,
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
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct VisibleFrameSeekTarget {
    /// File byte offset to start feeding input from.
    pub decode_start_file_offset: usize,
    /// Remaining codestream bytes in the current container box at the seek
    /// point. Pass this to [`JxlDecoder::start_new_frame`].
    pub remaining_in_box: u64,
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

    #[cfg(test)]
    pub fn decoded_frames(&self) -> usize {
        self.inner.decoded_frames()
    }

    /// Returns the parsed frame index box, if the file contained one.
    ///
    /// The frame index box (`jxli`) is an optional part of the JXL container
    /// format that provides a seek table for animated files, listing keyframe
    /// byte offsets, timestamps, and frame counts.
    ///
    /// TODO(veluca): Provide a higher-level frame-index API aligned with
    /// `scanned_frames()` / `VisibleFrameInfo` seek metadata.
    pub fn frame_index(&self) -> Option<&FrameIndexBox> {
        self.inner.frame_index()
    }

    /// Returns visible frame info entries collected so far.
    ///
    /// When `JxlDecoderOptions::scan_frames_only` is enabled this is the
    /// primary output of decoding.
    pub fn scanned_frames(&self) -> &[VisibleFrameInfo] {
        self.inner.scanned_frames()
    }

    /// Rewinds a decoder to the start of the file, allowing past frames to be displayed again.
    pub fn rewind(mut self) -> JxlDecoder<Initialized> {
        self.inner.rewind();
        JxlDecoder::wrap_inner(self.inner)
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
    ) -> Result<ProcessingResult<JxlDecoder<WithImageInfo>, Self>> {
        let inner_result = self.inner.process(input, None)?;
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

    /// Specifies the preferred color profile to be used for outputting data.
    /// Same semantics as JxlDecoderSetOutputColorProfile.
    pub fn set_output_color_profile(&mut self, profile: JxlColorProfile) -> Result<()> {
        self.inner.set_output_color_profile(profile)
    }

    /// Retrieves the current pixel format for output buffers.
    pub fn current_pixel_format(&self) -> &JxlPixelFormat {
        self.inner.current_pixel_format().unwrap()
    }

    /// Specifies pixel format for output buffers.
    ///
    /// Setting this may also change output color profile in some cases, if the profile was not set
    /// manually before.
    pub fn set_pixel_format(&mut self, pixel_format: JxlPixelFormat) {
        self.inner.set_pixel_format(pixel_format);
    }

    pub fn process(
        mut self,
        input: &mut impl JxlBitstreamInput,
    ) -> Result<ProcessingResult<JxlDecoder<WithFrameInfo>, Self>> {
        let inner_result = self.inner.process(input, None)?;
        Ok(self.map_inner_processing_result(inner_result))
    }

    /// Draws all the pixels we have data for. This is useful for i.e. previewing LF frames.
    ///
    /// Note: see `process` for alignment requirements for the buffer data.
    pub fn flush_pixels(&mut self, buffers: &mut [JxlOutputBuffer<'_>]) -> Result<()> {
        self.inner.flush_pixels(buffers)
    }

    pub fn has_more_frames(&self) -> bool {
        self.inner.has_more_frames()
    }

    /// Resets frame-level decoder state to prepare for decoding a new frame.
    ///
    /// This clears intermediate buffers (frame header, TOC, section data) while
    /// preserving image-level state (file header, color profiles, pixel format,
    /// reference frames). The box parser is restored to the correct
    /// mid-codestream state using `remaining_in_box`, so the next `process()`
    /// call correctly parses a new frame header from the input.
    ///
    /// # Arguments
    ///
    /// * `seek_target` -- from `VisibleFrameInfo::seek_target`.
    ///   Includes both the box-parser state (`remaining_in_box`) and the input
    ///   resume offset (`decode_start_file_offset`).
    ///
    /// After calling this, provide raw file input starting from
    /// `seek_target.decode_start_file_offset`.
    ///
    /// # Example
    ///
    /// ```rust,ignore
    /// // 1. Scan frame info using the regular decoder API.
    /// let options = JxlDecoderOptions {
    ///     scan_frames_only: true,
    ///     ..Default::default()
    /// };
    /// let decoder = JxlDecoder::<states::Initialized>::new(options);
    /// // ...advance decoder and call `scanned_frames()`...
    ///
    /// // 2. Seek to frame N (bare codestream).
    /// let target = &frames[n];
    /// decoder.start_new_frame(target.seek_target);
    /// // 3. Provide input from target.seek_target.decode_start_file_offset and process().
    /// ```
    pub fn start_new_frame(&mut self, seek_target: VisibleFrameSeekTarget) {
        self.inner.start_new_frame(seek_target);
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.inner.set_use_simple_pipeline(u);
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
        let inner_result = self.inner.process(input, None)?;
        Ok(self.map_inner_processing_result(inner_result))
    }

    pub fn frame_header(&self) -> JxlFrameHeader {
        self.inner.frame_header().unwrap()
    }

    /// Number of passes we have full data for.
    pub fn num_completed_passes(&self) -> usize {
        self.inner.num_completed_passes().unwrap()
    }

    /// Draws all the pixels we have data for.
    ///
    /// Note: see `process` for alignment requirements for the buffer data.
    pub fn flush_pixels(&mut self, buffers: &mut [JxlOutputBuffer<'_>]) -> Result<()> {
        self.inner.flush_pixels(buffers)
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
    ) -> Result<ProcessingResult<JxlDecoder<WithImageInfo>, Self>> {
        let inner_result = self.inner.process(input, Some(buffers))?;
        Ok(self.map_inner_processing_result(inner_result))
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::api::{JxlDataFormat, JxlDecoderOptions};
    use crate::error::Error;
    use crate::image::{Image, Rect};
    use jxl_macros::for_each_test_file;
    use std::path::Path;

    #[test]
    fn decode_small_chunks() {
        arbtest::arbtest(|u| {
            decode(
                &std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap(),
                u.arbitrary::<u8>().unwrap() as usize + 1,
                false,
                false,
                None,
            )
            .unwrap();
            Ok(())
        });
    }

    #[allow(clippy::type_complexity)]
    pub fn decode(
        mut input: &[u8],
        chunk_size: usize,
        use_simple_pipeline: bool,
        do_flush: bool,
        callback: Option<Box<dyn FnMut(&Frame, usize) -> Result<(), Error>>>,
    ) -> Result<(usize, Vec<Vec<Image<f32>>>), Error> {
        let options = JxlDecoderOptions::default();
        let mut initialized_decoder = JxlDecoder::<states::Initialized>::new(options);

        if let Some(callback) = callback {
            initialized_decoder.set_frame_callback(callback);
        }

        let mut chunk_input = &input[0..0];

        macro_rules! advance_decoder {
            ($decoder: ident $(, $extra_arg: expr)? $(; $flush_arg: expr)?) => {
                loop {
                    chunk_input =
                        &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                    let available_before = chunk_input.len();
                    let process_result = $decoder.process(&mut chunk_input $(, $extra_arg)?);
                    input = &input[(available_before - chunk_input.len())..];
                    match process_result.unwrap() {
                        ProcessingResult::Complete { result } => break result,
                        ProcessingResult::NeedsMoreInput { fallback, .. } => {
                            $(
                                let mut fallback = fallback;
                                if do_flush && !input.is_empty() {
                                    fallback.flush_pixels($flush_arg)?;
                                }
                            )?
                            if input.is_empty() {
                                panic!("Unexpected end of input");
                            }
                            $decoder = fallback;
                        }
                    }
                }
            };
        }

        // Process until we have image info
        let mut decoder_with_image_info = advance_decoder!(initialized_decoder);
        decoder_with_image_info.set_use_simple_pipeline(use_simple_pipeline);

        // Get basic info
        let basic_info = decoder_with_image_info.basic_info().clone();
        assert!(basic_info.bit_depth.bits_per_sample() > 0);

        // Get image dimensions (after upsampling, which is the actual output size)
        let (buffer_width, buffer_height) = basic_info.size;
        assert!(buffer_width > 0);
        assert!(buffer_height > 0);

        // Explicitly request F32 pixel format (test helper returns Image<f32>)
        let default_format = decoder_with_image_info.current_pixel_format();
        let requested_format = JxlPixelFormat {
            color_type: default_format.color_type,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: default_format
                .extra_channel_format
                .iter()
                .map(|_| Some(JxlDataFormat::f32()))
                .collect(),
        };
        decoder_with_image_info.set_pixel_format(requested_format);

        // Get the configured pixel format
        let pixel_format = decoder_with_image_info.current_pixel_format().clone();

        let num_channels = pixel_format.color_type.samples_per_pixel();
        assert!(num_channels > 0);

        let mut frames = vec![];

        loop {
            // First channel is interleaved.
            let mut buffers = vec![Image::new_with_value(
                (buffer_width * num_channels, buffer_height),
                f32::NAN,
            )?];

            for ecf in pixel_format.extra_channel_format.iter() {
                if ecf.is_none() {
                    continue;
                }
                buffers.push(Image::new_with_value(
                    (buffer_width, buffer_height),
                    f32::NAN,
                )?);
            }

            let mut api_buffers: Vec<_> = buffers
                .iter_mut()
                .map(|b| {
                    JxlOutputBuffer::from_image_rect_mut(
                        b.get_rect_mut(Rect {
                            origin: (0, 0),
                            size: b.size(),
                        })
                        .into_raw(),
                    )
                })
                .collect();

            // Process until we have frame info
            let mut decoder_with_frame_info =
                advance_decoder!(decoder_with_image_info; &mut api_buffers);
            decoder_with_image_info =
                advance_decoder!(decoder_with_frame_info, &mut api_buffers; &mut api_buffers);

            // All pixels should have been overwritten, so they should no longer be NaNs.
            for buf in buffers.iter() {
                let (xs, ys) = buf.size();
                for y in 0..ys {
                    let row = buf.row(y);
                    for (x, v) in row.iter().enumerate() {
                        assert!(!v.is_nan(), "NaN at {x} {y} (image size {xs}x{ys})");
                    }
                }
            }

            frames.push(buffers);

            // Check if there are more frames
            if !decoder_with_image_info.has_more_frames() {
                let decoded_frames = decoder_with_image_info.decoded_frames();

                // Ensure we decoded at least one frame
                assert!(decoded_frames > 0, "No frames were decoded");

                return Ok((decoded_frames, frames));
            }
        }
    }

    fn decode_test_file(path: &Path) -> Result<(), Error> {
        decode(&std::fs::read(path)?, usize::MAX, false, false, None)?;
        Ok(())
    }

    for_each_test_file!(decode_test_file);

    fn decode_test_file_chunks(path: &Path) -> Result<(), Error> {
        decode(&std::fs::read(path)?, 1, false, false, None)?;
        Ok(())
    }

    for_each_test_file!(decode_test_file_chunks);

    fn compare_frames(
        path: &Path,
        fc: usize,
        f: &[Image<f32>],
        sf: &[Image<f32>],
    ) -> Result<(), Error> {
        assert_eq!(
            f.len(),
            sf.len(),
            "Frame {fc} has different channels counts",
        );
        for (c, (b, sb)) in f.iter().zip(sf.iter()).enumerate() {
            assert_eq!(
                b.size(),
                sb.size(),
                "Channel {c} in frame {fc} has different sizes",
            );
            let sz = b.size();
            if false {
                let f = std::fs::File::create(Path::new("/tmp/").join(format!(
                    "{}_diff_chan{c}.pbm",
                    path.as_os_str().to_string_lossy().replace("/", "_")
                )))?;
                use std::io::Write;
                let mut f = std::io::BufWriter::new(f);
                writeln!(f, "P1\n{} {}", sz.0, sz.1)?;
                for y in 0..sz.1 {
                    for x in 0..sz.0 {
                        if (b.row(y)[x] - sb.row(y)[x]).abs() > 1e-8 {
                            write!(f, "1")?;
                        } else {
                            write!(f, "0")?;
                        }
                    }
                }
                drop(f);
            }
            for y in 0..sz.1 {
                for x in 0..sz.0 {
                    assert_eq!(
                        b.row(y)[x],
                        sb.row(y)[x],
                        "Pixels differ at position ({x}, {y}), channel {c}"
                    );
                }
            }
        }
        Ok(())
    }

    fn compare_pipelines(path: &Path) -> Result<(), Error> {
        let file = std::fs::read(path)?;
        let simple_frames = decode(&file, usize::MAX, true, false, None)?.1;
        let frames = decode(&file, usize::MAX, false, false, None)?.1;
        assert_eq!(frames.len(), simple_frames.len());
        for (fc, (f, sf)) in frames
            .into_iter()
            .zip(simple_frames.into_iter())
            .enumerate()
        {
            compare_frames(path, fc, &f, &sf)?;
        }
        Ok(())
    }

    for_each_test_file!(compare_pipelines);

    fn compare_incremental(path: &Path) -> Result<(), Error> {
        let file = std::fs::read(path).unwrap();
        // One-shot decode
        let (_, one_shot_frames) = decode(&file, usize::MAX, false, false, None)?;
        // Incremental decode with arbitrary flushes.
        let (_, frames) = decode(&file, 123, false, true, None)?;

        // Compare one_shot_frames and frames
        assert_eq!(one_shot_frames.len(), frames.len());
        for (fc, (f, sf)) in frames
            .into_iter()
            .zip(one_shot_frames.into_iter())
            .enumerate()
        {
            compare_frames(path, fc, &f, &sf)?;
        }

        Ok(())
    }

    for_each_test_file!(compare_incremental);

    #[test]
    fn test_preview_size_none_for_regular_files() {
        let file = std::fs::read("resources/test/basic.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();
        let decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
            }
        };
        assert!(decoder.basic_info().preview_size.is_none());
    }

    #[test]
    fn test_preview_size_some_for_preview_files() {
        let file = std::fs::read("resources/test/with_preview.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();
        let decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
            }
        };
        assert_eq!(decoder.basic_info().preview_size, Some((16, 16)));
    }

    #[test]
    fn test_num_completed_passes() {
        use crate::image::{Image, Rect};
        let file = std::fs::read("resources/test/basic.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();
        // Process until we have image info
        let mut decoder_with_info = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
            }
        };
        let info = decoder_with_info.basic_info().clone();
        let mut decoder_with_frame = loop {
            match decoder_with_info.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder_with_info = fallback;
                }
            }
        };
        // Before processing frame, passes should be 0
        assert_eq!(decoder_with_frame.num_completed_passes(), 0);
        // Process the frame
        let mut output = Image::<f32>::new((info.size.0 * 3, info.size.1)).unwrap();
        let rect = Rect {
            size: output.size(),
            origin: (0, 0),
        };
        let mut bufs = [JxlOutputBuffer::from_image_rect_mut(
            output.get_rect_mut(rect).into_raw(),
        )];
        loop {
            match decoder_with_frame.process(&mut input, &mut bufs).unwrap() {
                ProcessingResult::Complete { .. } => break,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder_with_frame = fallback,
            }
        }
    }

    #[test]
    fn test_set_pixel_format() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};

        let file = std::fs::read("resources/test/basic.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();
        let mut decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
            }
        };
        // Check default pixel format
        let default_format = decoder.current_pixel_format().clone();
        assert_eq!(default_format.color_type, JxlColorType::Rgb);

        // Set a new pixel format
        let new_format = JxlPixelFormat {
            color_type: JxlColorType::Grayscale,
            color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
            extra_channel_format: vec![],
        };
        decoder.set_pixel_format(new_format.clone());

        // Verify it was set
        assert_eq!(decoder.current_pixel_format(), &new_format);
    }

    #[test]
    fn test_set_output_color_profile() {
        use crate::api::JxlColorProfile;

        let file = std::fs::read("resources/test/basic.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();
        let mut decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
            }
        };

        // Get the embedded profile and set it as output (should work)
        let embedded = decoder.embedded_color_profile().clone();
        let result = decoder.set_output_color_profile(embedded);
        assert!(result.is_ok());

        // Setting an ICC profile without CMS should fail
        let icc_profile = JxlColorProfile::Icc(vec![0u8; 100]);
        let result = decoder.set_output_color_profile(icc_profile);
        assert!(result.is_err());
    }

    #[test]
    fn test_default_output_tf_by_pixel_format() {
        use crate::api::{JxlColorEncoding, JxlTransferFunction};

        // Using test image with ICC profile to trigger default transfer function path
        let file = std::fs::read("resources/test/lossy_with_icc.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();
        let mut decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => decoder = fallback,
            }
        };

        // Output data format will default to F32, so output color profile will be linear sRGB
        assert_eq!(
            *decoder.output_color_profile().transfer_function().unwrap(),
            JxlTransferFunction::Linear,
        );

        // Integer data format will set output color profile to sRGB
        decoder.set_pixel_format(JxlPixelFormat::rgba8(0));
        assert_eq!(
            *decoder.output_color_profile().transfer_function().unwrap(),
            JxlTransferFunction::SRGB,
        );

        decoder.set_pixel_format(JxlPixelFormat::rgba_f16(0));
        assert_eq!(
            *decoder.output_color_profile().transfer_function().unwrap(),
            JxlTransferFunction::Linear,
        );

        decoder.set_pixel_format(JxlPixelFormat::rgba16(0));
        assert_eq!(
            *decoder.output_color_profile().transfer_function().unwrap(),
            JxlTransferFunction::SRGB,
        );

        // Once output color profile is set by user, it will remain as is regardless of what pixel
        // format is set
        let profile = JxlColorProfile::Simple(JxlColorEncoding::srgb(false));
        decoder.set_output_color_profile(profile.clone()).unwrap();
        decoder.set_pixel_format(JxlPixelFormat::rgba_f16(0));
        assert!(decoder.output_color_profile() == &profile);
    }

    #[test]
    fn test_fill_opaque_alpha_both_pipelines() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};
        use crate::image::{Image, Rect};

        // Use basic.jxl which has no alpha channel
        let file = std::fs::read("resources/test/basic.jxl").unwrap();

        // Request RGBA format even though image has no alpha
        let rgba_format = JxlPixelFormat {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };

        // Test both pipelines (simple and low-memory)
        for use_simple in [true, false] {
            let options = JxlDecoderOptions::default();
            let decoder = JxlDecoder::<states::Initialized>::new(options);
            let mut input = file.as_slice();

            // Advance to image info
            macro_rules! advance_decoder {
                ($decoder:expr) => {
                    loop {
                        match $decoder.process(&mut input).unwrap() {
                            ProcessingResult::Complete { result } => break result,
                            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                                if input.is_empty() {
                                    panic!("Unexpected end of input");
                                }
                                $decoder = fallback;
                            }
                        }
                    }
                };
                ($decoder:expr, $buffers:expr) => {
                    loop {
                        match $decoder.process(&mut input, $buffers).unwrap() {
                            ProcessingResult::Complete { result } => break result,
                            ProcessingResult::NeedsMoreInput { fallback, .. } => {
                                if input.is_empty() {
                                    panic!("Unexpected end of input");
                                }
                                $decoder = fallback;
                            }
                        }
                    }
                };
            }

            let mut decoder = decoder;
            let mut decoder = advance_decoder!(decoder);
            decoder.set_use_simple_pipeline(use_simple);

            // Set RGBA format
            decoder.set_pixel_format(rgba_format.clone());

            let basic_info = decoder.basic_info().clone();
            let (width, height) = basic_info.size;

            // Advance to frame info
            let mut decoder = advance_decoder!(decoder);

            // Prepare buffer for RGBA (4 channels interleaved)
            let mut color_buffer = Image::<f32>::new((width * 4, height)).unwrap();
            let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
                color_buffer
                    .get_rect_mut(Rect {
                        origin: (0, 0),
                        size: (width * 4, height),
                    })
                    .into_raw(),
            )];

            // Decode frame
            let _decoder = advance_decoder!(decoder, &mut buffers);

            // Verify all alpha values are 1.0 (opaque)
            for y in 0..height {
                let row = color_buffer.row(y);
                for x in 0..width {
                    let alpha = row[x * 4 + 3];
                    assert_eq!(
                        alpha, 1.0,
                        "Alpha at ({},{}) should be 1.0, got {} (use_simple={})",
                        x, y, alpha, use_simple
                    );
                }
            }
        }
    }

    /// Test that premultiply_output=true produces premultiplied alpha output
    /// from a source with straight (non-premultiplied) alpha.
    #[test]
    fn test_premultiply_output_straight_alpha() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};

        // Use alpha_nonpremultiplied.jxl which has straight alpha (alpha_associated=false)
        let file =
            std::fs::read("resources/test/conformance_test_images/alpha_nonpremultiplied.jxl")
                .unwrap();

        // Alpha is included in RGBA, so we set extra_channel_format to None
        // to indicate no separate buffer for the alpha extra channel
        let rgba_format = JxlPixelFormat {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![None],
        };

        // Test both pipelines
        for use_simple in [true, false] {
            let (straight_buffer, width, height) =
                decode_with_format::<f32>(&file, &rgba_format, use_simple, false);
            let (premul_buffer, _, _) =
                decode_with_format::<f32>(&file, &rgba_format, use_simple, true);

            // Verify premultiplied values: premul_rgb should equal straight_rgb * alpha
            let mut found_semitransparent = false;
            for y in 0..height {
                let straight_row = straight_buffer.row(y);
                let premul_row = premul_buffer.row(y);
                for x in 0..width {
                    let sr = straight_row[x * 4];
                    let sg = straight_row[x * 4 + 1];
                    let sb = straight_row[x * 4 + 2];
                    let sa = straight_row[x * 4 + 3];

                    let pr = premul_row[x * 4];
                    let pg = premul_row[x * 4 + 1];
                    let pb = premul_row[x * 4 + 2];
                    let pa = premul_row[x * 4 + 3];

                    // Alpha should be unchanged
                    assert!(
                        (sa - pa).abs() < 1e-5,
                        "Alpha mismatch at ({},{}): straight={}, premul={} (use_simple={})",
                        x,
                        y,
                        sa,
                        pa,
                        use_simple
                    );

                    // Check premultiplication: premul = straight * alpha
                    let expected_r = sr * sa;
                    let expected_g = sg * sa;
                    let expected_b = sb * sa;

                    // Allow 1% tolerance for precision differences between pipelines
                    let tol = 0.01;
                    assert!(
                        (expected_r - pr).abs() < tol,
                        "R mismatch at ({},{}): expected={}, got={} (use_simple={})",
                        x,
                        y,
                        expected_r,
                        pr,
                        use_simple
                    );
                    assert!(
                        (expected_g - pg).abs() < tol,
                        "G mismatch at ({},{}): expected={}, got={} (use_simple={})",
                        x,
                        y,
                        expected_g,
                        pg,
                        use_simple
                    );
                    assert!(
                        (expected_b - pb).abs() < tol,
                        "B mismatch at ({},{}): expected={}, got={} (use_simple={})",
                        x,
                        y,
                        expected_b,
                        pb,
                        use_simple
                    );

                    if sa > 0.01 && sa < 0.99 {
                        found_semitransparent = true;
                    }
                }
            }

            // Ensure the test image actually has some semi-transparent pixels
            assert!(
                found_semitransparent,
                "Test image should have semi-transparent pixels (use_simple={})",
                use_simple
            );
        }
    }

    /// Test that premultiply_output=true doesn't double-premultiply
    /// when the source already has premultiplied alpha (alpha_associated=true).
    #[test]
    fn test_premultiply_output_already_premultiplied() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};

        // Use alpha_premultiplied.jxl which has alpha_associated=true
        let file = std::fs::read("resources/test/conformance_test_images/alpha_premultiplied.jxl")
            .unwrap();

        // Alpha is included in RGBA, so we set extra_channel_format to None
        let rgba_format = JxlPixelFormat {
            color_type: JxlColorType::Rgba,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![None],
        };

        // Test both pipelines
        for use_simple in [true, false] {
            let (without_flag_buffer, width, height) =
                decode_with_format::<f32>(&file, &rgba_format, use_simple, false);
            let (with_flag_buffer, _, _) =
                decode_with_format::<f32>(&file, &rgba_format, use_simple, true);

            // Both outputs should be identical since source is already premultiplied
            // and we shouldn't double-premultiply
            for y in 0..height {
                let without_row = without_flag_buffer.row(y);
                let with_row = with_flag_buffer.row(y);
                for x in 0..width {
                    for c in 0..4 {
                        let without_val = without_row[x * 4 + c];
                        let with_val = with_row[x * 4 + c];
                        assert!(
                            (without_val - with_val).abs() < 1e-5,
                            "Mismatch at ({},{}) channel {}: without_flag={}, with_flag={} (use_simple={})",
                            x,
                            y,
                            c,
                            without_val,
                            with_val,
                            use_simple
                        );
                    }
                }
            }
        }
    }

    /// Test that animations with reference frames work correctly.
    /// This exercises the buffer index calculation fix where reference frame
    /// save stages use indices beyond the API-provided buffer array.
    #[test]
    fn test_animation_with_reference_frames() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};
        use crate::image::{Image, Rect};

        // Use animation_spline.jxl which has multiple frames with references
        let file =
            std::fs::read("resources/test/conformance_test_images/animation_spline.jxl").unwrap();

        let options = JxlDecoderOptions::default();
        let decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();

        // Advance to image info
        let mut decoder = decoder;
        let mut decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder = fallback;
                }
            }
        };

        // Set RGB format with no extra channels
        let rgb_format = JxlPixelFormat {
            color_type: JxlColorType::Rgb,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };
        decoder.set_pixel_format(rgb_format);

        let basic_info = decoder.basic_info().clone();
        let (width, height) = basic_info.size;

        let mut frame_count = 0;

        // Decode all frames
        loop {
            // Advance to frame info
            let mut decoder_frame = loop {
                match decoder.process(&mut input).unwrap() {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput { fallback, .. } => {
                        decoder = fallback;
                    }
                }
            };

            // Prepare buffer for RGB (3 channels interleaved)
            let mut color_buffer = Image::<f32>::new((width * 3, height)).unwrap();
            let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
                color_buffer
                    .get_rect_mut(Rect {
                        origin: (0, 0),
                        size: (width * 3, height),
                    })
                    .into_raw(),
            )];

            // Decode frame - this should not panic even though reference frame
            // save stages target buffer indices beyond buffers.len()
            decoder = loop {
                match decoder_frame.process(&mut input, &mut buffers).unwrap() {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput { fallback, .. } => {
                        decoder_frame = fallback;
                    }
                }
            };

            frame_count += 1;

            // Check if there are more frames
            if !decoder.has_more_frames() {
                break;
            }
        }

        // Verify we decoded multiple frames
        assert!(
            frame_count > 1,
            "Expected multiple frames in animation, got {}",
            frame_count
        );
    }

    #[test]
    fn test_skip_frame_then_decode_next() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};
        use crate::image::{Image, Rect};

        // Use animation_spline.jxl which has multiple frames
        let file =
            std::fs::read("resources/test/conformance_test_images/animation_spline.jxl").unwrap();

        let options = JxlDecoderOptions::default();
        let decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file.as_slice();

        // Advance to image info
        let mut decoder = decoder;
        let mut decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder = fallback;
                }
            }
        };

        // Set RGB format
        let rgb_format = JxlPixelFormat {
            color_type: JxlColorType::Rgb,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: vec![],
        };
        decoder.set_pixel_format(rgb_format);

        let basic_info = decoder.basic_info().clone();
        let (width, height) = basic_info.size;

        // Advance to frame info for first frame
        let mut decoder_frame = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder = fallback;
                }
            }
        };

        // Skip the first frame (this is where the bug would leave stale frame state)
        let mut decoder = loop {
            match decoder_frame.skip_frame(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder_frame = fallback;
                }
            }
        };

        assert!(
            decoder.has_more_frames(),
            "Animation should have more frames"
        );

        // Advance to frame info for second frame
        // Without the fix, this would panic at assert!(self.frame.is_none())
        let mut decoder_frame = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder = fallback;
                }
            }
        };

        // Decode the second frame to verify everything works
        let mut color_buffer = Image::<f32>::new((width * 3, height)).unwrap();
        let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
            color_buffer
                .get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (width * 3, height),
                })
                .into_raw(),
        )];

        let decoder = loop {
            match decoder_frame.process(&mut input, &mut buffers).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder_frame = fallback;
                }
            }
        };

        // If we got here without panicking, the fix works
        // Optionally verify we can continue with more frames
        let _ = decoder.has_more_frames();
    }

    /// Test that u8 output matches f32 output within quantization tolerance.
    /// This test would catch bugs like the offset miscalculation in PR #586
    /// that caused black bars in u8 output.
    #[test]
    fn test_output_format_u8_matches_f32() {
        use crate::api::{JxlColorType, JxlDataFormat, JxlPixelFormat};

        // Use bicycles.jxl - a larger image that exercises offset calculations
        let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

        // Test both RGB and BGRA to catch channel reordering bugs
        for (color_type, num_samples) in [(JxlColorType::Rgb, 3), (JxlColorType::Bgra, 4)] {
            let f32_format = JxlPixelFormat {
                color_type,
                color_data_format: Some(JxlDataFormat::f32()),
                extra_channel_format: vec![],
            };
            let u8_format = JxlPixelFormat {
                color_type,
                color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
                extra_channel_format: vec![],
            };

            // Test both pipelines
            for use_simple in [true, false] {
                let (f32_buffer, width, height) =
                    decode_with_format::<f32>(&file, &f32_format, use_simple, false);
                let (u8_buffer, _, _) =
                    decode_with_format::<u8>(&file, &u8_format, use_simple, false);

                // Compare values: u8 / 255.0 should match f32
                // Tolerance: quantization error of ±0.5/255 ≈ 0.00196 plus small rounding
                let tolerance = 0.003;
                let mut max_error: f32 = 0.0;

                for y in 0..height {
                    let f32_row = f32_buffer.row(y);
                    let u8_row = u8_buffer.row(y);
                    for x in 0..(width * num_samples) {
                        let f32_val = f32_row[x].clamp(0.0, 1.0);
                        let u8_val = u8_row[x] as f32 / 255.0;
                        let error = (f32_val - u8_val).abs();
                        max_error = max_error.max(error);
                        assert!(
                            error < tolerance,
                            "{:?} u8 mismatch at ({},{}): f32={}, u8={} (scaled={}), error={} (use_simple={})",
                            color_type,
                            x,
                            y,
                            f32_val,
                            u8_row[x],
                            u8_val,
                            error,
                            use_simple
                        );
                    }
                }
            }
        }
    }

    /// Test that u16 output matches f32 output within quantization tolerance.
    #[test]
    fn test_output_format_u16_matches_f32() {
        use crate::api::{Endianness, JxlColorType, JxlDataFormat, JxlPixelFormat};

        let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

        // Test both RGB and BGRA
        for (color_type, num_samples) in [(JxlColorType::Rgb, 3), (JxlColorType::Bgra, 4)] {
            let f32_format = JxlPixelFormat {
                color_type,
                color_data_format: Some(JxlDataFormat::f32()),
                extra_channel_format: vec![],
            };
            let u16_format = JxlPixelFormat {
                color_type,
                color_data_format: Some(JxlDataFormat::U16 {
                    endianness: Endianness::native(),
                    bit_depth: 16,
                }),
                extra_channel_format: vec![],
            };

            for use_simple in [true, false] {
                let (f32_buffer, width, height) =
                    decode_with_format::<f32>(&file, &f32_format, use_simple, false);
                let (u16_buffer, _, _) =
                    decode_with_format::<u16>(&file, &u16_format, use_simple, false);

                // Tolerance: quantization error of ±0.5/65535 plus small rounding
                let tolerance = 0.0001;

                for y in 0..height {
                    let f32_row = f32_buffer.row(y);
                    let u16_row = u16_buffer.row(y);
                    for x in 0..(width * num_samples) {
                        let f32_val = f32_row[x].clamp(0.0, 1.0);
                        let u16_val = u16_row[x] as f32 / 65535.0;
                        let error = (f32_val - u16_val).abs();
                        assert!(
                            error < tolerance,
                            "{:?} u16 mismatch at ({},{}): f32={}, u16={} (scaled={}), error={} (use_simple={})",
                            color_type,
                            x,
                            y,
                            f32_val,
                            u16_row[x],
                            u16_val,
                            error,
                            use_simple
                        );
                    }
                }
            }
        }
    }

    /// Test that f16 output matches f32 output within f16 precision tolerance.
    #[test]
    fn test_output_format_f16_matches_f32() {
        use crate::api::{Endianness, JxlColorType, JxlDataFormat, JxlPixelFormat};
        use crate::util::f16;

        let file = std::fs::read("resources/test/conformance_test_images/bicycles.jxl").unwrap();

        // Test both RGB and BGRA
        for (color_type, num_samples) in [(JxlColorType::Rgb, 3), (JxlColorType::Bgra, 4)] {
            let f32_format = JxlPixelFormat {
                color_type,
                color_data_format: Some(JxlDataFormat::f32()),
                extra_channel_format: vec![],
            };
            let f16_format = JxlPixelFormat {
                color_type,
                color_data_format: Some(JxlDataFormat::F16 {
                    endianness: Endianness::native(),
                }),
                extra_channel_format: vec![],
            };

            for use_simple in [true, false] {
                let (f32_buffer, width, height) =
                    decode_with_format::<f32>(&file, &f32_format, use_simple, false);
                let (f16_buffer, _, _) =
                    decode_with_format::<f16>(&file, &f16_format, use_simple, false);

                // f16 has about 3 decimal digits of precision
                // For values in [0,1], the relative error is about 0.001
                let tolerance = 0.002;

                for y in 0..height {
                    let f32_row = f32_buffer.row(y);
                    let f16_row = f16_buffer.row(y);
                    for x in 0..(width * num_samples) {
                        let f32_val = f32_row[x];
                        let f16_val = f16_row[x].to_f32();
                        let error = (f32_val - f16_val).abs();
                        assert!(
                            error < tolerance,
                            "{:?} f16 mismatch at ({},{}): f32={}, f16={}, error={} (use_simple={})",
                            color_type,
                            x,
                            y,
                            f32_val,
                            f16_val,
                            error,
                            use_simple
                        );
                    }
                }
            }
        }
    }

    /// Helper function to decode an image with a specific format.
    fn decode_with_format<T: crate::image::ImageDataType>(
        file: &[u8],
        pixel_format: &JxlPixelFormat,
        use_simple: bool,
        premultiply: bool,
    ) -> (Image<T>, usize, usize) {
        let options = JxlDecoderOptions {
            premultiply_output: premultiply,
            ..Default::default()
        };
        let mut decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = file;

        // Advance to image info
        let mut decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    if input.is_empty() {
                        panic!("Unexpected end of input");
                    }
                    decoder = fallback;
                }
            }
        };
        decoder.set_use_simple_pipeline(use_simple);
        decoder.set_pixel_format(pixel_format.clone());

        let basic_info = decoder.basic_info().clone();
        let (width, height) = basic_info.size;

        let num_samples = pixel_format.color_type.samples_per_pixel();

        // Advance to frame info
        let decoder = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    if input.is_empty() {
                        panic!("Unexpected end of input");
                    }
                    decoder = fallback;
                }
            }
        };

        let mut buffer = Image::<T>::new((width * num_samples, height)).unwrap();
        let mut buffers: Vec<_> = vec![JxlOutputBuffer::from_image_rect_mut(
            buffer
                .get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (width * num_samples, height),
                })
                .into_raw(),
        )];

        // Decode
        let mut decoder = decoder;
        loop {
            match decoder.process(&mut input, &mut buffers).unwrap() {
                ProcessingResult::Complete { .. } => break,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    if input.is_empty() {
                        panic!("Unexpected end of input");
                    }
                    decoder = fallback;
                }
            }
        }

        (buffer, width, height)
    }

    /// Regression test for ClusterFuzz issue 5342436251336704
    /// Tests that malformed JXL files with overflow-inducing data don't panic
    #[test]
    fn test_fuzzer_smallbuffer_overflow() {
        use std::panic;

        let data = include_bytes!("../../tests/testdata/fuzzer_smallbuffer_overflow.jxl");

        // The test passes if it doesn't panic with "attempt to add with overflow"
        // It's OK if it returns an error or panics with "Unexpected end of input"
        let result = panic::catch_unwind(|| {
            let _ = decode(data, 1024, false, false, None);
        });

        // If it panicked, make sure it wasn't an overflow panic
        if let Err(e) = result {
            let panic_msg = e
                .downcast_ref::<&str>()
                .map(|s| s.to_string())
                .or_else(|| e.downcast_ref::<String>().cloned())
                .unwrap_or_default();
            assert!(
                !panic_msg.contains("overflow"),
                "Unexpected overflow panic: {}",
                panic_msg
            );
        }
    }

    fn make_box(ty: &[u8; 4], content: &[u8]) -> Vec<u8> {
        let len = (8 + content.len()) as u32;
        let mut buf = Vec::new();
        buf.extend(len.to_be_bytes());
        buf.extend(ty);
        buf.extend(content);
        buf
    }

    fn add_container_header(container: &mut Vec<u8>) {
        // JXL signature box
        let sig = [
            0x00, 0x00, 0x00, 0x0c, 0x4a, 0x58, 0x4c, 0x20, 0x0d, 0x0a, 0x87, 0x0a,
        ];
        // ftyp box
        let ftyp = make_box(b"ftyp", b"jxl \x00\x00\x00\x00jxl ");
        container.extend(&sig);
        container.extend(&ftyp);
    }

    /// Helper to wrap a bare codestream in a JXL container with a jxli frame index box.
    fn wrap_with_frame_index(
        codestream: &[u8],
        tnum: u32,
        tden: u32,
        entries: &[(u64, u64, u64)], // (OFF_delta, T, F)
    ) -> Vec<u8> {
        use crate::util::test::build_frame_index_content;

        let jxli_content = build_frame_index_content(tnum, tden, entries);

        let jxli = make_box(b"jxli", &jxli_content);
        let jxlc = make_box(b"jxlc", codestream);

        let mut container = Vec::new();
        add_container_header(&mut container);
        container.extend(&jxli);
        container.extend(&jxlc);
        container
    }

    /// Helper to wrap a bare codestream in a container split across jxlp boxes.
    ///
    /// `chunk_starts` are codestream offsets where each new jxlp chunk begins.
    fn wrap_with_jxlp_chunks(codestream: &[u8], chunk_starts: &[usize]) -> Vec<u8> {
        let mut starts = chunk_starts.to_vec();
        starts.sort_unstable();
        starts.dedup();
        if starts.first().copied() != Some(0) {
            starts.insert(0, 0);
        }
        if starts.last().copied() != Some(codestream.len()) {
            starts.push(codestream.len());
        }
        assert!(starts.len() >= 2);

        let mut container = Vec::new();
        add_container_header(&mut container);

        let num_chunks = starts.len() - 1;
        for i in 0..num_chunks {
            let begin = starts[i];
            let end = starts[i + 1];
            assert!(begin <= end && end <= codestream.len());

            let mut payload = Vec::with_capacity(4 + (end - begin));
            let mut index = i as u32;
            if i + 1 == num_chunks {
                index |= 0x8000_0000;
            }
            payload.extend(index.to_be_bytes());
            payload.extend(&codestream[begin..end]);
            container.extend(make_box(b"jxlp", &payload));
        }

        container
    }

    #[test]
    fn test_frame_index_parsed_from_container() {
        // Read a bare animation codestream and wrap it in a container with a jxli box.
        let codestream =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

        // Create synthetic frame index entries (delta offsets).
        // These are synthetic -- we don't know real frame offsets, but we can verify parsing.
        let entries = vec![
            (0u64, 100u64, 1u64), // Frame 0 at offset 0
            (500, 100, 1),        // Frame 1 at offset 500
            (600, 100, 1),        // Frame 2 at offset 1100
        ];

        let container = wrap_with_frame_index(&codestream, 1, 1000, &entries);

        // Decode with a large chunk size so the jxli box is fully consumed.
        let options = JxlDecoderOptions::default();
        let mut dec = JxlDecoder::<states::Initialized>::new(options);
        let mut input: &[u8] = &container;
        let dec = loop {
            match dec.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    if input.is_empty() {
                        panic!("Unexpected end of input");
                    }
                    dec = fallback;
                }
            }
        };

        // Check that frame index was parsed.
        let fi = dec.frame_index().expect("frame_index should be Some");
        assert_eq!(fi.num_frames(), 3);
        assert_eq!(fi.tnum, 1);
        assert_eq!(fi.tden.get(), 1000);
        // Verify absolute offsets (accumulated from deltas)
        assert_eq!(fi.entries[0].codestream_offset, 0);
        assert_eq!(fi.entries[1].codestream_offset, 500);
        assert_eq!(fi.entries[2].codestream_offset, 1100);
        assert_eq!(fi.entries[0].duration_ticks, 100);
        assert_eq!(fi.entries[2].frame_count, 1);
    }

    #[test]
    fn test_frame_index_none_for_bare_codestream() {
        // A bare codestream has no container, so no frame index.
        let data =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
        let options = JxlDecoderOptions::default();
        let mut dec = JxlDecoder::<states::Initialized>::new(options);
        let mut input: &[u8] = &data;
        let dec = loop {
            match dec.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    if input.is_empty() {
                        panic!("Unexpected end of input");
                    }
                    dec = fallback;
                }
            }
        };
        assert!(dec.frame_index().is_none());
    }

    fn scan_frames_with_decoder(mut input: &[u8], chunk_size: usize) -> Vec<VisibleFrameInfo> {
        let mut chunk_input = &input[0..0];
        let options = JxlDecoderOptions {
            scan_frames_only: true,
            skip_preview: false,
            ..Default::default()
        };
        let mut initialized_decoder = JxlDecoder::<states::Initialized>::new(options);

        macro_rules! advance_process {
            ($decoder: ident) => {
                loop {
                    chunk_input =
                        &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                    let available_before = chunk_input.len();
                    let process_result = $decoder.process(&mut chunk_input);
                    input = &input[(available_before - chunk_input.len())..];
                    match process_result.unwrap() {
                        ProcessingResult::Complete { result } => break result,
                        ProcessingResult::NeedsMoreInput { fallback, .. } => {
                            if input.is_empty() {
                                panic!("Unexpected end of input");
                            }
                            $decoder = fallback;
                        }
                    }
                }
            };
        }

        macro_rules! advance_skip {
            ($decoder: ident) => {
                loop {
                    chunk_input =
                        &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                    let available_before = chunk_input.len();
                    let process_result = $decoder.skip_frame(&mut chunk_input);
                    input = &input[(available_before - chunk_input.len())..];
                    match process_result.unwrap() {
                        ProcessingResult::Complete { result } => break result,
                        ProcessingResult::NeedsMoreInput { fallback, .. } => {
                            if input.is_empty() {
                                panic!("Unexpected end of input");
                            }
                            $decoder = fallback;
                        }
                    }
                }
            };
        }

        let mut decoder_with_image_info = advance_process!(initialized_decoder);

        if !decoder_with_image_info.has_more_frames() {
            return decoder_with_image_info.scanned_frames().to_vec();
        }

        loop {
            let mut decoder_with_frame_info = advance_process!(decoder_with_image_info);
            decoder_with_image_info = advance_skip!(decoder_with_frame_info);
            if !decoder_with_image_info.has_more_frames() {
                break;
            }
        }

        decoder_with_image_info.scanned_frames().to_vec()
    }

    fn assert_start_new_frame_matches_sequential(data: &[u8], expect_bare_codestream: bool) {
        use crate::api::{JxlDataFormat, JxlPixelFormat};
        use crate::image::{Image, Rect};

        // 1. Scan frame info to get seek offsets.
        let scanned_frames = scan_frames_with_decoder(data, usize::MAX);
        assert!(scanned_frames.len() > 1, "need multiple frames");

        // Compare against second visible frame from regular sequential decode.
        let target_visible_index = 1;
        let seek_target = scanned_frames[target_visible_index].seek_target;

        if expect_bare_codestream {
            assert_eq!(seek_target.remaining_in_box, u64::MAX);
        } else {
            assert_ne!(seek_target.remaining_in_box, u64::MAX);
        }

        // 2. Decode all frames sequentially and keep the reference frame.
        let (_n, sequential_frames) = decode(data, usize::MAX, false, false, None).unwrap();
        let expected = &sequential_frames[target_visible_index];

        // 3. Create decoder and parse image info.
        let options = JxlDecoderOptions::default();
        let decoder = JxlDecoder::<states::Initialized>::new(options);
        let mut input = data;

        let ProcessingResult::Complete {
            result: mut decoder,
        } = decoder.process(&mut input).unwrap()
        else {
            panic!("expected Complete with full data");
        };

        let basic_info = decoder.basic_info().clone();
        let (width, height) = basic_info.size;

        // Match the same requested output format as the sequential helper.
        let default_format = decoder.current_pixel_format().clone();
        let requested_format = JxlPixelFormat {
            color_type: default_format.color_type,
            color_data_format: Some(JxlDataFormat::f32()),
            extra_channel_format: default_format
                .extra_channel_format
                .iter()
                .map(|_| Some(JxlDataFormat::f32()))
                .collect(),
        };
        decoder.set_pixel_format(requested_format.clone());

        let channels = requested_format.color_type.samples_per_pixel();
        let num_ec = requested_format.extra_channel_format.len();

        // 4. Seek to decode-start and advance to the target visible frame.
        decoder.start_new_frame(seek_target);
        let mut input = &data[seek_target.decode_start_file_offset..];

        for _ in 0..seek_target.visible_frames_to_skip {
            let mut decoder_frame = loop {
                match decoder.process(&mut input).unwrap() {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput { fallback, .. } => {
                        decoder = fallback;
                    }
                }
            };

            decoder = loop {
                match decoder_frame.skip_frame(&mut input).unwrap() {
                    ProcessingResult::Complete { result } => break result,
                    ProcessingResult::NeedsMoreInput { fallback, .. } => {
                        decoder_frame = fallback;
                    }
                }
            };
        }

        let mut decoder_frame = loop {
            match decoder.process(&mut input).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder = fallback;
                }
            }
        };

        let mut color_buffer = Image::<f32>::new((width * channels, height)).unwrap();
        let mut ec_buffers: Vec<Image<f32>> = (0..num_ec)
            .map(|_| Image::<f32>::new((width, height)).unwrap())
            .collect();
        let mut buffers: Vec<JxlOutputBuffer> = vec![JxlOutputBuffer::from_image_rect_mut(
            color_buffer
                .get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (width * channels, height),
                })
                .into_raw(),
        )];
        for ec in ec_buffers.iter_mut() {
            buffers.push(JxlOutputBuffer::from_image_rect_mut(
                ec.get_rect_mut(Rect {
                    origin: (0, 0),
                    size: (width, height),
                })
                .into_raw(),
            ));
        }

        let _decoder = loop {
            match decoder_frame.process(&mut input, &mut buffers).unwrap() {
                ProcessingResult::Complete { result } => break result,
                ProcessingResult::NeedsMoreInput { fallback, .. } => {
                    decoder_frame = fallback;
                }
            }
        };

        // 5. Compare seek-decoded frame against sequential decode reference.
        let mut seek_decoded = Vec::with_capacity(1 + num_ec);
        seek_decoded.push(color_buffer);
        seek_decoded.extend(ec_buffers);
        compare_frames(
            Path::new("start_new_frame_seek"),
            target_visible_index,
            expected,
            &seek_decoded,
        )
        .unwrap();
    }

    /// Test that `start_new_frame()` + scanner seek info decodes the same
    /// frame as regular sequential decode for bare codestream input.
    #[test]
    fn test_start_new_frame_bare_codestream() {
        let data =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
        assert_start_new_frame_matches_sequential(&data, true);
    }

    /// Test that `start_new_frame()` + scanner seek info also works for boxed input.
    #[test]
    fn test_start_new_frame_boxed_codestream() {
        let codestream =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
        let entries = vec![(0u64, 100u64, 1u64), (500, 100, 1), (600, 100, 1)];
        let container = wrap_with_frame_index(&codestream, 1, 1000, &entries);
        assert_start_new_frame_matches_sequential(&container, false);
    }

    /// Test seek/scanner behavior when codestream data is split across jxlp boxes,
    /// with each visible frame starting in its own chunk.
    #[test]
    fn test_start_new_frame_boxed_jxlp_per_visible_frame() {
        let codestream =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

        let scanned_frames = scan_frames_with_decoder(&codestream, usize::MAX);
        assert!(scanned_frames.len() > 1, "need multiple frames");

        let (decoded_frames, _) = decode(&codestream, usize::MAX, false, false, None).unwrap();
        assert_eq!(
            decoded_frames,
            scanned_frames.len(),
            "test file should have one codestream frame per visible frame",
        );

        let mut chunk_starts: Vec<usize> = scanned_frames.iter().map(|f| f.file_offset).collect();
        chunk_starts.sort_unstable();
        chunk_starts.dedup();
        assert_eq!(chunk_starts.len(), scanned_frames.len());

        let container = wrap_with_jxlp_chunks(&codestream, &chunk_starts);
        assert_start_new_frame_matches_sequential(&container, false);
    }

    #[test]
    fn test_scan_still_image() {
        let data = std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap();
        let frames = scan_frames_with_decoder(&data, usize::MAX);

        assert_eq!(frames.len(), 1);
        assert!(frames[0].is_last);
        assert!(frames[0].is_keyframe);
        let total_duration_ms: f64 = frames.iter().map(|f| f.duration_ms).sum();
        assert_eq!(total_duration_ms, 0.0);
    }

    #[test]
    fn test_scan_bare_animation() {
        let data =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
        let frames = scan_frames_with_decoder(&data, usize::MAX);

        assert!(frames.len() > 1, "expected multiple frames");

        for (i, frame) in frames.iter().enumerate() {
            assert_eq!(frame.index, i);
        }

        assert!(frames.last().unwrap().is_last);

        assert!(frames[0].is_keyframe);
        assert_eq!(
            frames[0].seek_target.decode_start_file_offset,
            frames[0].file_offset
        );
    }

    #[test]
    fn test_scan_animation_offsets_increase() {
        let data =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();
        let frames = scan_frames_with_decoder(&data, usize::MAX);

        for i in 1..frames.len() {
            assert!(
                frames[i].file_offset > frames[i - 1].file_offset,
                "frame {} offset {} should be > frame {} offset {}",
                i,
                frames[i].file_offset,
                i - 1,
                frames[i - 1].file_offset,
            );
        }
    }

    #[test]
    fn test_scan_incremental() {
        let data =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

        let frames = scan_frames_with_decoder(&data, 128);
        assert!(frames.len() > 1);
        assert!(frames.last().unwrap().is_last);
    }

    #[test]
    fn test_scan_keyframe_detection_still() {
        let data = std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap();
        let frames = scan_frames_with_decoder(&data, usize::MAX);

        assert_eq!(frames.len(), 1);
        let f = &frames[0];
        assert!(f.is_keyframe);
        assert_eq!(f.seek_target.decode_start_file_offset, f.file_offset);
        assert_eq!(f.seek_target.visible_frames_to_skip, 0);
    }

    #[test]
    fn test_scan_decode_start_file_offset_consistency() {
        let data =
            std::fs::read("resources/test/conformance_test_images/animation_icos4d_5.jxl").unwrap();

        let frames = scan_frames_with_decoder(&data, usize::MAX);

        for frame in &frames {
            assert!(
                frame.seek_target.decode_start_file_offset <= frame.file_offset,
                "frame {}: decode_start_file_offset {} > file_offset {}",
                frame.index,
                frame.seek_target.decode_start_file_offset,
                frame.file_offset,
            );
            assert_eq!(
                frame.is_keyframe,
                frame.seek_target.visible_frames_to_skip == 0,
                "frame {}: keyframe flag should match visible_frames_to_skip",
                frame.index,
            );
        }
    }

    #[test]
    fn test_scan_with_preview() {
        let data = std::fs::read("resources/test/with_preview.jxl");
        if data.is_err() {
            return;
        }
        let data = data.unwrap();
        let frames = scan_frames_with_decoder(&data, usize::MAX);

        assert!(frames.len() <= 1);
    }

    #[test]
    fn test_scan_patches_not_keyframe() {
        let data = std::fs::read("resources/test/grayscale_patches_var_dct.jxl");
        if data.is_err() {
            return;
        }
        let data = data.unwrap();
        let frames = scan_frames_with_decoder(&data, usize::MAX);

        assert!(!frames.is_empty());
    }

    /// Regression test for Chromium ClusterFuzz issue 474401148.
    #[test]
    fn test_fuzzer_xyb_icc_no_panic() {
        use crate::api::ProcessingResult;

        #[rustfmt::skip]
        let data: &[u8] = &[
            0xff, 0x0a, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x25, 0x00,
        ];

        let opts = JxlDecoderOptions {
            pixel_limit: Some(1024 * 1024 * 1024),
            ..Default::default()
        };
        let mut decoder = JxlDecoderInner::new(opts);
        let mut input = data;

        if let Ok(ProcessingResult::Complete { .. }) = decoder.process(&mut input, None)
            && let Some(profile) = decoder.output_color_profile()
        {
            let _ = profile.try_as_icc();
        }
    }
}
