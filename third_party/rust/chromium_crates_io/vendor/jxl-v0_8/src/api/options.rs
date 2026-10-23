// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::JxlAuxBoxType;

#[cfg(test)]
#[derive(Debug, Clone, Copy, Default)]
pub struct TestOptions {
    pub use_simple_pipeline: bool,
    pub disable_16bit_modular_buffers: bool,
}

#[non_exhaustive]
pub struct JxlDecoderOptions {
    /// If true (default), applies the orientation transform from the image
    /// header; basic info reports the oriented size. If false, pixels are
    /// output in codestream order, basic info reports the codestream size,
    /// and the caller is responsible for applying the orientation.
    pub adjust_orientation: bool,
    pub render_spot_colors: bool,
    pub coalescing: bool,
    pub desired_intensity_target: Option<f32>,
    pub skip_preview: bool,
    /// Fail decoding images with more than this number of samples, or with frames with
    /// more than this number of samples. The limit counts the product of pixels and
    /// channels, so for example an image with 1 extra channel of size 1024x1024 has 4
    /// million samples.
    pub sample_limit: Option<usize>,
    /// Use high precision mode for decoding.
    /// When false (default), uses lower precision settings that match libjxl's default.
    /// When true, uses higher precision at the cost of performance.
    ///
    /// This affects multiple decoder decisions including spline rendering precision
    /// and potentially intermediate buffer storage (e.g., using f32 vs f16).
    pub high_precision: bool,
    /// Controls how alpha premultiplication is handled for output color channels.
    /// Default: [`AlphaMode::AsIs`].
    pub alpha_mode: AlphaMode,
    /// If true, only parse frame headers/TOC and skip section decoding.
    ///
    /// This is useful for collecting [`VisibleFrameInfo`](crate::api::VisibleFrameInfo)
    /// via the regular decoder API without producing pixels.
    pub scan_frames_only: bool,
    /// Additional boxes to request from the decoder.
    ///
    /// Note that if you request additional boxes, you must call `close_input()` when the
    /// file is over, and some cases of metadata boxes after the end of the codestream
    /// require special handling (see `[JxlDecoder::trailing_box]`).
    pub request_aux_boxes: Vec<JxlAuxBoxType>,
    /// Maximum profile and level allowed when decoding (default: [`ProfileLevel::Main5`]).
    /// When [`ProfileLevel::Main5`], enforces Level 5 complexity constraints defined by ISO/IEC 18181-1.
    /// When [`ProfileLevel::Main10`], allows Level 10 limits (which requires a container with a Level 10 `jxll` box).
    pub max_profile_level: ProfileLevel,
    #[cfg(test)]
    pub test_options: TestOptions,
}

/// Controls how alpha premultiplication is handled for output color channels.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub enum AlphaMode {
    /// Output color channels as encoded in the codestream (no conversion).
    #[default]
    AsIs,
    /// Ensure output color channels are premultiplied by alpha
    /// (multiplies RGB by alpha if the codestream alpha is unassociated).
    Premultiplied,
    /// Ensure output color channels are unpremultiplied (straight)
    /// (divides RGB by alpha if the codestream alpha is associated).
    Unpremultiplied,
}

/// Profile and level for JPEG XL complexity constraints (ISO/IEC 18181-1).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
pub enum ProfileLevel {
    /// Enforce Main Profile Level 5 complexity constraints.
    #[default]
    Main5,
    /// Allow Main Profile Level 10 limits (requires a container with a Level 10 `jxll` box).
    Main10,
}

impl std::fmt::Display for ProfileLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ProfileLevel::Main5 => write!(f, "Level 5"),
            ProfileLevel::Main10 => write!(f, "Level 10"),
        }
    }
}

impl Default for JxlDecoderOptions {
    fn default() -> Self {
        Self {
            adjust_orientation: true,
            render_spot_colors: true,
            coalescing: true,
            skip_preview: true,
            desired_intensity_target: None,
            sample_limit: None,
            high_precision: false,
            alpha_mode: AlphaMode::AsIs,
            scan_frames_only: false,
            request_aux_boxes: Vec::new(),
            max_profile_level: ProfileLevel::Main5,
            #[cfg(test)]
            test_options: TestOptions::default(),
        }
    }
}
