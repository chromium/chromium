// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::sync::Arc;

use crate::api::JxlCms;
use crate::api::JxlColorType;
use crate::api::JxlDataFormat;
use crate::api::JxlOutputBuffer;
use crate::bit_reader::BitReader;
use crate::error::{Error, Result};
use crate::features::epf::create_sigma_image;
use crate::headers::frame_header::Encoding;
use crate::headers::{Orientation, color_encoding::ColorSpace, extra_channels::ExtraChannel};
use crate::image::Rect;
#[cfg(test)]
use crate::render::SimpleRenderPipeline;
use crate::render::buffer_splitter::BufferSplitter;
use crate::render::{LowMemoryRenderPipeline, RenderPipeline, RenderPipelineBuilder, stages::*};
use crate::{
    api::JxlPixelFormat,
    frame::{DecoderState, Frame, LfGlobalState},
    headers::frame_header::FrameHeader,
    image::Image,
};

#[cfg(test)]
macro_rules! pipeline {
    ($frame: expr, $pipeline: ident, $op: expr) => {
        if $frame.use_simple_pipeline {
            let $pipeline = $frame
                .render_pipeline
                .as_mut()
                .unwrap()
                .downcast_mut::<SimpleRenderPipeline>()
                .unwrap();
            $op
        } else {
            use crate::render::LowMemoryRenderPipeline;
            let $pipeline = $frame
                .render_pipeline
                .as_mut()
                .unwrap()
                .downcast_mut::<LowMemoryRenderPipeline>()
                .unwrap();
            $op
        }
    };
}

#[cfg(not(test))]
macro_rules! pipeline {
    ($frame: expr, $pipeline: ident, $op: expr) => {{
        let $pipeline = $frame.render_pipeline.as_mut().unwrap();
        $op
    }};
}

pub(crate) use pipeline;

impl Frame {
    /// Add conversion stages for non-float output formats.
    /// This is needed before saving to U8/U16/F16 formats to convert from the pipeline's f32.
    fn add_conversion_stages<P: RenderPipeline>(
        mut pipeline: RenderPipelineBuilder<P>,
        channels: &[usize],
        data_format: JxlDataFormat,
    ) -> Result<RenderPipelineBuilder<P>> {
        use crate::render::stages::{
            ConvertF32ToF16Stage, ConvertF32ToU8Stage, ConvertF32ToU16Stage,
        };

        match data_format {
            JxlDataFormat::U8 { bit_depth } => {
                for &channel in channels {
                    pipeline =
                        pipeline.add_inout_stage(ConvertF32ToU8Stage::new(channel, bit_depth))?;
                }
            }
            JxlDataFormat::U16 { bit_depth, .. } => {
                for &channel in channels {
                    pipeline =
                        pipeline.add_inout_stage(ConvertF32ToU16Stage::new(channel, bit_depth))?;
                }
            }
            JxlDataFormat::F16 { .. } => {
                for &channel in channels {
                    pipeline = pipeline.add_inout_stage(ConvertF32ToF16Stage::new(channel))?;
                }
            }
            // F32 doesn't need conversion - the pipeline already uses f32
            JxlDataFormat::F32 { .. } => {}
        }
        Ok(pipeline)
    }

