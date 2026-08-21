// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::sync::Arc;
use std::collections::BTreeSet;

use super::render::pipeline;
use super::{
    HfMetaSplitter, HfMetaViews, LfImageSplitter,
    block_context_map::BlockContextMap,
    coeff_order::decode_coeff_orders,
    color_correlation_map::ColorCorrelationParams,
    group::decode_vardct_group,
    modular::{FullModularImage, ModularStreamId, Tree, decode_hf_metadata, decode_vardct_lf},
    quant_weights::DequantMatrices,
    quantizer::{LfQuantFactors, QuantizerParams},
};
use crate::error::Error;
use crate::features::epf::SigmaSource;
use crate::frame::block_context_map::{ZERO_DENSITY_CONTEXT_COUNT, ZERO_DENSITY_CONTEXT_LIMIT};
use crate::frame::group::VarDctBuffers;
use crate::frame::{DataStatus, GroupStatus};
use crate::headers::frame_header::FrameType;
use crate::image::Rect;
#[cfg(test)]
use crate::render::SimpleRenderPipeline;
use crate::render::buffer_splitter::BufferSplitter;
use crate::util::sync::{Mutex, RwLock};
use crate::util::{NewWithCapacity, PerThreadStorage};
use crate::util::{ShiftRightCeil, mirror};
use crate::{
    GROUP_DIM,
    bit_reader::BitReader,
    entropy_coding::decode::Histograms,
    error::Result,
    features::{noise::Noise, patches::PatchesDictionary, spline::Splines},
    frame::{
        DecoderState, Frame, HfGlobalState, HfMetadata, LfGlobalState, PassState, coeff_order,
    },
    headers::{
        color_encoding::ColorSpace,
        frame_header::{Encoding, FrameHeader},
        toc::Toc,
    },
    image::Image,
    render::RenderPipeline,
    util::{CeilLog2, Xorshift128Plus, tracing_wrappers::*},
};
use jxl_transforms::transform_map::*;

use crate::headers::CustomTransformData;
use crate::render::RenderPipelineInOutStage;
use crate::render::stages::Upsample8x;
use crate::render::{Channels, ChannelsMut};

fn upsample_lf_group(
    group: usize,
    pixels: &mut [Image<f32>; 3],
    lf_image: &[Image<f32>; 3],
    header: &FrameHeader,
    factors: &CustomTransformData,
) -> Result<()> {
    let group_dim = header.group_dim();
    let lf_group_dim = group_dim / 8;
    let (width_groups, _) = header.size_groups();
    let gx = group % width_groups;
    let gy = group / width_groups;

    let upsample = Upsample8x::new(factors, 0);
    let mut state = upsample.init_local_state()?.unwrap();

    let max_width = pixels.iter().map(|x| x.size().0).max().unwrap();

    // Temporary buffer for 8 output rows
    // We reuse this buffer for each iteration to minimize allocation
    let mut temp_out_buf: [_; 8] = std::array::from_fn(|_| vec![0.0f32; max_width + 128]);

    let mut input_rows_storage: [_; 5] = std::array::from_fn(|_| vec![0.0; max_width / 8 + 32]);

    for c in 0..3 {
        let lf_img = &lf_image[c];
        let out_img = &mut pixels[c];
        let (out_width, out_height) = out_img.size();

        let vs = header.vshift(c);
        let hs = header.hshift(c);

        let lf_group_dim_x = lf_group_dim >> hs;
        let lf_group_dim_y = lf_group_dim >> vs;
        let lf_x0 = gx * lf_group_dim_x;
        let lf_y0 = gy * lf_group_dim_y;

        let lf_width = lf_img.size().0.shrc(hs);
        let lf_height = lf_img.size().1.shrc(hs);

        let start_x = lf_x0.saturating_sub(2);
        let lf_x1 = (lf_x0 + lf_group_dim_x).min(lf_width);
        let end_x = (lf_x1 + 2).min(lf_width);
        let copy_width = end_x - start_x;

        for y in 0..lf_group_dim_y {
            let cy = lf_y0 + y;

            for dy in -2..=2 {
                let iy = cy as isize + dy;
                let iy = mirror(iy, lf_height);

                let storage = &mut input_rows_storage[(dy + 2) as usize];

                let save_start = if start_x == lf_x0 { 2 } else { 0 };
                let save_end = save_start + copy_width;

                storage[save_start..save_end].copy_from_slice(&lf_img.row(iy)[start_x..end_x]);

                if start_x == lf_x0 {
                    storage[0] = storage[2 + mirror(-2, copy_width)];
                    storage[1] = storage[2 + mirror(-1, copy_width)];
                }
                if end_x == lf_x1 {
                    storage[save_end] = storage[save_start + mirror(save_end as isize, save_end)];
                    storage[save_end + 1] =
                        storage[save_start + mirror(save_end as isize + 1, save_end)];
                }
            }

            let input_rows_refs = input_rows_storage.iter().map(|x| &x[..]).collect();
            let input_channels = Channels::new(input_rows_refs, 1, 5);

            {
                // Prepare output refs
                let output_rows_refs = temp_out_buf.iter_mut().map(|x| &mut x[..]).collect();
                let mut output_channels = ChannelsMut::new(output_rows_refs, 1, 8);

                upsample.process_row_chunk(
                    (0, 0),
                    lf_x1 - lf_x0,
                    &input_channels,
                    &mut output_channels,
                    Some(state.as_mut()),
                );
            }

            // Copy back to out_img
            let base_y = y * 8;
            for (i, buf) in temp_out_buf.iter().enumerate() {
                let out_y = base_y + i;
                if out_y < out_height {
                    out_img.row_mut(out_y)[..out_width].copy_from_slice(&buf[..out_width]);
                }
            }
        }
    }
    Ok(())
}

