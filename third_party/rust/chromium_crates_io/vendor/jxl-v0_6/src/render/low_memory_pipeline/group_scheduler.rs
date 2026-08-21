// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::ops::Range;

use crate::error::Result;
use crate::image::{OwnedRawImage, Rect};
use crate::render::LowMemoryRenderPipeline;
use crate::render::buffer_splitter::BufferSplitter;
use crate::render::internal::{ChannelInfo, Stage};
use crate::util::tracing_wrappers::*;

// Finds a small set of rectangles that cover all the "true" values in `ready_mask`,
// and calls `f` on each such rectangle.
fn foreach_ready_rect(
    ready_mask: [bool; 9],
    mut f: impl FnMut(Range<u8>, Range<u8>) -> Result<()>,
) -> Result<()> {
    // x range in middle row
    let xrange = (1 - ready_mask[3] as u8)..(2 + ready_mask[5] as u8);
    let can_extend_top = xrange.clone().all(|x| ready_mask[x as usize]);
    let can_extend_bottom = xrange.clone().all(|x| ready_mask[6 + x as usize]);
    let yrange = (1 - can_extend_top as u8)..(2 + can_extend_bottom as u8);
    f(xrange.clone(), yrange)?;

    if !can_extend_top {
        if ready_mask[1] {
            let xrange = (1 - ready_mask[0] as u8)..(2 + ready_mask[2] as u8);
            f(xrange, 0..1)?;
        } else {
            if ready_mask[0] {
                f(0..1, 0..1)?;
            }
            if ready_mask[2] {
                f(2..3, 0..1)?;
            }
        }
    } else {
        if ready_mask[0] && !xrange.contains(&0) {
            f(0..1, 0..1)?;
        }
        if ready_mask[2] && !xrange.contains(&2) {
            f(2..3, 0..1)?;
        }
    }

    if !can_extend_bottom {
        if ready_mask[7] {
            let xrange = (1 - ready_mask[6] as u8)..(2 + ready_mask[8] as u8);
            f(xrange, 2..3)?;
        } else {
            if ready_mask[6] {
                f(0..1, 2..3)?;
            }
            if ready_mask[8] {
                f(2..3, 2..3)?;
            }
        }
    } else {
        if ready_mask[6] && !xrange.contains(&0) {
            f(0..1, 2..3)?;
        }
        if ready_mask[8] && !xrange.contains(&2) {
            f(2..3, 2..3)?;
        }
    }

    Ok(())
}

fn ready_image_area(
    group_rect: Rect,
    group_position: (usize, usize),
    group_count: (usize, usize),
    input_size: (usize, usize),
    border_size: (usize, usize),
    xrange: Range<u8>,
    yrange: Range<u8>,
) -> Option<Rect> {
    let (gx, gy) = group_position;
    let y0 = match (gy == 0, yrange.start) {
        (true, 0) => group_rect.origin.1,
        (false, 0) => group_rect.origin.1 - border_size.1,
        (_, 1) => group_rect.origin.1 + border_size.1,
        // (_, 2)
        _ => group_rect.end().1 - border_size.1,
    };
    let x0 = match (gx == 0, xrange.start) {
        (true, 0) => group_rect.origin.0,
        (false, 0) => group_rect.origin.0 - border_size.0,
        (_, 1) => group_rect.origin.0 + border_size.0,
        // (_, 2)
        _ => group_rect.end().0 - border_size.0,
    };

    let y1 = match (gy + 1 == group_count.1, yrange.end) {
        (true, 3) => group_rect.end().1,
        (false, 3) => group_rect.end().1 + border_size.1,
        (_, 2) => group_rect.end().1 - border_size.1,
        // (_, 1)
        _ => group_rect.origin.1 + border_size.1,
    }
    .min(input_size.1);

    let x1 = match (gx + 1 == group_count.0, xrange.end) {
        (true, 3) => group_rect.end().0,
        (false, 3) => group_rect.end().0 + border_size.0,
        (_, 2) => group_rect.end().0 - border_size.0,
        // (_, 1)
        _ => group_rect.origin.0 + border_size.0,
    }
    .min(input_size.0);

    (x1 >= x0 && y1 >= y0).then_some(Rect {
        origin: (x0, y0),
        size: (x1 - x0, y1 - y0),
    })
}

impl LowMemoryRenderPipeline {
    pub(super) fn maybe_get_scratch_buffer(
        &self,
        channel: usize,
        kind: usize,
    ) -> Option<OwnedRawImage> {
        self.scratch_channel_buffers
            .try_lock()
            .ok()
            .and_then(|mut x| x[channel * 3 + kind].pop())
    }

    fn store_scratch_buffer(&self, channel: usize, kind: usize, image: OwnedRawImage) {
        let Some(mut buf) = self.scratch_channel_buffers.try_lock().ok() else {
            return;
        };
        if kind == 0
            && let Some(s) = self.group_scratch_buffers_limit
            && buf[channel * 3].len() >= s
        {
            // We are going over the limit of group-sized scratch buffers for
            // this channel - avoid storing the buffer.
            return;
        }
        buf[channel * 3 + kind].push(image)
    }

