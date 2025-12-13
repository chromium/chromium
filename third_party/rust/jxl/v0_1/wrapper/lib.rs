//! CXX-based FFI wrapper for jxl-rs decoder.
//!
//! This provides a C++-compatible API for the jxl-rs decoder using the CXX crate,
//! designed for integration with Chromium's Blink image decoder infrastructure.

use jxl::api::{
    JxlBasicInfo as InternalBasicInfo, JxlDecoder, JxlDecoderOptions, JxlOutputBuffer,
    JxlProgressiveMode, ProcessingResult, check_signature, states,
};
use jxl::headers::extra_channels::ExtraChannel;
use jxl::image::{Image, Rect};

#[cxx::bridge]
mod ffi {
    /// Status codes returned by decoder operations
    #[derive(Debug)]
    enum JxlRsStatus {
        Success = 0,
        Error = 1,
        NeedMoreInput = 2,
        BasicInfo = 3,
        Frame = 5,
        FullImage = 6,
    }

    /// Pixel format for decoded output
    #[derive(Debug)]
    enum JxlRsPixelFormat {
        Rgba8 = 0,
        Rgba16 = 1,
        RgbaF32 = 2,
    }

    /// Basic image information
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
    }

    /// Frame header information
    #[derive(Debug, Clone)]
    struct JxlRsFrameHeader {
        duration: u32,
        is_last: bool,
        name_length: u32,
    }

    extern "Rust" {
        /// Opaque decoder type
        type JxlRsDecoder;

        /// Create a new decoder instance
        fn jxl_rs_decoder_create() -> Box<JxlRsDecoder>;

        /// Reset decoder to initial state
        fn reset(self: &mut JxlRsDecoder);

        /// Set input data for decoding
        fn set_input(self: &mut JxlRsDecoder, data: &[u8], all_input: bool) -> JxlRsStatus;

        /// Process next decoding step
        fn process(self: &mut JxlRsDecoder) -> JxlRsStatus;

        /// Get basic image information
        fn get_basic_info(self: &JxlRsDecoder) -> JxlRsBasicInfo;

        /// Get frame header information
        fn get_frame_header(self: &JxlRsDecoder) -> JxlRsFrameHeader;

        /// Set output pixel format
        fn set_pixel_format(self: &mut JxlRsDecoder, format: JxlRsPixelFormat);

        /// Get decoded pixels - writes to provided buffer
        fn get_pixels(self: &mut JxlRsDecoder, buffer: &mut [u8]) -> JxlRsStatus;

        /// Get ICC color profile data as slice
        fn get_icc_profile(self: &JxlRsDecoder) -> &[u8];

        /// Get frame count
        fn get_frame_count(self: &JxlRsDecoder) -> u32;

        /// Check if more frames are available
        fn has_more_frames(self: &JxlRsDecoder) -> bool;

        /// Get last error message
        fn get_error(self: &JxlRsDecoder) -> &str;

        /// Check JXL signature in data
        fn jxl_rs_signature_check(data: &[u8]) -> bool;

        /// Get library version string
        fn jxl_rs_version() -> &'static str;
    }
}

// Re-export types for convenience
pub use ffi::{JxlRsBasicInfo, JxlRsFrameHeader, JxlRsPixelFormat, JxlRsStatus};

// =============================================================================
// Decoder Implementation
// =============================================================================

enum DecoderState {
    Initialized(JxlDecoder<states::Initialized>),
    WithImageInfo(JxlDecoder<states::WithImageInfo>),
    WithFrameInfo(JxlDecoder<states::WithFrameInfo>),
    Empty,
}

pub struct JxlRsDecoder {
    state: DecoderState,
    input_buffer: Vec<u8>,
    input_consumed: usize,
    all_input_received: bool,
    basic_info: JxlRsBasicInfo,
    frame_header: JxlRsFrameHeader,
    current_frame: u32,
    frame_count: u32,
    has_more_frames: bool,
    pixel_format: JxlRsPixelFormat,
    icc_profile: Vec<u8>,
    error_message: String,
    rgb_buffer: Vec<f32>,
    alpha_buffer: Vec<f32>,
}

impl JxlRsDecoder {
    fn new() -> Self {
        let mut options = JxlDecoderOptions::default();
        options.xyb_output_linear = false;
        options.progressive_mode = JxlProgressiveMode::FullFrame;

        Self {
            state: DecoderState::Initialized(JxlDecoder::new(options)),
            input_buffer: Vec::new(),
            input_consumed: 0,
            all_input_received: false,
            basic_info: JxlRsBasicInfo::default(),
            frame_header: JxlRsFrameHeader::default(),
            current_frame: 0,
            frame_count: 1,
            has_more_frames: true,
            pixel_format: JxlRsPixelFormat::Rgba8,
            icc_profile: Vec::new(),
            error_message: String::new(),
            rgb_buffer: Vec::new(),
            alpha_buffer: Vec::new(),
        }
    }

