// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    fmt::Debug,
    sync::atomic::{AtomicUsize, Ordering},
};

use crate::{
    error::Result,
    frame::modular::{
        DataStatus, ModularBufferInfo, ModularGridKind, Predictor, TransformScratchSpace,
        buffers::{ModularChannel, with_buffers},
        transforms::squeeze::{smooth_2d_unsqueeze, smooth_h_unsqueeze, smooth_v_unsqueeze},
    },
    headers::{frame_header::FrameHeader, modular::WeightedHeader},
    image::{Image, ImageRect, Rect},
    util::{AtomicRef, AtomicRefMut, SmallVec, tracing_wrappers::*},
};
use std::ops::{Deref, DerefMut};

use super::{RctOp, RctPermutation};

#[derive(Debug, Clone)]
pub enum TransformStep {
    Rct {
        buf_in: [usize; 3],
        buf_out: [usize; 3],
        op: RctOp,
        perm: RctPermutation,
    },
    Palette {
        buf_in: usize,
        buf_pal: usize,
        buf_out: Vec<usize>,
        num_colors: usize,
        num_deltas: usize,
        predictor: Predictor,
        wp_header: WeightedHeader,
    },
    HSqueeze {
        buf_in: [usize; 2],
        buf_out: usize,
        // If buf_in[0] was obtained via VSqueeze, the two source buffers
        // for that transform.
        buf_in_avg: Option<[usize; 2]>,
    },
    VSqueeze {
        buf_in: [usize; 2],
        buf_out: usize,
        // If buf_in[0] was obtained via HSqueeze, the two source buffers
        // for that transform.
        buf_in_avg: Option<[usize; 2]>,
    },
    Output {
        buf_in: usize,
        rect: Option<Rect>,
        group: usize,
        channel: usize,
    },
}

#[derive(Debug)]
pub struct TransformStepChunk {
    pub(super) step: TransformStep,

    // Grid position this transform should produce.
    // Note that this is a lie for Palette with AverageAll or Weighted, as the transform with
    // position (0, y) will produce the entire row of blocks (*, y) (and there will be no
    // transforms with position (x, y) with x > 0).
    pub(super) grid_pos: (usize, usize),

    // Number of missing final dependencies for this transform.
    // Note that this is updated *before* actually computing other transforms.
    pub(super) missing_final_deps: usize,

    // Number of dependencies that are still missing *during this progressive
    // preview phase*.
    pub(super) missing_deps: AtomicUsize,
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
enum SqueezeStepKind {
    Regular,
    Upsample1D(usize, usize),
    Upsample2D(usize, usize),
}

#[derive(Debug)]
struct SqueezeInfo<T> {
    kind: SqueezeStepKind,
    out_rect: Rect,
    in_avg: T,
    avg_rect: Rect,
    in_res: T,
    res_rect: Rect,
    in_next_avg: Option<T>,
    out_prev: Option<T>,
    next_avg_rect: Option<Rect>,
}

fn borrow_channel(
    buffers: &[ModularBufferInfo],
    x: (usize, usize),
) -> AtomicRef<'_, ModularChannel> {
    AtomicRef::map(buffers[x.0].buffer_grid[x.1].data.borrow(), |x| {
        x.as_ref().unwrap()
    })
}