    pub(super) fn render_with_new_group(
        &self,
        g: usize,
        buffer_splitter: &BufferSplitter,
    ) -> Result<()> {
        let buf = self.input_buffers.get(g);
        let (gx, gy) = self.shared.group_position(g);
        debug!("new data ready for group {gx},{gy}");

        // Prepare output buffers for the group.
        let (origin, size) = if let Some(e) = self.shared.extend_stage_index {
            let Stage::Extend(e) = &self.shared.stages[e] else {
                unreachable!("extend stage is not an extend stage");
            };
            (e.frame_origin, e.image_size)
        } else {
            ((0, 0), self.shared.input_size)
        };
        let gsz = 1 << self.shared.log_group_size;
        let group_rect = Rect {
            size: (gsz, gsz),
            origin: (gsz * gx, gsz * gy),
        }
        .clip(self.shared.input_size);

        for c in 0..self.shared.num_channels() {
            if !self.shared.channel_is_used[c] {
                continue;
            }
            let (bx, by) = self.border_size;
            let (sx, sy) = buf.data[c]
                .try_read()
                .unwrap()
                .as_ref()
                .unwrap()
                .byte_size();
            let ChannelInfo {
                ty,
                downsample: (dx, dy),
            } = self.shared.channel_info[0][c];
            let ty = ty.unwrap();
            let bx = bx >> dx;
            let by = by >> dy;
            let mut topbottom = if let Some(b) = buf.topbottom[c].try_write().unwrap().take() {
                b
            } else if let Some(b) = self.maybe_get_scratch_buffer(c, 1) {
                b
            } else {
                let height = 4 * by;
                let width = (1 << self.shared.log_group_size) * ty.size();
                OwnedRawImage::new_zeroed_with_padding((width, height), (0, 0), (0, 0))?
            };
            let mut leftright = if let Some(b) = buf.leftright[c].try_write().unwrap().take() {
                b
            } else if let Some(b) = self.maybe_get_scratch_buffer(c, 2) {
                b
            } else {
                let height = 1 << self.shared.log_group_size;
                let width = 4 * bx * ty.size();
                OwnedRawImage::new_zeroed_with_padding((width, height), (0, 0), (0, 0))?
            };
            let data = buf.data[c].try_read().unwrap();
            let input = data.as_ref().unwrap();
            if by != 0 {
                for y in 0..(2 * by).min(sy) {
                    topbottom.row_mut(y)[..sx].copy_from_slice(input.row(y));
                    topbottom.row_mut(4 * by - 1 - y)[..sx].copy_from_slice(input.row(sy - y - 1));
                }
            }
            if bx != 0 {
                let cs = (bx * 2 * ty.size()).min(sx);
                for y in 0..sy {
                    let row_out = leftright.row_mut(y);
                    let row_in = input.row(y);
                    row_out[..cs].copy_from_slice(&row_in[..cs]);
                    row_out[4 * bx * ty.size() - cs..].copy_from_slice(&row_in[sx - cs..]);
                }
            }
            *buf.leftright[c].try_write().unwrap() = Some(leftright);
            *buf.topbottom[c].try_write().unwrap() = Some(topbottom);
        }

        let ready_mask = self.input_buffers.mark_ready(g);

        let mut data = self.per_thread_data.get();
        data.ensure_populated(self)?;

        foreach_ready_rect(ready_mask, |xrange, yrange| {
            let Some(image_area) = ready_image_area(
                group_rect,
                (gx, gy),
                self.shared.group_count,
                self.shared.input_size,
                self.border_size,
                xrange,
                yrange,
            ) else {
                return Ok(());
            };

            let mut local_buffers = buffer_splitter.get_local_buffers(
                &self.save_buffer_info,
                image_area,
                false,
                self.shared.input_size,
                size,
                origin,
            );

            self.render_group(&mut data, (gx, gy), image_area, &mut local_buffers)?;
            Ok(())
        })?;

        self.input_buffers
            .mark_done(g, &self.shared, |channel, kind, image| {
                self.store_scratch_buffer(channel, kind, image)
            });

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_foreach_ready_rect() {
        for i in 0..512 {
            let mut ready_mask = [false; 9];
            for j in 0..9 {
                if (i >> j) & 1 == 1 {
                    ready_mask[j] = true;
                }
            }
            if !ready_mask[4] {
                continue;
            }

            let mut covered = [false; 9];
            foreach_ready_rect(ready_mask, |xr, yr| {
                for y in yr {
                    for x in xr.clone() {
                        let idx = (y as usize) * 3 + (x as usize);
                        assert!(
                            ready_mask[idx],
                            "Covered not ready index {} in mask {:?} (x={}, y={})",
                            idx, ready_mask, x, y
                        );
                        assert!(
                            !covered[idx],
                            "Double coverage of index {} in mask {:?}",
                            idx, ready_mask
                        );
                        covered[idx] = true;
                    }
                }
                Ok(())
            })
            .unwrap();

            for j in 0..9 {
                if ready_mask[j] {
                    assert!(
                        covered[j],
                        "Failed to cover index {} in mask {:?}",
                        j, ready_mask
                    );
                }
            }
        }
    }

    #[test]
    fn test_ready_image_area_clips_tiny_edge_group() {
        let area = ready_image_area(
            Rect {
                origin: (512, 512),
                size: (8, 8),
            },
            (1, 1),
            (2, 2),
            (520, 520),
            (18, 18),
            0..1,
            0..1,
        );
        assert_eq!(
            area,
            Some(Rect {
                origin: (494, 494),
                size: (26, 26),
            })
        );
    }
}
