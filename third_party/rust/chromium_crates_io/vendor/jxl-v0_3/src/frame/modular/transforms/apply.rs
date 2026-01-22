// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

use num_traits::FromPrimitive;

use crate::{
    error::{Error, Result},
    frame::modular::{
        ChannelInfo, ModularBufferInfo, ModularChannel, ModularGridKind, Predictor,
        borrowed_buffers::with_buffers,
    },
    headers::{
        self,
        frame_header::FrameHeader,
        modular::{TransformId, WeightedHeader},
    },
    image::Rect,
    util::{AtomicRef, AtomicRefMut, tracing_wrappers::*},
};
use std::ops::Deref;
use std::ops::DerefMut;

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
    },
    VSqueeze {
        buf_in: [usize; 2],
        buf_out: usize,
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
    // Number of inputs that are not yet available.
    pub(super) incomplete_deps: usize,
}

impl TransformStepChunk {
    // Marks that one dependency of this transform is ready, and potentially runs the transform,
    // returning the new buffers that are now ready.
    #[instrument(level = "trace", skip_all)]
    pub fn dep_ready(
        &mut self,
        frame_header: &FrameHeader,
        buffers: &mut [ModularBufferInfo],
    ) -> Result<Vec<(usize, usize)>> {
        self.incomplete_deps = self.incomplete_deps.checked_sub(1).unwrap();
        if self.incomplete_deps > 0 {
            trace!(
                "skipping transform chunk because incomplete_deps = {}",
                self.incomplete_deps
            );
            return Ok(vec![]);
        }
        let buf_out: &[usize] = match &self.step {
            TransformStep::Rct { buf_out, .. } => buf_out,
            TransformStep::Palette { buf_out, .. } => buf_out,
            TransformStep::HSqueeze { buf_out, .. } | TransformStep::VSqueeze { buf_out, .. } => {
                &[*buf_out]
            }
        };

        let out_grid_kind = buffers[buf_out[0]].grid_kind;
        let out_grid = buffers[buf_out[0]].get_grid_idx(out_grid_kind, self.grid_pos);
        let out_size = buffers[buf_out[0]].info.size;
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
                    *buffers[buf_out[i]].buffer_grid[out_grid].data.borrow_mut() =
                        Some(buffers[buf_in[i]].buffer_grid[out_grid].get_buffer()?);
                }
                with_buffers(buffers, buf_out, out_grid, false, |mut bufs| {
                    super::rct::do_rct_step(&mut bufs, *op, *perm);
                    Ok(())
                })?;
                Ok(buf_out.iter().map(|x| (*x, out_grid)).collect())
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                buf_out,
                ..
            } if buffers[*buf_in].info.size.0 == 0 => {
                // Nothing to do, just bookkeeping.
                buffers[*buf_in].buffer_grid[out_grid].mark_used();
                buffers[*buf_pal].buffer_grid[0].mark_used();
                with_buffers(buffers, buf_out, out_grid, false, |_| Ok(()))?;
                Ok(buf_out.iter().map(|x| (*x, out_grid)).collect())
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
                    let img_in =
                        AtomicRef::map(buffers[*buf_in].buffer_grid[out_grid].data.borrow(), |x| {
                            x.as_ref().unwrap()
                        });
                    let img_pal =
                        AtomicRef::map(buffers[*buf_pal].buffer_grid[0].data.borrow(), |x| {
                            x.as_ref().unwrap()
                        });
                    // Ensure that the output buffers are present.
                    // TODO(szabadka): Extend the callback to support many grid points.
                    with_buffers(buffers, buf_out, out_grid, false, |_| Ok(()))?;
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
                buffers[*buf_in].buffer_grid[out_grid].mark_used();
                buffers[*buf_pal].buffer_grid[0].mark_used();
                Ok(buf_out.iter().map(|x| (*x, out_grid)).collect())
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
                let mut generated_chunks = Vec::<(usize, usize)>::new();
                let grid_shape = buffers[buf_out[0]].grid_shape;
                {
                    assert_eq!(out_grid % grid_shape.0, 0);
                    let grid_y = out_grid / grid_shape.0;
                    let grid_y0 = grid_y.saturating_sub(1);
                    let grid_y1 = grid_y + 1;
                    let mut in_bufs = vec![];
                    for grid_x in 0..grid_shape.0 {
                        let grid = grid_y * grid_shape.0 + grid_x;
                        in_bufs.push(AtomicRef::map(
                            buffers[*buf_in].buffer_grid[grid].data.borrow(),
                            |x| x.as_ref().unwrap(),
                        ));
                        // Ensure that the output buffers are present.
                        // TODO(szabadka): Extend the callback to support many grid points.
                        with_buffers(buffers, buf_out, out_grid + grid_x, false, |_| Ok(()))?;
                    }
                    let in_buf_refs: Vec<&ModularChannel> =
                        in_bufs.iter().map(|x| x.deref()).collect();
                    let img_pal =
                        AtomicRef::map(buffers[*buf_pal].buffer_grid[0].data.borrow(), |x| {
                            x.as_ref().unwrap()
                        });
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
                buffers[*buf_pal].buffer_grid[0].mark_used();
                for grid_x in 0..grid_shape.0 {
                    buffers[*buf_in].buffer_grid[out_grid + grid_x].mark_used();
                    for buf in buf_out {
                        generated_chunks.push((*buf, out_grid + grid_x));
                    }
                }
                Ok(generated_chunks)
            }
            TransformStep::HSqueeze { buf_in, buf_out } => {
                let buf_avg = &buffers[buf_in[0]];
                let buf_res = &buffers[buf_in[1]];
                let in_grid = buf_avg.get_grid_idx(out_grid_kind, self.grid_pos);
                let res_grid = buf_res.get_grid_idx(out_grid_kind, self.grid_pos);
                {
                    trace!(
                        "HSqueeze {:?} -> {:?}, grid {out_grid} grid pos {:?}",
                        buf_in, buf_out, self.grid_pos
                    );
                    let (gx, gy) = self.grid_pos;
                    let in_avg = AtomicRef::map(buf_avg.buffer_grid[in_grid].data.borrow(), |x| {
                        x.as_ref().unwrap()
                    });
                    let has_next = gx + 1 < buffers[*buf_out].grid_shape.0;
                    let gx_next = if has_next { gx + 1 } else { gx };
                    let next_avg_grid = buf_avg.get_grid_idx(out_grid_kind, (gx_next, gy));
                    let in_next_avg =
                        AtomicRef::map(buf_avg.buffer_grid[next_avg_grid].data.borrow(), |x| {
                            x.as_ref().unwrap()
                        });
                    let in_next_avg_rect = if has_next {
                        Some(in_next_avg.data.get_rect(buf_avg.get_grid_rect(
                            frame_header,
                            out_grid_kind,
                            (gx_next, gy),
                        )))
                    } else {
                        None
                    };
                    let in_res = AtomicRef::map(buf_res.buffer_grid[res_grid].data.borrow(), |x| {
                        x.as_ref().unwrap()
                    });
                    let out_prev = if gx == 0 {
                        None
                    } else {
                        let prev_out_grid =
                            buffers[*buf_out].get_grid_idx(out_grid_kind, (gx - 1, gy));
                        Some(AtomicRef::map(
                            buffers[*buf_out].buffer_grid[prev_out_grid].data.borrow(),
                            |x| x.as_ref().unwrap(),
                        ))
                    };

                    with_buffers(buffers, &[*buf_out], out_grid, false, |mut bufs| {
                        super::squeeze::do_hsqueeze_step(
                            &in_avg.data.get_rect(buf_avg.get_grid_rect(
                                frame_header,
                                out_grid_kind,
                                (gx, gy),
                            )),
                            &in_res.data.get_rect(buf_res.get_grid_rect(
                                frame_header,
                                out_grid_kind,
                                (gx, gy),
                            )),
                            &in_next_avg_rect,
                            &out_prev,
                            &mut bufs,
                        );
                        Ok(())
                    })?;
                }
                buffers[buf_in[0]].buffer_grid[in_grid].mark_used();
                buffers[buf_in[1]].buffer_grid[res_grid].mark_used();
                Ok(vec![(*buf_out, out_grid)])
            }
            TransformStep::VSqueeze { buf_in, buf_out } => {
                let buf_avg = &buffers[buf_in[0]];
                let buf_res = &buffers[buf_in[1]];
                let in_grid = buf_avg.get_grid_idx(out_grid_kind, self.grid_pos);
                let res_grid = buf_res.get_grid_idx(out_grid_kind, self.grid_pos);
                {
                    trace!(
                        "VSqueeze {:?} -> {:?} grid: {out_grid:?} grid pos: {:?}",
                        buf_in, buf_out, self.grid_pos
                    );
                    let (gx, gy) = self.grid_pos;
                    let in_avg = AtomicRef::map(buf_avg.buffer_grid[in_grid].data.borrow(), |x| {
                        x.as_ref().unwrap()
                    });
                    let has_next = gy + 1 < buffers[*buf_out].grid_shape.1;
                    let gy_next = if has_next { gy + 1 } else { gy };
                    let next_avg_grid = buf_avg.get_grid_idx(out_grid_kind, (gx, gy_next));
                    let in_next_avg =
                        AtomicRef::map(buf_avg.buffer_grid[next_avg_grid].data.borrow(), |x| {
                            x.as_ref().unwrap()
                        });
                    let in_next_avg_rect = if has_next {
                        Some(in_next_avg.data.get_rect(buf_avg.get_grid_rect(
                            frame_header,
                            out_grid_kind,
                            (gx, gy_next),
                        )))
                    } else {
                        None
                    };
                    let in_res = AtomicRef::map(buf_res.buffer_grid[res_grid].data.borrow(), |x| {
                        x.as_ref().unwrap()
                    });
                    let out_prev = if gy == 0 {
                        None
                    } else {
                        let prev_out_grid =
                            buffers[*buf_out].get_grid_idx(out_grid_kind, (gx, gy - 1));
                        Some(AtomicRef::map(
                            buffers[*buf_out].buffer_grid[prev_out_grid].data.borrow(),
                            |x| x.as_ref().unwrap(),
                        ))
                    };
                    let avg_grid_rect =
                        buf_avg.get_grid_rect(frame_header, out_grid_kind, (gx, gy));
                    let res_grid_rect =
                        buf_res.get_grid_rect(frame_header, out_grid_kind, (gx, gy));
                    with_buffers(buffers, &[*buf_out], out_grid, false, |mut bufs| {
                        super::squeeze::do_vsqueeze_step(
                            &in_avg.data.get_rect(avg_grid_rect),
                            &in_res.data.get_rect(res_grid_rect),
                            &in_next_avg_rect,
                            &out_prev,
                            &mut bufs,
                        );
                        Ok(())
                    })?;
                }
                buffers[buf_in[0]].buffer_grid[in_grid].mark_used();
                buffers[buf_in[1]].buffer_grid[res_grid].mark_used();
                Ok(vec![(*buf_out, out_grid)])
            }
        }
    }
}