impl SqueezeInfo<(usize, usize)> {
    fn new(
        buffers: &[ModularBufferInfo],
        buf_in: [usize; 2],
        buf_out: usize,
        buf_in_avg: Option<[usize; 2]>,
        output_grid_pos: (usize, usize),
        frame_header: &FrameHeader,
        vertical: bool,
    ) -> Self {
        let buf_avg = &buffers[buf_in[0]];
        let buf_res = &buffers[buf_in[1]];
        let output_grid_kind = buffers[buf_out].grid_kind;
        let in_grid = buf_avg.get_grid_idx(output_grid_kind, output_grid_pos);
        let res_grid = buf_res.get_grid_idx(output_grid_kind, output_grid_pos);
        let kind = if buf_res.buffer_grid[res_grid].data_status != DataStatus::Zero {
            SqueezeStepKind::Regular
        } else if let Some([avg2_buf, res2_buf]) = buf_in_avg {
            let avg2_grid = buffers[avg2_buf].get_grid_idx(output_grid_kind, output_grid_pos);
            let res2_grid = buffers[res2_buf].get_grid_idx(output_grid_kind, output_grid_pos);
            if buffers[res2_buf].buffer_grid[res2_grid].data_status == DataStatus::Zero {
                SqueezeStepKind::Upsample2D(avg2_buf, avg2_grid)
            } else {
                SqueezeStepKind::Upsample1D(buf_in[0], in_grid)
            }
        } else {
            SqueezeStepKind::Upsample1D(buf_in[0], in_grid)
        };
        let (gx, gy) = output_grid_pos;
        let mut out_rect =
            buffers[buf_out].get_grid_rect(frame_header, output_grid_kind, output_grid_pos);
        out_rect.origin = if output_grid_kind == ModularGridKind::None {
            (0, 0)
        } else {
            let out_shift = buffers[buf_out].info.shift.unwrap_or((0, 0));
            let out_grid_dim = output_grid_kind.grid_dim(frame_header, out_shift);
            (gx * out_grid_dim.0, gy * out_grid_dim.1)
        };
        let pos_next = if vertical {
            (gy < buffers[buf_out].grid_shape.1 - 1).then(|| (gx, gy + 1))
        } else {
            (gx < buffers[buf_out].grid_shape.0 - 1).then(|| (gx + 1, gy))
        };
        let pos_prev = if vertical {
            (gy > 0).then(|| (gx, gy - 1))
        } else {
            (gx > 0).then(|| (gx - 1, gy))
        };
        let next_avg_grid = pos_next.map(|x| buf_avg.get_grid_idx(output_grid_kind, x));
        let prev_out_grid = pos_prev.map(|x| buffers[buf_out].get_grid_idx(output_grid_kind, x));
        let next_avg_rect =
            pos_next.map(|x| buf_avg.get_grid_rect(frame_header, output_grid_kind, x));
        Self {
            kind,
            out_rect,
            in_avg: (buf_in[0], in_grid),
            avg_rect: buf_avg.get_grid_rect(frame_header, output_grid_kind, output_grid_pos),
            in_res: (buf_in[1], res_grid),
            res_rect: buf_res.get_grid_rect(frame_header, output_grid_kind, output_grid_pos),
            in_next_avg: next_avg_grid.map(|x| (buf_in[0], x)),
            out_prev: prev_out_grid.map(|x| (buf_out, x)),
            next_avg_rect,
        }
    }

    // The lifetimes prevent calling decrement_refs while buffers are still borrowed.
    fn borrow<'a>(
        &'a self,
        buffers: &'a [ModularBufferInfo],
    ) -> SqueezeInfo<AtomicRef<'a, ModularChannel>> {
        SqueezeInfo {
            kind: self.kind,
            out_rect: self.out_rect,
            in_avg: borrow_channel(buffers, self.in_avg),
            avg_rect: self.avg_rect,
            in_res: borrow_channel(buffers, self.in_res),
            res_rect: self.res_rect,
            in_next_avg: self.in_next_avg.map(|x| borrow_channel(buffers, x)),
            out_prev: self.out_prev.map(|x| borrow_channel(buffers, x)),
            next_avg_rect: self.next_avg_rect,
        }
    }

    fn decrement_refs(self, buffers: &[ModularBufferInfo], is_final: bool) {
        buffers[self.in_avg.0].buffer_grid[self.in_avg.1].mark_used(is_final);
        buffers[self.in_res.0].buffer_grid[self.in_res.1].mark_used(is_final);
    }
}

