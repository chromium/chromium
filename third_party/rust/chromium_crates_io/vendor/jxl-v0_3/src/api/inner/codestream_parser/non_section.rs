// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::io::IoSliceMut;

use crate::{
    api::{
        Endianness, JxlBasicInfo, JxlBitDepth, JxlColorEncoding, JxlColorProfile, JxlColorType,
        JxlDataFormat, JxlDecoderOptions, JxlExtraChannel, JxlPixelFormat,
        inner::codestream_parser::SectionState,
    },
    bit_reader::BitReader,
    error::{Error, Result},
    frame::{DecoderState, Frame, Section},
    headers::{
        FileHeader, JxlHeader, color_encoding::ColorSpace, encodings::UnconditionalCoder,
        frame_header::FrameHeader, toc::IncrementalTocReader,
    },
    icc::IncrementalIccReader,
};

use super::{CodestreamParser, SectionBuffer};
use crate::api::ToneMapping;

fn check_size_limit(
    pixel_limit: Option<usize>,
    (xs, ys): (usize, usize),
    num_ec: usize,
) -> Result<()> {
    if let Some(limit) = pixel_limit {
        let xs = xs.max(16); // xsize is always at least 64 bytes.
        let total_pixels = xs.saturating_mul(ys).saturating_mul(3 + num_ec);
        if total_pixels >= limit {
            return Err(Error::ImageSizeTooLarge(xs, ys));
        }
    };
    Ok(())
}