#[instrument(level = "trace", err)]
fn check_equal_channels(
    channels: &[(usize, ChannelInfo)],
    first_chan: usize,
    num: usize,
) -> Result<()> {
    if first_chan + num > channels.len() {
        return Err(Error::InvalidChannelRange(
            first_chan,
            first_chan + num,
            channels.len(),
        ));
    }
    for inc in 1..num {
        if !channels[first_chan]
            .1
            .is_equivalent(&channels[first_chan + inc].1)
        {
            return Err(Error::MixingDifferentChannels);
        }
    }
    Ok(())
}

fn meta_apply_single_transform(
    transform: &headers::modular::Transform,
    header: &headers::modular::GroupHeader,
    channels: &mut Vec<(usize, ChannelInfo)>,
    transform_steps: &mut Vec<TransformStep>,
    mut add_transform_buffer: impl FnMut(ChannelInfo, String) -> usize,
) -> Result<()> {
    match transform.id {
        TransformId::Rct => {
            let begin_channel = transform.begin_channel as usize;
            let op = RctOp::from_u32(transform.rct_type % 7).unwrap();
            let perm = RctPermutation::from_u32(transform.rct_type / 7)
                .expect("header decoding should ensure rct_type < 42");
            check_equal_channels(channels, begin_channel, 3)?;
            let mut buf_in = [0; 3];
            let buf_out = [
                channels[begin_channel].0,
                channels[begin_channel + 1].0,
                channels[begin_channel + 2].0,
            ];
            for i in 0..3 {
                let c = &mut channels[begin_channel + i];
                let mut info = c.1;
                info.output_channel_idx = -1;
                c.0 = add_transform_buffer(
                    info,
                    format!(
                        "RCT (op {op:?} perm {perm:?}) starting at channel {begin_channel}, \
			 input {i}"
                    ),
                );
                buf_in[i] = c.0;
            }
            transform_steps.push(TransformStep::Rct {
                buf_out,
                buf_in,
                op,
                perm,
            });
            trace!("applied RCT: {channels:?}");
        }
        TransformId::Squeeze => {
            let steps = if transform.squeezes.is_empty() {
                super::squeeze::default_squeeze(channels)
            } else {
                transform.squeezes.clone()
            };
            for step in steps {
                super::squeeze::check_squeeze_params(channels, &step)?;
                let in_place = step.in_place;
                let horizontal = step.horizontal;
                let begin_channel = step.begin_channel as usize;
                let num_channels = step.num_channels as usize;
                let end_channel = begin_channel + num_channels;
                let new_chan_offset = if in_place {
                    end_channel
                } else {
                    channels.len()
                };
                for ic in 0..num_channels {
                    let chan = &channels[begin_channel + ic].1;
                    let new_shift = if let Some(shift) = chan.shift {
                        if shift.0 > 30 || shift.1 > 30 {
                            return Err(Error::TooManySqueezes);
                        }
                        if horizontal {
                            Some((shift.0 + 1, shift.1))
                        } else {
                            Some((shift.0, shift.1 + 1))
                        }
                    } else {
                        None
                    };
                    let w = chan.size.0;
                    let h = chan.size.1;
                    let (new_size_0, new_size_1) = if horizontal {
                        ((w.div_ceil(2), h), (w - w.div_ceil(2), h))
                    } else {
                        ((w, h.div_ceil(2)), (w, h - h.div_ceil(2)))
                    };
                    let new_0 = ChannelInfo {
                        output_channel_idx: -1,
                        shift: new_shift,
                        size: new_size_0,
                        bit_depth: chan.bit_depth,
                    };
                    let buf_0 = add_transform_buffer(
                        new_0,
                        format!("Squeezed channel, original channel {}", begin_channel + ic),
                    );
                    let new_1 = ChannelInfo {
                        output_channel_idx: -1,
                        shift: new_shift,
                        size: new_size_1,
                        bit_depth: chan.bit_depth,
                    };
                    let buf_1 = add_transform_buffer(
                        new_1,
                        format!("Squeeze residual, original channel {}", begin_channel + ic),
                    );
                    if horizontal {
                        transform_steps.push(TransformStep::HSqueeze {
                            buf_in: [buf_0, buf_1],
                            buf_out: channels[begin_channel + ic].0,
                        });
                    } else {
                        transform_steps.push(TransformStep::VSqueeze {
                            buf_in: [buf_0, buf_1],
                            buf_out: channels[begin_channel + ic].0,
                        });
                    }
                    channels[begin_channel + ic] = (buf_0, new_0);
                    channels.insert(new_chan_offset + ic, (buf_1, new_1));
                    trace!("applied squeeze: {channels:?}");
                }
            }
        }
        TransformId::Palette => {
            let begin_channel = transform.begin_channel as usize;
            let num_channels = transform.num_channels as usize;
            let num_colors = transform.num_colors as usize;
            let num_deltas = transform.num_deltas as usize;
            let pred = Predictor::from_u32(transform.predictor_id)
                .expect("header decoding should ensure a valid predictor");
            check_equal_channels(channels, begin_channel, num_channels)?;
            // We already checked the bit_depth for all channels from `begin_channel` is
            // equal in the line above.
            let bit_depth = channels[begin_channel].1.bit_depth;
            let pchan_info = ChannelInfo {
                output_channel_idx: -1,
                shift: None,
                size: (num_colors + num_deltas, num_channels),
                bit_depth,
            };
            let pchan = add_transform_buffer(
                pchan_info,
                format!(
                    "Palette for palette transform starting at channel {begin_channel} with \
		     {num_channels} channels"
                ),
            );
            let mut inchan_info = channels[begin_channel].1;
            inchan_info.output_channel_idx = -1;
            let inchan = add_transform_buffer(
                inchan_info,
                format!(
                    "Pixel data for palette transform starting at channel {begin_channel} with \
		     {num_channels} channels",
                ),
            );
            transform_steps.push(TransformStep::Palette {
                buf_in: inchan,
                buf_pal: pchan,
                buf_out: channels[begin_channel..(begin_channel + num_channels)]
                    .iter()
                    .map(|x| x.0)
                    .collect(),
                num_colors,
                num_deltas,
                predictor: pred,
                wp_header: header.wp_header.clone(),
            });
            channels.drain(begin_channel + 1..begin_channel + num_channels);
            channels[begin_channel].0 = inchan;
            channels.insert(0, (pchan, pchan_info));
            trace!("applied palette: {channels:?}");
        }
        TransformId::Invalid => {
            unreachable!("header decoding for invalid transforms should fail");
        }
    }
    Ok(())
}