impl<'a> SqueezeInfo<AtomicRef<'a, ModularChannel>> {
    fn in_avg_rect(&self) -> ImageRect<'_, i32> {
        self.in_avg.data.get_rect(self.avg_rect)
    }
    fn in_res_rect(&self) -> ImageRect<'_, i32> {
        self.in_res.data.get_rect(self.res_rect)
    }
    fn in_next_avg_rect(&self) -> Option<ImageRect<'_, i32>> {
        self.in_next_avg
            .as_ref()
            .map(|x| x.data.get_rect(self.next_avg_rect.unwrap()))
    }
}

impl TransformStepChunk {
    fn buf_out(&self) -> &[usize] {
        match &self.step {
            TransformStep::Rct { buf_out, .. } => buf_out,
            TransformStep::Palette { buf_out, .. } => buf_out,
            TransformStep::HSqueeze { buf_out, .. } | TransformStep::VSqueeze { buf_out, .. } => {
                std::slice::from_ref(buf_out)
            }
            TransformStep::Output { .. } => &[],
        }
    }

    // (group, channel)
    pub fn output_info(&self) -> Option<(usize, usize)> {
        match &self.step {
            TransformStep::Output { group, channel, .. } => Some((*group, *channel)),
            _ => None,
        }
    }

    // Returns true if this was the last remaining final dep.
    pub fn final_dep_ready(&mut self) -> bool {
        self.missing_final_deps = self.missing_final_deps.checked_sub(1).unwrap();
        self.missing_final_deps == 0
    }

    pub fn ready_for_final_render(&self) -> bool {
        self.missing_final_deps == 0
    }

    pub fn no_current_deps(&self) -> bool {
        self.missing_deps.load(Ordering::Relaxed) == 0
    }

    // Returns true if this was the last remaining current dep.
    pub fn current_dep_ready(&self) -> bool {
        let v = self.missing_deps.fetch_sub(1, Ordering::Relaxed);
        assert_ne!(v, 0);
        v == 1
    }

    pub fn add_current_dep(&mut self) {
        self.missing_deps.fetch_add(1, Ordering::Relaxed);
    }

