// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{
        Endianness, JxlBasicInfo, JxlBitDepth, JxlColorEncoding, JxlColorProfile, JxlColorType,
        JxlDataFormat, JxlDecoderOptions, JxlExtraChannel, JxlPixelFormat, ToneMapping,
        inner::{CodestreamParser, codestream_parser::check_size_limit},
    },
    bit_reader::BitReader,
    error::{Error, Result},
    headers::{FileHeader, JxlHeader, color_encoding::ColorSpace},
    icc::IncrementalIccReader,
};

pub struct ImageInfo {
    file_header: Option<FileHeader>,
    basic_info: Option<JxlBasicInfo>,
    embedded_color_profile: Option<JxlColorProfile>,
    icc_parser: Option<IncrementalIccReader>,
}

impl ImageInfo {
    pub fn new() -> Self {
        Self {
            file_header: None,
            basic_info: None,
            embedded_color_profile: None,
            icc_parser: None,
        }
    }

    pub fn is_complete(&self) -> bool {
        self.file_header.is_some() && self.embedded_color_profile.is_some()
    }

    pub fn has_preview(&self) -> bool {
        self.file_header
            .as_ref()
            .unwrap()
            .image_metadata
            .preview
            .is_some()
    }

    pub fn file_header(&self) -> &FileHeader {
        self.file_header.as_ref().unwrap()
    }

    pub fn basic_info(&self) -> &JxlBasicInfo {
        self.basic_info.as_ref().unwrap()
    }

    pub fn embedded_color_profile(&self) -> &JxlColorProfile {
        self.embedded_color_profile.as_ref().unwrap()
    }

    fn is_gray(&self) -> bool {
        self.file_header().image_metadata.color_encoding.color_space == ColorSpace::Gray
    }

    fn xyb_encoded(&self) -> bool {
        self.file_header().image_metadata.xyb_encoded
    }

    pub fn parse_file_header(
        &mut self,
        decode_options: &JxlDecoderOptions,
        br: &mut BitReader,
        bits: &mut usize,
    ) -> Result<()> {
        let file_header = FileHeader::read(br)?;
        let xsize = file_header.size.xsize() as usize;
        let ysize = file_header.size.ysize() as usize;
        check_size_limit(
            decode_options.sample_limit,
            (xsize, ysize),
            file_header.image_metadata.extra_channel_info.len(),
        )?;
        if let Some(preview) = &file_header.image_metadata.preview {
            check_size_limit(
                decode_options.sample_limit,
                (preview.xsize() as usize, preview.ysize() as usize),
                file_header.image_metadata.extra_channel_info.len(),
            )?;
        }
        let data = &file_header.image_metadata;
        self.basic_info = Some(JxlBasicInfo {
            size: if data.orientation.is_transposing() {
                (ysize, xsize)
            } else {
                (xsize, ysize)
            },
            bit_depth: if data.bit_depth.floating_point_sample() {
                JxlBitDepth::Float {
                    bits_per_sample: data.bit_depth.bits_per_sample(),
                    exponent_bits_per_sample: data.bit_depth.exponent_bits_per_sample(),
                }
            } else {
                JxlBitDepth::Int {
                    bits_per_sample: data.bit_depth.bits_per_sample(),
                }
            },
            orientation: data.orientation,
            extra_channels: data
                .extra_channel_info
                .iter()
                .map(|info| JxlExtraChannel {
                    ec_type: info.ec_type,
                    alpha_associated: info.alpha_associated(),
                })
                .collect(),
            animation: data
                .animation
                .as_ref()
                .map(|anim| crate::api::JxlAnimation {
                    tps_numerator: anim.tps_numerator,
                    tps_denominator: anim.tps_denominator,
                    num_loops: anim.num_loops,
                    have_timecodes: anim.have_timecodes,
                }),
            uses_original_profile: !data.xyb_encoded,
            tone_mapping: ToneMapping {
                intensity_target: data.tone_mapping.intensity_target,
                min_nits: data.tone_mapping.min_nits,
                relative_to_max_display: data.tone_mapping.relative_to_max_display,
                linear_below: data.tone_mapping.linear_below,
            },
            preview_size: data
                .preview
                .as_ref()
                .map(|p| (p.xsize() as usize, p.ysize() as usize)),
        });
        self.file_header = Some(file_header);
        *bits = br.total_bits_read();
        Ok(())
    }