#[instrument(level = "trace", ret)]
pub fn meta_apply_transforms(
    channels: &[ChannelInfo],
    header: &headers::modular::GroupHeader,
) -> Result<(Vec<ModularBufferInfo>, Vec<TransformStep>)> {
    let mut buffer_info = vec![];
    let mut transform_steps = vec![];
    // (buffer id, channel info)
    let mut channels: Vec<_> = channels.iter().cloned().enumerate().collect();

    // First, add all the pre-transform channels to the buffer list.
    for chan in channels.iter() {
        buffer_info.push(ModularBufferInfo {
            info: chan.1,
            coded_channel_id: -1,
            description: format!(
                "Input channel {}, size {}x{}",
                chan.0, chan.1.size.0, chan.1.size.1
            ),
            // To be filled by make_grids.
            grid_kind: ModularGridKind::None,
            grid_shape: (0, 0),
            buffer_grid: vec![],
        });
    }

    let mut add_transform_buffer = |info, description| {
        buffer_info.push(ModularBufferInfo {
            info,
            coded_channel_id: -1,
            description,
            // To be filled by make_grids.
            grid_kind: ModularGridKind::None,
            grid_shape: (0, 0),
            buffer_grid: vec![],
        });
        buffer_info.len() - 1
    };

    // Apply transforms to the channel list.
    for transform in &header.transforms {
        meta_apply_single_transform(
            transform,
            header,
            &mut channels,
            &mut transform_steps,
            &mut add_transform_buffer,
        )?;
    }

    // All the channels left over at the end of applying transforms are the channels that are
    // actually coded.
    for (chid, chan) in channels.iter().enumerate() {
        buffer_info[chan.0].coded_channel_id = chid as isize;
    }

    #[cfg(feature = "tracing")]
    for (i, transform) in transform_steps.iter().enumerate() {
        trace!("Transform step {i}: {transform:?}");
    }

    Ok((buffer_info, transform_steps))
}