    // Runs this transform. This function *will* crash if the transform is not ready.
    #[instrument(level = "trace", skip_all)]
    pub fn do_run(
        &self,
        frame_header: &FrameHeader,
        buffers: &[ModularBufferInfo],
        tranform_scratch_space: &mut TransformScratchSpace,
        pass_to_pipeline: &mut dyn FnMut(usize, usize, bool, Image<i32>) -> Result<()>,
    ) -> Result<()> {
        let is_final = self.missing_final_deps == 0;
        let buf_out = self.buf_out();

        // *INPUT* values for Output transforms.
        let (out_grid_kind, out_grid, out_size) =
            if let TransformStep::Output { buf_in, .. } = self.step {
                let b = buf_in;
                (
                    buffers[b].grid_kind,
                    buffers[b].get_grid_idx(buffers[b].grid_kind, self.grid_pos),
                    buffers[b].info.size,
                )
            } else {
                let b = buf_out[0];
                (
                    buffers[b].grid_kind,
                    buffers[b].get_grid_idx(buffers[b].grid_kind, self.grid_pos),
                    buffers[b].info.size,
                )
            };
        for bo in buf_out {
            assert_eq!(out_grid_kind, buffers[*bo].grid_kind);
            assert_eq!(out_size, buffers[*bo].info.size);
        }

        match &self.step {
            TransformStep::Rct {
                buf_in,
                buf_out,
                op,
                perm,
            } => {
                for i in 0..3 {
                    assert_eq!(out_grid_kind, buffers[buf_in[i]].grid_kind);
                    assert_eq!(out_size, buffers[buf_in[i]].info.size);
                    // Optimistically move the buffers to the output if possible.
                    // If not, creates buffers in the output that are a copy of the input buffers.
                    // This should be rare.
                    let b_in = &buffers[buf_in[i]].buffer_grid[out_grid];
                    let b_out = &buffers[buf_out[i]].buffer_grid[out_grid];
                    if b_in.data_status == DataStatus::Zero && !b_in.has_buffer() {
                        b_out.ensure_buffer(&buffers[buf_out[i]].info)?;
                    } else {
                        *b_out.data.borrow_mut() = Some(b_in.get_buffer(is_final)?);
                    }
                }
                with_buffers(buffers, buf_out, out_grid, |mut bufs| {
                    super::rct::do_rct_step(&mut bufs, *op, *perm);
                    Ok(())
                })?;
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                buf_out,
                ..
            } if buffers[*buf_in].info.size.0 == 0 => {
                // Nothing to do, just bookkeeping.
                buffers[*buf_in].buffer_grid[out_grid].mark_used(is_final);
                buffers[*buf_pal].buffer_grid[0].mark_used(is_final);
                with_buffers(buffers, buf_out, out_grid, |_| Ok(()))?;
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                buf_out,
                num_colors,
                num_deltas,
                predictor,
                ..
            } if !predictor.requires_full_row() => {
                assert_eq!(out_grid_kind, buffers[*buf_in].grid_kind);
                assert_eq!(out_size, buffers[*buf_in].info.size);

                {
                    let img_pal = borrow_channel(buffers, (*buf_pal, 0));
                    // Ensure that the output buffers are present.
                    // TODO(szabadka): Extend the callback to support many grid points.
                    with_buffers(buffers, buf_out, out_grid, |_| Ok(()))?;
                    let grid_shape = buffers[buf_out[0]].grid_shape;
                    let grid_x = out_grid % grid_shape.0;
                    let grid_y = out_grid / grid_shape.0;
                    let border = if *predictor == Predictor::Zero { 0 } else { 1 };
                    let grid_x0 = grid_x.saturating_sub(border);
                    let grid_y0 = grid_y.saturating_sub(border);
                    let grid_x1 = grid_x + 1;
                    let grid_y1 = grid_y + 1;
                    let mut out_bufs = vec![];
                    for i in buf_out {
                        for gy in grid_y0..grid_y1 {
                            for gx in grid_x0..grid_x1 {
                                let grid = gy * grid_shape.0 + gx;
                                let buf = &buffers[*i];
                                let b = &buf.buffer_grid[grid];
                                let data = b.data.borrow_mut();
                                out_bufs.push(AtomicRefMut::map(data, |x| x.as_mut().unwrap()));
                            }
                        }
                    }
                    let mut out_buf_refs: Vec<&mut ModularChannel> =
                        out_bufs.iter_mut().map(|x| x.deref_mut()).collect();
                    if matches!(predictor, Predictor::Zero)
                        && buffers[*buf_in].buffer_grid[out_grid].data_status == DataStatus::Zero
                    {
                        super::palette::zero_palette_step_one_group(
                            &img_pal,
                            &mut out_buf_refs,
                            grid_x - grid_x0,
                            grid_y - grid_y0,
                            grid_x1 - grid_x0,
                            grid_y1 - grid_y0,
                            *num_colors,
                            *num_deltas,
                        );
                    } else {
                        let img_in = borrow_channel(buffers, (*buf_in, out_grid));
                        super::palette::do_palette_step_one_group(
                            &img_in,
                            &img_pal,
                            &mut out_buf_refs,
                            grid_x - grid_x0,
                            grid_y - grid_y0,
                            grid_x1 - grid_x0,
                            grid_y1 - grid_y0,
                            *num_colors,
                            *num_deltas,
                            *predictor,
                        );
                    }
                }
                buffers[*buf_in].buffer_grid[out_grid].mark_used(is_final);
                buffers[*buf_pal].buffer_grid[0].mark_used(is_final);
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                buf_out,
                num_colors,
                num_deltas,
                predictor,
                wp_header,
            } => {
                assert_eq!(out_grid_kind, buffers[*buf_in].grid_kind);
                assert_eq!(out_size, buffers[*buf_in].info.size);
                let grid_shape = buffers[buf_out[0]].grid_shape;
                {
                    assert_eq!(out_grid % grid_shape.0, 0);
                    let grid_y = out_grid / grid_shape.0;
                    let grid_y0 = grid_y.saturating_sub(1);
                    let grid_y1 = grid_y + 1;
                    let mut in_bufs = vec![];
                    for grid_x in 0..grid_shape.0 {
                        let grid = grid_y * grid_shape.0 + grid_x;
                        in_bufs.push(borrow_channel(buffers, (*buf_in, grid)));
                        // Ensure that the output buffers are present.
                        // TODO(szabadka): Extend the callback to support many grid points.
                        with_buffers(buffers, buf_out, out_grid + grid_x, |_| Ok(()))?;
                    }
                    let in_buf_refs: Vec<&ModularChannel> =
                        in_bufs.iter().map(|x| x.deref()).collect();
                    let img_pal = borrow_channel(buffers, (*buf_pal, 0));
                    let mut out_bufs = vec![];
                    for i in buf_out {
                        for grid_y in grid_y0..grid_y1 {
                            for grid_x in 0..grid_shape.0 {
                                let grid = grid_y * grid_shape.0 + grid_x;
                                let buf = &buffers[*i];
                                let b = &buf.buffer_grid[grid];
                                let data = b.data.borrow_mut();
                                out_bufs.push(AtomicRefMut::map(data, |x| x.as_mut().unwrap()));
                            }
                        }
                    }
                    let mut out_buf_refs: Vec<&mut ModularChannel> =
                        out_bufs.iter_mut().map(|x| x.deref_mut()).collect();
                    super::palette::do_palette_step_group_row(
                        &in_buf_refs,
                        &img_pal,
                        &mut out_buf_refs,
                        grid_y - grid_y0,
                        grid_shape.0,
                        *num_colors,
                        *num_deltas,
                        *predictor,
                        wp_header,
                    )?;
                }
                buffers[*buf_pal].buffer_grid[0].mark_used(is_final);
                for grid_x in 0..grid_shape.0 {
                    buffers[*buf_in].buffer_grid[out_grid + grid_x].mark_used(is_final);
                }
            }
            TransformStep::HSqueeze {
                buf_in,
                buf_out,
                buf_in_avg,
            } => {
                let info = SqueezeInfo::new(
                    buffers,
                    *buf_in,
                    *buf_out,
                    *buf_in_avg,
                    self.grid_pos,
                    frame_header,
                    false,
                );
                trace!(
                    "HSqueeze {:?} -> {:?}, grid {out_grid} grid pos {:?}: {info:?}",
                    buf_in, buf_out, self.grid_pos
                );
                with_buffers(buffers, &[*buf_out], out_grid, |mut bufs| {
                    if bufs.is_empty() {
                        return Ok(());
                    }
                    match info.kind {
                        SqueezeStepKind::Upsample2D(b, _) => {
                            assert_eq!(bufs.len(), 1);
                            smooth_2d_unsqueeze(
                                &buffers[b],
                                frame_header,
                                info.out_rect,
                                &mut bufs[0].data,
                                &mut tranform_scratch_space.smooth_unsqueeze_buffer,
                            )
                        }
                        SqueezeStepKind::Upsample1D(b, _) => {
                            assert_eq!(bufs.len(), 1);
                            smooth_h_unsqueeze(
                                &buffers[b],
                                frame_header,
                                info.out_rect,
                                &mut bufs[0].data,
                                &mut tranform_scratch_space.smooth_unsqueeze_buffer,
                            )
                        }
                        SqueezeStepKind::Regular => {
                            let info = info.borrow(buffers);
                            super::squeeze::do_hsqueeze_step(
                                &info.in_avg_rect(),
                                &info.in_res_rect(),
                                &info.in_next_avg_rect(),
                                &info.out_prev,
                                &mut bufs,
                            )
                        }
                    }
                    Ok(())
                })?;
                info.decrement_refs(buffers, is_final);
            }
            TransformStep::VSqueeze {
                buf_in,
                buf_out,
                buf_in_avg,
            } => {
                let info = SqueezeInfo::new(
                    buffers,
                    *buf_in,
                    *buf_out,
                    *buf_in_avg,
                    self.grid_pos,
                    frame_header,
                    true,
                );
                trace!(
                    "VSqueeze {:?} -> {:?}, grid {out_grid} grid pos {:?}: {info:?}",
                    buf_in, buf_out, self.grid_pos
                );
                with_buffers(buffers, &[*buf_out], out_grid, |mut bufs| {
                    if bufs.is_empty() {
                        return Ok(());
                    }
                    match info.kind {
                        SqueezeStepKind::Upsample2D(b, _) => {
                            assert_eq!(bufs.len(), 1);
                            smooth_2d_unsqueeze(
                                &buffers[b],
                                frame_header,
                                info.out_rect,
                                &mut bufs[0].data,
                                &mut tranform_scratch_space.smooth_unsqueeze_buffer,
                            )
                        }
                        SqueezeStepKind::Upsample1D(b, _) => {
                            assert_eq!(bufs.len(), 1);
                            smooth_v_unsqueeze(
                                &buffers[b],
                                frame_header,
                                info.out_rect,
                                &mut bufs[0].data,
                                &mut tranform_scratch_space.smooth_unsqueeze_buffer,
                            )
                        }
                        SqueezeStepKind::Regular => {
                            let info = info.borrow(buffers);
                            super::squeeze::do_vsqueeze_step(
                                &info.in_avg_rect(),
                                &info.in_res_rect(),
                                &info.in_next_avg_rect(),
                                &info.out_prev,
                                &mut bufs,
                            )
                        }
                    }
                    Ok(())
                })?;
                info.decrement_refs(buffers, is_final);
            }
            TransformStep::Output {
                buf_in,
                rect,
                group,
                channel,
            } => {
                debug!("Rendering channel {channel:?}, rect {rect:?}, group {group}");
                let buf = &buffers[*buf_in].buffer_grid[out_grid];
                if buf.data_status == DataStatus::Zero && !buf.has_buffer() {
                    let zero = Image::new(rect.map(|x| x.size).unwrap_or(buf.size))?;
                    pass_to_pipeline(*channel, *group, is_final, zero)?;
                } else {
                    let modular_buf = buf.get_buffer(is_final)?;
                    if let Some(rect) = rect {
                        let mut cropped = Image::new(rect.size)?;
                        let src_view = modular_buf.data.get_rect(*rect);
                        for y in 0..rect.size.1 {
                            cropped.row_mut(y).copy_from_slice(src_view.row(y));
                        }
                        pass_to_pipeline(*channel, *group, is_final, cropped)?;
                    } else {
                        pass_to_pipeline(*channel, *group, is_final, modular_buf.data)?;
                    }
                }
            }
        };

        Ok(())
    }