    pub fn decode_and_render_hf_groups(
        &mut self,
        api_buffers: &mut Option<&mut [JxlOutputBuffer<'_>]>,
        pixel_format: &JxlPixelFormat,
        groups: Vec<(usize, Vec<(usize, BitReader)>)>,
    ) -> Result<()> {
        if self.render_pipeline.is_none() {
            assert_eq!(groups.iter().map(|x| x.1.len()).sum::<usize>(), 0);
            // We don't yet have any output ready (as the pipeline would be initialized otherwise),
            // so exit without doing anything.
            return Ok(());
        }

        let mut buffers: Vec<Option<JxlOutputBuffer>> = Vec::new();

        macro_rules! buffers_from_api {
            ($get_next: expr) => {
                if pixel_format.color_data_format.is_some() {
                    buffers.push($get_next);
                }

                for fmt in &pixel_format.extra_channel_format {
                    if fmt.is_some() {
                        buffers.push($get_next);
                    }
                }
            };
        }

        if let Some(api_buffers) = api_buffers {
            let mut api_buffers_iter = api_buffers.iter_mut();
            buffers_from_api!(Some(JxlOutputBuffer::reborrow(
                api_buffers_iter.next().unwrap(),
            )));
        } else {
            buffers_from_api!(None);
        }

        // Temporarily remove the reference/lf frames to be saved; we will move them back once
        // rendering is done.
        let mut reference_frame_data = std::mem::take(&mut self.reference_frame_data);
        let mut lf_frame_data = std::mem::take(&mut self.lf_frame_data);

        if let Some(ref_images) = &mut reference_frame_data {
            buffers.extend(ref_images.iter_mut().map(|img| {
                let rect = Rect {
                    size: img.size(),
                    origin: (0, 0),
                };
                Some(JxlOutputBuffer::from_image_rect_mut(
                    img.get_rect_mut(rect).into_raw(),
                ))
            }));
        };

        if let Some(lf_images) = &mut lf_frame_data {
            buffers.extend(lf_images.iter_mut().map(|img| {
                let rect = Rect {
                    size: img.size(),
                    origin: (0, 0),
                };
                Some(JxlOutputBuffer::from_image_rect_mut(
                    img.get_rect_mut(rect).into_raw(),
                ))
            }));
        };

        pipeline!(self, p, p.check_buffer_sizes(&mut buffers[..])?);

        let mut buffer_splitter = BufferSplitter::new(&mut buffers[..]);

        pipeline!(self, p, p.render_outside_frame(&mut buffer_splitter)?);

        // Render data from the lf global section, if we didn't do so already, before rendering HF.
        if !self.lf_global_was_rendered {
            self.lf_global_was_rendered = true;
            let lf_global = self.lf_global.as_mut().unwrap();
            let mut pass_to_pipeline = |chan, group, num_passes, image| {
                pipeline!(
                    self,
                    p,
                    p.set_buffer_for_group(chan, group, num_passes, image, &mut buffer_splitter)?
                );
                Ok(())
            };
            lf_global
                .modular_global
                .process_output(0, 0, &self.header, &mut pass_to_pipeline)?;
            for group in 0..self.header.num_lf_groups() {
                lf_global.modular_global.process_output(
                    1,
                    group,
                    &self.header,
                    &mut pass_to_pipeline,
                )?;
            }
        }

        for (group, passes) in groups {
            // TODO(veluca): render all the available passes at once.
            for (pass, br) in passes {
                self.decode_hf_group(group, pass, br, &mut buffer_splitter)?;
            }
        }

        self.reference_frame_data = reference_frame_data;
        self.lf_frame_data = lf_frame_data;

        Ok(())
    }

    pub(crate) fn build_render_pipeline<T: RenderPipeline>(
        decoder_state: &DecoderState,
        frame_header: &FrameHeader,
        lf_global: &LfGlobalState,
        epf_sigma: &Option<Arc<Image<f32>>>,
        pixel_format: &JxlPixelFormat,
    ) -> Result<Box<T>> {
        let num_channels = frame_header.num_extra_channels as usize + 3;
        let num_temp_channels = if frame_header.has_noise() { 3 } else { 0 };
        let metadata = &decoder_state.file_header.image_metadata;
        let mut pipeline = RenderPipelineBuilder::<T>::new(
            num_channels + num_temp_channels,
            frame_header.size_upsampled(),
            frame_header.upsampling.ilog2() as usize,
            frame_header.log_group_dim(),
            frame_header.passes.num_passes as usize,
        );

        if frame_header.encoding == Encoding::Modular {
            if decoder_state.file_header.image_metadata.xyb_encoded {
                pipeline = pipeline
                    .add_inout_stage(ConvertModularXYBToF32Stage::new(0, &lf_global.lf_quant))?
            } else {
                for i in 0..3 {
                    pipeline = pipeline
                        .add_inout_stage(ConvertModularToF32Stage::new(i, metadata.bit_depth))?;
                }
            }
        }
        for i in 3..num_channels {
            pipeline =
                pipeline.add_inout_stage(ConvertModularToF32Stage::new(i, metadata.bit_depth))?;
        }

        for c in 0..3 {
            if frame_header.hshift(c) != 0 {
                pipeline = pipeline.add_inout_stage(HorizontalChromaUpsample::new(c))?;
            }
            if frame_header.vshift(c) != 0 {
                pipeline = pipeline.add_inout_stage(VerticalChromaUpsample::new(c))?;
            }
        }

        let filters = &frame_header.restoration_filter;
        if filters.gab {
            pipeline = pipeline
                .add_inout_stage(GaborishStage::new(
                    0,
                    filters.gab_x_weight1,
                    filters.gab_x_weight2,
                ))?
                .add_inout_stage(GaborishStage::new(
                    1,
                    filters.gab_y_weight1,
                    filters.gab_y_weight2,
                ))?
                .add_inout_stage(GaborishStage::new(
                    2,
                    filters.gab_b_weight1,
                    filters.gab_b_weight2,
                ))?;
        }

        let rf = &frame_header.restoration_filter;
        if rf.epf_iters >= 3 {
            pipeline = pipeline.add_inout_stage(Epf0Stage::new(
                rf.epf_pass0_sigma_scale,
                rf.epf_border_sad_mul,
                rf.epf_channel_scale,
                epf_sigma.as_ref().unwrap().clone(),
            ))?
        }
        if rf.epf_iters >= 1 {
            pipeline = pipeline.add_inout_stage(Epf1Stage::new(
                1.0,
                rf.epf_border_sad_mul,
                rf.epf_channel_scale,
                epf_sigma.as_ref().unwrap().clone(),
            ))?
        }
        if rf.epf_iters >= 2 {
            pipeline = pipeline.add_inout_stage(Epf2Stage::new(
                rf.epf_pass2_sigma_scale,
                rf.epf_border_sad_mul,
                rf.epf_channel_scale,
                epf_sigma.as_ref().unwrap().clone(),
            ))?
        }

        let late_ec_upsample = frame_header.upsampling > 1
            && frame_header
                .ec_upsampling
                .iter()
                .all(|x| *x == frame_header.upsampling);

        if !late_ec_upsample {
            let transform_data = &decoder_state.file_header.transform_data;
            for (ec, ec_up) in frame_header.ec_upsampling.iter().enumerate() {
                if *ec_up > 1 {
                    pipeline = match *ec_up {
                        2 => pipeline.add_inout_stage(Upsample2x::new(transform_data, 3 + ec)),
                        4 => pipeline.add_inout_stage(Upsample4x::new(transform_data, 3 + ec)),
                        8 => pipeline.add_inout_stage(Upsample8x::new(transform_data, 3 + ec)),
                        _ => unreachable!(),
                    }?;
                }
            }
        }

        if frame_header.has_patches() {
            pipeline = pipeline.add_inplace_stage(PatchesStage {
                patches: lf_global.patches.clone().unwrap(),
                extra_channels: metadata.extra_channel_info.clone(),
                decoder_state: decoder_state.reference_frames.clone(),
            })?
        }

        if frame_header.has_splines() {
            pipeline = pipeline.add_inplace_stage(SplinesStage::new(
                lf_global.splines.clone().unwrap(),
                frame_header.size(),
                &lf_global.color_correlation_params.unwrap_or_default(),
                decoder_state.high_precision,
            )?)?
        }

        if frame_header.upsampling > 1 {
            let transform_data = &decoder_state.file_header.transform_data;
            let nb_channels = if late_ec_upsample {
                3 + frame_header.ec_upsampling.len()
            } else {
                3
            };
            for c in 0..nb_channels {
                pipeline = match frame_header.upsampling {
                    2 => pipeline.add_inout_stage(Upsample2x::new(transform_data, c)),
                    4 => pipeline.add_inout_stage(Upsample4x::new(transform_data, c)),
                    8 => pipeline.add_inout_stage(Upsample8x::new(transform_data, c)),
                    _ => unreachable!(),
                }?;
            }
        }

        if frame_header.has_noise() {
            pipeline = pipeline
                .add_inout_stage(ConvolveNoiseStage::new(num_channels))?
                .add_inout_stage(ConvolveNoiseStage::new(num_channels + 1))?
                .add_inout_stage(ConvolveNoiseStage::new(num_channels + 2))?
                .add_inplace_stage(AddNoiseStage::new(
                    *lf_global.noise.as_ref().unwrap(),
                    lf_global.color_correlation_params.unwrap_or_default(),
                    num_channels,
                ))?;
        }

        // Calculate the actual number of API-provided buffers based on pixel_format.
        // This is the number of buffers the caller provides, NOT the theoretical max.
        // When extra_channel_format[i] is None, that channel doesn't get a buffer.
        let num_api_buffers = std::iter::once(&pixel_format.color_data_format)
            .chain(pixel_format.extra_channel_format.iter())
            .filter(|x| x.is_some())
            .count();
        assert_eq!(
            pixel_format.extra_channel_format.len(),
            frame_header.num_extra_channels as usize
        );

        assert!(frame_header.lf_level == 0 || !frame_header.can_be_referenced);

        if frame_header.lf_level != 0 {
            for i in 0..3 {
                pipeline = pipeline.add_save_stage(
                    &[i],
                    Orientation::Identity,
                    num_api_buffers + i,
                    JxlColorType::Grayscale,
                    JxlDataFormat::f32(),
                    false,
                )?;
            }
        }
        if frame_header.can_be_referenced && frame_header.save_before_ct {
            for i in 0..num_channels {
                pipeline = pipeline.add_save_stage(
                    &[i],
                    Orientation::Identity,
                    num_api_buffers + i,
                    JxlColorType::Grayscale,
                    JxlDataFormat::f32(),
                    false,
                )?;
            }
        }

        let mut linear = false;
        let output_color_info = OutputColorInfo::from_header(&decoder_state.file_header)?;
        if frame_header.do_ycbcr {
            pipeline = pipeline.add_inplace_stage(YcbcrToRgbStage::new(0))?;
        } else if decoder_state.file_header.image_metadata.xyb_encoded {
            pipeline = pipeline.add_inplace_stage(XybStage::new(0, output_color_info.clone()))?;
            if decoder_state.xyb_output_linear {
                linear = true;
            } else {
                pipeline = pipeline
                    .add_inplace_stage(FromLinearStage::new(0, output_color_info.tf.clone()))?;
            }
        }

        if frame_header.needs_blending() {
            if linear {
                pipeline = pipeline
                    .add_inplace_stage(FromLinearStage::new(0, output_color_info.tf.clone()))?;
                linear = false;
            }
            pipeline = pipeline.add_inplace_stage(BlendingStage::new(
                frame_header,
                &decoder_state.file_header,
                decoder_state.reference_frames.clone(),
            )?)?;
            // TODO(veluca): we might not need to add an extend stage if the image size is
            // compatible with the frame size.
            pipeline = pipeline.add_extend_stage(ExtendToImageDimensionsStage::new(
                frame_header,
                &decoder_state.file_header,
                decoder_state.reference_frames.clone(),
            )?)?;
        }

        if frame_header.can_be_referenced && !frame_header.save_before_ct {
            if linear {
                pipeline = pipeline
                    .add_inplace_stage(FromLinearStage::new(0, output_color_info.tf.clone()))?;
                linear = false;
            }
            for i in 0..num_channels {
                pipeline = pipeline.add_save_stage(
                    &[i],
                    Orientation::Identity,
                    num_api_buffers + i,
                    JxlColorType::Grayscale,
                    JxlDataFormat::f32(),
                    false,
                )?;
            }
        }

        if decoder_state.render_spotcolors {
            for (i, info) in decoder_state
                .file_header
                .image_metadata
                .extra_channel_info
                .iter()
                .enumerate()
            {
                if info.ec_type == ExtraChannel::SpotColor {
                    pipeline = pipeline
                        .add_inplace_stage(SpotColorStage::new(i, info.spot_color.unwrap()))?;
                }
            }
        }

        if frame_header.is_visible() {
            let color_space = decoder_state
                .file_header
                .image_metadata
                .color_encoding
                .color_space;
            let num_color_channels = if color_space == ColorSpace::Gray {
                1
            } else {
                3
            };
            // Find the alpha channel info (index and metadata) if the color type requires alpha
            let alpha_channel_info = if pixel_format.color_type.has_alpha() {
                decoder_state
                    .file_header
                    .image_metadata
                    .extra_channel_info
                    .iter()
                    .enumerate()
                    .find(|x| x.1.ec_type == ExtraChannel::Alpha)
            } else {
                None
            };
            let alpha_in_color = alpha_channel_info.map(|x| x.0 + 3);
            // Check if the source alpha is already premultiplied (alpha_associated)
            let source_alpha_associated =
                alpha_channel_info.is_some_and(|(_, info)| info.alpha_associated());
            if pixel_format.color_type.is_grayscale() && num_color_channels == 3 {
                return Err(Error::NotGrayscale);
            }
            if decoder_state.file_header.image_metadata.xyb_encoded
                && decoder_state.xyb_output_linear
                && !linear
            {
                pipeline = pipeline
                    .add_inplace_stage(ToLinearStage::new(0, output_color_info.tf.clone()))?;
            }
            // Determine if we need to fill opaque alpha:
            // - color_type requests alpha (has_alpha() is true)
            // - but no actual alpha channel exists in the image (alpha_in_color is None)
            let fill_opaque_alpha = pixel_format.color_type.has_alpha() && alpha_in_color.is_none();

            // Determine if we should premultiply:
            // - premultiply_output is requested
            // - there is an alpha channel in the output
            // - source is not already premultiplied (to avoid double-premultiplication)
            let should_premultiply = decoder_state.premultiply_output
                && alpha_in_color.is_some()
                && !source_alpha_associated;

            let color_source_channels: &[usize] =
                match (pixel_format.color_type.is_grayscale(), alpha_in_color) {
                    (true, None) => &[0],
                    (true, Some(c)) => &[0, c],
                    (false, None) => &[0, 1, 2],
                    (false, Some(c)) => &[0, 1, 2, c],
                };
            if let Some(df) = &pixel_format.color_data_format {
                // Add premultiply stage if needed (before conversion to output format)
                if should_premultiply && let Some(alpha_channel) = alpha_in_color {
                    pipeline = pipeline.add_inplace_stage(PremultiplyAlphaStage::new(
                        0,
                        num_color_channels,
                        alpha_channel,
                    ))?;
                }
                // Add conversion stages for non-float output formats
                pipeline = Self::add_conversion_stages(pipeline, color_source_channels, *df)?;
                pipeline = pipeline.add_save_stage(
                    color_source_channels,
                    metadata.orientation,
                    0,
                    pixel_format.color_type,
                    *df,
                    fill_opaque_alpha,
                )?;
            }
            for i in 0..frame_header.num_extra_channels as usize {
                if let Some(df) = &pixel_format.extra_channel_format[i] {
                    // Add conversion stages for non-float output formats
                    pipeline = Self::add_conversion_stages(pipeline, &[3 + i], *df)?;
                    pipeline = pipeline.add_save_stage(
                        &[3 + i],
                        metadata.orientation,
                        1 + i,
                        JxlColorType::Grayscale,
                        *df,
                        false,
                    )?;
                }
            }
        }
        pipeline.build()
    }

    pub fn prepare_render_pipeline(
        &mut self,
        pixel_format: &JxlPixelFormat,
        _cms: Option<&dyn JxlCms>,
    ) -> Result<()> {
        let lf_global = self.lf_global.as_mut().unwrap();
        let epf_sigma = if self.header.restoration_filter.epf_iters > 0 {
            let sigma_image = create_sigma_image(&self.header, lf_global, &self.hf_meta)?;
            Some(Arc::new(sigma_image))
        } else {
            None
        };

        #[cfg(test)]
        let render_pipeline = if self.use_simple_pipeline {
            Self::build_render_pipeline::<SimpleRenderPipeline>(
                &self.decoder_state,
                &self.header,
                lf_global,
                &epf_sigma,
                pixel_format,
            )? as Box<dyn std::any::Any>
        } else {
            Self::build_render_pipeline::<LowMemoryRenderPipeline>(
                &self.decoder_state,
                &self.header,
                lf_global,
                &epf_sigma,
                pixel_format,
            )? as Box<dyn std::any::Any>
        };
        #[cfg(not(test))]
        let render_pipeline = Self::build_render_pipeline::<LowMemoryRenderPipeline>(
            &self.decoder_state,
            &self.header,
            lf_global,
            &epf_sigma,
            pixel_format,
        )?;
        self.render_pipeline = Some(render_pipeline);
        self.lf_global_was_rendered = false;
        Ok(())
    }
}