#[derive(Debug)]
pub enum LocalTransformBuffer<'a> {
    // This channel has been consumed by some transform.
    Empty,
    // This channel has not been written to yet.
    Placeholder(ChannelInfo),
    // Temporary, locally-allocated channel.
    Owned(ModularChannel),
    // Channel belonging to the global image.
    Borrowed(&'a mut ModularChannel),
}

impl LocalTransformBuffer<'_> {
    fn channel_info(&self) -> ChannelInfo {
        match self {
            LocalTransformBuffer::Empty => unreachable!("an empty buffer has no channel info"),
            LocalTransformBuffer::Owned(m) => m.channel_info(),
            LocalTransformBuffer::Placeholder(c) => *c,
            LocalTransformBuffer::Borrowed(m) => m.channel_info(),
        }
    }

    fn borrow_mut(&mut self) -> &mut ModularChannel {
        match self {
            LocalTransformBuffer::Owned(m) => m,
            LocalTransformBuffer::Borrowed(m) => m,
            LocalTransformBuffer::Empty => unreachable!("tried to borrow an empty channel"),
            LocalTransformBuffer::Placeholder(_) => {
                unreachable!("tried to borrow a placeholder channel")
            }
        }
    }

    fn take(&mut self) -> Self {
        assert!(!matches!(self, LocalTransformBuffer::Empty));
        let mut r = LocalTransformBuffer::Empty;
        std::mem::swap(self, &mut r);
        r
    }

    fn allocate_if_needed(&mut self) -> Result<()> {
        if let LocalTransformBuffer::Placeholder(c) = self {
            *self = LocalTransformBuffer::Owned(ModularChannel::new_with_shift(
                c.size,
                c.shift,
                c.bit_depth,
            )?);
        }
        Ok(())
    }
}