    // Iterates over the list of outputs for this transform.
    // Except for palette, we only output 1 (squeeze) or 3 (RCT) buffers.
    // For non-delta palette, in most cases we output 1, 3 or 4 channels.
    pub fn outputs(&self, buffers: &[ModularBufferInfo]) -> SmallVec<(usize, usize), 4> {
        let buf_out = self.buf_out();
        let b = buf_out.first().copied().unwrap_or(0);
        let out_grid_kind = buffers[b].grid_kind;
        let out_grid = buffers[b].get_grid_idx(out_grid_kind, self.grid_pos);
        let grid_offset_up = match &self.step {
            TransformStep::Palette {
                buf_in,
                buf_out,
                predictor,
                ..
            } if buffers[*buf_in].info.size.0 != 0 && predictor.requires_full_row() => {
                buffers[buf_out[0]].grid_shape.0
            }
            TransformStep::Output { .. } => 0,
            _ => 1,
        };

        buf_out
            .iter()
            .flat_map(move |x| (0..grid_offset_up).map(move |y| (*x, out_grid + y)))
            .collect()
    }
}

#[derive(Debug)]
pub struct TransformDependency {
    pub buffer: usize,
    pub grid: usize,
    pub order_only: bool,
}

impl TransformDependency {
    fn new(buffer: usize, grid: usize) -> Self {
        Self {
            buffer,
            grid,
            order_only: false,
        }
    }

