// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use apply::TransformStep;
pub use apply::TransformStepChunk;
use num_derive::FromPrimitive;

use crate::frame::modular::ModularBuffer;
use crate::headers::frame_header::FrameHeader;
use crate::util::AtomicRefCell;
use crate::util::tracing_wrappers::*;

use super::{ModularBufferInfo, ModularGridKind, Predictor};

pub(super) mod apply;
mod palette;
mod rct;
mod squeeze;

#[derive(Debug, FromPrimitive, PartialEq, Clone, Copy)]
pub enum RctPermutation {
    Rgb = 0,
    Gbr = 1,
    Brg = 2,
    Rbg = 3,
    Grb = 4,
    Bgr = 5,
}

#[derive(Debug, FromPrimitive, PartialEq, Clone, Copy)]
pub enum RctOp {
    Noop = 0,
    AddFirstToThird = 1,
    AddFirstToSecond = 2,
    AddFirstToSecondAndThird = 3,
    AddAvgToSecond = 4,
    AddFirstToThirdAndAvgToSecond = 5,
    YCoCg = 6,
}

#[instrument(level = "trace", skip_all, ret)]
pub fn make_grids(
    frame_header: &FrameHeader,
    transform_steps: Vec<TransformStep>,
    section_buffer_indices: &[Vec<usize>],
    buffer_info: &mut Vec<ModularBufferInfo>,
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
            TransformStep::HSqueeze { buf_in, buf_out }
            | TransformStep::VSqueeze { buf_in, buf_out } => {
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
        let is_output = g.info.output_channel_idx >= 0;
        g.buffer_grid = get_grid_indices(g.grid_shape)
            .map(|(x, y)| ModularBuffer {
                data: AtomicRefCell::new(None),
                remaining_uses: if is_output { 1 } else { 0 },
                used_by_transforms: vec![],
                size: g
                    .get_grid_rect(frame_header, g.grid_kind, (x as usize, y as usize))
                    .size,
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
                incomplete_deps: 0,
            });
            ts
        };

    let add_grid_use = |ts: usize,
                        input_buffer_idx: usize,
                        output_grid_kind: ModularGridKind,
                        output_grid_shape: (usize, usize),
                        output_grid_pos: (isize, isize),
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
        if !buffer_info[input_buffer_idx].buffer_grid[input_grid_pos]
            .used_by_transforms
            .contains(&ts)
        {
            buffer_info[input_buffer_idx].buffer_grid[input_grid_pos].remaining_uses += 1;
            buffer_info[input_buffer_idx].buffer_grid[input_grid_pos]
                .used_by_transforms
                .push(ts);
            grid_transform_steps[ts].incomplete_deps += 1;
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
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    add_grid_use(
                        ts,
                        *buf_in,
                        out_kind,
                        out_shape,
                        (x, y),
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    let offsets = match predictor {
                        Predictor::Zero => [].as_slice(),
                        _ => &[(0, -1), (-1, 0), (-1, -1)],
                    };
                    for (dx, dy) in offsets {
                        for out in buf_out.iter() {
                            add_grid_use(
                                ts,
                                *out,
                                out_kind,
                                out_shape,
                                (x + dx, y + dy),
                                &mut grid_transform_steps,
                                buffer_info,
                            );
                        }
                    }
                }
            }
            TransformStep::HSqueeze { buf_in, buf_out } => {
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
                            &mut grid_transform_steps,
                            buffer_info,
                        );
                    }
                    // Next average
                    add_grid_use(
                        ts,
                        buf_in[0],
                        out_kind,
                        out_shape,
                        (x + 1, y),
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    // Previous decoded
                    add_grid_use(
                        ts,
                        *buf_out,
                        out_kind,
                        out_shape,
                        (x - 1, y),
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                }
            }
            TransformStep::VSqueeze { buf_in, buf_out } => {
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
                            &mut grid_transform_steps,
                            buffer_info,
                        );
                    }
                    // Next average
                    add_grid_use(
                        ts,
                        buf_in[0],
                        out_kind,
                        out_shape,
                        (x, y + 1),
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                    // Previous decoded
                    add_grid_use(
                        ts,
                        *buf_out,
                        out_kind,
                        out_shape,
                        (x, y - 1),
                        &mut grid_transform_steps,
                        buffer_info,
                    );
                }
            }
        }
    }

    trace!(?grid_transform_steps, ?buffer_info);

    grid_transform_steps
}