#[instrument(level = "trace", ret)]
pub fn meta_apply_local_transforms<'a, 'b>(
    channels_in: Vec<&'a mut ModularChannel>,
    buffer_storage: &'b mut Vec<LocalTransformBuffer<'a>>,
    header: &headers::modular::GroupHeader,
) -> Result<(Vec<&'b mut ModularChannel>, Vec<TransformStep>)> {
    let mut transform_steps = vec![];

    // (buffer id, channel info)
    let mut channels: Vec<_> = channels_in
        .iter()
        .map(|x| x.channel_info())
        .enumerate()
        .collect();

    debug!(?channels, "initial channels");

    // First, add all the pre-transform channels to the buffer list.
    buffer_storage.extend(channels_in.into_iter().map(LocalTransformBuffer::Borrowed));

    #[allow(unused_variables)]
    let mut add_transform_buffer = |info, description| {
        trace!(description, ?info, "adding channel buffer");
        buffer_storage.push(LocalTransformBuffer::Placeholder(info));
        buffer_storage.len() - 1
    };

    // Apply transforms to the channel list.
    for transform in &header.transforms {
        meta_apply_single_transform(
            transform,
            header,
            &mut channels,
            &mut transform_steps,
            &mut add_transform_buffer,
        )?;
    }

    debug!(?channels, ?buffer_storage, "channels after transforms");
    debug!(?transform_steps);

    // Ensure that the buffer indices in `channels` appear in increasing order, by reordering them
    // if necessary.
    if !channels.iter().map(|x| x.0).is_sorted() {
        let mut buf_new_position: Vec<_> = channels.iter().map(|x| x.0).collect();
        buf_new_position.sort();
        let buf_tmp: Vec<_> = channels
            .iter()
            .map(|x| {
                let mut b = LocalTransformBuffer::Empty;
                std::mem::swap(&mut b, &mut buffer_storage[x.0]);
                b
            })
            .collect();

        let mut buf_remap: Vec<_> = (0..buffer_storage.len()).collect();

        for (new_pos, (ch_info, buf)) in buf_new_position
            .iter()
            .cloned()
            .zip(channels.iter_mut().zip(buf_tmp.into_iter()))
        {
            assert!(matches!(
                buffer_storage[new_pos],
                LocalTransformBuffer::Empty
            ));
            buf_remap[ch_info.0] = new_pos;
            buffer_storage[new_pos] = buf;
            ch_info.0 = new_pos;
        }

        for step in transform_steps.iter_mut() {
            use std::iter::once;
            match step {
                TransformStep::Rct {
                    buf_in, buf_out, ..
                } => {
                    for b in buf_in.iter_mut().chain(buf_out.iter_mut()) {
                        *b = buf_remap[*b];
                    }
                }
                TransformStep::Palette {
                    buf_in,
                    buf_pal,
                    buf_out,
                    ..
                } => {
                    for b in once(buf_in).chain(once(buf_pal)).chain(buf_out.iter_mut()) {
                        *b = buf_remap[*b];
                    }
                }
                TransformStep::HSqueeze { buf_in, buf_out }
                | TransformStep::VSqueeze { buf_in, buf_out } => {
                    for b in once(buf_out).chain(buf_in.iter_mut()) {
                        *b = buf_remap[*b];
                    }
                }
            }
        }
    }

    debug!(?channels, ?buffer_storage, "sorted channels");

    debug!(?transform_steps);

    // Since RCT steps will try to transfer buffers from the source channels to the destination
    // channels, make sure we do the reverse transformation here (to have the caller-provided
    // buffers be used for writing temporary data).
    for ts in transform_steps.iter() {
        if let TransformStep::Rct {
            buf_in, buf_out, ..
        } = ts
        {
            for c in 0..3 {
                assert_eq!(
                    buffer_storage[buf_in[c]].channel_info(),
                    buffer_storage[buf_out[c]].channel_info()
                );
                assert!(matches!(
                    buffer_storage[buf_in[c]],
                    LocalTransformBuffer::Placeholder(_)
                ));
                buffer_storage.swap(buf_in[c], buf_out[c]);
            }
        }
    }

    debug!(?channels, ?buffer_storage, "RCT-adjusted channels");

    // Allocate all the coded channels if they aren't yet.
    for (buf, _) in channels.iter() {
        buffer_storage[*buf].allocate_if_needed()?;
    }

    debug!(?channels, ?buffer_storage, "allocated buffers");

    // Extract references to to-be-decoded buffers.
    let mut coded_buffers = Vec::with_capacity(channels.len());
    let mut buffer_tail = &mut buffer_storage[..];
    let mut last_buffer = None;
    for (buf, _) in channels {
        let offset = if let Some(lb) = last_buffer {
            buf.checked_sub(lb).unwrap()
        } else {
            buf + 1
        };
        let cur_buf;
        (cur_buf, buffer_tail) = buffer_tail.split_at_mut(offset);
        coded_buffers.push(cur_buf.last_mut().unwrap().borrow_mut());
        last_buffer = Some(buf);
    }

    Ok((coded_buffers, transform_steps))
}