    fn set_error(&mut self, msg: &str) {
        self.error_message = msg.to_string();
    }

    fn process_internal(&mut self) -> JxlRsStatus {
        let input_slice = &self.input_buffer[self.input_consumed..];
        if input_slice.is_empty() && !self.all_input_received {
            return JxlRsStatus::NeedMoreInput;
        }

        let state = std::mem::replace(&mut self.state, DecoderState::Empty);

        match state {
            DecoderState::Initialized(decoder) => {
                let mut input = input_slice;
                let input_before = input.len();
                match decoder.process(&mut input) {
                    Ok(ProcessingResult::Complete { result }) => {
                        self.input_consumed += input_before - input.len();
                        self.basic_info = JxlRsBasicInfo::from(result.basic_info());

                        let color_profile = result.embedded_color_profile();
                        let icc = color_profile.as_icc();
                        if !icc.is_empty() {
                            self.icc_profile = icc.into_owned();
                        }
                        self.state = DecoderState::WithImageInfo(result);
                        JxlRsStatus::BasicInfo
                    }
                    Ok(ProcessingResult::NeedsMoreInput { fallback, .. }) => {
                        self.input_consumed += input_before - input.len();
                        self.state = DecoderState::Initialized(fallback);
                        if self.all_input_received {
                            self.set_error("Incomplete JXL header");
                            JxlRsStatus::Error
                        } else {
                            JxlRsStatus::NeedMoreInput
                        }
                    }
                    Err(e) => {
                        self.set_error(&format!("Decoder error: {}", e));
                        JxlRsStatus::Error
                    }
                }
            }

            DecoderState::WithImageInfo(decoder) => {
                let mut input = input_slice;
                let input_before = input.len();
                match decoder.process(&mut input) {
                    Ok(ProcessingResult::Complete { result }) => {
                        self.input_consumed += input_before - input.len();
                        let fh = result.frame_header();
                        self.frame_header.duration = fh.duration.map(|d| d as u32).unwrap_or(0);
                        self.frame_header.is_last = false;
                        self.frame_header.name_length = fh.name.len() as u32;
                        self.state = DecoderState::WithFrameInfo(result);
                        JxlRsStatus::Frame
                    }
                    Ok(ProcessingResult::NeedsMoreInput { fallback, .. }) => {
                        self.input_consumed += input_before - input.len();
                        self.state = DecoderState::WithImageInfo(fallback);
                        if self.all_input_received {
                            self.set_error("Incomplete frame header");
                            JxlRsStatus::Error
                        } else {
                            JxlRsStatus::NeedMoreInput
                        }
                    }
                    Err(e) => {
                        self.set_error(&format!("Frame header error: {}", e));
                        JxlRsStatus::Error
                    }
                }
            }

            DecoderState::WithFrameInfo(decoder) => {
                let mut input = input_slice;
                let input_before = input.len();

                let width = self.basic_info.width as usize;
                let height = self.basic_info.height as usize;

                let mut rgb_image = match Image::<f32>::new_with_value((width * 3, height), 0.0f32) {
                    Ok(img) => img,
                    Err(e) => {
                        self.set_error(&format!("Failed to allocate RGB buffer: {}", e));
                        return JxlRsStatus::Error;
                    }
                };

                let mut alpha_image = if self.basic_info.has_alpha {
                    match Image::<f32>::new_with_value((width, height), 0.0f32) {
                        Ok(img) => Some(img),
                        Err(e) => {
                            self.set_error(&format!("Failed to allocate alpha buffer: {}", e));
                            return JxlRsStatus::Error;
                        }
                    }
                } else {
                    None
                };

                let result = if let Some(ref mut alpha_img) = alpha_image {
                    let rgb_output = JxlOutputBuffer::from_image_rect_mut(
                        rgb_image.get_rect_mut(Rect { origin: (0, 0), size: (width * 3, height) }).into_raw()
                    );
                    let alpha_output = JxlOutputBuffer::from_image_rect_mut(
                        alpha_img.get_rect_mut(Rect { origin: (0, 0), size: (width, height) }).into_raw()
                    );
                    decoder.process(&mut input, &mut [rgb_output, alpha_output])
                } else {
                    let rgb_output = JxlOutputBuffer::from_image_rect_mut(
                        rgb_image.get_rect_mut(Rect { origin: (0, 0), size: (width * 3, height) }).into_raw()
                    );
                    decoder.process(&mut input, &mut [rgb_output])
                };

                // Copy RGB data from image rows to flat buffer
                self.rgb_buffer.clear();
                self.rgb_buffer.reserve(width * height * 3);
                for y in 0..height {
                    let src_row = rgb_image.row(y);
                    self.rgb_buffer.extend_from_slice(&src_row[..width * 3]);
                }

                // Copy alpha data if present
                self.alpha_buffer.clear();
                if let Some(ref alpha_img) = alpha_image {
                    self.alpha_buffer.reserve(width * height);
                    for y in 0..height {
                        let src_row = alpha_img.row(y);
                        self.alpha_buffer.extend_from_slice(&src_row[..width]);
                    }
                }

                match result {
                    Ok(ProcessingResult::Complete { result }) => {
                        self.input_consumed += input_before - input.len();
                        self.current_frame += 1;
                        self.has_more_frames = result.has_more_frames();
                        if self.has_more_frames {
                            self.state = DecoderState::WithImageInfo(result);
                        } else {
                            self.frame_count = self.current_frame;
                            self.state = DecoderState::WithImageInfo(result);
                        }
                        JxlRsStatus::FullImage
                    }
                    Ok(ProcessingResult::NeedsMoreInput { fallback, .. }) => {
                        self.input_consumed += input_before - input.len();
                        self.state = DecoderState::WithFrameInfo(fallback);
                        if self.all_input_received {
                            self.set_error("Incomplete frame data");
                            JxlRsStatus::Error
                        } else {
                            JxlRsStatus::NeedMoreInput
                        }
                    }
                    Err(e) => {
                        self.set_error(&format!("Frame decode error: {}", e));
                        JxlRsStatus::Error
                    }
                }
            }

            DecoderState::Empty => {
                self.set_error("Internal error: decoder in empty state");
                JxlRsStatus::Error
            }
        }
    }
}