impl Frame {
    pub fn from_header_and_toc(
        frame_header: FrameHeader,
        toc: Toc,
        mut decoder_state: DecoderState,
    ) -> Result<Self> {
        if frame_header.is_visible() {
            decoder_state.visible_frame_index += 1;
            decoder_state.nonvisible_frame_index = 0;
        } else {
            decoder_state.nonvisible_frame_index += 1;
        }
        if frame_header.frame_type == FrameType::LFFrame && frame_header.lf_level == 1 {
            decoder_state.lf_frame_was_rendered = false;
        }
        let image_metadata = &decoder_state.file_header.image_metadata;
        let is_gray = !frame_header.do_ycbcr
            && !image_metadata.xyb_encoded
            && image_metadata.color_encoding.color_space == ColorSpace::Gray;
        let color_channels = if is_gray { 1 } else { 3 };
        let size_blocks = frame_header.size_blocks();
        let lf_image = if frame_header.encoding == Encoding::VarDCT {
            if frame_header.has_lf_frame() {
                if decoder_state.lf_frames[frame_header.lf_level as usize].is_none() {
                    return Err(Error::NoLfFrame(frame_header.lf_level));
                } else {
                    None
                }
            } else {
                Some([
                    Image::new(size_blocks)?,
                    Image::new(size_blocks)?,
                    Image::new(size_blocks)?,
                ])
            }
        } else {
            None
        };
        let size_color_tiles = (size_blocks.0.div_ceil(8), size_blocks.1.div_ceil(8));
        let hf_meta = if frame_header.encoding == Encoding::VarDCT {
            Some(HfMetadata {
                ytox_map: Image::new(size_color_tiles)?,
                ytob_map: Image::new(size_color_tiles)?,
                raw_quant_map: Image::new(size_blocks)?,
                transform_map: Image::new_with_value(
                    size_blocks,
                    HfTransformType::INVALID_TRANSFORM,
                )?,
                epf_map: Image::new(size_blocks)?,
                quant_lf: Image::new(size_blocks)?,
            })
        } else {
            None
        };

        let reference_frame_data = if frame_header.can_be_referenced {
            let image_size = &decoder_state.file_header.size;
            let image_size = (image_size.xsize() as usize, image_size.ysize() as usize);
            let sz = if frame_header.save_before_ct {
                frame_header.size_upsampled()
            } else {
                image_size
            };

            let num_ref_channels = 3 + image_metadata.extra_channel_info.len();
            Some(
                (0..num_ref_channels)
                    .map(|_| Image::new(sz))
                    .collect::<Result<Vec<_>>>()?,
            )
        } else {
            None
        };

        let lf_frame_data = if frame_header.lf_level != 0 {
            Some(
                (0..3)
                    .map(|_| Image::new(frame_header.size_upsampled()))
                    .collect::<Result<Vec<_>, _>>()?
                    .try_into()
                    .unwrap(),
            )
        } else {
            None
        };

        let num_extra_channels = image_metadata.extra_channel_info.len();

        Ok(Self {
            #[cfg(test)]
            use_simple_pipeline: decoder_state.use_simple_pipeline,
            group_status: GroupStatus::new(&frame_header),
            header: frame_header,
            color_channels,
            toc,
            lf_global: None,
            hf_global: None,
            lf_image,
            hf_meta,
            decoder_state,
            render_pipeline: None,
            reference_frame_data,
            lf_frame_data,
            section0_render_up_to_date: false,
            vardct_buffers: PerThreadStorage::new(VarDctBuffers::new),
            patches: Arc::new(RwLock::new(PatchesDictionary::new(num_extra_channels))),
            splines: Arc::new(RwLock::new(Splines::default())),
            noise: Arc::new(RwLock::new(Noise::default())),
            lf_quant: Arc::new(RwLock::new(LfQuantFactors::default())),
            color_correlation_params: Arc::new(RwLock::new(ColorCorrelationParams::default())),
            epf_sigma: Arc::new(RwLock::new(SigmaSource::default())),
            dirty_lf_groups: BTreeSet::new(),
        })
    }

