// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Minimal C++ wrapper for jxl-rs decoder.
//!
//! Two decoder types are exposed:
//!
//! - `JxlRsFrameScanner`: lightweight frame-header-only scanner that discovers
//!   frame count, durations, and seek offsets without decoding any pixels.
//! - `JxlRsDecoder`: full pixel decoder with a state-machine API,
//!   plus seeking and progressive flush support.

use jxl::api::{
    check_signature, Endianness, JxlBasicInfo, JxlColorEncoding, JxlColorProfile, JxlColorType,
    JxlDataFormat, JxlDecoderInner, JxlDecoderOptions, JxlOutputBuffer, JxlPixelFormat,
    JxlProgressiveMode, ProcessingResult, VisibleFrameInfo,
};
use jxl::headers::extra_channels::ExtraChannel;

#[cxx::bridge(namespace = "blink::jxl_rs")]
mod ffi {
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum JxlRsStatus {
        Success = 0,
        Error = 1,
        NeedMoreInput = 2,
    }

    #[derive(Debug, Clone, Copy)]
    enum JxlRsPixelFormat {
        Rgba8 = 0,
        Rgba16 = 1,
        RgbaF16 = 2,
        RgbaF32 = 3,
        Bgra8 = 4,
    }

    #[derive(Debug, Clone)]
    struct JxlRsBasicInfo {
        width: u32,
        height: u32,
        bits_per_sample: u32,
        num_extra_channels: u32,
        has_alpha: bool,
        alpha_premultiplied: bool,
        have_animation: bool,
        animation_loop_count: u32,
        animation_tps_numerator: u32,
        animation_tps_denominator: u32,
        uses_original_profile: bool,
        orientation: u32,
        is_grayscale: bool,
    }

    #[derive(Debug, Clone, Default)]
    struct JxlRsFrameHeader {
        duration_ms: f64,
        name_length: u32,
    }

    /// Result of a process call.
    #[derive(Debug, Clone)]
    struct JxlRsProcessResult {
        status: JxlRsStatus,
        bytes_consumed: usize,
    }

    /// Information about a single visible frame discovered by the scanner.
    #[derive(Debug, Clone, Default)]
    struct JxlRsVisibleFrameInfo {
        /// Duration in milliseconds.
        duration_ms: f64,
        /// Whether this frame can be decoded independently (no dependencies).
        is_keyframe: bool,
        /// Whether this is the last frame in the codestream.
        is_last: bool,
        /// File byte offset to start feeding input from when seeking.
        decode_start_file_offset: usize,
    }

