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

pub(super) struct InputBuffer {
    // One buffer per channel.
    pub(super) data: Vec<Option<OwnedRawImage>>,
    // Storage for left/right borders. Includes corners.
    pub(super) leftright: Vec<Option<OwnedRawImage>>,
    // Storage for top/bottom borders. Includes corners.
    pub(super) topbottom: Vec<Option<OwnedRawImage>>,
    // Number of ready channels in the current pass.
    ready_channels: usize,
    pub(super) is_ready: bool,
    num_completed_groups_3x3: usize,
}

impl InputBuffer {
    pub(super) fn set_buffer(&mut self, chan: usize, buf: OwnedRawImage) {
        assert!(self.data[chan].is_none());
        self.data[chan] = Some(buf);
        self.ready_channels += 1;
    }

    pub(super) fn new(num_channels: usize) -> Self {
        let b = || (0..num_channels).map(|_| None).collect();
        Self {
            data: b(),
            leftright: b(),
            topbottom: b(),
            ready_channels: 0,
            is_ready: false,
            num_completed_groups_3x3: 0,
        }
    }
}

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

impl LowMemoryRenderPipeline {
    pub(super) fn maybe_get_scratch_buffer(
        &mut self,
        channel: usize,
        kind: usize,
    ) -> Option<OwnedRawImage> {
        self.scratch_channel_buffers[channel * 3 + kind].pop()
    }

    fn store_scratch_buffer(&mut self, channel: usize, kind: usize, image: OwnedRawImage) {
        self.scratch_channel_buffers[channel * 3 + kind].push(image)
    }