    fn new_order_only(buffer: usize, grid: usize) -> Self {
        Self {
            buffer,
            grid,
            order_only: true,
        }
    }
}

impl TransformStepChunk {
    // List of input buffers for this transform.
    // We use a stack-size-9 SmallVec because upsampling squeezes touch 9 buffers total (and that is the maximum for
    // non-delta-palette transforms)
    pub fn dependecies(
        &self,
        buffers: &[ModularBufferInfo],
        frame_header: &FrameHeader,
    ) -> SmallVec<TransformDependency, 9> {
        match &self.step {
            TransformStep::Rct { buf_in, .. } => {
                let b = buf_in[0];
                let grid_idx = buffers[b].get_grid_idx(buffers[b].grid_kind, self.grid_pos);
                buf_in
                    .iter()
                    .map(|x| TransformDependency::new(*x, grid_idx))
                    .collect()
            }
            TransformStep::Output { buf_in, .. } => {
                let b = *buf_in;
                let grid_idx = buffers[b].get_grid_idx(buffers[b].grid_kind, self.grid_pos);
                std::iter::once(TransformDependency::new(b, grid_idx)).collect()
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                predictor,
                ..
            } if !predictor.requires_full_row() => {
                let b = *buf_in;
                let grid_idx = buffers[b].get_grid_idx(buffers[b].grid_kind, self.grid_pos);
                [(b, grid_idx), (*buf_pal, 0)]
                    .into_iter()
                    .map(|(a, b)| TransformDependency::new(a, b))
                    .collect()
            }
            TransformStep::Palette {
                buf_in, buf_pal, ..
            } => {
                let b = *buf_in;
                let mut ans = SmallVec::new();
                let grid_shape = buffers[b].grid_shape;
                let grid_idx = buffers[b].get_grid_idx(buffers[b].grid_kind, self.grid_pos);
                ans.push(TransformDependency::new(*buf_pal, 0));
                for grid_x in 0..grid_shape.0 {
                    ans.push(TransformDependency::new(b, grid_idx + grid_x));
                }
                ans
            }
            TransformStep::VSqueeze {
                buf_in,
                buf_out,
                buf_in_avg,
            }
            | TransformStep::HSqueeze {
                buf_in,
                buf_out,
                buf_in_avg,
            } => {
                let info = SqueezeInfo::new(
                    buffers,
                    *buf_in,
                    *buf_out,
                    *buf_in_avg,
                    self.grid_pos,
                    frame_header,
                    matches!(self.step, TransformStep::VSqueeze { .. }),
                );
                let mut ans = SmallVec::new();
                match info.kind {
                    SqueezeStepKind::Regular => {
                        ans.push(TransformDependency::new(info.in_avg.0, info.in_avg.1));
                        ans.push(TransformDependency::new(info.in_res.0, info.in_res.1));
                        if let Some(na) = info.in_next_avg {
                            ans.push(TransformDependency::new_order_only(na.0, na.1));
                        }
                        if let Some(op) = info.out_prev {
                            ans.push(TransformDependency::new_order_only(op.0, op.1));
                        }
                    }
                    SqueezeStepKind::Upsample1D(b, g) | SqueezeStepKind::Upsample2D(b, g) => {
                        let (xs, ys) = buffers[b].grid_shape;
                        let (gx, gy) = (g % xs, g / xs);
                        ans.push(TransformDependency::new(b, g));
                        let mut add = |x, y| {
                            ans.push(TransformDependency::new_order_only(b, y * xs + x));
                        };
                        add(gx.saturating_sub(1), gy.saturating_sub(1));
                        add(gx, gy.saturating_sub(1));
                        add((gx + 1).min(xs - 1), gy.saturating_sub(1));
                        add(gx.saturating_sub(1), gy);
                        add((gx + 1).min(xs - 1), gy);
                        add(gx.saturating_sub(1), (gy + 1).min(ys - 1));
                        add(gx, (gy + 1).min(ys - 1));
                        add((gx + 1).min(xs - 1), (gy + 1).min(ys - 1));
                    }
                }
                ans
            }
        }
    }
}
