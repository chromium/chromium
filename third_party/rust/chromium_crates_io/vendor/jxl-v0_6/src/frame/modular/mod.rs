// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::util::sync::{
    Mutex,
    atomic::{AtomicBool, Ordering},
};
use std::{
    cmp::min,
    collections::{BTreeSet, HashSet},
    fmt::Debug,
};

use crate::{
    bit_reader::BitReader,
    error::{Error, Result},
    frame::{
        ColorCorrelationParams, DataStatus, HfMetaViews,
        block_context_map::BlockContextMap,
        modular::{
            buffers::{ModularBuffer, ModularChannel},
            transforms::step::{TransformDependency, TransformStepChunk},
        },
        quantizer::{self, LfQuantFactors, QuantizerParams},
    },
    headers::{
        ImageMetadata, JxlHeader,
        bit_depth::BitDepth,
        frame_header::FrameHeader,
        modular::{GroupHeader, TransformId},
    },
    image::{Image, Rect},
    render::buffer_splitter::OutputChannelRef,
    util::{CeilLog2, PerThreadStorage, tracing_wrappers::*},
};
use jxl_transforms::transform_map::*;

mod buffers;
mod decode;
mod flat_tree;
mod predict;
mod transforms;
mod tree;

use buffers::with_buffers;
pub use decode::ModularStreamId;
use decode::decode_modular_subbitstream;
pub use predict::Predictor;
pub use tree::Tree;

// Two rows on top, two pixels to the left, two pixels to the right.
const IMAGE_PADDING: (usize, usize) = (4, 2);
const IMAGE_OFFSET: (usize, usize) = (2, 2);

#[derive(Clone, PartialEq, Eq, Copy)]
struct ChannelInfo {
    // The index of the output channel in the render pipeline.
    output_channel_idx: Option<usize>,
    // width, height
    size: (usize, usize),
    shift: Option<(usize, usize)>, // None for meta-channels
    bit_depth: BitDepth,
}

impl Debug for ChannelInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}x{}", self.size.0, self.size.1)?;
        if let Some(shift) = self.shift {
            write!(f, "(shift {},{})", shift.0, shift.1)?;
        } else {
            write!(f, "(meta)")?;
        }
        write!(f, "{:?}", self.bit_depth)?;
        if let Some(oc) = self.output_channel_idx {
            write!(f, "(output channel {})", oc)?;
        }
        Ok(())
    }
}

impl ChannelInfo {
    fn is_meta(&self) -> bool {
        self.shift.is_none()
    }

    fn is_meta_or_small(&self, group_dim: usize) -> bool {
        self.is_meta() || (self.size.0 <= group_dim && self.size.1 <= group_dim)
    }

    fn is_shift_in_range(&self, min: usize, max: usize) -> bool {
        // This might be called with max < min, in which case we just return false.
        // This matches libjxl behaviour.
        self.shift.is_some_and(|(a, b)| {
            let shift = a.min(b);
            min <= shift && shift <= max
        })
    }

    fn is_equivalent(&self, other: &ChannelInfo) -> bool {
        self.size == other.size && self.shift == other.shift && self.bit_depth == other.bit_depth
    }
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy)]
enum ModularGridKind {
    // Single big channel.
    None,
    // 2048x2048 image-pixels (if modular_group_shift == 1).
    Lf,
    // 256x256 image-pixels (if modular_group_shift == 1).
    Hf,
}

impl ModularGridKind {
    fn grid_dim(&self, frame_header: &FrameHeader, shift: (usize, usize)) -> (usize, usize) {
        let group_dim = match self {
            ModularGridKind::None => 0,
            ModularGridKind::Lf => frame_header.lf_group_dim(),
            ModularGridKind::Hf => frame_header.group_dim(),
        };
        (group_dim >> shift.0, group_dim >> shift.1)
    }
    fn grid_shape(&self, frame_header: &FrameHeader) -> (usize, usize) {
        match self {
            ModularGridKind::None => (1, 1),
            ModularGridKind::Lf => frame_header.size_lf_groups(),
            ModularGridKind::Hf => frame_header.size_groups(),
        }
    }
}

#[derive(Debug)]
struct ModularBufferInfo {
    info: ChannelInfo,
    // The index of coded channel in the bit-stream, or -1 for non-coded channels.
    coded_channel_id: isize,
    #[cfg_attr(not(feature = "tracing"), allow(dead_code))]
    description: String,
    grid_kind: ModularGridKind,
    grid_shape: (usize, usize),
    buffer_grid: Vec<ModularBuffer>,
}

impl ModularBufferInfo {
    fn get_grid_idx(
        &self,
        output_grid_kind: ModularGridKind,
        output_grid_pos: (usize, usize),
    ) -> usize {
        let grid_pos = match (output_grid_kind, self.grid_kind) {
            (_, ModularGridKind::None) => (0, 0),
            (ModularGridKind::Lf, ModularGridKind::Lf)
            | (ModularGridKind::Hf, ModularGridKind::Hf) => output_grid_pos,
            (ModularGridKind::Hf, ModularGridKind::Lf) => {
                (output_grid_pos.0 / 8, output_grid_pos.1 / 8)
            }
            _ => unreachable!("invalid combination of output grid kind and buffer grid kind"),
        };
        self.grid_shape.0 * grid_pos.1 + grid_pos.0
    }