    pub(super) fn render_with_new_group(
        &mut self,
        g: usize,
        buffer_splitter: &mut BufferSplitter,
    ) -> Result<()> {
        let buf = &mut self.input_buffers[g];
        assert!(buf.ready_channels <= self.shared.num_used_channels());
        if buf.ready_channels != self.shared.num_used_channels() {
            return Ok(());
        }
        buf.ready_channels = 0;
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

        {
            for c in 0..self.shared.num_channels() {
                if !self.shared.channel_is_used[c] {
                    continue;
                }
                let (bx, by) = self.border_size;
                let (sx, sy) = self.input_buffers[g].data[c].as_ref().unwrap().byte_size();
                let ChannelInfo {
                    ty,
                    downsample: (dx, dy),
                } = self.shared.channel_info[0][c];
                let ty = ty.unwrap();
                let bx = bx >> dx;
                let by = by >> dy;
                let mut topbottom = if let Some(b) = self.input_buffers[g].topbottom[c].take() {
                    b
                } else if let Some(b) = self.maybe_get_scratch_buffer(c, 1) {
                    b
                } else {
                    let height = 4 * by;
                    let width = (1 << self.shared.log_group_size) * ty.size();
                    OwnedRawImage::new_zeroed_with_padding((width, height), (0, 0), (0, 0))?
                };
                let mut leftright = if let Some(b) = self.input_buffers[g].leftright[c].take() {
                    b
                } else if let Some(b) = self.maybe_get_scratch_buffer(c, 2) {
                    b
                } else {
                    let height = 1 << self.shared.log_group_size;
                    let width = 4 * bx * ty.size();
                    OwnedRawImage::new_zeroed_with_padding((width, height), (0, 0), (0, 0))?
                };
                let input = self.input_buffers[g].data[c].as_ref().unwrap();
                if by != 0 {
                    for y in 0..(2 * by).min(sy) {
                        topbottom.row_mut(y)[..sx].copy_from_slice(input.row(y));
                        topbottom.row_mut(4 * by - 1 - y)[..sx]
                            .copy_from_slice(input.row(sy - y - 1));
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
                self.input_buffers[g].leftright[c] = Some(leftright);
                self.input_buffers[g].topbottom[c] = Some(topbottom);
            }
            self.input_buffers[g].is_ready = true;
        }

        let gxm1 = gx.saturating_sub(1);
        let gym1 = gy.saturating_sub(1);
        let gxp1 = (gx + 1).min(self.shared.group_count.0 - 1);
        let gyp1 = (gy + 1).min(self.shared.group_count.1 - 1);
        let gw = self.shared.group_count.0;
        // TODO(veluca): this code probably needs to be adapted for multithreading.
        let mut ready_mask = [
            self.input_buffers[gym1 * gw + gxm1].is_ready,
            self.input_buffers[gym1 * gw + gx].is_ready,
            self.input_buffers[gym1 * gw + gxp1].is_ready,
            self.input_buffers[gy * gw + gxm1].is_ready,
            self.input_buffers[gy * gw + gx].is_ready, // should be guaranteed to be 1.
            self.input_buffers[gy * gw + gxp1].is_ready,
            self.input_buffers[gyp1 * gw + gxm1].is_ready,
            self.input_buffers[gyp1 * gw + gx].is_ready,
            self.input_buffers[gyp1 * gw + gxp1].is_ready,
        ];
        // We can only render a corner if we have all the 4 adjacent groups. Thus, mask out corners if
        // the corresponding side buffers are not ready.
        ready_mask[0] &= ready_mask[1];
        ready_mask[0] &= ready_mask[3];
        ready_mask[2] &= ready_mask[1];
        ready_mask[2] &= ready_mask[5];
        ready_mask[6] &= ready_mask[3];
        ready_mask[6] &= ready_mask[7];
        ready_mask[8] &= ready_mask[5];
        ready_mask[8] &= ready_mask[7];

        foreach_ready_rect(ready_mask, |xrange, yrange| {
            let y0 = match (gy == 0, yrange.start) {
                (true, 0) => group_rect.origin.1,
                (false, 0) => group_rect.origin.1 - self.border_size.1,
                (_, 1) => group_rect.origin.1 + self.border_size.1,
                // (_, 2)
                _ => group_rect.end().1 - self.border_size.1,
            };
            let x0 = match (gx == 0, xrange.start) {
                (true, 0) => group_rect.origin.0,
                (false, 0) => group_rect.origin.0 - self.border_size.0,
                (_, 1) => group_rect.origin.0 + self.border_size.0,
                // (_, 2)
                _ => group_rect.end().0 - self.border_size.0,
            };

            let y1 = match (gy + 1 == self.shared.group_count.1, yrange.end) {
                (true, 3) => group_rect.end().1,
                (false, 3) => group_rect.end().1 + self.border_size.1,
                (_, 2) => group_rect.end().1 - self.border_size.1,
                // (_, 1)
                _ => group_rect.origin.1 + self.border_size.1,
            };

            let x1 = match (gx + 1 == self.shared.group_count.0, xrange.end) {
                (true, 3) => group_rect.end().0,
                (false, 3) => group_rect.end().0 + self.border_size.0,
                (_, 2) => group_rect.end().0 - self.border_size.0,
                // (_, 1)
                _ => group_rect.origin.0 + self.border_size.0,
            };

            let image_area = Rect {
                origin: (x0, y0),
                size: (x1 - x0, y1 - y0),
            };

            let mut local_buffers = buffer_splitter.get_local_buffers(
                &self.save_buffer_info,
                image_area,
                false,
                self.shared.input_size,
                size,
                origin,
            );

            self.render_group((gx, gy), image_area, &mut local_buffers)?;
            Ok(())
        })?;

        for c in 0..self.input_buffers[g].data.len() {
            if let Some(b) = std::mem::take(&mut self.input_buffers[g].data[c]) {
                self.store_scratch_buffer(c, 0, b);
            }
        }

        // Clear border buffers that will not be used again.
        // This is certainly the case if *all* the groups in the 3x3 group area around
        // the current group are complete.
        if self.shared.group_chan_complete[g].iter().all(|x| *x) {
            for g in [
                gym1 * gw + gxm1,
                gym1 * gw + gx,
                gym1 * gw + gxp1,
                gy * gw + gxm1,
                gy * gw + gx,
                gy * gw + gxp1,
                gyp1 * gw + gxm1,
                gyp1 * gw + gx,
                gyp1 * gw + gxp1,
            ] {
                self.input_buffers[g].num_completed_groups_3x3 += 1;
                if self.input_buffers[g].num_completed_groups_3x3 != 9 {
                    continue;
                }
                for c in 0..self.input_buffers[g].data.len() {
                    if let Some(b) = std::mem::take(&mut self.input_buffers[g].topbottom[c]) {
                        self.store_scratch_buffer(c, 1, b);
                    }
                    if let Some(b) = std::mem::take(&mut self.input_buffers[g].leftright[c]) {
                        self.store_scratch_buffer(c, 2, b);
                    }
                }
            }
        }

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
}