// =============================================================================
// CXX Bridge Implementation
// =============================================================================

fn jxl_rs_decoder_create() -> Box<JxlRsDecoder> {
    Box::new(JxlRsDecoder::new())
}

impl JxlRsDecoder {
    fn reset(&mut self) {
        let mut options = JxlDecoderOptions::default();
        options.xyb_output_linear = false;
        options.progressive_mode = JxlProgressiveMode::FullFrame;

        self.state = DecoderState::Initialized(JxlDecoder::new(options));
        self.input_buffer.clear();
        self.input_consumed = 0;
        self.all_input_received = false;
        self.basic_info = JxlRsBasicInfo::default();
        self.frame_header = JxlRsFrameHeader::default();
        self.current_frame = 0;
        self.frame_count = 1;
        self.has_more_frames = true;
        self.icc_profile.clear();
        self.error_message.clear();
        self.rgb_buffer.clear();
        self.alpha_buffer.clear();
    }

    fn set_input(&mut self, data: &[u8], all_input: bool) -> JxlRsStatus {
        if !data.is_empty() {
            self.input_buffer.clear();
            self.input_buffer.extend_from_slice(data);
            self.input_consumed = 0;
        }
        self.all_input_received = all_input;
        JxlRsStatus::Success
    }

    fn process(&mut self) -> JxlRsStatus {
        self.process_internal()
    }

    fn get_basic_info(&self) -> JxlRsBasicInfo {
        self.basic_info.clone()
    }

    fn get_frame_header(&self) -> JxlRsFrameHeader {
        self.frame_header.clone()
    }

    fn set_pixel_format(&mut self, format: JxlRsPixelFormat) {
        self.pixel_format = format;
    }