    extern "Rust" {
        // ---- Frame scanner (lightweight, no pixel decoding) ----
        type JxlRsFrameScanner;

        fn jxl_rs_frame_scanner_create(pixel_limit: u64) -> Box<JxlRsFrameScanner>;

        /// Feed data to the scanner. Returns Success when all frames have been
        /// scanned (last frame seen), NeedMoreInput if more data is needed, or
        /// Error on failure.
        fn feed(
            self: &mut JxlRsFrameScanner,
            data: &[u8],
            all_input: bool,
        ) -> JxlRsProcessResult;

        /// Get basic info (valid after first successful feed).
        fn get_basic_info(self: &JxlRsFrameScanner) -> JxlRsBasicInfo;

        /// Get ICC profile data.
        fn get_icc_profile(self: &JxlRsFrameScanner) -> &[u8];

        /// Number of visible frames discovered so far.
        fn frame_count(self: &JxlRsFrameScanner) -> usize;

        /// Get info for a specific frame index.
        fn get_frame_info(self: &JxlRsFrameScanner, index: usize) -> JxlRsVisibleFrameInfo;

        /// Whether basic info has been parsed.
        fn has_basic_info(self: &JxlRsFrameScanner) -> bool;

        // ---- Full pixel decoder ----
        type JxlRsDecoder;

        fn jxl_rs_decoder_create(pixel_limit: u64, premultiply_alpha: bool) -> Box<JxlRsDecoder>;
        fn jxl_rs_signature_check(data: &[u8]) -> bool;

        /// Set the output pixel format. Must be called after getting basic info.
        fn set_pixel_format(
            self: &mut JxlRsDecoder,
            format: JxlRsPixelFormat,
            num_extra_channels: u32,
        );

        /// Parse headers until basic info is available.
        fn parse_basic_info(
            self: &mut JxlRsDecoder,
            data: &[u8],
            all_input: bool,
        ) -> JxlRsProcessResult;

        /// Parse until next frame header is available.
        fn parse_frame_header(
            self: &mut JxlRsDecoder,
            data: &[u8],
            all_input: bool,
        ) -> JxlRsProcessResult;

        /// Decode frame pixels with custom stride (for direct frame buffer
        /// decoding).
        fn decode_frame_with_stride(
            self: &mut JxlRsDecoder,
            data: &[u8],
            all_input: bool,
            buffer: &mut [u8],
            width: u32,
            height: u32,
            row_stride: usize,
        ) -> JxlRsProcessResult;

        /// Flush whatever pixels have been decoded so far into the buffer.
        /// Used for progressive rendering.
        fn flush_pixels(
            self: &mut JxlRsDecoder,
            buffer: &mut [u8],
            width: u32,
            height: u32,
            row_stride: usize,
        ) -> JxlRsProcessResult;

        /// Get basic info (valid after parse_basic_info succeeds).
        fn get_basic_info(self: &JxlRsDecoder) -> JxlRsBasicInfo;

        /// Get frame header (valid after parse_frame_header succeeds).
        fn get_frame_header(self: &JxlRsDecoder) -> JxlRsFrameHeader;

        /// Get ICC profile data (valid after parse_basic_info succeeds).
        /// Returns an empty slice if no embedded ICC profile exists.
        fn get_icc_profile(self: &JxlRsDecoder) -> &[u8];

        /// Check if more frames are available.
        fn has_more_frames(self: &JxlRsDecoder) -> bool;

        /// Seek the decoder to a specific frame discovered by the scanner.
        /// Looks up the full seek target from the scanner's internal data
        /// and configures the decoder. After calling this, provide input
        /// starting from `get_frame_info(index).decode_start_file_offset`.
        fn jxl_rs_seek_decoder_to_frame(
            scanner: &JxlRsFrameScanner,
            decoder: &mut JxlRsDecoder,
            index: usize,
        );
    }
}

use ffi::*;

// ---------------------------------------------------------------------------
// Frame Scanner
// ---------------------------------------------------------------------------

/// Lightweight scanner that discovers frame info without decoding pixels.
pub struct JxlRsFrameScanner {
    decoder: JxlDecoderInner,
    icc_profile: Vec<u8>,
    has_basic_info: bool,
}

fn jxl_rs_frame_scanner_create(pixel_limit: u64) -> Box<JxlRsFrameScanner> {
    let mut opts = JxlDecoderOptions::default();
    opts.scan_frames_only = true;
    if pixel_limit > 0 {
        opts.pixel_limit = Some(pixel_limit as usize);
    }

    Box::new(JxlRsFrameScanner {
        decoder: JxlDecoderInner::new(opts),
        icc_profile: Vec::new(),
        has_basic_info: false,
    })
}

impl JxlRsFrameScanner {
    fn feed(&mut self, data: &[u8], all_input: bool) -> JxlRsProcessResult {
        let mut input = data;
        let len_before = input.len();

        loop {
            match self.decoder.process(&mut input, None) {
                Ok(ProcessingResult::Complete { .. }) => {
                    if !self.has_basic_info && self.decoder.basic_info().is_some() {
                        self.has_basic_info = true;
                        if let Some(profile) = self.decoder.output_color_profile() {
                            if let Some(icc) = profile.try_as_icc() {
                                if !icc.is_empty() {
                                    self.icc_profile = icc.into_owned();
                                }
                            }
                        }
                    }

                    if !self.decoder.has_more_frames() {
                        return JxlRsProcessResult {
                            status: JxlRsStatus::Success,
                            bytes_consumed: len_before - input.len(),
                        };
                    }
                }
                Ok(ProcessingResult::NeedsMoreInput { .. }) => {
                    return JxlRsProcessResult {
                        status: if all_input {
                            JxlRsStatus::Error
                        } else {
                            JxlRsStatus::NeedMoreInput
                        },
                        bytes_consumed: len_before - input.len(),
                    };
                }
                Err(_) => {
                    return JxlRsProcessResult {
                        status: JxlRsStatus::Error,
                        bytes_consumed: 0,
                    };
                }
            }
        }
    }

