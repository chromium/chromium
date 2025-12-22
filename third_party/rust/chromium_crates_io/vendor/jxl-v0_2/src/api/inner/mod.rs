// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#[cfg(test)]
use crate::api::FrameCallback;
use crate::{
    api::JxlFrameHeader,
    error::{Error, Result},
};

use super::{JxlBasicInfo, JxlColorProfile, JxlDecoderOptions, JxlPixelFormat};
use box_parser::BoxParser;
use codestream_parser::CodestreamParser;

mod box_parser;
mod codestream_parser;
mod process;

/// Low-level, less-type-safe API.
pub struct JxlDecoderInner {
    options: JxlDecoderOptions,
    box_parser: BoxParser,
    codestream_parser: CodestreamParser,
}

impl JxlDecoderInner {
    /// Creates a new decoder with the given options and, optionally, CMS.
    pub fn new(options: JxlDecoderOptions) -> Self {
        JxlDecoderInner {
            options,
            box_parser: BoxParser::new(),
            codestream_parser: CodestreamParser::new(),
        }
    }

    #[cfg(test)]
    pub fn set_frame_callback(&mut self, callback: Box<FrameCallback>) {
        self.codestream_parser.frame_callback = Some(callback);
    }

    #[cfg(test)]
    pub fn decoded_frames(&self) -> usize {
        self.codestream_parser.decoded_frames
    }

    /// Obtains the image's basic information, if available.
    pub fn basic_info(&self) -> Option<&JxlBasicInfo> {
        self.codestream_parser.basic_info.as_ref()
    }

    /// Retrieves the file's color profile, if available.
    pub fn embedded_color_profile(&self) -> Option<&JxlColorProfile> {
        self.codestream_parser.embedded_color_profile.as_ref()
    }

    /// Retrieves the current output color profile, if available.
    pub fn output_color_profile(&self) -> Option<&JxlColorProfile> {
        self.codestream_parser.output_color_profile.as_ref()
    }

    /// Specifies the preferred color profile to be used for outputting data.
    /// Same semantics as JxlDecoderSetOutputColorProfile.
    pub fn set_output_color_profile(&mut self, profile: JxlColorProfile) -> Result<()> {
        if let (JxlColorProfile::Icc(_), None) = (&profile, &self.options.cms) {
            return Err(Error::ICCOutputNoCMS);
        }
        self.codestream_parser.output_color_profile = Some(profile);
        Ok(())
    }

    pub fn current_pixel_format(&self) -> Option<&JxlPixelFormat> {
        self.codestream_parser.pixel_format.as_ref()
    }

    pub fn set_pixel_format(&mut self, pixel_format: JxlPixelFormat) {
        self.codestream_parser.pixel_format = Some(pixel_format);
    }

    pub fn frame_header(&self) -> Option<JxlFrameHeader> {
        let frame_header = self.codestream_parser.frame.as_ref()?.header();
        // The render pipeline always adds ExtendToImageDimensionsStage which extends
        // frames to the full image size. So the output size is always the image size,
        // not the frame's upsampled size.
        let size = self.codestream_parser.basic_info.as_ref()?.size;
        Some(JxlFrameHeader {
            name: frame_header.name.clone(),
            duration: self
                .codestream_parser
                .animation
                .as_ref()
                .map(|anim| frame_header.duration(anim)),
            size,
        })
    }

    /// Number of passes we have full data for.
    /// Returns the minimum number of passes completed across all groups.
    pub fn num_completed_passes(&self) -> Option<usize> {
        Some(self.codestream_parser.num_completed_passes())
    }

    /// Fully resets the decoder to its initial state.
    ///
    /// This clears all state including pixel_format. For animation loop playback,
    /// consider using [`rewind`](Self::rewind) instead which preserves pixel_format.
    ///
    /// After calling this, the caller should provide input from the beginning of the file.
    pub fn reset(&mut self) {
        // TODO(veluca): keep track of frame offsets for skipping.
        self.box_parser = BoxParser::new();
        self.codestream_parser = CodestreamParser::new();
    }

    /// Rewinds for animation loop replay, keeping pixel_format setting.
    ///
    /// This resets the decoder but preserves the pixel_format configuration,
    /// so the caller doesn't need to re-set it after rewinding.
    ///
    /// After calling this, the caller should provide input from the beginning of the file.
    /// Headers will be re-parsed, then frames can be decoded again.
    ///
    /// Returns `true` if pixel_format was preserved, `false` if none was set.
    pub fn rewind(&mut self) -> bool {
        self.box_parser = BoxParser::new();
        self.codestream_parser.rewind().is_some()
    }

    pub fn has_more_frames(&self) -> bool {
        self.codestream_parser.has_more_frames
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.codestream_parser.set_use_simple_pipeline(u);
    }
}