    pub fn allow_rendering_before_last_pass(&self) -> bool {
        if self
            .lf_global
            .as_ref()
            .is_none_or(|x| !x.modular_global.can_do_partial_render())
        {
            return false;
        }

        self.header.frame_type == FrameType::RegularFrame
            || (self.header.frame_type == FrameType::LFFrame
                && self.header.lf_level == 1
                // TODO(veluca): this should probably be "there is no alpha".
                && self.header.num_extra_channels == 0)
    }

    /// Given a bit reader pointing at the end of the TOC, returns a vector of `BitReader`s, each
    /// of which reads a specific section.
    pub fn sections<'a>(&self, br: &'a mut BitReader) -> Result<Vec<BitReader<'a>>> {
        debug!(toc = ?self.toc);
        let ret = self
            .toc
            .entries
            .iter()
            .scan(br, |br, count| Some(br.split_at(*count as usize)))
            .collect::<Result<Vec<_>>>()?;
        if !self.toc.permuted {
            return Ok(ret);
        }
        let mut inv_perm = vec![0; ret.len()];
        for (i, pos) in self.toc.permutation.iter().enumerate() {
            inv_perm[*pos as usize] = i;
        }
        let mut shuffled_ret = ret.clone();
        for (br, pos) in ret.into_iter().zip(inv_perm) {
            shuffled_ret[pos] = br;
        }
        Ok(shuffled_ret)
    }

    #[instrument(level = "debug", skip_all)]
    pub fn decode_lf_global(&mut self, br: &mut BitReader, allow_partial: bool) -> Result<()> {
        debug!(section_size = br.total_bits_available());

        if let Some(lfg) = &self.lf_global {
            br.skip_bits(lfg.total_bits_read)?;
        } else {
            trace!(pos = br.total_bits_read());

            if self.header.has_patches() {
                info!("decoding patches");
                let p = PatchesDictionary::read(
                    br,
                    self.header.size_padded().0,
                    self.header.size_padded().1,
                    self.decoder_state.extra_channel_info().len(),
                    &self.decoder_state.reference_frames[..],
                )?;
                *self.patches.try_write().unwrap() = p;
            }

            if self.header.has_splines() {
                info!("decoding splines");
                let s = Splines::read(br, self.header.width * self.header.height)?;
                *self.splines.try_write().unwrap() = s;
            }

            if self.header.has_noise() {
                info!("decoding noise");
                let n = Noise::read(br)?;
                *self.noise.try_write().unwrap() = n;
            }

            let lf_quant = LfQuantFactors::new(br)?;
            *self.lf_quant.try_write().unwrap() = lf_quant.clone();
            debug!(?lf_quant);

            let quant_params = if self.header.encoding == Encoding::VarDCT {
                info!("decoding VarDCT quantizer params");
                Some(QuantizerParams::read(br)?)
            } else {
                None
            };
            debug!(?quant_params);

            let block_context_map = if self.header.encoding == Encoding::VarDCT {
                info!("decoding block context map");
                Some(BlockContextMap::read(br)?)
            } else {
                None
            };
            debug!(?block_context_map);

            let color_correlation_params = if self.header.encoding == Encoding::VarDCT {
                info!("decoding color correlation params");
                let ccp = ColorCorrelationParams::read(br)?;
                *self.color_correlation_params.try_write().unwrap() = ccp;
                Some(ccp)
            } else {
                None
            };
            debug!(?color_correlation_params);

            // Validate spline parameters
            if self.header.has_splines() {
                let color_correlation_params = self.color_correlation_params.try_read().unwrap();
                self.splines.try_write().unwrap().initialize_draw_cache(
                    self.header.size().0 as u64,
                    self.header.size().1 as u64,
                    &color_correlation_params,
                    self.decoder_state.high_precision,
                )?;
            }

            let tree = if br.read(1)? == 1 {
                let size_limit = (1024
                    + self.header.width as usize
                        * self.header.height as usize
                        * (self.color_channels + self.decoder_state.extra_channel_info().len())
                        / 16)
                    .min(1 << 22);
                Some(Tree::read(br, size_limit)?)
            } else {
                None
            };

            let modular_global = FullModularImage::read(
                &self.header,
                &self.decoder_state.file_header.image_metadata,
                self.modular_color_channels(),
                br,
            )?;

            // Ensure that, if we call this function again, we resume from just after
            // reading modular global data (excluding section 0 channels).
            let total_bits_read = br.total_bits_read();

            self.lf_global = Some(LfGlobalState {
                lf_quant,
                quant_params,
                block_context_map,
                color_correlation_params,
                tree,
                modular_global,
                total_bits_read,
            });
        }

        let lf_global = self.lf_global.as_mut().unwrap();

        if lf_global.modular_global.read_section0(
            &self.header,
            &lf_global.tree,
            br,
            allow_partial,
        )? {
            // Request a global re-render.
            self.section0_render_up_to_date = false;
        }

        Ok(())
    }

    pub fn decode_lf_group(
        header: &FrameHeader,
        decoder_state: &DecoderState,
        lf_global: &LfGlobalState,
        group: usize,
        br: &mut BitReader,
        lf_splitter: Option<&LfImageSplitter>,
        hf_meta_splitter: Option<&HfMetaSplitter>,
    ) -> Result<()> {
        debug!(section_size = br.total_bits_available());
        let r = header.lf_group_rect(group);
        let cr = Rect {
            origin: (r.origin.0 >> 3, r.origin.1 >> 3),
            size: (r.size.0.div_ceil(8), r.size.1.div_ceil(8)),
        };

        if header.encoding == Encoding::VarDCT && !header.has_lf_frame() {
            info!("decoding VarDCT LF with group id {}", group);
            let splitter_lf = lf_splitter.as_ref().unwrap();
            let splitter_hf = hf_meta_splitter.as_ref().unwrap();
            let mut lf_views = [
                splitter_lf.borrow_rect(0, r),
                splitter_lf.borrow_rect(1, r),
                splitter_lf.borrow_rect(2, r),
            ];
            let mut quant_lf_view = splitter_hf.quant_lf.borrow_typed_rect::<u8>(r);
            decode_vardct_lf(
                group,
                header,
                &decoder_state.file_header.image_metadata,
                &lf_global.tree,
                lf_global.color_correlation_params.as_ref().unwrap(),
                lf_global.quant_params.as_ref().unwrap(),
                &lf_global.lf_quant,
                lf_global.block_context_map.as_ref().unwrap(),
                &mut lf_views,
                &mut quant_lf_view,
                br,
            )?;
        }

        lf_global.modular_global.read_stream(
            ModularStreamId::ModularLF(group),
            header,
            &lf_global.tree,
            br,
            None,
        )?;
        if header.encoding == Encoding::VarDCT {
            info!("decoding HF metadata with group id {}", group);
            let splitter_hf = hf_meta_splitter.as_ref().unwrap();
            let mut hf_views = HfMetaViews {
                ytox_map: splitter_hf.ytox_map.borrow_typed_rect::<i8>(cr),
                ytob_map: splitter_hf.ytob_map.borrow_typed_rect::<i8>(cr),
                raw_quant_map: splitter_hf.raw_quant_map.borrow_typed_rect::<i32>(r),
                transform_map: splitter_hf.transform_map.borrow_typed_rect::<u8>(r),
                epf_map: splitter_hf.epf_map.borrow_typed_rect::<u8>(r),
            };
            decode_hf_metadata(
                group,
                header,
                &decoder_state.file_header.image_metadata,
                &lf_global.tree,
                &mut hf_views,
                br,
            )?;
        }
        Ok(())
    }

    pub fn post_decode_lf_group(&mut self, group: usize) {
        self.dirty_lf_groups.insert(group);
        let lf_global = self.lf_global.as_mut().unwrap();
        lf_global.modular_global.mark_final(1, group);
    }

    #[instrument(level = "debug", skip_all)]
    pub fn decode_hf_global(&mut self, br: &mut BitReader) -> Result<()> {
        debug!(section_size = br.total_bits_available());
        if self.header.encoding == Encoding::VarDCT {
            let lf_global = self.lf_global.as_mut().unwrap();
            let dequant_matrices = DequantMatrices::decode(&self.header, lf_global, br)?;
            let block_context_map = lf_global.block_context_map.as_mut().unwrap();
            let num_histo_bits = self.header.num_groups().ceil_log2();
            let num_histograms: u32 = br.read(num_histo_bits)? as u32 + 1;
            info!(
                "Processing HFGlobal section with {} passes and {} histograms",
                self.header.passes.num_passes, num_histograms
            );
            let mut passes: Vec<PassState> = vec![];
            #[allow(unused_variables)]
            for i in 0..self.header.passes.num_passes as usize {
                let used_orders = match br.read(2)? {
                    0 => 0x5f,
                    1 => 0x13,
                    2 => 0,
                    _ => br.read(coeff_order::NUM_ORDERS)?,
                } as u32;
                debug!(used_orders);
                let coeff_orders = decode_coeff_orders(used_orders, br)?;
                assert_eq!(coeff_orders.len(), 3 * coeff_order::NUM_ORDERS);
                let num_contexts = num_histograms as usize * block_context_map.num_ac_contexts();
                info!(
                    "Decoding histograms for pass {} with {} contexts",
                    i, num_contexts
                );
                let mut histograms = Histograms::decode(num_contexts, br, true)?;
                // Pad the context map to avoid index out of bounds in decode_vardct_group (group.rs#L514@752e6a4).
                let padding = ZERO_DENSITY_CONTEXT_LIMIT - ZERO_DENSITY_CONTEXT_COUNT;
                histograms.resize(num_contexts + padding);
                debug!("Found {} histograms", histograms.num_histograms());
                passes.push(PassState {
                    coeff_orders,
                    histograms,
                });
            }
            // Since the render pipeline keeps finalized channels, we don't need to store
            // HF coefficients if there is a single pass.
            let hf_coefficients = if passes.len() <= 1 {
                vec![]
            } else {
                (0..self.header.num_groups())
                    .map(|_| {
                        let sz = GROUP_DIM * GROUP_DIM * 3;
                        let mut v = Vec::new_with_capacity(sz)?;
                        v.resize(sz, 0);
                        Ok(Mutex::new(v))
                    })
                    .collect::<Result<_>>()?
            };

            self.hf_global = Some(HfGlobalState {
                num_histograms,
                passes,
                dequant_matrices,
                hf_coefficients,
            });
        }
        // Set EPF sigma values to the correct values if we are doing EPF.
        if self.header.restoration_filter.epf_iters > 0 {
            *self.epf_sigma.try_write().unwrap() = SigmaSource::new(
                &self.header,
                self.lf_global.as_ref().unwrap(),
                &self.hf_meta,
            )?;
        }
        Ok(())
    }

    pub fn render_noise_for_group(
        &self,
        group: usize,
        complete: bool,
        buffer_splitter: &BufferSplitter,
    ) -> Result<()> {
        // TODO(sboukortt): consider making this a dedicated stage
        // TODO(veluca): SIMD.
        let num_channels = self.header.num_extra_channels as usize + 3;

        let group_dim = self.header.group_dim() as u32;
        let xsize_groups = self.header.size_groups().0;
        let gx = (group % xsize_groups) as u32;
        let gy = (group / xsize_groups) as u32;
        let upsampling = self.header.upsampling;
        let upsampled_size = self.header.size_upsampled();

        // Total buffer covers the upsampled region for this group
        let buf_x1 = ((gx + 1) * upsampling * group_dim) as usize;
        let buf_y1 = ((gy + 1) * upsampling * group_dim) as usize;
        let buf_xsize = buf_x1.min(upsampled_size.0) - (gx * upsampling * group_dim) as usize;
        let buf_ysize = buf_y1.min(upsampled_size.1) - (gy * upsampling * group_dim) as usize;

        let bits_to_float = |bits: u32| f32::from_bits((bits >> 9) | 0x3F800000);

        // Get all 3 noise channel buffers upfront
        let mut bufs = [
            pipeline!(self, p, p.get_buffer(num_channels)?),
            pipeline!(self, p, p.get_buffer(num_channels + 1)?),
            pipeline!(self, p, p.get_buffer(num_channels + 2)?),
        ];

        const FLOATS_PER_BATCH: usize =
            Xorshift128Plus::N * std::mem::size_of::<u64>() / std::mem::size_of::<f32>();
        let mut batch = [0u64; Xorshift128Plus::N];

        // libjxl iterates through upsampling subdivisions with separate RNG seeds.
        // For each subregion, a single RNG is shared across all 3 channels.
        for iy in 0..upsampling {
            for ix in 0..upsampling {
                // Seed coordinates for this subregion (matches libjxl)
                let x0 = (gx * upsampling + ix) * group_dim;
                let y0 = (gy * upsampling + iy) * group_dim;

                // Create RNG with this subregion's seed - shared across all 3 channels
                let mut rng = Xorshift128Plus::new_with_seeds(
                    self.decoder_state.visible_frame_index as u32,
                    self.decoder_state.nonvisible_frame_index as u32,
                    x0,
                    y0,
                );

                // Subregion boundaries within the buffer
                let sub_x0 = (ix * group_dim) as usize;
                let sub_y0 = (iy * group_dim) as usize;
                let sub_x1 = ((ix + 1) * group_dim) as usize;
                let sub_y1 = ((iy + 1) * group_dim) as usize;

                // Clamp to actual buffer size
                let sub_xsize = sub_x1.min(buf_xsize).saturating_sub(sub_x0);
                let sub_ysize = sub_y1.min(buf_ysize).saturating_sub(sub_y0);

                // Skip if this subregion is entirely outside the buffer
                if sub_xsize == 0 || sub_ysize == 0 {
                    continue;
                }

                // Fill all 3 channels with this subregion's noise, sharing the RNG
                for buf in &mut bufs {
                    for y in 0..sub_ysize {
                        let row = buf.row_mut(sub_y0 + y);
                        for batch_index in 0..sub_xsize.div_ceil(FLOATS_PER_BATCH) {
                            rng.fill(&mut batch);
                            let batch_size =
                                (sub_xsize - batch_index * FLOATS_PER_BATCH).min(FLOATS_PER_BATCH);
                            for i in 0..batch_size {
                                let x = sub_x0 + FLOATS_PER_BATCH * batch_index + i;
                                let k = i / 2;
                                let high_bytes = i % 2 != 0;
                                let bits = if high_bytes {
                                    ((batch[k] & 0xFFFFFFFF00000000) >> 32) as u32
                                } else {
                                    (batch[k] & 0xFFFFFFFF) as u32
                                };
                                row[x] = bits_to_float(bits);
                            }
                        }
                    }
                }
            }
        }

        // Set all buffers after filling
        let [buf0, buf1, buf2] = bufs;
        pipeline!(
            self,
            p,
            p.set_buffer_for_group(num_channels, group, complete, buf0, buffer_splitter)?
        );
        pipeline!(
            self,
            p,
            p.set_buffer_for_group(num_channels + 1, group, complete, buf1, buffer_splitter)?
        );
        pipeline!(
            self,
            p,
            p.set_buffer_for_group(num_channels + 2, group, complete, buf2, buffer_splitter)?
        );
        Ok(())
    }

    #[instrument(level = "debug", skip(self, passes, buffer_splitter))]
    pub fn decode_and_render_varct_and_noise(
        &self,
        group: usize,
        passes: &mut [(usize, BitReader)],
        buffer_splitter: &BufferSplitter,
        force_render: bool,
    ) -> Result<()> {
        // Group was fully rendered already, nothing to do.
        if self.group_status.final_vardct_render_done.contains(&group) {
            return Ok(());
        }

        let complete = self.group_status.colour_complete(group);

        // Render VarDCT if we are decoding the last pass, or if we are requesting an eager
        // render.
        let render_vardct = complete || force_render;

        if render_vardct && !complete {
            assert!(self.allow_rendering_before_last_pass());
        }

        if !render_vardct && passes.is_empty() {
            return Ok(());
        }

        if self.header.has_noise() && render_vardct {
            self.render_noise_for_group(group, complete, buffer_splitter)?;
        }

        if self.header.encoding != Encoding::VarDCT {
            return Ok(());
        }

        let lf_global = self.lf_global.as_ref().unwrap();
        let mut pixels = if render_vardct {
            Some([
                pipeline!(self, p, p.get_buffer(0))?,
                pipeline!(self, p, p.get_buffer(1))?,
                pipeline!(self, p, p.get_buffer(2))?,
            ])
        } else {
            None
        };
        let hf_meta = self.hf_meta.as_ref().unwrap();

        let lf_image: &[Image<f32>; 3] = if self.header.has_lf_frame() {
            self.decoder_state.lf_frames[self.header.lf_level as usize]
                .as_ref()
                .unwrap()
        } else {
            self.lf_image.as_ref().unwrap()
        };
        if self.group_status.channel_status[group][0] == DataStatus::Zero && render_vardct {
            info!("Upsampling LF for group {group}");
            upsample_lf_group(
                group,
                pixels.as_mut().unwrap(),
                lf_image,
                &self.header,
                &self.decoder_state.file_header.transform_data,
            )?;
        } else {
            info!("Decoding VarDCT group {group}");
            let hf_global = self.hf_global.as_ref().unwrap();
            let mut buffers = self.vardct_buffers.get();
            buffers.ensure_allocated()?;
            decode_vardct_group(
                group,
                passes,
                &self.header,
                lf_global,
                hf_global,
                hf_meta,
                lf_image,
                &self
                    .decoder_state
                    .file_header
                    .transform_data
                    .opsin_inverse_matrix
                    .quant_biases,
                &mut pixels,
                &mut buffers,
            )?;
        }
        if let Some(pixels) = pixels {
            for (c, img) in pixels.into_iter().enumerate() {
                pipeline!(
                    self,
                    p,
                    p.set_buffer_for_group(c, group, complete, img, buffer_splitter)?
                );
            }
        }

        Ok(())
    }

    #[instrument(level = "debug", skip(self, passes, buffer_splitter))]
    pub fn decode_hf_group(
        &self,
        group: usize,
        passes: &mut [(usize, BitReader)],
        buffer_splitter: &BufferSplitter,
        force_render: bool,
    ) -> Result<()> {
        if passes.is_empty() {
            assert!(force_render);
        }

        self.decode_and_render_varct_and_noise(group, passes, buffer_splitter, force_render)?;

        let pass_to_pipeline = |chan, group, complete, image: Image<i32>| {
            pipeline!(
                self,
                p,
                p.set_buffer_for_group(chan, group, complete, image, &*buffer_splitter)?
            );
            Ok(())
        };

        let lf_global = self.lf_global.as_ref().unwrap();
        for (pass, br) in passes.iter_mut() {
            lf_global.modular_global.read_stream(
                ModularStreamId::ModularHF { group, pass: *pass },
                &self.header,
                &lf_global.tree,
                br,
                Some(&pass_to_pipeline),
            )?;
        }

        Ok(())
    }
}