    pub fn parse_color_encoding(&mut self, br: &mut BitReader, bits: &mut usize) -> Result<()> {
        // Parse (or extract from file header) the ICC profile.
        let embedded_color_profile = if self.file_header().image_metadata.color_encoding.want_icc {
            if self.icc_parser.is_none() {
                self.icc_parser = Some(IncrementalIccReader::new(br)?);
            }
            let icc_parser = self.icc_parser.as_mut().unwrap();
            *bits = br.total_bits_read();
            for _ in 0..icc_parser.remaining() {
                match icc_parser.read_one(br) {
                    Ok(()) => *bits = br.total_bits_read(),
                    Err(Error::OutOfBounds(c)) => {
                        // Estimate >= one bit per remaining character to read.
                        return Err(Error::OutOfBounds(c + icc_parser.remaining() / 8));
                    }
                    Err(e) => return Err(e),
                }
            }
            let icc_result = self.icc_parser.take().unwrap().finalize(br);
            JxlColorProfile::Icc(icc_result?)
        } else {
            JxlColorProfile::Simple(JxlColorEncoding::from_internal(
                &self.file_header().image_metadata.color_encoding,
            )?)
        };
        self.embedded_color_profile = Some(embedded_color_profile);
        br.jump_to_byte_boundary()?;
        *bits = br.total_bits_read();
        Ok(())
    }
}

impl CodestreamParser {
    pub(in super::super) fn update_default_output_options(&mut self) {
        assert!(self.image_info.is_complete());

        // Only set default pixel_format if not already configured
        if self.pixel_format.is_none() {
            self.pixel_format = Some(JxlPixelFormat {
                color_type: if self.image_info.is_gray() {
                    JxlColorType::Grayscale
                } else {
                    JxlColorType::Rgb
                },
                color_data_format: Some(JxlDataFormat::F32 {
                    endianness: Endianness::native(),
                }),
                extra_channel_format: vec![
                    Some(JxlDataFormat::F32 {
                        endianness: Endianness::native()
                    });
                    self.image_info
                        .file_header()
                        .image_metadata
                        .extra_channel_info
                        .len()
                ],
            });
        }

        let embedded_color_profile = self.image_info.embedded_color_profile.as_ref().unwrap();
        let pixel_format = self.pixel_format.as_ref().unwrap();

        // Determine default output color profile following libjxl logic:
        // - For XYB: use embedded if can_output_to(), else:
        //   - if float samples are requested: linear sRGB,
        //   - else: sRGB
        // - For non-XYB: use embedded color profile
        let output_color_profile = if self.image_info.xyb_encoded() {
            // Use embedded if we can output to it, otherwise fall back to sRGB
            let base_encoding = if embedded_color_profile.can_output_to() {
                match &embedded_color_profile {
                    JxlColorProfile::Simple(enc) => enc.clone(),
                    JxlColorProfile::Icc(_) => {
                        unreachable!("can_output_to returns false for ICC")
                    }
                }
            } else {
                let data_format = pixel_format
                    .color_data_format
                    .unwrap_or(JxlDataFormat::U8 { bit_depth: 8 });
                let is_float = matches!(
                    data_format,
                    JxlDataFormat::F32 { .. } | JxlDataFormat::F16 { .. }
                );
                if is_float {
                    JxlColorEncoding::linear_srgb(self.image_info.is_gray())
                } else {
                    JxlColorEncoding::srgb(self.image_info.is_gray())
                }
            };

            JxlColorProfile::Simple(base_encoding)
        } else {
            embedded_color_profile.clone()
        };
        self.output_color_profile = Some(output_color_profile);
    }
}