    fn get_basic_info(&self) -> JxlRsBasicInfo {
        let mut info = self
            .decoder
            .basic_info()
            .map(JxlRsBasicInfo::from)
            .unwrap_or_default();

        if let Some(profile) = self.decoder.embedded_color_profile() {
            info.is_grayscale = matches!(
                profile,
                JxlColorProfile::Simple(JxlColorEncoding::GrayscaleColorSpace { .. })
            );
        }

        info
    }

    fn get_icc_profile(&self) -> &[u8] {
        &self.icc_profile
    }

    fn frame_count(&self) -> usize {
        self.decoder.scanned_frames().len()
    }

    fn get_frame_info(&self, index: usize) -> JxlRsVisibleFrameInfo {
        let frames = self.decoder.scanned_frames();
        frames
            .get(index)
            .map(JxlRsVisibleFrameInfo::from)
            .unwrap_or_default()
    }

    fn has_basic_info(&self) -> bool {
        self.has_basic_info
    }
}

// ---------------------------------------------------------------------------
// Full Pixel Decoder
// ---------------------------------------------------------------------------

/// Full pixel decoder with seeking and progressive flush support.
pub struct JxlRsDecoder {
    decoder: JxlDecoderInner,
    pixel_format: Option<JxlPixelFormat>,
    icc_profile: Vec<u8>,
}

fn jxl_rs_decoder_create(pixel_limit: u64, premultiply_alpha: bool) -> Box<JxlRsDecoder> {
    let mut opts = JxlDecoderOptions::default();
    opts.progressive_mode = JxlProgressiveMode::Pass;
    opts.premultiply_output = premultiply_alpha;
    if pixel_limit > 0 {
        opts.pixel_limit = Some(pixel_limit as usize);
    }

    Box::new(JxlRsDecoder {
        decoder: JxlDecoderInner::new(opts),
        pixel_format: None,
        icc_profile: Vec::new(),
    })
}

fn jxl_rs_signature_check(data: &[u8]) -> bool {
    data.len() >= 2
        && matches!(
            check_signature(&data[..data.len().min(12)]),
            ProcessingResult::Complete { result: Some(_) }
        )
}

impl JxlRsDecoder {
    fn set_pixel_format(&mut self, format: JxlRsPixelFormat, num_extra_channels: u32) {
        let pixel_format = match format {
            JxlRsPixelFormat::Rgba8 => JxlPixelFormat {
                color_type: JxlColorType::Rgba,
                color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
                extra_channel_format: vec![None; num_extra_channels as usize],
            },
            JxlRsPixelFormat::Rgba16 => JxlPixelFormat {
                color_type: JxlColorType::Rgba,
                color_data_format: Some(JxlDataFormat::U16 {
                    endianness: Endianness::native(),
                    bit_depth: 16,
                }),
                extra_channel_format: vec![None; num_extra_channels as usize],
            },
            JxlRsPixelFormat::RgbaF16 => JxlPixelFormat {
                color_type: JxlColorType::Rgba,
                color_data_format: Some(JxlDataFormat::F16 {
                    endianness: Endianness::native(),
                }),
                extra_channel_format: vec![None; num_extra_channels as usize],
            },
            JxlRsPixelFormat::RgbaF32 => JxlPixelFormat {
                color_type: JxlColorType::Rgba,
                color_data_format: Some(JxlDataFormat::F32 {
                    endianness: Endianness::native(),
                }),
                extra_channel_format: vec![None; num_extra_channels as usize],
            },
            JxlRsPixelFormat::Bgra8 => JxlPixelFormat {
                color_type: JxlColorType::Bgra,
                color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
                extra_channel_format: vec![None; num_extra_channels as usize],
            },
            _ => JxlPixelFormat {
                color_type: JxlColorType::Rgba,
                color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
                extra_channel_format: vec![None; num_extra_channels as usize],
            },
        };
        self.decoder.set_pixel_format(pixel_format.clone());
        self.pixel_format = Some(pixel_format);
    }