    fn get_grid_rect(
        &self,
        frame_header: &FrameHeader,
        output_grid_kind: ModularGridKind,
        output_grid_pos: (usize, usize),
    ) -> Rect {
        let chan_size = self.info.size;
        if output_grid_kind == ModularGridKind::None {
            assert_eq!(self.grid_kind, output_grid_kind);
            return Rect {
                origin: (0, 0),
                size: chan_size,
            };
        }
        let shift = self.info.shift.unwrap();
        let grid_dim = output_grid_kind.grid_dim(frame_header, shift);
        let bx = output_grid_pos.0 * grid_dim.0;
        let by = output_grid_pos.1 * grid_dim.1;
        let size = (
            (chan_size.0 - bx).min(grid_dim.0),
            (chan_size.1 - by).min(grid_dim.1),
        );
        let origin = match (output_grid_kind, self.grid_kind) {
            (ModularGridKind::Lf, ModularGridKind::Lf)
            | (ModularGridKind::Hf, ModularGridKind::Hf) => (0, 0),
            (_, ModularGridKind::None) => (bx, by),
            (ModularGridKind::Hf, ModularGridKind::Lf) => {
                let lf_grid_dim = self.grid_kind.grid_dim(frame_header, shift);
                (bx % lf_grid_dim.0, by % lf_grid_dim.1)
            }
            _ => unreachable!("invalid combination of output grid kind and buffer grid kind"),
        };
        if size.0 == 0 || size.1 == 0 {
            Rect {
                origin: (0, 0),
                size: (0, 0),
            }
        } else {
            Rect { origin, size }
        }
    }
}

struct TransformScratchSpace {
    smooth_unsqueeze_buffer: ([Vec<f32>; 5], Vec<i32>),
}

impl Debug for TransformScratchSpace {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "TransformScratchSpace")
    }
}

impl TransformScratchSpace {
    fn new() -> TransformScratchSpace {
        TransformScratchSpace {
            smooth_unsqueeze_buffer: (std::array::from_fn(|_| vec![]), vec![]),
        }
    }
}

/// A modular image is a sequence of channels to which one or more transforms might have been
/// applied. We represent a modular image as a list of buffers, some of which are coded in the
/// bitstream; other buffers are obtained as the output of one of the transformation steps.
/// Some buffers are marked as `output`: those are the buffers corresponding to the pre-transform
/// image channels.
/// The buffers are internally divided in grids, matching the sizes of the groups they are coded
/// in (with appropriate shifts), or the size of the data produced by applying the appropriate
/// transforms to each of the groups in the input of the transforms.
#[derive(Debug)]
pub struct FullModularImage {
    transform_scratch_space: PerThreadStorage<TransformScratchSpace>,
    buffer_info: Vec<ModularBufferInfo>,
    transform_steps: Vec<TransformStepChunk>,
    // List of buffer indices of the channels of the modular image encoded in each kind of section.
    // In order, LfGlobal, LfGroup, HfGroup(pass 0), ..., HfGroup(last pass).
    section_buffer_indices: Vec<Vec<usize>>,
    can_do_partial_render: bool,
    can_do_early_partial_render: bool,
    needed_section0_channels_for_early_render: usize,
    has_decoded_data: AtomicBool,
    global_header: Option<GroupHeader>,
    output_transforms_for_group: Vec<Vec<usize>>,
    pending_transforms: BTreeSet<usize>,
    rerendered_buffers: HashSet<(usize, usize)>,
    delayed_ready_sections: Mutex<BTreeSet<(usize, usize)>>,
    // Whether each channel is used or not by the render pipeline.
    pipeline_used_channels: Vec<bool>,
    // Stack of transform steps that are ready to process.
    ready_transform_steps: Mutex<Vec<usize>>,
}

impl FullModularImage {
    pub fn can_do_partial_render(&self) -> bool {
        self.can_do_partial_render
    }

    pub fn can_do_early_partial_render(&self) -> bool {
        self.can_do_early_partial_render
    }

    pub fn set_pipeline_used_channels(&mut self, used: &[bool]) {
        self.pipeline_used_channels = used.to_vec();
    }

