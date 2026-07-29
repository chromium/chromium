// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{collections::HashSet, sync::Arc};

use crate::{
    api::JxlDecoderOptions,
    entropy_coding::decode::Histograms,
    error::Result,
    features::{noise::Noise, patches::PatchesDictionary, spline::Splines},
    headers::{
        FileHeader,
        extra_channels::ExtraChannelInfo,
        frame_header::{Encoding, FrameHeader, FrameType},
        permutation::Permutation,
        toc::Toc,
    },
    image::Image,
    util::tracing_wrappers::*,
};
use adaptive_lf_smoothing::adaptive_lf_smoothing;
use block_context_map::BlockContextMap;
use color_correlation_map::ColorCorrelationParams;
use modular::{FullModularImage, Tree};
use quant_weights::DequantMatrices;
use quantizer::{LfQuantFactors, QuantizerParams};

use crate::features::epf::SigmaSource;
use crate::util::AtomicRefCell;

mod adaptive_lf_smoothing;
mod block_context_map;
mod coeff_order;
pub mod color_correlation_map;
pub mod decode;
mod group;
pub mod lf_preview;
pub mod modular;
mod quant_weights;
pub mod quantizer;
pub mod render;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Section {
    LfGlobal,
    Lf { group: usize },
    HfGlobal,
    Hf { group: usize, pass: usize },
}

#[derive(Debug)]
pub struct LfGlobalState {
    lf_quant: LfQuantFactors,
    pub quant_params: Option<QuantizerParams>,
    block_context_map: Option<BlockContextMap>,
    color_correlation_params: Option<ColorCorrelationParams>,
    tree: Option<Tree>,
    modular_global: FullModularImage,
    total_bits_read: usize,
}

pub struct PassState {
    coeff_orders: Vec<Permutation>,
    histograms: Histograms,
}

pub struct HfGlobalState {
    num_histograms: u32,
    passes: Vec<PassState>,
    dequant_matrices: DequantMatrices,
    hf_coefficients: Option<(Image<i32>, Image<i32>, Image<i32>)>,
}

#[derive(Debug)]
pub struct ReferenceFrame {
    pub frame: Vec<Image<f32>>,
    pub saved_before_color_transform: bool,
}

impl ReferenceFrame {
    #[cfg(test)]
    pub fn blank(
        width: usize,
        height: usize,
        num_channels: usize,
        saved_before_color_transform: bool,
    ) -> Result<Self> {
        let frame = (0..num_channels)
            .map(|_| Image::new((width, height)))
            .collect::<Result<_>>()?;
        Ok(Self {
            frame,
            saved_before_color_transform,
        })
    }
    #[cfg(test)]
    pub fn random<R: rand::Rng>(
        mut rng: &mut R,
        width: usize,
        height: usize,
        num_channels: usize,
        saved_before_color_transform: bool,
    ) -> Result<Self> {
        let frame = (0..num_channels)
            .map(|_| Image::new_random((width, height), &mut rng))
            .collect::<Result<_>>()?;
        Ok(Self {
            frame,
            saved_before_color_transform,
        })
    }
}

#[derive(Debug)]
pub struct DecoderState {
    pub(super) file_header: FileHeader,
    pub(super) reference_frames: Arc<[Option<ReferenceFrame>; Self::MAX_STORED_FRAMES]>,
    pub(super) lf_frames: [Option<[Image<f32>; 3]>; Self::NUM_LF_FRAMES],
    pub render_spotcolors: bool,
    #[cfg(test)]
    pub use_simple_pipeline: bool,
    pub visible_frame_index: usize,
    pub nonvisible_frame_index: usize,
    pub high_precision: bool,
    pub premultiply_output: bool,
    // Whether the latest level 1 LF frame was fully rendered.
    // If this is set to `true`, early flushing in the main frame
    // (before HF is available) will do nothing.
    pub lf_frame_was_rendered: bool,
}

impl DecoderState {
    pub const MAX_STORED_FRAMES: usize = 4;
    pub const NUM_LF_FRAMES: usize = 4;