impl TransformStep {
    // Marks that one dependency of this transform is ready, and potentially runs the transform,
    // returning the new buffers that are now ready.
    pub fn local_apply(&self, buffers: &mut [LocalTransformBuffer]) -> Result<()> {
        match self {
            TransformStep::Rct {
                buf_in,
                buf_out,
                op,
                perm,
            } => {
                for i in 0..3 {
                    assert_eq!(
                        buffers[buf_in[i]].channel_info(),
                        buffers[buf_out[i]].channel_info()
                    );
                }
                let [mut a, mut b, mut c] = [
                    buffers[buf_in[0]].take(),
                    buffers[buf_in[1]].take(),
                    buffers[buf_in[2]].take(),
                ];
                {
                    let mut bufs = [a.borrow_mut(), b.borrow_mut(), c.borrow_mut()];
                    super::rct::do_rct_step(&mut bufs, *op, *perm);
                }
                buffers[buf_out[0]] = a;
                buffers[buf_out[1]] = b;
                buffers[buf_out[2]] = c;
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
                for b in buf_out.iter() {
                    assert_eq!(
                        buffers[*b].channel_info().size,
                        buffers[*buf_in].channel_info().size
                    );
                    buffers[*b].allocate_if_needed()?;
                }
                let mut img_in = buffers[*buf_in].take();
                let mut img_pal = buffers[*buf_pal].take();
                let mut out_bufs: Vec<_> = buf_out.iter().map(|x| buffers[*x].take()).collect();
                {
                    let mut bufs: Vec<_> = out_bufs.iter_mut().map(|x| x.borrow_mut()).collect();
                    super::palette::do_palette_step_general(
                        img_in.borrow_mut(),
                        img_pal.borrow_mut(),
                        &mut bufs,
                        *num_colors,
                        *num_deltas,
                        *predictor,
                        wp_header,
                    );
                }
                for (pos, buf) in buf_out.iter().zip(out_bufs.into_iter()) {
                    buffers[*pos] = buf;
                }
            }
            TransformStep::HSqueeze { buf_in, buf_out } => {
                buffers[*buf_out].allocate_if_needed()?;
                let mut out_buf = buffers[*buf_out].take();
                let mut in_avg = buffers[buf_in[0]].take();
                let mut in_res = buffers[buf_in[1]].take();
                {
                    let mut bufs: Vec<_> = vec![out_buf.borrow_mut()];
                    let in_avg = &in_avg.borrow_mut().data;
                    let in_res = &in_res.borrow_mut().data;
                    super::squeeze::do_hsqueeze_step(
                        &in_avg.get_rect(Rect {
                            size: in_avg.size(),
                            origin: (0, 0),
                        }),
                        &in_res.get_rect(Rect {
                            size: in_res.size(),
                            origin: (0, 0),
                        }),
                        &None,
                        &None,
                        &mut bufs,
                    );
                }
                buffers[*buf_out] = out_buf;
            }
            TransformStep::VSqueeze { buf_in, buf_out } => {
                buffers[*buf_out].allocate_if_needed()?;
                let mut out_buf = buffers[*buf_out].take();
                let mut in_avg = buffers[buf_in[0]].take();
                let mut in_res = buffers[buf_in[1]].take();
                {
                    let mut bufs: Vec<_> = vec![out_buf.borrow_mut()];
                    let in_avg = &in_avg.borrow_mut().data;
                    let in_res = &in_res.borrow_mut().data;
                    super::squeeze::do_vsqueeze_step(
                        &in_avg.get_rect(Rect {
                            size: in_avg.size(),
                            origin: (0, 0),
                        }),
                        &in_res.get_rect(Rect {
                            size: in_res.size(),
                            origin: (0, 0),
                        }),
                        &None,
                        &None,
                        &mut bufs,
                    );
                }
                buffers[*buf_out] = out_buf;
            }
        };

        Ok(())
    }
}