    #[instrument(level = "debug", skip_all)]
    pub fn read(
        frame_header: &FrameHeader,
        image_metadata: &ImageMetadata,
        modular_color_channels: usize,
        br: &mut BitReader,
    ) -> Result<Self> {
        let mut channels = vec![];
        for c in 0..modular_color_channels {
            let shift = (frame_header.hshift(c), frame_header.vshift(c));
            let size = frame_header.size();
            channels.push(ChannelInfo {
                output_channel_idx: Some(c),
                size: (size.0.div_ceil(1 << shift.0), size.1.div_ceil(1 << shift.1)),
                shift: Some(shift),
                bit_depth: image_metadata.bit_depth,
            });
        }

        for (idx, ecups) in frame_header.ec_upsampling.iter().enumerate() {
            let shift_ec = ecups.ceil_log2();
            let shift_color = frame_header.upsampling.ceil_log2();
            let shift = shift_ec
                .checked_sub(shift_color)
                .expect("ec_upsampling >= upsampling should be checked in frame header")
                as usize;
            let size = frame_header.size_upsampled();
            let size = (
                size.0.div_ceil(*ecups as usize),
                size.1.div_ceil(*ecups as usize),
            );
            channels.push(ChannelInfo {
                output_channel_idx: Some(3 + idx),
                size,
                shift: Some((shift, shift)),
                bit_depth: image_metadata.bit_depth,
            });
        }

        let num_channels = channels.len();

        #[cfg(feature = "tracing")]
        for (i, ch) in channels.iter().enumerate() {
            trace!("Modular channel {i}: {ch:?}");
        }

        if channels.is_empty() {
            return Ok(Self {
                transform_scratch_space: PerThreadStorage::new(TransformScratchSpace::new),
                buffer_info: vec![],
                transform_steps: vec![],
                section_buffer_indices: vec![vec![]; 2 + frame_header.passes.num_passes as usize],
                can_do_partial_render: true,
                can_do_early_partial_render: false,
                needed_section0_channels_for_early_render: 0,
                has_decoded_data: AtomicBool::new(false),
                global_header: None,
                pipeline_used_channels: vec![],
                output_transforms_for_group: vec![vec![]; frame_header.num_groups()],
                ready_transform_steps: Mutex::new(vec![]),
                pending_transforms: BTreeSet::new(),
                rerendered_buffers: HashSet::new(),
                delayed_ready_sections: Mutex::new(BTreeSet::new()),
            });
        }

        trace!("reading modular header");
        let header = GroupHeader::read(br)?;

        // Disallow progressive rendering with multi-channel palette transforms
        // or delta-palette.
        let has_problematic_palette_transform = header.transforms.iter().any(|x| {
            x.id == TransformId::Palette
                && (x.num_channels > 1 || x.predictor_id != Predictor::Zero as u32)
        });

        let has_squeeze_transform = header
            .transforms
            .iter()
            .any(|x| x.id == TransformId::Squeeze);

        let (mut buffer_info, transform_steps) =
            transforms::meta_apply::meta_apply_transforms(&channels, &header)?;

        // Assign each (channel, group) pair present in the bitstream to the section in which it
        // will be decoded.
        let mut section_buffer_indices: Vec<Vec<usize>> = vec![];

        let mut sorted_buffers: Vec<_> = buffer_info
            .iter()
            .enumerate()
            .filter_map(|(i, b)| {
                if b.coded_channel_id >= 0 {
                    Some((b.coded_channel_id, i))
                } else {
                    None
                }
            })
            .collect();

        sorted_buffers.sort_by_key(|x| x.0);

        section_buffer_indices.push(
            sorted_buffers
                .iter()
                .take_while(|x| {
                    buffer_info[x.1]
                        .info
                        .is_meta_or_small(frame_header.group_dim())
                })
                .map(|x| x.1)
                .collect(),
        );

        section_buffer_indices.push(
            sorted_buffers
                .iter()
                .skip_while(|x| {
                    buffer_info[x.1]
                        .info
                        .is_meta_or_small(frame_header.group_dim())
                })
                .filter(|x| buffer_info[x.1].info.is_shift_in_range(3, usize::MAX))
                .map(|x| x.1)
                .collect(),
        );

        for pass in 0..frame_header.passes.num_passes as usize {
            let (min_shift, max_shift) = frame_header.passes.downsampling_bracket(pass);
            section_buffer_indices.push(
                sorted_buffers
                    .iter()
                    .skip_while(|x| {
                        buffer_info[x.1]
                            .info
                            .is_meta_or_small(frame_header.group_dim())
                    })
                    .filter(|x| {
                        buffer_info[x.1]
                            .info
                            .is_shift_in_range(min_shift, max_shift)
                    })
                    .map(|x| x.1)
                    .collect(),
            );
        }

        // Ensure that the channel list in each group is sorted by actual channel ID.
        for list in section_buffer_indices.iter_mut() {
            list.sort_by_key(|x| buffer_info[*x].coded_channel_id);
        }

        trace!(?section_buffer_indices);
        #[cfg(feature = "tracing")]
        for (section, indices) in section_buffer_indices.iter().enumerate() {
            let section_name = match section {
                0 => "LF global".to_string(),
                1 => "LF groups".to_string(),
                _ => format!("HF groups, pass {}", section - 2),
            };
            trace!("Coded modular channels in {section_name}");
            for i in indices {
                let bi = &buffer_info[*i];
                trace!(
                    "Channel {i} {:?} coded id: {}",
                    bi.info, bi.coded_channel_id
                );
            }
        }

        let transform_steps = transforms::meta_apply::make_grids(
            frame_header,
            transform_steps,
            &section_buffer_indices,
            &mut buffer_info,
            modular_color_channels,
        );

        #[cfg(feature = "tracing")]
        for (i, bi) in buffer_info.iter().enumerate() {
            trace!(
                "Channel {i} {:?} coded_id: {} '{}' {:?} grid {:?}",
                bi.info, bi.coded_channel_id, bi.description, bi.grid_kind, bi.grid_shape
            );
            for (pos, buf) in bi.buffer_grid.iter().enumerate() {
                trace!(
                    "Channel {i} grid {pos} ({}, {})  size: {:?}, uses: {:?}, used_by: c {:?} f {:?}",
                    pos % bi.grid_shape.0,
                    pos / bi.grid_shape.0,
                    buf.size,
                    buf.remaining_uses,
                    buf.used_by_transforms_current,
                    buf.used_by_transforms_final,
                );
            }
        }

        #[cfg(feature = "tracing")]
        for (i, ts) in transform_steps.iter().enumerate() {
            trace!("Transform {i}: {ts:?}");
        }

        let mut output_transforms_for_group = vec![vec![]; frame_header.num_groups()];

        for (i, t) in transform_steps.iter().enumerate() {
            if let Some((g, _)) = t.output_info() {
                output_transforms_for_group[g].push(i);
            }
        }

        let num_meta_channels = buffer_info
            .iter()
            .filter(|b| b.coded_channel_id >= 0 && b.info.is_meta())
            .count();

        Ok(FullModularImage {
            transform_scratch_space: PerThreadStorage::new(TransformScratchSpace::new),
            buffer_info,
            transform_steps,
            section_buffer_indices,
            can_do_partial_render: !has_problematic_palette_transform,
            can_do_early_partial_render: !has_problematic_palette_transform
                && has_squeeze_transform,
            needed_section0_channels_for_early_render: num_channels + num_meta_channels,
            has_decoded_data: AtomicBool::new(false),
            global_header: Some(header),
            output_transforms_for_group,
            pipeline_used_channels: vec![],
            ready_transform_steps: Mutex::new(vec![]),
            pending_transforms: BTreeSet::new(),
            rerendered_buffers: HashSet::new(),
            delayed_ready_sections: Mutex::new(BTreeSet::new()),
        })
    }