    fn parse_basic_info(&mut self, data: &[u8], all_input: bool) -> JxlRsProcessResult {
        let mut input = data;
        let len_before = input.len();

        match self.decoder.process(&mut input, None) {
            Ok(ProcessingResult::Complete { .. }) => {
                // Extract ICC profile on first successful parse.
                // Use try_as_icc() which returns None on error instead of
                // as_icc() which panics on malformed color profiles.
                if self.icc_profile.is_empty() {
                    if let Some(profile) = self.decoder.output_color_profile() {
                        if let Some(icc) = profile.try_as_icc() {
                            if !icc.is_empty() {
                                self.icc_profile = icc.into_owned();
                            }
                        }
                    }
                }
                JxlRsProcessResult {
                    status: JxlRsStatus::Success,
                    bytes_consumed: len_before - input.len(),
                }
            }
            Ok(ProcessingResult::NeedsMoreInput { .. }) => {
                if all_input {
                    JxlRsProcessResult {
                        status: JxlRsStatus::Error,
                        bytes_consumed: 0,
                    }
                } else {
                    JxlRsProcessResult {
                        status: JxlRsStatus::NeedMoreInput,
                        bytes_consumed: len_before - input.len(),
                    }
                }
            }
            Err(_) => JxlRsProcessResult {
                status: JxlRsStatus::Error,
                bytes_consumed: 0,
            },
        }
    }

    fn parse_frame_header(&mut self, data: &[u8], all_input: bool) -> JxlRsProcessResult {
        let mut input = data;
        let len_before = input.len();

        match self.decoder.process(&mut input, None) {
            Ok(ProcessingResult::Complete { .. }) => JxlRsProcessResult {
                status: JxlRsStatus::Success,
                bytes_consumed: len_before - input.len(),
            },
            Ok(ProcessingResult::NeedsMoreInput { .. }) => {
                if all_input {
                    JxlRsProcessResult {
                        status: JxlRsStatus::Error,
                        bytes_consumed: 0,
                    }
                } else {
                    JxlRsProcessResult {
                        status: JxlRsStatus::NeedMoreInput,
                        bytes_consumed: len_before - input.len(),
                    }
                }
            }
            Err(_) => JxlRsProcessResult {
                status: JxlRsStatus::Error,
                bytes_consumed: 0,
            },
        }
    }

    /// Build a JxlOutputBuffer for the given buffer dimensions.
    /// Panics if the buffer is too small (programming error).
    fn make_output_buffer<'a>(
        &self,
        buffer: &'a mut [u8],
        width: u32,
        height: u32,
        row_stride: usize,
    ) -> JxlOutputBuffer<'a> {
        let bytes_per_pixel = self
            .pixel_format
            .as_ref()
            .and_then(|f| f.color_data_format.as_ref())
            .map(|d| d.bytes_per_sample() * 4)
            .unwrap_or(4);
        let bytes_per_row = width as usize * bytes_per_pixel;

        JxlOutputBuffer::new_with_stride(buffer, height as usize, bytes_per_row, row_stride)
    }

    fn decode_frame_with_stride(
        &mut self,
        data: &[u8],
        all_input: bool,
        buffer: &mut [u8],
        width: u32,
        height: u32,
        row_stride: usize,
    ) -> JxlRsProcessResult {
        let mut input = data;
        let len_before = input.len();

        let output = self.make_output_buffer(buffer, width, height, row_stride);

        match self.decoder.process(&mut input, Some(&mut [output])) {
            Ok(ProcessingResult::Complete { .. }) => JxlRsProcessResult {
                status: JxlRsStatus::Success,
                bytes_consumed: len_before - input.len(),
            },
            Ok(ProcessingResult::NeedsMoreInput { .. }) => {
                if all_input {
                    JxlRsProcessResult {
                        status: JxlRsStatus::Error,
                        bytes_consumed: 0,
                    }
                } else {
                    JxlRsProcessResult {
                        status: JxlRsStatus::NeedMoreInput,
                        bytes_consumed: len_before - input.len(),
                    }
                }
            }
            Err(_) => JxlRsProcessResult {
                status: JxlRsStatus::Error,
                bytes_consumed: 0,
            },
        }
    }

    fn flush_pixels(
        &mut self,
        buffer: &mut [u8],
        width: u32,
        height: u32,
        row_stride: usize,
    ) -> JxlRsProcessResult {
        let output = self.make_output_buffer(buffer, width, height, row_stride);

        match self.decoder.flush_pixels(&mut [output]) {
            Ok(()) => JxlRsProcessResult {
                status: JxlRsStatus::Success,
                bytes_consumed: 0,
            },
            Err(_) => JxlRsProcessResult {
                status: JxlRsStatus::Error,
                bytes_consumed: 0,
            },
        }
    }

    fn get_basic_info(&self) -> JxlRsBasicInfo {
        let mut info = self
            .decoder
            .basic_info()
            .map(JxlRsBasicInfo::from)
            .unwrap_or_default();

        // Check if the image is grayscale based on the embedded color profile.
        if let Some(profile) = self.decoder.embedded_color_profile() {
            info.is_grayscale = matches!(
                profile,
                JxlColorProfile::Simple(JxlColorEncoding::GrayscaleColorSpace { .. })
            );
        }

        info
    }

    fn extract_frame_header(&self) -> Option<JxlRsFrameHeader> {
        let fh = self.decoder.frame_header()?;
        Some(JxlRsFrameHeader {
            duration_ms: fh.duration.unwrap_or(0.0),
            name_length: fh.name.len() as u32,
        })
    }

    fn get_frame_header(&self) -> JxlRsFrameHeader {
        self.extract_frame_header().unwrap_or_default()
    }

    fn get_icc_profile(&self) -> &[u8] {
        &self.icc_profile
    }

    fn has_more_frames(&self) -> bool {
        self.decoder.has_more_frames()
    }
}

