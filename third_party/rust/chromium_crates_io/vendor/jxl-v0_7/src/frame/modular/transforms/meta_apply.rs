// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use num_traits::FromPrimitive;

use super::{RctOp, RctPermutation};
use crate::error::{Error, Result};
use crate::frame::modular::buffers::NeededBorders;
use crate::frame::modular::transforms::step::{TransformStep, TransformStepChunk};
use crate::frame::modular::{
    ChannelInfo, ModularBuffer, ModularBufferInfo, ModularGridKind, ModularStorage, Predictor,
};
use crate::headers::frame_header::FrameHeader;
use crate::headers::modular::TransformId;
use crate::headers::{self};
use crate::image::Rect;
use crate::util::ShiftRightCeil;
use crate::util::sync::atomic::AtomicUsize;
use crate::util::tracing_wrappers::*;

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

#[allow(clippy::too_many_arguments)]
pub(super) fn meta_apply_single_transform(
    transform: &headers::modular::Transform,
    header: &headers::modular::GroupHeader,
    channels: &mut Vec<(usize, ChannelInfo)>,
    transform_steps: &mut Vec<TransformStep>,
    mut add_transform_buffer: impl FnMut(ChannelInfo, String) -> usize,
    cumulative_palette_samples: &mut usize,
    max_palette_samples: usize,
    max_channels: usize,
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
                info.output_channel_idx = None;
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
                        output_channel_idx: None,
                        shift: new_shift,
                        size: new_size_0,
                        bit_depth: chan.bit_depth,
                        followed_by_palette: chan.followed_by_palette,
                    };
                    let buf_0 = add_transform_buffer(
                        new_0,
                        format!("Squeezed channel, original channel {}", begin_channel + ic),
                    );
                    let new_1 = ChannelInfo {
                        output_channel_idx: None,
                        shift: new_shift,
                        size: new_size_1,
                        bit_depth: chan.bit_depth,
                        followed_by_palette: chan.followed_by_palette,
                    };
                    let buf_1 = add_transform_buffer(
                        new_1,
                        format!("Squeeze residual, original channel {}", begin_channel + ic),
                    );
                    let buf_out = channels[begin_channel + ic].0;
                    if horizontal {
                        transform_steps.push(TransformStep::HSqueeze {
                            buf_in: [buf_0, buf_1],
                            buf_out,
                            upsample: None,
                        });
                    } else {
                        transform_steps.push(TransformStep::VSqueeze {
                            buf_in: [buf_0, buf_1],
                            buf_out,
                            upsample: None,
                        });
                    }
                    channels[begin_channel + ic] = (buf_0, new_0);
                    channels.insert(new_chan_offset + ic, (buf_1, new_1));
                    if channels.len() > max_channels {
                        return Err(Error::TooManyModularChannels(channels.len(), max_channels));
                    }
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

            let palette_samples = (num_colors + num_deltas).saturating_mul(num_channels);
            *cumulative_palette_samples =
                cumulative_palette_samples.saturating_add(palette_samples);
            if *cumulative_palette_samples > max_palette_samples {
                return Err(Error::PaletteTooLarge(
                    *cumulative_palette_samples,
                    max_palette_samples,
                ));
            }

            // We already checked the bit_depth for all channels from `begin_channel` is
            // equal in the line above.
            let bit_depth = channels[begin_channel].1.bit_depth;
            let pchan_info = ChannelInfo {
                output_channel_idx: None,
                shift: None,
                size: (num_colors + num_deltas, num_channels),
                bit_depth,
                followed_by_palette: false,
            };
            let pchan = add_transform_buffer(
                pchan_info,
                format!(
                    "Palette for palette transform starting at channel {begin_channel} with {num_channels} channels",
                ),
            );
            let mut inchan_info = channels[begin_channel].1;
            inchan_info.output_channel_idx = None;
            inchan_info.followed_by_palette = true;
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
                num_deltas,
                predictor: pred,
                wp_header: header.wp_header.clone(),
            });
            channels.drain(begin_channel + 1..begin_channel + num_channels);
            channels[begin_channel] = (inchan, inchan_info);
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
    max_palette_samples: usize,
    max_channels: usize,
    storage: ModularStorage,
) -> Result<(Vec<ModularBufferInfo>, Vec<TransformStep>)> {
    if channels.len() > max_channels {
        return Err(Error::TooManyModularChannels(channels.len(), max_channels));
    }

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
            storage,
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
            storage,
        });
        buffer_info.len() - 1
    };

    let mut cumulative_palette_samples = 0;
    // Apply transforms to the channel list.
    for transform in &header.transforms {
        meta_apply_single_transform(
            transform,
            header,
            &mut channels,
            &mut transform_steps,
            &mut add_transform_buffer,
            &mut cumulative_palette_samples,
            max_palette_samples,
            max_channels,
        )?;
        if channels.len() > max_channels {
            return Err(Error::TooManyModularChannels(channels.len(), max_channels));
        }
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

#[instrument(level = "trace", skip_all, ret)]
pub fn make_grids(
    frame_header: &FrameHeader,
    transform_steps: Vec<TransformStep>,
    section_buffer_indices: &[Vec<usize>],
    buffer_info: &mut Vec<ModularBufferInfo>,
    color_channels: usize,
) -> Vec<TransformStepChunk> {
    // Initialize grid sizes, starting from coded channels.
    for i in section_buffer_indices[1].iter() {
        buffer_info[*i].grid_kind = ModularGridKind::Lf;
    }
    for buffer_indices in section_buffer_indices.iter().skip(2) {
        for i in buffer_indices.iter() {
            buffer_info[*i].grid_kind = ModularGridKind::Hf;
        }
    }

    trace!(?buffer_info, "post set grid kind for coded channels");

    // Transforms can be un-applied in the opposite order they appear with in the array,
    // so we can use that information to propagate grid kinds.

    for step in transform_steps.iter().rev() {
        match step {
            TransformStep::Rct {
                buf_in, buf_out, ..
            } => {
                let grid_in = buffer_info[buf_in[0]].grid_kind;
                for i in 0..3 {
                    assert_eq!(grid_in, buffer_info[buf_in[i]].grid_kind);
                }
                for i in 0..3 {
                    buffer_info[buf_out[i]].grid_kind = grid_in;
                }
            }
            TransformStep::Palette {
                buf_in, buf_out, ..
            } => {
                for buf in buf_out.iter() {
                    buffer_info[*buf].grid_kind = buffer_info[*buf_in].grid_kind;
                }
            }
            TransformStep::HSqueeze {
                buf_in, buf_out, ..
            }
            | TransformStep::VSqueeze {
                buf_in, buf_out, ..
            } => {
                let mut grid_kind = buffer_info[buf_in[0]]
                    .grid_kind
                    .max(buffer_info[buf_in[1]].grid_kind);
                if grid_kind == ModularGridKind::None
                    && !buffer_info[*buf_out]
                        .info
                        .is_meta_or_small(frame_header.group_dim())
                {
                    grid_kind = ModularGridKind::Hf;
                }
                buffer_info[*buf_out].grid_kind = grid_kind;
            }
            TransformStep::Output { .. } => {
                unreachable!()
            }
        }
    }

    // Set grid shapes.
    for buf in buffer_info.iter_mut() {
        buf.grid_shape = buf.grid_kind.grid_shape(frame_header);
    }

    trace!(?buffer_info, "post propagate grid kind");

    let get_grid_indices = |shape: (usize, usize)| {
        (0..shape.1).flat_map(move |y| (0..shape.0).map(move |x| (x as isize, y as isize)))
    };

    // Create grids.
    for g in buffer_info.iter_mut() {
        g.buffer_grid = get_grid_indices(g.grid_shape)
            .map(|(x, y)| {
                ModularBuffer::new(
                    g.get_grid_rect(frame_header, g.grid_kind, (x as usize, y as usize))
                        .size,
                )
            })
            .collect();
    }

    trace!(?buffer_info, "with grids");

    let add_transform_step =
        |transform: &TransformStep,
         grid_pos: (isize, isize),
         grid_transform_steps: &mut Vec<TransformStepChunk>| {
            let ts = grid_transform_steps.len();
            grid_transform_steps.push(TransformStepChunk {
                step: transform.clone(),
                grid_pos: (grid_pos.0 as usize, grid_pos.1 as usize),
                missing_final_deps: 0,
                missing_deps: AtomicUsize::new(0),
            });
            ts
        };

    let add_grid_use = |ts: usize,
                        input_buffer_idx: usize,
                        output_grid_kind: ModularGridKind,
                        output_grid_shape: (usize, usize),
                        output_grid_pos: (isize, isize),
                        needed_borders: NeededBorders,
                        grid_transform_steps: &mut Vec<TransformStepChunk>,
                        buffer_info: &mut Vec<ModularBufferInfo>| {
        let output_grid_size = (output_grid_shape.0 as isize, output_grid_shape.1 as isize);
        if output_grid_pos.0 < 0
            || output_grid_pos.0 >= output_grid_size.0
            || output_grid_pos.1 < 0
            || output_grid_pos.1 >= output_grid_size.1
        {
            // Skip adding uses of non-existent grid positions.
            return;
        }
        let output_grid_pos = (output_grid_pos.0 as usize, output_grid_pos.1 as usize);
        let input_grid_pos =
            buffer_info[input_buffer_idx].get_grid_idx(output_grid_kind, output_grid_pos);
        let grid = &mut buffer_info[input_buffer_idx].buffer_grid[input_grid_pos];
        grid.needed_borders.topbottom |= needed_borders.topbottom;
        grid.needed_borders.leftright |= needed_borders.leftright;
        if needed_borders.is_empty() {
            grid.full_uses_count_final += 1;
        }
        if !grid.used_by_transforms_final.contains(&ts) {
            grid_transform_steps[ts].missing_final_deps += 1;
            grid.used_by_transforms_final.push(ts);
        }
    };

    // Add grid-ed transforms.
    let mut grid_transform_steps = vec![];

    for transform in transform_steps {
        match &transform {
            TransformStep::Rct {
                buf_in, buf_out, ..
            } => {
                // Easy case: we just depend on the 3 input buffers in the same location.
                let out_kind = buffer_info[buf_out[0]].grid_kind;
                let out_shape = buffer_info[buf_out[0]].grid_shape;
                for (x, y) in get_grid_indices(out_shape) {
                    let ts = add_transform_step(&transform, (x, y), &mut grid_transform_steps);
                    for bin in buf_in {
                        add_grid_use(
                            ts,
                            *bin,
                            out_kind,
                            out_shape,
                            (x, y),
                            NeededBorders::NONE,
                            &mut grid_transform_steps,
                            buffer_info,
                        );
                    }
                }
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                buf_out,
                predictor,
                ..
            } if predictor.requires_full_row() => {
                // Delta palette with AverageAll or Weighted. Those are special, because we can
                // only make progress one full image row at a time (since we need decoded values
                // from the previous row or two rows).
                let out_kind = buffer_info[buf_out[0]].grid_kind;
                let out_shape = buffer_info[buf_out[0]].grid_shape;
                let mut ts = 0;
                for (x, y) in get_grid_indices(out_shape) {
                    if x == 0 {
                        ts = add_transform_step(&transform, (x, y), &mut grid_transform_steps);
                        add_grid_use(
                            ts,
                            *buf_pal,
                            out_kind,
                            out_shape,
                            (x, y),
                            NeededBorders::NONE,
                            &mut grid_transform_steps,
                            buffer_info,
                        );
                    }
                    add_grid_use(
                        ts,
                        *buf_in,
                        out_kind,
                        out_shape,
                        (x, y),
                        NeededBorders::NONE,
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    for out in buf_out.iter() {
                        add_grid_use(
                            ts,
                            *out,
                            out_kind,
                            out_shape,
                            (x, y - 1),
                            NeededBorders::TOPBOTTOM,
                            &mut grid_transform_steps,
                            buffer_info,
                        );
                    }
                }
            }
            TransformStep::Palette {
                buf_in,
                buf_pal,
                buf_out,
                predictor,
                ..
            } => {
                // Maybe-delta palette: we depend on the palette and the input buffer in the same
                // location. We may also depend on other grid positions in the output buffer,
                // according to the used predictor.
                let out_kind = buffer_info[buf_out[0]].grid_kind;
                let out_shape = buffer_info[buf_out[0]].grid_shape;
                for (x, y) in get_grid_indices(out_shape) {
                    let ts = add_transform_step(&transform, (x, y), &mut grid_transform_steps);
                    add_grid_use(
                        ts,
                        *buf_pal,
                        out_kind,
                        out_shape,
                        (x, y),
                        NeededBorders::NONE,
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    add_grid_use(
                        ts,
                        *buf_in,
                        out_kind,
                        out_shape,
                        (x, y),
                        NeededBorders::NONE,
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    let offsets = match predictor {
                        Predictor::Zero => [].as_slice(),
                        _ => &[(0, -1), (-1, 0), (-1, -1)],
                    };
                    for &(dx, dy) in offsets {
                        let borders = NeededBorders {
                            topbottom: dy != 0,
                            leftright: dx != 0 && dy == 0,
                        };
                        for out in buf_out.iter() {
                            add_grid_use(
                                ts,
                                *out,
                                out_kind,
                                out_shape,
                                (x + dx, y + dy),
                                borders,
                                &mut grid_transform_steps,
                                buffer_info,
                            );
                        }
                    }
                }
            }
            TransformStep::HSqueeze {
                buf_in, buf_out, ..
            }
            | TransformStep::VSqueeze {
                buf_in, buf_out, ..
            } => {
                let vertical = matches!(transform, TransformStep::VSqueeze { .. });
                let out_kind = buffer_info[*buf_out].grid_kind;
                let out_shape = buffer_info[*buf_out].grid_shape;
                for (x, y) in get_grid_indices(out_shape) {
                    let ts = add_transform_step(&transform, (x, y), &mut grid_transform_steps);
                    // Average and residuals from the same position
                    for bin in buf_in {
                        add_grid_use(
                            ts,
                            *bin,
                            out_kind,
                            out_shape,
                            (x, y),
                            NeededBorders::NONE,
                            &mut grid_transform_steps,
                            buffer_info,
                        );
                    }
                    // 3x3 neighbor grid positions for upsampling & next average
                    let cur_grid =
                        buffer_info[buf_in[0]].get_grid_idx(out_kind, (x as usize, y as usize));
                    for dx in -1..=1 {
                        for dy in -1..=1 {
                            if dx == 0 && dy == 0 {
                                continue;
                            }
                            let target_x = (x + dx).clamp(0, out_shape.0 as isize - 1);
                            let target_y = (y + dy).clamp(0, out_shape.1 as isize - 1);
                            let target_grid = buffer_info[buf_in[0]]
                                .get_grid_idx(out_kind, (target_x as usize, target_y as usize));
                            if target_grid != cur_grid {
                                let borders = NeededBorders {
                                    topbottom: dy != 0,
                                    leftright: dx != 0 && dy == 0,
                                };
                                add_grid_use(
                                    ts,
                                    buf_in[0],
                                    out_kind,
                                    out_shape,
                                    (x + dx, y + dy),
                                    borders,
                                    &mut grid_transform_steps,
                                    buffer_info,
                                );
                            }
                        }
                    }
                    // Previous decoded
                    let prev_pos = if vertical { (x, y - 1) } else { (x - 1, y) };
                    let prev_borders = if vertical {
                        NeededBorders::TOPBOTTOM
                    } else {
                        NeededBorders::LEFTRIGHT
                    };
                    add_grid_use(
                        ts,
                        *buf_out,
                        out_kind,
                        out_shape,
                        prev_pos,
                        prev_borders,
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                }
            }
            TransformStep::Output { .. } => {
                unreachable!()
            }
        }
    }

    // Write on transform outputs which step produces them.
    for (ts, step) in grid_transform_steps.iter().enumerate() {
        for &(buf, grid) in step.outputs(buffer_info).iter() {
            buffer_info[buf].buffer_grid[grid].produced_by_step = Some(ts);
        }
    }

    // Add "output" transform steps.
    // TODO(veluca): filter this by pipeline_used_channels.
    for (idx, bi) in buffer_info.iter_mut().enumerate() {
        if let Some(chan) = bi.info.output_channel_idx {
            let (shift_x, shift_y) = bi.info.shift.unwrap_or((0, 0));
            let gsx = frame_header.group_dim().shrc(shift_x);
            let gsy = frame_header.group_dim().shrc(shift_y);
            // If the channel has a grid size of Hf groups, then we can just output
            // the entire buffer. Otherwise, we need to copy the region of the image
            // that corresponds to a HF group.
            let grid_dim = match bi.grid_kind {
                ModularGridKind::None => frame_header.size().0.max(frame_header.size().1),
                ModularGridKind::Hf => frame_header.group_dim(),
                ModularGridKind::Lf => frame_header.lf_group_dim(),
            };
            let gtx = grid_dim.shrc(shift_x);
            let gty = grid_dim.shrc(shift_y);

            let group_shape = frame_header.size_groups();
            for y in 0..group_shape.1 {
                let gy = y * gsy / gty;
                let gyo = y * gsy - gy * gty;
                for x in 0..group_shape.0 {
                    let gx = x * gsx / gtx;
                    let gxo = x * gsx - gx * gtx;
                    let grid = gy * bi.grid_shape.0 + gx;
                    let rect = if gxo == 0 && gyo == 0 && gtx == gsx && gty == gsy {
                        None
                    } else {
                        let rect = Rect {
                            origin: (gxo, gyo),
                            size: (gsx, gsy),
                        };
                        Some(rect.clip(bi.buffer_grid[grid].size))
                    };
                    let chans = if chan == 0 && color_channels == 1 {
                        0..=2
                    } else {
                        chan..=chan
                    };
                    for c in chans {
                        bi.buffer_grid[grid]
                            .used_by_transforms_final
                            .push(grid_transform_steps.len());
                        bi.buffer_grid[grid].full_uses_count_final += 1;
                        grid_transform_steps.push(TransformStepChunk {
                            step: TransformStep::Output {
                                buf_in: idx,
                                rect,
                                group: y * frame_header.size_groups().0 + x,
                                channel: c,
                            },
                            grid_pos: (gx, gy),
                            missing_final_deps: 1,
                            missing_deps: AtomicUsize::new(0),
                        });
                    }
                }
            }
        }
    }

    for bi in buffer_info.iter_mut() {
        for grid in bi.buffer_grid.iter_mut() {
            grid.remaining_final_uses = AtomicUsize::new(grid.used_by_transforms_final.len());
        }
    }

    trace!(?grid_transform_steps, ?buffer_info);

    grid_transform_steps
}