    // Returns whether there is new data in this section and thus we should
    // trigger a global re-render.
    pub fn read_section0(
        &mut self,
        frame_header: &FrameHeader,
        global_tree: &Option<Tree>,
        br: &mut BitReader,
        allow_partial: bool,
    ) -> Result<bool> {
        let allow_partial = allow_partial && self.can_do_early_partial_render;
        let mut decoded_if_partial = 0;
        let ret = with_buffers(
            &self.buffer_info,
            &self.section_buffer_indices[0],
            0,
            |bufs| {
                decode_modular_subbitstream(
                    bufs,
                    ModularStreamId::GlobalData.get_id(frame_header),
                    self.global_header.clone(),
                    global_tree,
                    br,
                    Some(&mut decoded_if_partial),
                )
            },
        );

        let total_buffers = self.section_buffer_indices[0].len();

        let num_decoded = match (ret, allow_partial) {
            // Decoded section completely.
            (Ok(_), _) => total_buffers,
            (Err(_), true) => decoded_if_partial,
            (Err(e), false) => {
                return Err(e);
            }
        };

        // Avoid green martians
        self.has_decoded_data.fetch_or(
            num_decoded >= self.needed_section0_channels_for_early_render && num_decoded > 0,
            Ordering::Relaxed,
        );

        if num_decoded >= total_buffers {
            self.mark_final(0, 0);
            self.delayed_ready_sections
                .try_lock()
                .unwrap()
                .insert((0, 0));
            // We don't run transforms here - we ask the caller to call `run_all_transforms`
            // at least once per decode.
            return Ok(true);
        }
        let mut need_rerender = false;
        for b in self.section_buffer_indices[0].iter().take(num_decoded) {
            let buf = &mut self.buffer_info[*b].buffer_grid[0];
            if buf.data_status == DataStatus::Final {
                continue;
            }
            need_rerender |= buf.data_status != DataStatus::Partial;
            // If we did a partial decode, we cannot be 100% sure of whether we correctly
            // decoded all the sections. Thus, mark the sections as partially decoded.
            buf.data_status = DataStatus::Partial;
        }
        Ok(need_rerender)
    }

