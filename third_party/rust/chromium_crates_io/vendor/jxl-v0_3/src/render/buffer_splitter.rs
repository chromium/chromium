// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{api::JxlOutputBuffer, headers::Orientation, image::Rect, util::ShiftRightCeil};

// Information for splitting the output buffers.
#[derive(Debug)]
pub(super) struct SaveStageBufferInfo {
    pub(super) downsample: (u8, u8),
    pub(super) orientation: Orientation,
    pub(super) byte_size: usize,
    pub(super) after_extend: bool,
}

/// Data structure responsible for handing out access to portions of the output buffers.
pub struct BufferSplitter<'a, 'b>(&'a mut [Option<JxlOutputBuffer<'b>>]);

impl<'a, 'b> BufferSplitter<'a, 'b> {
    pub fn new(bufs: &'a mut [Option<JxlOutputBuffer<'b>>]) -> Self {
        Self(bufs)
    }

    pub(super) fn get_local_buffers(
        &mut self,
        save_buffer_info: &[Option<SaveStageBufferInfo>],
        rect: Rect,
        outside_current_frame: bool,
        frame_size: (usize, usize),
        full_image_size: (usize, usize),
        frame_origin: (isize, isize),
    ) -> Vec<Option<JxlOutputBuffer<'_>>> {
        let mut local_buffers = vec![];
        let buffers = &mut *self.0;
        local_buffers.reserve(buffers.len());
        for _ in 0..buffers.len() {
            local_buffers.push(None::<JxlOutputBuffer>);
        }
        let rect = if !outside_current_frame {
            rect.clip(frame_size)
        } else {
            rect
        };
        for (i, (info, buf)) in save_buffer_info.iter().zip(buffers.iter_mut()).enumerate() {
            let Some(bi) = info else {
                // We never write to this buffer.
                continue;
            };
            let Some(buf) = buf.as_mut() else {
                // The buffer to write into was not provided.
                continue;
            };
            if outside_current_frame && !bi.after_extend {
                // Before-extend stages do not write to rects outside the current frame.
                continue;
            }
            let mut channel_rect = rect.downsample(bi.downsample);
            if !outside_current_frame {
                let frame_size = (
                    frame_size.0.shrc(bi.downsample.0),
                    frame_size.1.shrc(bi.downsample.1),
                );
                channel_rect = channel_rect.clip(frame_size);
                if bi.after_extend {
                    // clip this rect to its visible area in the full image (in full image coordinates).
                    let origin = (
                        rect.origin.0 as isize + frame_origin.0,
                        rect.origin.1 as isize + frame_origin.1,
                    );
                    let end = (
                        origin.0 + rect.size.0 as isize,
                        origin.1 + rect.size.1 as isize,
                    );
                    let origin = (origin.0.max(0) as usize, origin.1.max(0) as usize);
                    let end = (
                        end.0.min(full_image_size.0 as isize).max(0) as usize,
                        end.1.min(full_image_size.1 as isize).max(0) as usize,
                    );
                    channel_rect = Rect {
                        origin,
                        size: (
                            end.0.saturating_sub(origin.0),
                            end.1.saturating_sub(origin.1),
                        ),
                    };
                }
            }
            if channel_rect.size.0 == 0 || channel_rect.size.1 == 0 {
                // Buffer would be empty anyway.
                continue;
            }
            let channel_rect = bi.orientation.display_rect(channel_rect, full_image_size);
            let channel_rect = channel_rect.to_byte_rect_sz(bi.byte_size);
            local_buffers[i] = Some(buf.rect(channel_rect));
        }
        local_buffers
    }

    pub fn get_full_buffers(&mut self) -> &mut [Option<JxlOutputBuffer<'b>>] {
        &mut *self.0
    }
}
