// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::api::JxlColorProfile;
use crate::api::JxlColorType;
use crate::api::JxlDataFormat;
use crate::api::JxlOutputBuffer;
use crate::bit_reader::BitReader;
use crate::error::{Error, Result};
use crate::features::epf::SigmaSource;
use crate::features::noise::Noise;
use crate::features::patches::PatchesDictionary;
use crate::features::spline::Splines;
use crate::frame::DataStatus;
use crate::frame::color_correlation_map::ColorCorrelationParams;
use crate::frame::quantizer::LfQuantFactors;
use crate::headers::frame_header::Encoding;
use crate::headers::frame_header::FrameType;
use crate::headers::{Orientation, color_encoding::ColorSpace, extra_channels::ExtraChannel};
use crate::image::Image;
use crate::image::Rect;
use crate::util::AtomicRefCell;
use std::sync::Arc;

#[cfg(test)]
use crate::render::SimpleRenderPipeline;
use crate::render::buffer_splitter::BufferSplitter;
use crate::render::{LowMemoryRenderPipeline, RenderPipeline, RenderPipelineBuilder, stages::*};
use crate::{
    api::JxlPixelFormat,
    frame::{DecoderState, Frame},
    headers::frame_header::FrameHeader,
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
        clamp_range_for_f16: Option<(f32, f32)>,
    ) -> RenderPipelineBuilder<P> {
        use crate::render::stages::{
            ConvertF32ToF16Stage, ConvertF32ToU8Stage, ConvertF32ToU16Stage,
        };

        match data_format {
            JxlDataFormat::U8 { bit_depth } => {
                for &channel in channels {
                    pipeline =
                        pipeline.add_inout_stage(ConvertF32ToU8Stage::new(channel, bit_depth));
                }
            }
            JxlDataFormat::U16 { bit_depth, .. } => {
                for &channel in channels {
                    pipeline =
                        pipeline.add_inout_stage(ConvertF32ToU16Stage::new(channel, bit_depth));
                }
            }
            JxlDataFormat::F16 { .. } => {
                for &channel in channels {
                    pipeline = pipeline.add_inout_stage(
                        ConvertF32ToF16Stage::new_with_clamp_range(channel, clamp_range_for_f16),
                    );
                }
            }
            // F32 doesn't need conversion - the pipeline already uses f32
            JxlDataFormat::F32 { .. } => {}
        }
        pipeline
    }

    /// Returns `true` if any pixels were written to the output buffers during
    /// this call, `false` if the call was a no-op for the buffers (e.g. no new
    /// HF groups, no flush work, or the render pipeline was not yet ready).
    pub fn decode_and_render_hf_groups(
        &mut self,
        api_buffers: &mut Option<&mut [JxlOutputBuffer<'_>]>,
        pixel_format: &JxlPixelFormat,
        groups: Vec<(usize, Vec<(usize, BitReader)>)>,
        do_flush: bool,
        output_profile: &JxlColorProfile,
    ) -> Result<bool> {
        if !do_flush && groups.is_empty() {
            // Nothing to do.
            return Ok(false);
        }

        if self.render_pipeline.is_none() || self.lf_global.is_none() {
            assert_eq!(groups.iter().map(|x| x.1.len()).sum::<usize>(), 0);
            // We don't yet have any output ready (as the pipeline would be initialized otherwise),
            // so exit without doing anything.
            return Ok(false);
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

        let should_render_non_final = self.allow_rendering_before_last_pass() && do_flush;

        let modular_global = &mut self.lf_global.as_mut().unwrap().modular_global;

        modular_global.set_pipeline_used_channels(pipeline!(self, p, p.used_channel_mask()));

        // STEP 1: figure out what modular buffers will be finalized during this decode, and mark them
        // as such.
        for (group, passes) in groups.iter() {
            self.group_status.need_vardct_flush.insert(*group);
            self.group_status.need_modular_flush.insert(*group);
            if self.header.encoding == Encoding::VarDCT {
                let status = if passes
                    .last()
                    .is_some_and(|x| x.0 + 1 >= self.header.passes.num_passes as usize)
                {
                    DataStatus::Final
                } else {
                    DataStatus::Partial
                };
                for c in 0..3 {
                    self.group_status.update_status(*group, c, status);
                }
            }
            for (pass, _) in passes.iter() {
                modular_global.mark_final(2 + *pass, *group);
            }
        }

        // STEP 2: mark all the groups that will need a progressive re-render if
        // we are flushing.

        // Request re-renders in updated LF groups if those contain meaningful
        // data and we are decoding a Modular image.
        if modular_global.can_do_early_partial_render()
            && self.section0_render_up_to_date
            && self.header.encoding == Encoding::Modular
        {
            for lg in std::mem::take(&mut self.dirty_lf_groups) {
                let lgx = lg % self.header.size_lf_groups().0;
                let lgy = lg / self.header.size_lf_groups().0;
                let (sgx, sgy) = self.header.size_groups();
                for iy in 0..10 {
                    let gy = (lgy * 8 + iy).saturating_sub(1);
                    if gy >= sgy {
                        continue;
                    }
                    for ix in 0..10 {
                        let gx = (lgx * 8 + ix).saturating_sub(1);
                        if gx >= sgx {
                            continue;
                        }
                        self.group_status.need_modular_flush.insert(gy * sgx + gx);
                    }
                }
            }
        }

        let has_decoded_data = match self.header.encoding {
            Encoding::VarDCT => self.hf_global.is_some() || self.header.has_lf_frame(),
            Encoding::Modular => modular_global.has_decoded_data(),
        };

        // If section0 data is dirty, re-render everything.
        if !self.section0_render_up_to_date && has_decoded_data {
            self.section0_render_up_to_date = true;
            for g in 0..self.header.num_groups() {
                self.group_status.need_vardct_flush.insert(g);
                self.group_status.need_modular_flush.insert(g);
            }
        }

        if should_render_non_final {
            if self.header.encoding == Encoding::VarDCT {
                for group in self.group_status.need_vardct_flush.iter() {
                    pipeline!(self, p, p.mark_group_to_rerender(*group));
                    modular_global.request_rerender(&self.header, *group);
                }
            }
            for group in std::mem::take(&mut self.group_status.need_modular_flush) {
                if self.header.lf_level != 0 {
                    let (gsx, gsy) = self.header.size_groups();
                    let gx = group % gsx;
                    let gy = group / gsx;
                    let gxm = gx.saturating_sub(1);
                    let gxp = (gx + 1).min(gsx - 1);
                    let gym = gy.saturating_sub(1);
                    let gyp = (gy + 1).min(gsy - 1);
                    modular_global.request_rerender(&self.header, gym * gsx + gxm);
                    modular_global.request_rerender(&self.header, gym * gsx + gx);
                    modular_global.request_rerender(&self.header, gym * gsx + gxp);
                    modular_global.request_rerender(&self.header, gy * gsx + gxm);
                    modular_global.request_rerender(&self.header, gy * gsx + gx);
                    modular_global.request_rerender(&self.header, gy * gsx + gxp);
                    modular_global.request_rerender(&self.header, gyp * gsx + gxm);
                    modular_global.request_rerender(&self.header, gyp * gsx + gx);
                    modular_global.request_rerender(&self.header, gyp * gsx + gxp);
                } else {
                    modular_global.request_rerender(&self.header, group);
                }
            }
        }

        // STEP 3: Run all the transforms that could be run already.
        // We do this because some modular images might not have coded channels in HF, so
        // all the coded channels were already decoded and the modular decoder does not
        // automatically call run_all_transforms unless a new channel is decoded.

        // ... but first, make sure modular_global is ready to run.
        modular_global.prepare_render(&self.header, |g, c, is_final| {
            self.group_status.update_status(
                g,
                c,
                if is_final {
                    DataStatus::Final
                } else {
                    DataStatus::Partial
                },
            );
            self.group_status.need_vardct_flush.insert(g);
            if should_render_non_final {
                pipeline!(self, p, p.mark_group_to_rerender(g));
            }
        });

        let mut pass_to_pipeline = |chan, group, complete, image: Image<i32>| {
            pipeline!(
                self,
                p,
                p.set_buffer_for_group(chan, group, complete, image, &mut buffer_splitter)?
            );
            Ok(())
        };

        modular_global.run_all_transforms(&self.header, &mut pass_to_pipeline)?;

        // STEP 4: decode the groups, eagerly decoding all the data.
        for (group, mut passes) in groups {
            self.decode_hf_group(group, &mut passes, &mut buffer_splitter, do_flush)?;
        }

        self.lf_global
            .as_ref()
            .unwrap()
            .modular_global
            .validate_state_after_transforms();

        // STEP 5: re-render VarDCT/noise data in rendered groups for which it was
        // not rendered.
        if should_render_non_final || self.group_status.incomplete_groups == 0 {
            for g in std::mem::take(&mut self.group_status.need_vardct_flush) {
                self.decode_hf_group(g, &mut [], &mut buffer_splitter, true)?;
            }
        }

        let regions = buffer_splitter.into_changed_regions();
        let rendered = !regions.is_empty() && self.header.frame_type == FrameType::RegularFrame;

        self.reference_frame_data = reference_frame_data;
        self.lf_frame_data = lf_frame_data;

        if self.header.frame_type == FrameType::LFFrame
            && self.header.lf_level == 1
            && has_decoded_data
        {
            if do_flush && let Some(buffers) = api_buffers {
                return self.maybe_preview_lf_frame(
                    pixel_format,
                    buffers,
                    &regions[..],
                    output_profile,
                );
            } else if self.group_status.incomplete_groups == 0 {
                // If we are not requesting another flush at the end of the LF frame, we
                // probably have a partial render. Ensure we re-render the LF frame when
                // decoding the actual frame.
                self.decoder_state.lf_frame_was_rendered = false;
            }
        }

        Ok(rendered)
    }

    #[allow(clippy::too_many_arguments)]
    pub(crate) fn build_render_pipeline<T: RenderPipeline>(
        decoder_state: &DecoderState,
        frame_header: &FrameHeader,
        patches: Arc<AtomicRefCell<PatchesDictionary>>,
        splines: Arc<AtomicRefCell<Splines>>,
        noise: Arc<AtomicRefCell<Noise>>,
        lf_quant: Arc<AtomicRefCell<LfQuantFactors>>,
        color_correlation_params: Arc<AtomicRefCell<ColorCorrelationParams>>,
        epf_sigma: Arc<AtomicRefCell<SigmaSource>>,
        pixel_format: &JxlPixelFormat,
        output_profile: &JxlColorProfile,
    ) -> Result<Box<T>> {
        let num_channels = frame_header.num_extra_channels as usize + 3;
        let num_temp_channels = if frame_header.has_noise() { 3 } else { 0 };
        let metadata = &decoder_state.file_header.image_metadata;
        let mut pipeline = RenderPipelineBuilder::<T>::new(
            num_channels + num_temp_channels,
            frame_header.size_upsampled(),
            frame_header.upsampling.ilog2() as usize,
            frame_header.log_group_dim(),
            // TODO(veluca): we should instead have modular mode participate in buffer reuse.
            if frame_header.encoding == Encoding::Modular {
                Some(0)
            } else {
                None
            },
        );

        if frame_header.encoding == Encoding::Modular {
            if decoder_state.file_header.image_metadata.xyb_encoded {
                pipeline = pipeline.add_inout_stage(ConvertModularXYBToF32Stage::new(0, lf_quant))
            } else {
                for i in 0..3 {
                    pipeline = pipeline
                        .add_inout_stage(ConvertModularToF32Stage::new(i, metadata.bit_depth));
                }
            }
        }
        for i in 3..num_channels {
            let ec_bit_depth = metadata.extra_channel_info[i - 3].bit_depth();
            pipeline = pipeline.add_inout_stage(ConvertModularToF32Stage::new(i, ec_bit_depth));
        }

        for c in 0..3 {
            if frame_header.hshift(c) != 0 {
                pipeline = pipeline.add_inout_stage(HorizontalChromaUpsample::new(c));
            }
            if frame_header.vshift(c) != 0 {
                pipeline = pipeline.add_inout_stage(VerticalChromaUpsample::new(c));
            }
        }

        let filters = &frame_header.restoration_filter;
        if filters.gab {
            pipeline = pipeline
                .add_inout_stage(GaborishStage::new(
                    0,
                    filters.gab_x_weight1,
                    filters.gab_x_weight2,
                ))
                .add_inout_stage(GaborishStage::new(
                    1,
                    filters.gab_y_weight1,
                    filters.gab_y_weight2,
                ))
                .add_inout_stage(GaborishStage::new(
                    2,
                    filters.gab_b_weight1,
                    filters.gab_b_weight2,
                ));
        }

        let rf = &frame_header.restoration_filter;
        if rf.epf_iters >= 3 {
            pipeline = pipeline.add_inout_stage(Epf0Stage::new(
                rf.epf_pass0_sigma_scale,
                rf.epf_border_sad_mul,
                rf.epf_channel_scale,
                epf_sigma.clone(),
            ))
        }
        if rf.epf_iters >= 1 {
            pipeline = pipeline.add_inout_stage(Epf1Stage::new(
                1.0,
                rf.epf_border_sad_mul,
                rf.epf_channel_scale,
                epf_sigma.clone(),
            ))
        }
        if rf.epf_iters >= 2 {
            pipeline = pipeline.add_inout_stage(Epf2Stage::new(
                rf.epf_pass2_sigma_scale,
                rf.epf_border_sad_mul,
                rf.epf_channel_scale,
                epf_sigma.clone(),
            ))
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
                    };
                }
            }
        }

        if frame_header.has_patches() {
            pipeline = pipeline.add_inplace_stage(PatchesStage::new(
                patches,
                metadata.extra_channel_info.clone(),
                decoder_state.reference_frames.clone(),
            ))
        }

        if frame_header.has_splines() {
            pipeline = pipeline.add_inplace_stage(SplinesStage::new(splines))
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
                };
            }
        }

        if frame_header.has_noise() {
            pipeline = pipeline
                .add_inout_stage(ConvolveNoiseStage::new(num_channels))
                .add_inout_stage(ConvolveNoiseStage::new(num_channels + 1))
                .add_inout_stage(ConvolveNoiseStage::new(num_channels + 2))
                .add_inplace_stage(AddNoiseStage::new(
                    noise,
                    color_correlation_params,
                    num_channels,
                ));
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
                );
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
                );
            }
        }

        let output_color_info = OutputColorInfo::from_header(&decoder_state.file_header)?;

        // Determine output TF: use output profile's TF if available, else fall back to embedded profile's TF.
        // Note: output_color_info (luminances, opsin matrix) always comes from the embedded profile;
        // CMS handles any primaries conversion if the output profile differs.
        let output_tf = output_profile
            .transfer_function()
            .map(|tf| {
                TransferFunction::from_api_tf(
                    tf,
                    output_color_info.intensity_target,
                    output_color_info.luminances,
                )
            })
            .unwrap_or_else(|| output_color_info.tf.clone());

        // Clamp transfer-domain values while converting to f16 so we don't
        // emit wild out-of-range values to downstream consumers.
        //
        // PQ has a bounded signal domain [0,1].
        // HLG may carry modest overshoot/undershoot (e.g. from narrow-range
        // workflows), so preserve headroom with a looser clamp.
        let clamp_range_for_f16 = match &output_tf {
            TransferFunction::Pq { .. } => Some((0.0, 1.0)),
            TransferFunction::Hlg { .. } => Some((-0.074, 1.1)),
            _ => None,
        };

        let xyb_encoded = decoder_state.file_header.image_metadata.xyb_encoded;

        if frame_header.do_ycbcr {
            pipeline = pipeline.add_inplace_stage(YcbcrToRgbStage::new(0));
        } else if xyb_encoded {
            pipeline = pipeline.add_inplace_stage(XybStage::new(0, output_color_info.clone()));
        }

        // XYB output is linear, so apply transfer function, but only if output is not linear itself
        if xyb_encoded && !output_tf.is_linear() {
            pipeline = pipeline.add_inplace_stage(FromLinearStage::new(0, output_tf.clone()));
        }

        if frame_header.needs_blending() {
            pipeline = pipeline.add_inplace_stage(BlendingStage::new(
                frame_header,
                &decoder_state.file_header,
                decoder_state.reference_frames.clone(),
            )?);
            // TODO(veluca): we might not need to add an extend stage if the image size is
            // compatible with the frame size.
            pipeline = pipeline.add_extend_stage(ExtendToImageDimensionsStage::new(
                frame_header,
                &decoder_state.file_header,
                decoder_state.reference_frames.clone(),
            )?);
        }

        if frame_header.can_be_referenced && !frame_header.save_before_ct {
            for i in 0..num_channels {
                pipeline = pipeline.add_save_stage(
                    &[i],
                    Orientation::Identity,
                    num_api_buffers + i,
                    JxlColorType::Grayscale,
                    JxlDataFormat::f32(),
                    false,
                );
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
                        .add_inplace_stage(SpotColorStage::new(i, info.spot_color.unwrap()));
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
                    ));
                }
                // Add conversion stages for non-float output formats
                pipeline = Self::add_conversion_stages(
                    pipeline,
                    color_source_channels,
                    *df,
                    clamp_range_for_f16,
                );
                pipeline = pipeline.add_save_stage(
                    color_source_channels,
                    metadata.orientation,
                    0,
                    pixel_format.color_type,
                    *df,
                    fill_opaque_alpha,
                );
            }
            let mut save_idx = if pixel_format.color_data_format.is_some() {
                1
            } else {
                0
            };
            for i in 0..frame_header.num_extra_channels as usize {
                if let Some(df) = &pixel_format.extra_channel_format[i] {
                    // Add conversion stages for non-float output formats
                    pipeline = Self::add_conversion_stages(pipeline, &[3 + i], *df, None);
                    pipeline = pipeline.add_save_stage(
                        &[3 + i],
                        metadata.orientation,
                        save_idx,
                        JxlColorType::Grayscale,
                        *df,
                        false,
                    );
                    save_idx += 1;
                }
            }
        }
        pipeline.build()
    }

    pub fn prepare_render_pipeline(
        &mut self,
        pixel_format: &JxlPixelFormat,
        output_profile: &JxlColorProfile,
    ) -> Result<()> {
        #[cfg(test)]
        let render_pipeline = if self.use_simple_pipeline {
            Self::build_render_pipeline::<SimpleRenderPipeline>(
                &self.decoder_state,
                &self.header,
                self.patches.clone(),
                self.splines.clone(),
                self.noise.clone(),
                self.lf_quant.clone(),
                self.color_correlation_params.clone(),
                self.epf_sigma.clone(),
                pixel_format,
                output_profile,
            )? as Box<dyn std::any::Any>
        } else {
            Self::build_render_pipeline::<LowMemoryRenderPipeline>(
                &self.decoder_state,
                &self.header,
                self.patches.clone(),
                self.splines.clone(),
                self.noise.clone(),
                self.lf_quant.clone(),
                self.color_correlation_params.clone(),
                self.epf_sigma.clone(),
                pixel_format,
                output_profile,
            )? as Box<dyn std::any::Any>
        };
        #[cfg(not(test))]
        let render_pipeline = Self::build_render_pipeline::<LowMemoryRenderPipeline>(
            &self.decoder_state,
            &self.header,
            self.patches.clone(),
            self.splines.clone(),
            self.noise.clone(),
            self.lf_quant.clone(),
            self.color_correlation_params.clone(),
            self.epf_sigma.clone(),
            pixel_format,
            output_profile,
        )?;
        self.render_pipeline = Some(render_pipeline);
        self.section0_render_up_to_date = false;
        Ok(())
    }
}