    #[allow(clippy::type_complexity)]
    #[instrument(
        level = "debug",
        skip(self, frame_header, global_tree, br, pass_to_pipeline),
        ret
    )]
    pub fn read_stream(
        &self,
        stream: ModularStreamId,
        frame_header: &FrameHeader,
        global_tree: &Option<Tree>,
        br: &mut BitReader,
        pass_to_pipeline: Option<&dyn Fn(usize, usize, bool, Image<i32>) -> Result<()>>,
    ) -> Result<()> {
        if self.buffer_info.is_empty() {
            info!("No modular channels to decode");
            return Ok(());
        }
        let (section_id, grid) = match stream {
            ModularStreamId::ModularLF(group) => (1, group),
            ModularStreamId::ModularHF { pass, group } => (2 + pass, group),
            _ => {
                unreachable!(
                    "read_stream should only be used for streams that are part of the main Modular image"
                );
            }
        };

        with_buffers(
            &self.buffer_info,
            &self.section_buffer_indices[section_id],
            grid,
            |bufs| {
                decode_modular_subbitstream(
                    bufs,
                    stream.get_id(frame_header),
                    None,
                    global_tree,
                    br,
                    None,
                )?;
                Ok(())
            },
        )?;

        self.has_decoded_data.fetch_or(
            !self.section_buffer_indices[section_id].is_empty(),
            Ordering::Relaxed,
        );

        let mut ready_steps = vec![];
        if section_id == 1 {
            self.delayed_ready_sections
                .lock()
                .unwrap()
                .insert((1, grid));
        } else {
            self.mark_section_ready(section_id, grid, &mut ready_steps);
        }

        if let Some(pass_to_pipeline) = pass_to_pipeline {
            self.run_transforms(frame_header, pass_to_pipeline, &mut ready_steps)
        } else {
            self.ready_transform_steps
                .lock()
                .unwrap()
                .extend_from_slice(&ready_steps);
            Ok(())
        }
    }

    fn update_deps(&self, buf: usize, grid: usize, ready_steps: &mut Vec<usize>) {
        for t in self.buffer_info[buf].buffer_grid[grid]
            .used_by_transforms_current
            .try_lock()
            .unwrap()
            .drain(..)
        {
            if self.transform_steps[t].current_dep_ready() {
                ready_steps.push(t);
            }
        }
    }

    fn mark_section_ready(&self, section_id: usize, grid: usize, ready_steps: &mut Vec<usize>) {
        for buf in self.section_buffer_indices[section_id].iter().copied() {
            self.update_deps(buf, grid, ready_steps);
        }
    }

    pub fn mark_final(&mut self, section_id: usize, grid: usize) {
        let mut buffer_stack = vec![];
        let mut stack = vec![];
        for b in self.section_buffer_indices[section_id].iter() {
            buffer_stack.push((*b, grid));
        }
        loop {
            if let Some((b, g)) = buffer_stack.pop() {
                let buf = &mut self.buffer_info[b];
                let grid = &mut buf.buffer_grid[g];
                if grid.data_status == DataStatus::Final {
                    continue;
                }
                self.rerendered_buffers.insert((b, g));
                for v in grid.used_by_transforms_final.iter() {
                    stack.push(*v);
                }
                grid.data_status = DataStatus::Final;
                grid.remaining_uses
                    .store(grid.used_by_transforms_final.len(), Ordering::Relaxed);
            }
            if let Some(v) = stack.pop() {
                if !self.transform_steps[v].final_dep_ready() {
                    continue;
                }
                self.pending_transforms.insert(v);
                for &(b, g) in self.transform_steps[v].outputs(&self.buffer_info).iter() {
                    buffer_stack.push((b, g));
                }
            }
            if stack.is_empty() && buffer_stack.is_empty() {
                break;
            }
        }
    }

    // Should only be called after *all* calls to mark_final for this round of rendering are done.
    pub fn request_rerender(&mut self, frame_header: &FrameHeader, group: usize) {
        assert!(self.can_do_partial_render());
        let mut stack = self.output_transforms_for_group[group].clone();

        while let Some(t) = stack.pop() {
            // If a transform is ready to run its final render, we either already enqueued it
            // or we already ran it.
            if self.transform_steps[t].ready_for_final_render() {
                continue;
            }
            // Avoid visiting transforms potentially exponentially many times.
            if !self.pending_transforms.insert(t) {
                continue;
            }
            for TransformDependency {
                buffer,
                grid,
                order_only,
            } in self.transform_steps[t]
                .dependecies(&self.buffer_info, frame_header)
                .iter()
            {
                let buf = &mut self.buffer_info[*buffer].buffer_grid[*grid];
                // Force a re-render only of those buffers that we fully use.
                // TODO(veluca): investigate why we need `buf.has_buffer()` here.
                if *order_only && buf.has_buffer() {
                    continue;
                }
                if let Some(b) = buf.produced_by_step
                    && buf.data_status != DataStatus::Final
                {
                    self.rerendered_buffers.insert((*buffer, *grid));
                    // The data in this buffer is no longer guaranteed to be all-0.
                    // In usual images, this is mostly only relevant in palette images,
                    // but in principle one could apply transforms to Squeeze residuals.
                    buf.data_status = DataStatus::Partial;
                    stack.push(b);
                }
            }
        }
    }

    pub fn prepare_render(
        &mut self,
        frame_header: &FrameHeader,
        mut group_callback: impl FnMut(usize, usize, bool),
    ) {
        for t in self.pending_transforms.iter().cloned() {
            // If this will produce output, tell the caller.
            if let Some((g, c)) = self.transform_steps[t].output_info() {
                group_callback(g, c, self.transform_steps[t].ready_for_final_render());
            }
            let mut has_current_deps = false;
            // Add dependency edges from *all* the buffers that will be modified and that are used.
            for TransformDependency { buffer, grid, .. } in self.transform_steps[t]
                .dependecies(&self.buffer_info, frame_header)
                .iter()
            {
                if self.rerendered_buffers.contains(&(*buffer, *grid)) {
                    let buf = &mut self.buffer_info[*buffer].buffer_grid[*grid];
                    // TODO(veluca): account for *non-final* uses here, when we actually
                    // deallocate temporary buffers.
                    buf.used_by_transforms_current.try_lock().unwrap().push(t);
                    self.transform_steps[t].add_current_dep();
                    has_current_deps = true;
                }
            }
            // Make sure that transforms that need to run, but don't need to wait for
            // actual decoding, are actually run.
            if !has_current_deps {
                self.ready_transform_steps.try_lock().unwrap().push(t);
            }
        }
        self.pending_transforms.clear();
        self.rerendered_buffers.clear();
        for (s, g) in std::mem::take(&mut *self.delayed_ready_sections.try_lock().unwrap()) {
            self.mark_section_ready(s, g, &mut self.ready_transform_steps.try_lock().unwrap());
        }
    }

    fn run_transform(
        &self,
        frame_header: &FrameHeader,
        tfm: usize,
        scratch_space: &mut TransformScratchSpace,
        pass_to_pipeline: &dyn Fn(usize, usize, bool, Image<i32>) -> Result<()>,
        ready_steps: &mut Vec<usize>,
    ) -> Result<()> {
        self.transform_steps[tfm].do_run(
            frame_header,
            &self.buffer_info,
            scratch_space,
            pass_to_pipeline,
        )?;

        for &(buf, grid) in self.transform_steps[tfm].outputs(&self.buffer_info).iter() {
            self.update_deps(buf, grid, ready_steps);
        }
        Ok(())
    }

    pub fn run_transforms(
        &self,
        frame_header: &FrameHeader,
        pass_to_pipeline: &dyn Fn(usize, usize, bool, Image<i32>) -> Result<()>,
        ready_steps: &mut Vec<usize>,
    ) -> Result<()> {
        let mut scratch_space = self.transform_scratch_space.get();
        while let Some(t) = ready_steps.pop() {
            self.run_transform(
                frame_header,
                t,
                &mut scratch_space,
                pass_to_pipeline,
                ready_steps,
            )?;
        }
        Ok(())
    }

    pub fn take_ready_steps(&mut self) -> Vec<usize> {
        std::mem::take(self.ready_transform_steps.get_mut().unwrap())
    }

    pub fn validate_state_after_transforms(&self) {
        for (i, t) in self.transform_steps.iter().enumerate() {
            if !t.no_current_deps() {
                panic!("Transform {i} did not run but was expected to: {t:?}")
            }
        }
        for b in self.buffer_info.iter() {
            for bg in b.buffer_grid.iter() {
                debug_assert!(
                    bg.used_by_transforms_current.try_lock().unwrap().is_empty(),
                    "{b:?} {bg:?}"
                );
            }
        }
    }

    pub fn has_decoded_data(&self) -> bool {
        self.has_decoded_data.load(Ordering::Relaxed)
    }
}

