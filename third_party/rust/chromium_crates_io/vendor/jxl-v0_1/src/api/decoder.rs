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
use crate::{api::JxlFrameHeader, error::Result};
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
    // TODO(veluca): once frame skipping is implemented properly, expose that in the API.

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

    pub fn current_pixel_format(&self) -> &JxlPixelFormat {
        self.inner.current_pixel_format().unwrap()
    }

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

    pub fn has_more_frames(&self) -> bool {
        self.inner.has_more_frames()
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.inner.set_use_simple_pipeline(u);
    }
}

impl JxlDecoder<WithFrameInfo> {
    /// Skip the current frame.
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
    pub fn flush_pixels(&mut self, buffers: &mut [JxlOutputBuffer<'_>]) -> Result<()> {
        self.inner.flush_pixels(buffers)
    }

    /// Guarantees to populate exactly the appropriate part of the buffers.
    /// Wants one buffer for each non-ignored pixel type, i.e. color channels and each extra channel.
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
    use crate::util::test::assert_almost_abs_eq_coords;
    use jxl_macros::for_each_test_file;
    use std::path::Path;

    #[test]
    fn decode_small_chunks() {
        arbtest::arbtest(|u| {
            decode(
                &std::fs::read("resources/test/green_queen_vardct_e3.jxl").unwrap(),
                u.arbitrary::<u8>().unwrap() as usize + 1,
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
        callback: Option<Box<dyn FnMut(&Frame, usize) -> Result<(), Error>>>,
    ) -> Result<(usize, Vec<Vec<Image<f32>>>), Error> {
        let options = JxlDecoderOptions::default();
        let mut initialized_decoder = JxlDecoder::<states::Initialized>::new(options);

        if let Some(callback) = callback {
            initialized_decoder.set_frame_callback(callback);
        }

        let mut chunk_input = &input[0..0];

        macro_rules! advance_decoder {
            ($decoder: ident $(, $extra_arg: expr)?) => {
                loop {
                    chunk_input =
                        &input[..(chunk_input.len().saturating_add(chunk_size)).min(input.len())];
                    let available_before = chunk_input.len();
                    let process_result = $decoder.process(&mut chunk_input $(, $extra_arg)?);
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
            // Process until we have frame info
            let mut decoder_with_frame_info = advance_decoder!(decoder_with_image_info);

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

            decoder_with_image_info = advance_decoder!(decoder_with_frame_info, &mut api_buffers);

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
        decode(&std::fs::read(path)?, usize::MAX, false, None)?;
        Ok(())
    }

    for_each_test_file!(decode_test_file);

    fn decode_test_file_chunks(path: &Path) -> Result<(), Error> {
        decode(&std::fs::read(path)?, 1, false, None)?;
        Ok(())
    }

    for_each_test_file!(decode_test_file_chunks);

    fn compare_pipelines(path: &Path) -> Result<(), Error> {
        let file = std::fs::read(path)?;
        let simple_frames = decode(&file, usize::MAX, true, None)?.1;
        let frames = decode(&file, usize::MAX, false, None)?.1;
        assert_eq!(frames.len(), simple_frames.len());
        for (fc, (f, sf)) in frames
            .into_iter()
            .zip(simple_frames.into_iter())
            .enumerate()
        {
            assert_eq!(
                f.len(),
                sf.len(),
                "Frame {fc} has different channels counts",
            );
            for (c, (b, sb)) in f.into_iter().zip(sf.into_iter()).enumerate() {
                assert_eq!(
                    b.size(),
                    sb.size(),
                    "Channel {c} in frame {fc} has different sizes",
                );
                // TODO(veluca): This check actually succeeds if we disable SIMD.
                // With SIMD, the exact output of computations in epf.rs appear to depend on the
                // lane that the computation was done in (???). We should investigate this.
                // b.as_rect().check_equal(sb.as_rect());
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
                        assert_almost_abs_eq_coords(b.row(y)[x], sb.row(y)[x], 1e-5, (x, y), c);
                    }
                }
            }
        }
        Ok(())
    }

    for_each_test_file!(compare_pipelines);

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
}
