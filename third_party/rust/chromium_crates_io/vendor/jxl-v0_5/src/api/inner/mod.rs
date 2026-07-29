// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#[cfg(test)]
use crate::api::FrameCallback;
use crate::api::{JxlFrameHeader, VisibleFrameInfo, VisibleFrameSeekTarget};

use super::{JxlBasicInfo, JxlColorProfile, JxlDecoderOptions, JxlPixelFormat};
use box_parser::BoxParser;
use codestream_parser::CodestreamParser;

mod box_parser;
mod codestream_parser;
mod process;

pub use box_parser::BoxParserCheckpoint;

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

    pub fn current_pixel_format(&self) -> Option<&JxlPixelFormat> {
        self.codestream_parser.pixel_format.as_ref()
    }

    pub fn set_pixel_format(&mut self, pixel_format: JxlPixelFormat) {
        // TODO(veluca): return an error if we are asking for both planar and
        // interleaved-in-color alpha.
        self.codestream_parser.pixel_format = Some(pixel_format);
        self.codestream_parser.update_default_output_options();
    }

    pub fn frame_header(&self) -> Option<JxlFrameHeader> {
        if !self.codestream_parser.has_frame() {
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

    pub fn has_more_frames(&self) -> bool {
        self.codestream_parser.has_more_frames()
    }

    /// Returns visible frame info entries collected during parsing.
    pub fn scanned_frames(&self) -> &[VisibleFrameInfo] {
        self.codestream_parser.scanned_frames()
    }

    pub fn start_new_frame(&mut self, seek_target: VisibleFrameSeekTarget) {
        self.box_parser
            .reset_to_checkpoint(seek_target.box_parser_checkpoint);
        self.codestream_parser.start_new_frame(
            seek_target.visible_frames_to_skip,
            seek_target.box_parser_checkpoint.consumed_codestream,
        );
    }

    #[cfg(test)]
    pub(crate) fn set_use_simple_pipeline(&mut self, u: bool) {
        self.codestream_parser.set_use_simple_pipeline(u);
    }

    pub fn file_length(&self) -> Option<u64> {
        self.codestream_parser.file_length
    }
}

#[cfg(test)]
mod tests {
    use super::JxlDecoderInner;
    use crate::api::JxlDecoderOptions;

    #[test]
    fn basic_info_not_visible_before_embedded_profile() {
        let data = std::fs::read("resources/test/conformance_test_images/cmyk_layers.jxl").unwrap();
        let mut decoder = JxlDecoderInner::new(JxlDecoderOptions::default());

        for chunk in data.chunks(64) {
            let mut input = chunk;
            let _ = decoder.process(&mut input, None);

            if decoder.embedded_color_profile().is_none() {
                assert!(decoder.basic_info().is_none());
            }

            if decoder.basic_info().is_some() {
                assert!(decoder.embedded_color_profile().is_some());
                return;
            }
        }

        panic!("failed to reach image-info state while parsing cmyk_layers.jxl");
    }

    /// Regression test: an out-of-order `jxlp` stream with trailing container
    /// bytes used to spin forever when header parsing needed more codestream
    /// than the boxes provided.
    #[test]
    fn ooo_jxlp_with_trailing_bytes_does_not_hang() {
        let data = include_bytes!("../../../tests/testdata/ooo_jxlp_with_trailing_bytes.jxl");

        let mut decoder = JxlDecoderInner::new(JxlDecoderOptions::default());
        let mut input = data.as_slice();
        let result = decoder.process(&mut input, None);
        assert!(
            matches!(
                result,
                Ok(crate::api::ProcessingResult::NeedsMoreInput { .. })
            ),
            "{result:?}"
        );
    }
}