impl CodestreamParser {
    #[cold]
    pub(super) fn process_non_section(&mut self, decode_options: &JxlDecoderOptions) -> Result<()> {
        if self.decoder_state.is_none() && self.file_header.is_none() {
            // We don't have a file header yet. Try parsing that.
            let mut br = BitReader::new(&self.non_section_buf);
            br.skip_bits(self.non_section_bit_offset as usize)?;
            let file_header = FileHeader::read(&mut br)?;
            let xsize = file_header.size.xsize() as usize;
            let ysize = file_header.size.ysize() as usize;
            check_size_limit(
                decode_options.pixel_limit,
                (xsize, ysize),
                file_header.image_metadata.extra_channel_info.len(),
            )?;
            if let Some(preview) = &file_header.image_metadata.preview {
                check_size_limit(
                    decode_options.pixel_limit,
                    (preview.xsize() as usize, preview.ysize() as usize),
                    file_header.image_metadata.extra_channel_info.len(),
                )?;
            }
            let data = &file_header.image_metadata;
            self.animation = data.animation.clone();
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
            let bits = br.total_bits_read();
            self.non_section_buf.consume(bits / 8);
            self.non_section_bit_offset = (bits % 8) as u8;
        }

        if self.decoder_state.is_none() && self.embedded_color_profile.is_none() {
            let file_header = self.file_header.as_ref().unwrap();
            // Parse (or extract from file header) the ICC profile.
            let mut br = BitReader::new(&self.non_section_buf);
            br.skip_bits(self.non_section_bit_offset as usize)?;
            let embedded_color_profile = if file_header.image_metadata.color_encoding.want_icc {
                if self.icc_parser.is_none() {
                    self.icc_parser = Some(IncrementalIccReader::new(&mut br)?);
                }
                let icc_parser = self.icc_parser.as_mut().unwrap();
                let mut bits = br.total_bits_read();
                for _ in 0..icc_parser.remaining() {
                    match icc_parser.read_one(&mut br) {
                        Ok(()) => bits = br.total_bits_read(),
                        Err(Error::OutOfBounds(c)) => {
                            self.non_section_buf.consume(bits / 8);
                            self.non_section_bit_offset = (bits % 8) as u8;
                            // Estimate >= one bit per remaining character to read.
                            return Err(Error::OutOfBounds(c + icc_parser.remaining() / 8));
                        }
                        Err(e) => return Err(e),
                    }
                }
                let icc_result = self.icc_parser.take().unwrap().finalize(&mut br);
                self.non_section_buf.consume(bits / 8);
                self.non_section_bit_offset = (bits % 8) as u8;
                JxlColorProfile::Icc(icc_result?)
            } else {
                JxlColorProfile::Simple(JxlColorEncoding::from_internal(
                    &file_header.image_metadata.color_encoding,
                )?)
            };
            // Determine default output color profile following libjxl logic:
            // - For XYB: use embedded if can_output_to(), else linear sRGB fallback
            // - For non-XYB: use embedded color profile
            let output_color_profile = if file_header.image_metadata.xyb_encoded {
                let is_gray =
                    file_header.image_metadata.color_encoding.color_space == ColorSpace::Gray;

                // Use embedded if we can output to it, otherwise fall back to linear sRGB
                let base_encoding = if embedded_color_profile.can_output_to() {
                    match &embedded_color_profile {
                        JxlColorProfile::Simple(enc) => enc.clone(),
                        JxlColorProfile::Icc(_) => {
                            unreachable!("can_output_to returns false for ICC")
                        }
                    }
                } else {
                    JxlColorEncoding::linear_srgb(is_gray)
                };

                JxlColorProfile::Simple(base_encoding)
            } else {
                embedded_color_profile.clone()
            };
            self.embedded_color_profile = Some(embedded_color_profile.clone());
            // Only set default output_color_profile if not already configured by user
            if self.output_color_profile.is_none() {
                self.output_color_profile = Some(output_color_profile);
            } else {
                // Validate user's output color profile choice (libjxl compatibility)
                // For non-XYB without CMS: only same encoding as embedded is allowed
                let user_profile = self.output_color_profile.as_ref().unwrap();
                if !file_header.image_metadata.xyb_encoded
                    && decode_options.cms.is_none()
                    && *user_profile != embedded_color_profile
                {
                    return Err(Error::NonXybOutputNoCMS);
                }
            }
            // Only set default pixel_format if not already configured (e.g. via rewind)
            if self.pixel_format.is_none() {
                self.pixel_format = Some(JxlPixelFormat {
                    color_type: if file_header.image_metadata.color_encoding.color_space
                        == ColorSpace::Gray
                    {
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
                        file_header.image_metadata.extra_channel_info.len()
                    ],
                });
            }

            let mut br = BitReader::new(&self.non_section_buf);
            br.skip_bits(self.non_section_bit_offset as usize)?;
            br.jump_to_byte_boundary()?;
            self.non_section_buf.consume(br.total_bits_read() / 8);

            // We now have image information.
            let mut decoder_state = DecoderState::new(self.file_header.take().unwrap());
            decoder_state.render_spotcolors = decode_options.render_spot_colors;
            decoder_state.high_precision = decode_options.high_precision;
            decoder_state.premultiply_output = decode_options.premultiply_output;
            self.decoder_state = Some(decoder_state);
            // Reset bit offset to 0 since we've consumed everything up to a byte boundary
            self.non_section_bit_offset = 0;
            return Ok(());
        }

        let decoder_state = self.decoder_state.as_mut().unwrap();

        if self.frame_header.is_none() {
            // We don't have a frame header yet. Try parsing that.
            let mut br = BitReader::new(&self.non_section_buf);
            br.skip_bits(self.non_section_bit_offset as usize)?;

            // For preview frames, use the preview dimensions instead of main image dimensions
            let nonserialized = if !self.preview_done {
                decoder_state
                    .file_header
                    .preview_frame_header_nonserialized()
                    .unwrap_or_else(|| decoder_state.file_header.frame_header_nonserialized())
            } else {
                decoder_state.file_header.frame_header_nonserialized()
            };

            let mut frame_header = FrameHeader::read_unconditional(&(), &mut br, &nonserialized)?;
            frame_header.postprocess(&nonserialized);
            check_size_limit(
                decode_options.pixel_limit,
                frame_header.size(),
                frame_header.num_extra_channels as usize,
            )?;

            // Initialize storage buffers for available sections.
            self.lf_global_section = None;
            self.lf_sections.clear();
            self.hf_global_section = None;
            self.hf_sections = (0..frame_header.num_groups())
                .map(|_| (0..frame_header.passes.num_passes).map(|_| None).collect())
                .collect();
            self.candidate_hf_sections.clear();

            self.frame_header = Some(frame_header);
            let bits = br.total_bits_read();
            self.non_section_buf.consume(bits / 8);
            self.non_section_bit_offset = (bits % 8) as u8;
        }

        let toc = {
            let mut br = BitReader::new(&self.non_section_buf);
            br.skip_bits(self.non_section_bit_offset as usize)?;
            if self.toc_parser.is_none() {
                let num_toc_entries = self.frame_header.as_ref().unwrap().num_toc_entries();
                self.toc_parser = Some(IncrementalTocReader::new(num_toc_entries as u32, &mut br)?);
            }

            let toc_parser = self.toc_parser.as_mut().unwrap();
            let mut bits = br.total_bits_read();
            while !toc_parser.is_complete() {
                match toc_parser.read_step(&mut br) {
                    Ok(()) => bits = br.total_bits_read(),
                    Err(Error::OutOfBounds(c)) => {
                        self.non_section_buf.consume(bits / 8);
                        self.non_section_bit_offset = (bits % 8) as u8;
                        // Estimate >= 16 bits per remaining entry to read.
                        return Err(Error::OutOfBounds(
                            c + toc_parser.remaining_entries() as usize * 2,
                        ));
                    }
                    Err(e) => return Err(e),
                }
            }
            br.jump_to_byte_boundary()?;

            bits = br.total_bits_read();
            self.non_section_buf.consume(bits / 8);
            self.non_section_bit_offset = (bits % 8) as u8;
            self.toc_parser.take().unwrap().finalize()
        };

        // Save file_header before creating frame (for preview frame recovery)
        self.saved_file_header = self.decoder_state.as_ref().map(|ds| ds.file_header.clone());

        let frame = Frame::from_header_and_toc(
            self.frame_header.take().unwrap(),
            toc,
            self.decoder_state.take().unwrap(),
        )?;

        let mut sections: Vec<_> = frame
            .toc()
            .entries
            .iter()
            .map(|x| SectionBuffer {
                len: *x as usize,
                data: vec![],
                section: Section::LfGlobal, // will be fixed later
            })
            .collect();

        let order = if frame.toc().permuted {
            frame.toc().permutation.0.clone()
        } else {
            (0..sections.len() as u32).collect()
        };

        if sections.len() > 1 {
            let base_sections = [Section::LfGlobal, Section::HfGlobal];
            let lf_sections = (0..frame.header().num_lf_groups()).map(|x| Section::Lf { group: x });
            let hf_sections = (0..frame.header().passes.num_passes).flat_map(|p| {
                (0..frame.header().num_groups()).map(move |g| Section::Hf {
                    group: g,
                    pass: p as usize,
                })
            });

            for section in base_sections
                .into_iter()
                .chain(lf_sections)
                .chain(hf_sections)
            {
                sections[order[frame.get_section_idx(section)] as usize].section = section;
            }
        }

        self.sections = sections.into_iter().collect();
        self.ready_section_data = 0;

        // Move data from the pre-section buffer into the sections.
        for buf in self.sections.iter_mut() {
            if self.non_section_buf.is_empty() {
                break;
            }
            let mut data = Vec::new();
            data.try_reserve_exact(buf.len)?;
            data.resize(buf.len, 0);
            buf.data = data;
            self.ready_section_data += self
                .non_section_buf
                .take(&mut [IoSliceMut::new(&mut buf.data)]);
        }

        self.section_state =
            SectionState::new(frame.header().num_lf_groups(), frame.header().num_groups());

        self.frame = Some(frame);

        Ok(())
    }
}
