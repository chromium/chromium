// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use box_parser::BoxParser;
use codestream_parser::CodestreamParser;

use super::{JxlBasicInfo, JxlColorProfile, JxlDecoderOptions, JxlPixelFormat};
#[cfg(test)]
use crate::api::FrameCallback;
use crate::api::{JxlFrameHeader, VisibleFrameInfo, VisibleFrameSeekTarget};
use crate::error::{Error, Result};

mod box_parser;
mod codestream_parser;
pub(crate) mod process;

pub use box_parser::{BoxParserCheckpoint, JxlAuxBox, JxlAuxBoxType};

/// Low-level, less-type-safe API.
pub struct JxlDecoderInner {
    options: JxlDecoderOptions,
    box_parser: BoxParser,
    codestream_parser: CodestreamParser,
}

impl JxlDecoderInner {
    /// Creates a new decoder with the given options and, optionally, CMS.
    pub fn new(options: JxlDecoderOptions) -> Self {
        let box_parser = BoxParser::with_aux_boxes(options.request_aux_boxes.iter().copied());
        JxlDecoderInner {
            options,
            box_parser,
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

    pub fn set_pixel_format(&mut self, pixel_format: JxlPixelFormat) -> Result<()> {
        // TODO(veluca): return an error if we are asking for both planar and
        // interleaved-in-color alpha.
        // Frame render pipelines are built for the pixel format that is
        // current when the frame's TOC is parsed, so the format can only be
        // changed before the first frame header is decoded, i.e. right after
        // basic info becomes available.
        let frame_header_was_decoded = self
            .codestream_parser
            .frame_info
            .current_frame_header()
            .is_some()
            || !self.codestream_parser.scanned_frames().is_empty();
        if frame_header_was_decoded
            && self.codestream_parser.pixel_format.as_ref() != Some(&pixel_format)
        {
            return Err(Error::PixelFormatChangedAfterFirstFrame);
        }
        self.codestream_parser.pixel_format = Some(pixel_format);
        self.codestream_parser.update_default_output_options();
        Ok(())
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

    pub(crate) fn trailing_box(&self) -> Option<&JxlAuxBox> {
        self.box_parser.trailing_box()
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

    #[cfg(test)]
    pub(crate) fn disable_16bit_modular_buffers(&mut self) {
        self.codestream_parser.disable_16bit_modular_buffers();
    }

    pub fn file_length(&self) -> Option<u64> {
        self.codestream_parser.file_length
    }

    pub(crate) fn aux_boxes(&self, box_type: JxlAuxBoxType) -> &[JxlAuxBox] {
        self.box_parser.aux_boxes(box_type)
    }
}

#[cfg(test)]
mod tests {
    use super::JxlDecoderInner;
    use crate::api::{JxlAuxBoxType, JxlDecoderOptions};

    #[test]
    fn basic_info_not_visible_before_embedded_profile() {
        let data = std::fs::read("resources/test/conformance_test_images/cmyk_layers.jxl").unwrap();
        let mut decoder = JxlDecoderInner::new(JxlDecoderOptions::default());

        for chunk in data.chunks(64) {
            let mut input = chunk;
            let _ = decoder.process(&mut input, None, None);

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

    #[test]
    fn incomplete_ooo_jxlp() {
        let data = include_bytes!("../../../tests/testdata/incomplete_ooo_jxlp.jxl");

        let mut decoder = JxlDecoderInner::new(JxlDecoderOptions::default());
        let mut input = data.as_slice();
        let result = decoder.process(&mut input, None, None);
        assert!(
            matches!(result, Err(crate::error::Error::UnexpectedCodestreamBoxEnd)),
            "{result:?}"
        );
    }

    #[test]
    fn consume_trailing_on_bare_codestream() {
        let data = include_bytes!("../../../resources/test/basic.jxl");
        let mut buf = &data[..];

        let options = JxlDecoderOptions {
            request_aux_boxes: vec![JxlAuxBoxType::EXIF],
            scan_frames_only: true,
            ..Default::default()
        };
        let mut decoder = JxlDecoderInner::new(options);

        while decoder.has_more_frames() {
            decoder.process(&mut buf, None, None).unwrap();
        }
        decoder.process_trailing_data(&mut buf).unwrap();
    }

    #[test]
    fn aux_box_before_codestream() {
        let data = [
            (&include_bytes!("../../../tests/testdata/exif.jxl")[..], 170),
            (
                &include_bytes!("../../../tests/testdata/exif_brob.jxl")[..],
                120,
            ),
        ];

        for (mut buf, expected_size) in data {
            let options = JxlDecoderOptions {
                request_aux_boxes: vec![JxlAuxBoxType::EXIF],
                scan_frames_only: true,
                ..Default::default()
            };
            let mut decoder = JxlDecoderInner::new(options);

            while decoder.has_more_frames() {
                decoder.process(&mut buf, None, None).unwrap();
            }
            decoder.process_trailing_data(&mut buf).unwrap();

            let exif = &decoder.aux_boxes(JxlAuxBoxType::EXIF)[0];
            assert_eq!(exif.raw_data().len(), expected_size);
            assert!(decoder.trailing_box().is_none());
        }
    }

    #[test]
    fn aux_box_trailing_finite() {
        let data = [
            (
                &include_bytes!("../../../tests/testdata/exif_trailing_finite.jxl")[..],
                170,
            ),
            (
                &include_bytes!("../../../tests/testdata/exif_brob_trailing_finite.jxl")[..],
                120,
            ),
        ];

        for (mut buf, expected_size) in data {
            let options = JxlDecoderOptions {
                request_aux_boxes: vec![JxlAuxBoxType::EXIF],
                scan_frames_only: true,
                ..Default::default()
            };
            let mut decoder = JxlDecoderInner::new(options);

            while decoder.has_more_frames() {
                decoder.process(&mut buf, None, None).unwrap();
            }
            decoder.process_trailing_data(&mut buf).unwrap();

            let exif = &decoder.aux_boxes(JxlAuxBoxType::EXIF)[0];
            assert_eq!(exif.raw_data().len(), expected_size);
            assert!(decoder.trailing_box().is_none());
        }
    }

    #[test]
    fn aux_box_trailing_infinite() {
        let data = [
            (
                &include_bytes!("../../../tests/testdata/exif_trailing_infinite.jxl")[..],
                170,
            ),
            (
                &include_bytes!("../../../tests/testdata/exif_brob_trailing_infinite.jxl")[..],
                120,
            ),
        ];

        for (mut buf, expected_size) in data {
            let options = JxlDecoderOptions {
                request_aux_boxes: vec![JxlAuxBoxType::EXIF],
                scan_frames_only: true,
                ..Default::default()
            };
            let mut decoder = JxlDecoderInner::new(options);

            while decoder.has_more_frames() {
                decoder.process(&mut buf, None, None).unwrap();
            }
            decoder.process_trailing_data(&mut buf).unwrap();

            let trailing = decoder.trailing_box().unwrap();
            assert_eq!(trailing.box_type(), JxlAuxBoxType::EXIF);
            assert_eq!(trailing.raw_data().len() + buf.len(), expected_size);
        }
    }

    #[test]
    fn aux_box_seek() {
        let data = include_bytes!("../../../tests/testdata/multiple_aux.jxl");

        let ty_foo = JxlAuxBoxType(*b"foo ");
        let ty_bar = JxlAuxBoxType(*b"bar ");

        let options = JxlDecoderOptions {
            request_aux_boxes: vec![ty_bar],
            scan_frames_only: true,
            ..Default::default()
        };
        let mut decoder = JxlDecoderInner::new(options);

        let mut buf = &data[..];
        while decoder.has_more_frames() {
            decoder.process(&mut buf, None, None).unwrap();
        }
        decoder.process_trailing_data(&mut buf).unwrap();
        assert!(decoder.aux_boxes(ty_foo).is_empty());
        assert_eq!(decoder.aux_boxes(ty_bar).len(), 2);

        let seek_target = decoder.scanned_frames()[0].seek_target;
        decoder.start_new_frame(seek_target);

        let mut buf = &data[(seek_target.decode_start_file_offset as usize)..];
        while decoder.has_more_frames() {
            decoder.process(&mut buf, None, None).unwrap();
        }
        decoder.process_trailing_data(&mut buf).unwrap();
        assert!(decoder.aux_boxes(ty_foo).is_empty());
        assert_eq!(decoder.aux_boxes(ty_bar).len(), 2);
    }
}