    pub fn new(file_header: FileHeader, options: &JxlDecoderOptions) -> Self {
        Self {
            file_header,
            reference_frames: Arc::new([None, None, None, None]),
            lf_frames: std::array::from_fn(|_| None),
            render_spotcolors: options.render_spot_colors,
            #[cfg(test)]
            use_simple_pipeline: false,
            visible_frame_index: 0,
            nonvisible_frame_index: 0,
            high_precision: options.high_precision,
            premultiply_output: options.premultiply_output,
            lf_frame_was_rendered: false,
        }
    }

    pub fn extra_channel_info(&self) -> &Vec<ExtraChannelInfo> {
        &self.file_header.image_metadata.extra_channel_info
    }

    pub fn reference_frame(&self, i: usize) -> Option<&ReferenceFrame> {
        assert!(i < Self::MAX_STORED_FRAMES);
        self.reference_frames[i].as_ref()
    }

    #[cfg(test)]
    pub fn set_use_simple_pipeline(&mut self, u: bool) {
        self.use_simple_pipeline = u;
    }
}

pub struct HfMetadata {
    ytox_map: Image<i8>,
    ytob_map: Image<i8>,
    pub raw_quant_map: Image<i32>,
    pub transform_map: Image<u8>,
    pub epf_map: Image<u8>,
    used_hf_types: u32,
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum DataStatus {
    Zero,
    Partial,
    Final,
}

// TODO(veluca): consider merging the modular rendering infra
// with VarDCT rendering infra. That would likely remove the
// need for this custom tracking.
#[derive(Debug)]
struct GroupStatus {
    // Groups that should be rendered on the next call to flush().
    need_vardct_flush: HashSet<usize>,
    need_modular_flush: HashSet<usize>,
    channel_status: Vec<Vec<DataStatus>>,
    final_vardct_render_done: HashSet<usize>,
    incomplete_groups: usize,
}

impl GroupStatus {
    fn new(frame_header: &FrameHeader) -> Self {
        let count = frame_header.num_groups();
        let ecs = frame_header.num_extra_channels as usize;
        // We don't track noise channels because we pretend they always
        // have the same status as VarDCT channels.
        GroupStatus {
            need_vardct_flush: HashSet::new(),
            need_modular_flush: HashSet::new(),
            channel_status: vec![vec![DataStatus::Zero; 3 + ecs]; count],
            final_vardct_render_done: HashSet::new(),
            incomplete_groups: count,
        }
    }

    fn update_status(&mut self, group: usize, channel: usize, status: DataStatus) {
        let ss = &mut self.channel_status[group];
        let all_complete = ss.iter().all(|x| *x == DataStatus::Final);
        ss[channel] = status;
        if !all_complete && ss.iter().all(|x| *x == DataStatus::Final) {
            self.incomplete_groups = self.incomplete_groups.checked_sub(1).unwrap();
        }
    }

    fn colour_complete(&self, group: usize) -> bool {
        self.channel_status[group][..3]
            .iter()
            .all(|x| *x == DataStatus::Final)
    }
}

pub struct Frame {
    header: FrameHeader,
    toc: Toc,
    color_channels: usize,
    lf_global: Option<LfGlobalState>,
    hf_global: Option<HfGlobalState>,
    lf_image: Option<[Image<f32>; 3]>,
    quant_lf: Image<u8>,
    hf_meta: Option<HfMetadata>,
    decoder_state: DecoderState,
    #[cfg(test)]
    use_simple_pipeline: bool,
    #[cfg(test)]
    render_pipeline: Option<Box<dyn std::any::Any>>,
    #[cfg(not(test))]
    render_pipeline: Option<Box<crate::render::LowMemoryRenderPipeline>>,
    reference_frame_data: Option<Vec<Image<f32>>>,
    lf_frame_data: Option<[Image<f32>; 3]>,
    section0_render_up_to_date: bool,
    /// Reusable buffers for VarDCT group decoding.
    vardct_buffers: Option<group::VarDctBuffers>,
    group_status: GroupStatus,
    patches: Arc<AtomicRefCell<PatchesDictionary>>,
    splines: Arc<AtomicRefCell<Splines>>,
    noise: Arc<AtomicRefCell<Noise>>,
    lf_quant: Arc<AtomicRefCell<LfQuantFactors>>,
    color_correlation_params: Arc<AtomicRefCell<ColorCorrelationParams>>,
    epf_sigma: Arc<AtomicRefCell<SigmaSource>>,
    // LF groups that received data and thus should trigger a modular
    // re-render of the corresponding groups.
    dirty_lf_groups: HashSet<usize>,
}

impl Frame {
    pub fn toc(&self) -> &Toc {
        &self.toc
    }