    fn get_pixels(&mut self, buffer: &mut [u8]) -> JxlRsStatus {
        if self.rgb_buffer.is_empty() {
            self.error_message = "No decoded image available".to_string();
            return JxlRsStatus::Error;
        }

        let width = self.basic_info.width as usize;
        let height = self.basic_info.height as usize;
        let has_alpha = self.basic_info.has_alpha;

        match self.pixel_format {
            JxlRsPixelFormat::Rgba8 => {
                let required_size = width * height * 4;
                if buffer.len() < required_size {
                    self.error_message = "Buffer too small".to_string();
                    return JxlRsStatus::Error;
                }
                for pixel_idx in 0..(width * height) {
                    let rgb_idx = pixel_idx * 3;
                    let dst_idx = pixel_idx * 4;
                    buffer[dst_idx] = (self.rgb_buffer[rgb_idx].clamp(0.0, 1.0) * 255.0) as u8;
                    buffer[dst_idx + 1] = (self.rgb_buffer[rgb_idx + 1].clamp(0.0, 1.0) * 255.0) as u8;
                    buffer[dst_idx + 2] = (self.rgb_buffer[rgb_idx + 2].clamp(0.0, 1.0) * 255.0) as u8;
                    buffer[dst_idx + 3] = if has_alpha {
                        (self.alpha_buffer[pixel_idx].clamp(0.0, 1.0) * 255.0) as u8
                    } else {
                        255
                    };
                }
            }
            JxlRsPixelFormat::Rgba16 => {
                let required_size = width * height * 8;
                if buffer.len() < required_size {
                    self.error_message = "Buffer too small".to_string();
                    return JxlRsStatus::Error;
                }
                for pixel_idx in 0..(width * height) {
                    let rgb_idx = pixel_idx * 3;
                    let dst_idx = pixel_idx * 8; // 4 u16s = 8 bytes per pixel
                    let r = (self.rgb_buffer[rgb_idx].clamp(0.0, 1.0) * 65535.0) as u16;
                    let g = (self.rgb_buffer[rgb_idx + 1].clamp(0.0, 1.0) * 65535.0) as u16;
                    let b = (self.rgb_buffer[rgb_idx + 2].clamp(0.0, 1.0) * 65535.0) as u16;
                    let a = if has_alpha {
                        (self.alpha_buffer[pixel_idx].clamp(0.0, 1.0) * 65535.0) as u16
                    } else {
                        65535
                    };
                    buffer[dst_idx..dst_idx + 2].copy_from_slice(&r.to_ne_bytes());
                    buffer[dst_idx + 2..dst_idx + 4].copy_from_slice(&g.to_ne_bytes());
                    buffer[dst_idx + 4..dst_idx + 6].copy_from_slice(&b.to_ne_bytes());
                    buffer[dst_idx + 6..dst_idx + 8].copy_from_slice(&a.to_ne_bytes());
                }
            }
            JxlRsPixelFormat::RgbaF32 => {
                let required_size = width * height * 16;
                if buffer.len() < required_size {
                    self.error_message = "Buffer too small".to_string();
                    return JxlRsStatus::Error;
                }
                for pixel_idx in 0..(width * height) {
                    let rgb_idx = pixel_idx * 3;
                    let dst_idx = pixel_idx * 16; // 4 f32s = 16 bytes per pixel
                    let r = self.rgb_buffer[rgb_idx];
                    let g = self.rgb_buffer[rgb_idx + 1];
                    let b = self.rgb_buffer[rgb_idx + 2];
                    let a = if has_alpha {
                        self.alpha_buffer[pixel_idx]
                    } else {
                        1.0
                    };
                    buffer[dst_idx..dst_idx + 4].copy_from_slice(&r.to_ne_bytes());
                    buffer[dst_idx + 4..dst_idx + 8].copy_from_slice(&g.to_ne_bytes());
                    buffer[dst_idx + 8..dst_idx + 12].copy_from_slice(&b.to_ne_bytes());
                    buffer[dst_idx + 12..dst_idx + 16].copy_from_slice(&a.to_ne_bytes());
                }
            }
            _ => {
                self.error_message = "Unknown pixel format".to_string();
                return JxlRsStatus::Error;
            }
        }

        JxlRsStatus::Success
    }

    fn get_icc_profile(&self) -> &[u8] {
        &self.icc_profile
    }

    fn get_frame_count(&self) -> u32 {
        self.frame_count
    }

    fn has_more_frames(&self) -> bool {
        self.has_more_frames
    }

    fn get_error(&self) -> &str {
        &self.error_message
    }
}

fn jxl_rs_signature_check(data: &[u8]) -> bool {
    if data.len() < 2 {
        return false;
    }
    let bytes = &data[..data.len().min(12)];
    matches!(check_signature(bytes), ProcessingResult::Complete { result: Some(_) })
}

fn jxl_rs_version() -> &'static str {
    "jxl-rs-capi 0.1.0"
}

// =============================================================================
// Default Implementations
// =============================================================================

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
        }
    }
}

impl From<&InternalBasicInfo> for JxlRsBasicInfo {
    fn from(info: &InternalBasicInfo) -> Self {
        let has_alpha = info.extra_channels.iter().any(|ec| {
            matches!(ec.ec_type, ExtraChannel::Alpha)
        });
        let (animation_loop_count, animation_tps_numerator, animation_tps_denominator) =
            match &info.animation {
                Some(anim) => (anim.num_loops, anim.tps_numerator, anim.tps_denominator),
                None => (0, 1, 1000),
            };
        Self {
            width: info.size.0 as u32,
            height: info.size.1 as u32,
            bits_per_sample: info.bit_depth.bits_per_sample(),
            num_extra_channels: info.extra_channels.len() as u32,
            has_alpha,
            alpha_premultiplied: false,
            have_animation: info.animation.is_some(),
            animation_loop_count,
            animation_tps_numerator,
            animation_tps_denominator,
            uses_original_profile: info.uses_original_profile,
            orientation: info.orientation as u32,
        }
    }
}

impl Default for JxlRsFrameHeader {
    fn default() -> Self {
        Self {
            duration: 0,
            is_last: false,
            name_length: 0,
        }
    }
}