#[allow(clippy::too_many_arguments)]
fn dequant_lf(
    r: Rect,
    lf: &mut [OutputChannelRef],
    quant_lf: &mut OutputChannelRef,
    input: [&Image<i32>; 3],
    color_correlation_params: &ColorCorrelationParams,
    quant_params: &QuantizerParams,
    lf_quant: &LfQuantFactors,
    mul: f32,
    frame_header: &FrameHeader,
    bctx: &BlockContextMap,
) -> Result<()> {
    let inv_quant_lf = (quantizer::GLOBAL_SCALE_DENOM as f32)
        / (quant_params.global_scale as f32 * quant_params.quant_lf as f32);
    let lf_factors = lf_quant.quant_factors.map(|factor| factor * inv_quant_lf);

    if frame_header.is444() {
        let [lf0, lf1, lf2] = lf else {
            unreachable!();
        };
        let fac_x = lf_factors[0] * mul;
        let fac_y = lf_factors[1] * mul;
        let fac_b = lf_factors[2] * mul;
        let cfl_fac_x = color_correlation_params.y_to_x_lf();
        let cfl_fac_b = color_correlation_params.y_to_b_lf();
        for y in 0..r.size.1 {
            let quant_row_x = input[1].row(y);
            let quant_row_y = input[0].row(y);
            let quant_row_b = input[2].row(y);
            let dec_row_x = lf0.typed_row_mut::<f32>(y);
            let dec_row_y = lf1.typed_row_mut::<f32>(y);
            let dec_row_b = lf2.typed_row_mut::<f32>(y);
            for x in 0..r.size.0 {
                let in_x = quant_row_x[x] as f32 * fac_x;
                let in_y = quant_row_y[x] as f32 * fac_y;
                let in_b = quant_row_b[x] as f32 * fac_b;
                dec_row_y[x] = in_y;
                dec_row_x[x] = in_y * cfl_fac_x + in_x;
                dec_row_b[x] = in_y * cfl_fac_b + in_b;
            }
        }
    } else {
        for c in 0..3 {
            let rect_size = (
                r.size.0 >> frame_header.hshift(c),
                r.size.1 >> frame_header.vshift(c),
            );
            let fac = lf_factors[c] * mul;
            let ch = input[if c < 2 { c ^ 1 } else { c }];
            for y in 0..rect_size.1 {
                let quant_row = ch.row(y);
                let row = lf[c].typed_row_mut::<f32>(y);
                for (x, val) in quant_row.iter().enumerate() {
                    row[x] = *val as f32 * fac;
                }
            }
        }
    }
    if bctx.num_lf_contexts <= 1 {
        for y in 0..r.size.1 {
            quant_lf.typed_row_mut::<u8>(y)[..r.size.0].fill(0);
        }
    } else {
        for y in 0..r.size.1 {
            let qlf_row_val = quant_lf.typed_row_mut::<u8>(y);
            let quant_row_x = input[1].row(y >> frame_header.vshift(0));
            let quant_row_y = input[0].row(y >> frame_header.vshift(1));
            let quant_row_b = input[2].row(y >> frame_header.vshift(2));
            for x in 0..r.size.0 {
                let bucket_x = bctx.lf_thresholds[0]
                    .iter()
                    .filter(|&t| quant_row_x[x >> frame_header.hshift(0)] > *t)
                    .count();
                let bucket_y = bctx.lf_thresholds[1]
                    .iter()
                    .filter(|&t| quant_row_y[x >> frame_header.hshift(1)] > *t)
                    .count();
                let bucket_b = bctx.lf_thresholds[2]
                    .iter()
                    .filter(|&t| quant_row_b[x >> frame_header.hshift(2)] > *t)
                    .count();
                let mut bucket = bucket_x;
                bucket *= bctx.lf_thresholds[2].len() + 1;
                bucket += bucket_b;
                bucket *= bctx.lf_thresholds[1].len() + 1;
                bucket += bucket_y;
                qlf_row_val[x] = bucket as u8;
            }
        }
    }
    Ok(())
}