    pub fn header(&self) -> &FrameHeader {
        &self.header
    }

    pub fn total_bytes_in_toc(&self) -> usize {
        self.toc.entries.iter().map(|x| *x as usize).sum()
    }

    #[instrument(level = "debug", skip(self), ret)]
    pub fn get_section_idx(&self, section: Section) -> usize {
        if self.header.num_toc_entries() == 1 {
            0
        } else {
            match section {
                Section::LfGlobal => 0,
                Section::Lf { group } => 1 + group,
                Section::HfGlobal => self.header.num_lf_groups() + 1,
                Section::Hf { group, pass } => {
                    2 + self.header.num_lf_groups() + self.header.num_groups() * pass + group
                }
            }
        }
    }

    pub fn can_do_early_rendering(&self) -> bool {
        if matches!(
            self.header.frame_type,
            FrameType::ReferenceOnly | FrameType::SkipProgressive
        ) {
            return false;
        }
        if self.header.has_lf_frame() {
            return true;
        }
        if self.header.encoding == Encoding::VarDCT {
            return false;
        }
        self.lf_global
            .as_ref()
            .map(|x| x.modular_global.can_do_early_partial_render())
            .unwrap_or_default()
    }

    pub fn finalize_lf(&mut self) -> Result<()> {
        if self.header.should_do_adaptive_lf_smoothing() {
            let lf_global = self.lf_global.as_mut().unwrap();
            let lf_quant = &lf_global.lf_quant;
            let inv_quant_lf = lf_global.quant_params.as_mut().unwrap().inv_quant_lf();
            adaptive_lf_smoothing(
                [
                    inv_quant_lf * lf_quant.quant_factors[0],
                    inv_quant_lf * lf_quant.quant_factors[1],
                    inv_quant_lf * lf_quant.quant_factors[2],
                ],
                self.lf_image.as_mut().unwrap(),
            )
        } else {
            Ok(())
        }
    }

    pub fn finalize(mut self) -> Result<Option<DecoderState>> {
        // First, drop the render pipeline to ensure that no other references to the reference
        // frames are around.
        self.render_pipeline = None;
        // Save reference frame if this frame can be referenced and was actually decoded.
        // If reference_frame_data is None (frame was skipped), we don't save it.
        // Subsequent frames referencing this slot may fail.
        if self.header.can_be_referenced
            && let Some(frame_data) = self.reference_frame_data
        {
            info!("Saving frame in slot {}", self.header.save_as_reference);
            let rf = Arc::get_mut(&mut self.decoder_state.reference_frames)
                .expect("remaining references to reference_frames");
            rf[self.header.save_as_reference as usize] = Some(ReferenceFrame {
                frame: frame_data,
                saved_before_color_transform: self.header.save_before_ct,
            });
        }

        if self.header.lf_level != 0 {
            self.decoder_state.lf_frames[(self.header.lf_level - 1) as usize] = self.lf_frame_data;
        }
        let decoder_state = if self.header.is_last {
            None
        } else {
            Some(self.decoder_state)
        };
        Ok(decoder_state)
    }

    fn modular_color_channels(&self) -> usize {
        if self.header.encoding == Encoding::VarDCT {
            0
        } else {
            self.color_channels
        }
    }
}