/// Seek the decoder to a specific frame discovered by the scanner.
/// The full seek target (including internal fields like remaining_in_box
/// and visible_frames_to_skip) is looked up from the scanner's data.
fn jxl_rs_seek_decoder_to_frame(
    scanner: &JxlRsFrameScanner,
    decoder: &mut JxlRsDecoder,
    index: usize,
) {
    let frames = scanner.decoder.scanned_frames();
    let frame = &frames[index];
    decoder.decoder.start_new_frame(frame.seek_target);
}

impl Default for JxlRsBasicInfo {
    fn default() -> Self {
        Self {
            width: 0,
            height: 0,
            bits_per_sample: 8,
            num_extra_channels: 0,
            has_alpha: false,
            alpha_premultiplied: false,
            have_animation: false,
            animation_loop_count: 0,
            animation_tps_numerator: 1,
            animation_tps_denominator: 1000,
            uses_original_profile: false,
            orientation: 1,
            is_grayscale: false,
        }
    }
}

impl From<&VisibleFrameInfo> for JxlRsVisibleFrameInfo {
    fn from(f: &VisibleFrameInfo) -> Self {
        Self {
            duration_ms: f.duration_ms,
            is_keyframe: f.is_keyframe,
            is_last: f.is_last,
            decode_start_file_offset: f.seek_target.decode_start_file_offset,
        }
    }
}

impl From<&JxlBasicInfo> for JxlRsBasicInfo {
    fn from(info: &JxlBasicInfo) -> Self {
        let has_alpha = info
            .extra_channels
            .iter()
            .any(|ec| matches!(ec.ec_type, ExtraChannel::Alpha));
        let (loop_count, tps_num, tps_den) = info
            .animation
            .as_ref()
            .map(|a| (a.num_loops, a.tps_numerator, a.tps_denominator))
            .unwrap_or((0, 1, 1000));
        Self {
            width: info.size.0 as u32,
            height: info.size.1 as u32,
            bits_per_sample: info.bit_depth.bits_per_sample(),
            num_extra_channels: info.extra_channels.len() as u32,
            has_alpha,
            alpha_premultiplied: false,
            have_animation: info.animation.is_some(),
            animation_loop_count: loop_count,
            animation_tps_numerator: tps_num,
            animation_tps_denominator: tps_den,
            uses_original_profile: info.uses_original_profile,
            orientation: info.orientation as u32,
            // Note: is_grayscale is set by get_basic_info() after checking the
            // color profile, since JxlBasicInfo doesn't contain color info.
            is_grayscale: false,
        }
    }
}