#[allow(clippy::too_many_arguments)]
pub fn decode_vardct_lf(
    group: usize,
    frame_header: &FrameHeader,
    image_metadata: &ImageMetadata,
    global_tree: &Option<Tree>,
    color_correlation_params: &ColorCorrelationParams,
    quant_params: &QuantizerParams,
    lf_quant: &LfQuantFactors,
    bctx: &BlockContextMap,
    lf: &mut [OutputChannelRef],
    quant_lf: &mut OutputChannelRef,
    br: &mut BitReader,
) -> Result<()> {
    let extra_precision = br.read(2)?;
    debug!(?extra_precision);
    let mul = 1.0 / (1 << extra_precision) as f32;
    let stream_id = ModularStreamId::VarDCTLF(group).get_id(frame_header);
    debug!(?stream_id);
    let r = frame_header.lf_group_rect(group);
    debug!(?r);
    let shrink_rect = |size: (usize, usize), c| {
        (
            size.0 >> frame_header.hshift(c),
            size.1 >> frame_header.vshift(c),
        )
    };
    let mut buffers = [
        ModularChannel::new(shrink_rect(r.size, 1), image_metadata.bit_depth)?,
        ModularChannel::new(shrink_rect(r.size, 0), image_metadata.bit_depth)?,
        ModularChannel::new(shrink_rect(r.size, 2), image_metadata.bit_depth)?,
    ];
    decode_modular_subbitstream(
        buffers.iter_mut().collect(),
        stream_id,
        None,
        global_tree,
        br,
        None,
    )?;
    dequant_lf(
        r,
        lf,
        quant_lf,
        [&buffers[0].data, &buffers[1].data, &buffers[2].data],
        color_correlation_params,
        quant_params,
        lf_quant,
        mul,
        frame_header,
        bctx,
    )
}

pub fn decode_hf_metadata(
    group: usize,
    frame_header: &FrameHeader,
    image_metadata: &ImageMetadata,
    global_tree: &Option<Tree>,
    hf_meta: &mut HfMetaViews,
    br: &mut BitReader,
) -> Result<()> {
    let stream_id = ModularStreamId::LFMeta(group).get_id(frame_header);
    debug!(?stream_id);
    let r = frame_header.lf_group_rect(group);
    debug!(?r);
    let upper_bound = r.size.0 * r.size.1;
    let count_num_bits = upper_bound.ceil_log2();
    let count: usize = br.read(count_num_bits)? as usize + 1;
    debug!(?count);
    let cr = Rect {
        origin: (r.origin.0 >> 3, r.origin.1 >> 3),
        size: (r.size.0.div_ceil(8), r.size.1.div_ceil(8)),
    };
    let mut buffers = [
        ModularChannel::new_with_shift(cr.size, Some((3, 3)), image_metadata.bit_depth)?,
        ModularChannel::new_with_shift(cr.size, Some((3, 3)), image_metadata.bit_depth)?,
        ModularChannel::new((count, 2), image_metadata.bit_depth)?,
        ModularChannel::new(r.size, image_metadata.bit_depth)?,
    ];
    decode_modular_subbitstream(
        buffers.iter_mut().collect(),
        stream_id,
        None,
        global_tree,
        br,
        None,
    )?;
    let ytox_image = &buffers[0].data;
    let ytob_image = &buffers[1].data;
    let i8min: i32 = i8::MIN.into();
    let i8max: i32 = i8::MAX.into();
    for y in 0..cr.size.1 {
        let row_in_x = ytox_image.row(y);
        let row_in_b = ytob_image.row(y);
        let row_out_x = hf_meta.ytox_map.typed_row_mut::<i8>(y);
        let row_out_b = hf_meta.ytob_map.typed_row_mut::<i8>(y);
        for x in 0..cr.size.0 {
            row_out_x[x] = row_in_x[x].clamp(i8min, i8max) as i8;
            row_out_b[x] = row_in_b[x].clamp(i8min, i8max) as i8;
        }
    }
    let transform_image = &buffers[2].data;
    let epf_image = &buffers[3].data;
    let mut num: usize = 0;
    for y in 0..r.size.1 {
        let epf_row_in = epf_image.row(y);
        let epf_row_out = hf_meta.epf_map.typed_row_mut::<u8>(y);
        for x in 0..r.size.0 {
            let epf_val = epf_row_in[x];
            if !(0..8).contains(&epf_val) {
                return Err(Error::InvalidEpfValue(epf_val));
            }
            epf_row_out[x] = epf_val as u8;
            if hf_meta.transform_map.typed_row_mut::<u8>(y)[x] != HfTransformType::INVALID_TRANSFORM
            {
                continue;
            }
            if num >= count {
                return Err(Error::InvalidVarDCTTransformMap);
            }
            let raw_transform = transform_image.row(0)[num];
            let raw_quant = 1 + transform_image.row(1)[num].clamp(0, 255);
            let transform_type = HfTransformType::from_usize(raw_transform as usize)
                .ok_or(Error::InvalidVarDCTTransform(raw_transform as usize))?;

            let cx = covered_blocks_x(transform_type) as usize;
            let cy = covered_blocks_y(transform_type) as usize;
            if (cx > 1 || cy > 1) && !frame_header.is444() {
                return Err(Error::InvalidBlockSizeForChromaSubsampling);
            }
            let next_group = ((x / 32 + 1) * 32, (y / 32 + 1) * 32);
            if x + cx > min(r.size.0, next_group.0) || y + cy > min(r.size.1, next_group.1) {
                return Err(Error::HFBlockOutOfBounds);
            }
            num += 1;

            for iy in 0..cy {
                let trans_row = hf_meta.transform_map.typed_row_mut::<u8>(y + iy);
                let rq_row = hf_meta.raw_quant_map.typed_row_mut::<i32>(y + iy);
                for ix in 0..cx {
                    let is_first_block = iy == 0 && ix == 0;
                    let transform_id =
                        (raw_transform as u8) | (if is_first_block { 128 } else { 0 });
                    trans_row[x + ix] = transform_id;
                    rq_row[x + ix] = raw_quant;
                }
            }
        }
    }
    Ok(())
}

pub fn decode_quant_table(
    index: usize,
    frame_header: &FrameHeader,
    (required_size_x, required_size_y): (usize, usize),
    global_tree: &Option<Tree>,
    br: &mut BitReader,
) -> Result<Vec<i32>> {
    let bit_depth = BitDepth::integer_samples(8);
    let mut image = [
        ModularChannel::new((required_size_x, required_size_y), bit_depth)?,
        ModularChannel::new((required_size_x, required_size_y), bit_depth)?,
        ModularChannel::new((required_size_x, required_size_y), bit_depth)?,
    ];
    let stream_id = ModularStreamId::QuantTable(index).get_id(frame_header);
    decode_modular_subbitstream(
        image.iter_mut().collect(),
        stream_id,
        None,
        global_tree,
        br,
        None,
    )?;
    let mut qtable = Vec::with_capacity(required_size_x * required_size_y * 3);
    for channel in image.iter_mut() {
        for entry in channel
            .data
            .get_rect(Rect {
                size: (required_size_x, required_size_y),
                origin: (0, 0),
            })
            .iter()
        {
            qtable.push(entry);
            if entry <= 0 {
                return Err(Error::InvalidRawQuantTable);
            }
        }
    }
    Ok(qtable)
}
