// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    collections::BTreeSet,
    ops::{Deref, DerefMut},
};

use crate::util::sync::Mutex;

use crate::{
    api::JxlOutputBuffer,
    headers::Orientation,
    image::{Image, ImageDataType, Rect},
    util::{ChannelVec, ShiftRightCeil},
};

pub struct OutputChannelSplitter<'a> {
    // Safety invariant: all the currently-borrowed rects of are stored in
    // `borrowed_rects`.
    buffer: JxlOutputBuffer<'a>,
    borrowed_rects: Mutex<BTreeSet<Rect>>,
}

impl<'a> Drop for OutputChannelSplitter<'a> {
    fn drop(&mut self) {
        assert!(
            self.borrowed_rects
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .is_empty()
        )
    }
}

impl<'a> OutputChannelSplitter<'a> {
    pub fn new(buffer: JxlOutputBuffer<'a>) -> Self {
        Self {
            buffer,
            borrowed_rects: Mutex::new(BTreeSet::new()),
        }
    }

    pub fn from_image<T: ImageDataType>(image: &'a mut Image<T>) -> Self {
        let size = image.size();
        let raw = image
            .get_rect_mut(Rect {
                origin: (0, 0),
                size,
            })
            .into_raw();
        Self::new(JxlOutputBuffer::from_image_rect_mut(raw))
    }

    pub fn borrow_typed_rect<T: ImageDataType>(&self, rect: Rect) -> OutputChannelRef<'a, '_> {
        self.borrow_rect(rect.to_byte_rect(T::DATA_TYPE_ID))
    }

    #[allow(unsafe_code)]
    pub fn borrow_rect(&self, rect: Rect) -> OutputChannelRef<'a, '_> {
        let mut rects = self
            .borrowed_rects
            .lock()
            .unwrap_or_else(|e| e.into_inner());
        for r in rects.iter() {
            assert!(!r.intersects(&rect));
        }
        rects.insert(rect);

        OutputChannelRef {
            s: self,
            r: rect,
            // SAFETY: we just checked that `self.borrowed_rects` does not contain
            // any rects that intersect with the new rect. Since by safety invariant
            // all the currently-borrowed rects are in `self.borrowed_rects`, the
            // new rect does not intersect the old ones.
            buf: Some(unsafe { self.buffer.rect(rect) }),
        }
    }
}

pub struct OutputChannelRef<'a, 'b> {
    s: &'a OutputChannelSplitter<'b>,
    buf: Option<JxlOutputBuffer<'b>>,
    // Safety invariant: `r` is the rect that was used to obtain `buf` from `s`.
    r: Rect,
}

impl<'a, 'b> Deref for OutputChannelRef<'a, 'b> {
    type Target = JxlOutputBuffer<'b>;
    fn deref(&self) -> &Self::Target {
        self.buf.as_ref().unwrap()
    }
}

impl<'a, 'b> DerefMut for OutputChannelRef<'a, 'b> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.buf.as_mut().unwrap()
    }
}

impl<'a, 'b> Drop for OutputChannelRef<'a, 'b> {
    fn drop(&mut self) {
        self.buf = None;
        let mut b = self
            .s
            .borrowed_rects
            .lock()
            .unwrap_or_else(|e| e.into_inner());
        // Safety note: we just dropped the local buffer, and the
        // safety invariant of `self` says that this is the correct rect
        // to remove.
        assert!(b.remove(&self.r));
    }
}

// Information for splitting the output buffers.
#[derive(Debug)]
pub struct SaveStageBufferInfo {
    pub downsample: (u8, u8),
    pub orientation: Orientation,
    pub byte_size: usize,
    pub after_extend: bool,
}

/// Data structure responsible for handing out access to portions of the output buffers.
pub struct BufferSplitter<'a> {
    buffers: ChannelVec<Option<OutputChannelSplitter<'a>>>,
    requested_rects: Mutex<Vec<Rect>>,
}

impl<'a> BufferSplitter<'a> {
    pub fn new(bufs: &'a mut [Option<JxlOutputBuffer<'_>>]) -> Self {
        Self {
            requested_rects: Mutex::new(vec![]),
            buffers: bufs
                .iter_mut()
                .map(|x| {
                    x.as_mut()
                        .map(|x| OutputChannelSplitter::new(JxlOutputBuffer::reborrow(x)))
                })
                .collect(),
        }
    }

    pub(crate) fn get_local_buffers(
        &self,
        save_buffer_info: &[Option<SaveStageBufferInfo>],
        rect: Rect,
        outside_current_frame: bool,
        frame_size: (usize, usize),
        full_image_size: (usize, usize),
        frame_origin: (isize, isize),
    ) -> ChannelVec<Option<OutputChannelRef<'a, '_>>> {
        {
            let mut req = self.requested_rects.lock().unwrap();
            req.push(rect);
        }
        let rect = if !outside_current_frame {
            rect.clip(frame_size)
        } else {
            rect
        };

        save_buffer_info
            .iter()
            .zip(self.buffers.iter())
            .map(|(info, buf)| {
                let Some(bi) = info else {
                    // We never write to this buffer.
                    return None;
                };
                let buf = buf.as_ref()?;
                if outside_current_frame && !bi.after_extend {
                    // Before-extend stages do not write to rects outside the current frame.
                    return None;
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
                    return None;
                }
                let channel_rect = bi.orientation.display_rect(channel_rect, full_image_size);
                let channel_rect = channel_rect.to_byte_rect_sz(bi.byte_size);
                Some(buf.borrow_rect(channel_rect))
            })
            .collect()
    }

    pub fn into_changed_regions(self) -> Vec<Rect> {
        self.requested_rects.into_inner().unwrap()
    }

    #[cfg(test)]
    pub fn get_full_buffers(&self) -> ChannelVec<Option<OutputChannelRef<'a, '_>>> {
        self.buffers
            .iter()
            .map(|x| {
                x.as_ref().map(|x| {
                    x.borrow_rect(Rect {
                        origin: (0, 0),
                        size: x.buffer.byte_size(),
                    })
                })
            })
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{api::JxlOutputBuffer, headers::Orientation, image::Rect};

    #[test]
    fn test_buffer_splitter_basic() {
        let mut empty_bufs: [Option<JxlOutputBuffer>; 0] = [];
        let splitter = BufferSplitter::new(&mut empty_bufs);
        assert!(splitter.get_full_buffers().is_empty());

        let mut raw0 = vec![0u8; 100];
        let mut raw1 = vec![0u8; 200];
        {
            let buf0 = JxlOutputBuffer::new(&mut raw0, 10, 10);
            let buf1 = JxlOutputBuffer::new(&mut raw1, 10, 20);
            let mut bufs = [Some(buf0), Some(buf1), None];
            let splitter = BufferSplitter::new(&mut bufs);

            {
                let mut full = splitter.get_full_buffers();
                assert_eq!(full.len(), 3);
                assert!(full[2].is_none());

                full[0].as_mut().unwrap().row_mut(0)[0] = 42;
                full[1].as_mut().unwrap().row_mut(1)[2] = 99;
            }
        }

        assert_eq!(raw0[0], 42);
        assert_eq!(raw1[20 + 2], 99);
    }

    #[test]
    #[should_panic]
    fn test_buffer_splitter_overlapping_borrows_panic() {
        let mut raw0 = vec![0u8; 400];
        let buf0 = JxlOutputBuffer::new(&mut raw0, 20, 20);
        let mut bufs = [Some(buf0)];
        let splitter = BufferSplitter::new(&mut bufs);

        let info = vec![Some(SaveStageBufferInfo {
            downsample: (0, 0),
            orientation: Orientation::Identity,
            byte_size: 1,
            after_extend: false,
        })];

        let _local1 = splitter.get_local_buffers(
            &info,
            Rect {
                origin: (0, 0),
                size: (10, 10),
            },
            false,
            (20, 20),
            (20, 20),
            (0, 0),
        );

        let _local2 = splitter.get_local_buffers(
            &info,
            Rect {
                origin: (5, 5),
                size: (10, 10),
            },
            false,
            (20, 20),
            (20, 20),
            (0, 0),
        );
    }

    #[cfg(not(any(target_family = "wasm", target_arch = "wasm32")))]
    #[test]
    fn test_buffer_splitter_multithreaded_arbtest() {
        arbtest::arbtest(|u| {
            let width = u.int_in_range(16..=128)?;
            let height = u.int_in_range(16..=128)?;
            let num_threads = u.int_in_range(2..=8)?;
            let num_channels = u.int_in_range(1..=4)?;

            let downsample_x: u8 = u.int_in_range(0..=2)?;
            let downsample_y: u8 = u.int_in_range(0..=2)?;
            let byte_size: usize = *u.choose(&[1, 2, 4])?;
            let orientation = Orientation::Identity;

            let info: Vec<Option<SaveStageBufferInfo>> = (0..num_channels)
                .map(|ch| {
                    if ch % 2 == 0 {
                        Some(SaveStageBufferInfo {
                            downsample: (downsample_x, downsample_y),
                            orientation,
                            byte_size,
                            after_extend: false,
                        })
                    } else {
                        None
                    }
                })
                .collect();

            let mut raws: Vec<Vec<u8>> = (0..num_channels)
                .map(|_| vec![0u8; width * height * byte_size])
                .collect();
            let mut tiles = Vec::new();

            {
                let mut bufs: Vec<Option<JxlOutputBuffer>> = raws
                    .iter_mut()
                    .enumerate()
                    .map(|(i, raw)| {
                        if i == num_channels - 1 && num_channels > 1 {
                            None
                        } else {
                            Some(JxlOutputBuffer::new(raw, height, width * byte_size))
                        }
                    })
                    .collect();

                let splitter = BufferSplitter::new(&mut bufs);

                let align_x = 1usize << downsample_x;
                let align_y = 1usize << downsample_y;
                let tile_w = u.int_in_range(1..=4)? * align_x;
                let tile_h = u.int_in_range(1..=4)? * align_y;

                let mut tile_id = 1u8;

                for y in (0..height).step_by(tile_h) {
                    for x in (0..width).step_by(tile_w) {
                        let w = tile_w.min(width - x);
                        let h = tile_h.min(height - y);
                        let rect = Rect {
                            origin: (x, y),
                            size: (w, h),
                        };
                        tiles.push((rect, tile_id));
                        tile_id = tile_id.wrapping_add(1);
                    }
                }

                let chunk_size = tiles.len().div_ceil(num_threads);
                let tile_chunks: Vec<_> = tiles
                    .chunks(chunk_size.max(1))
                    .map(|c| c.to_vec())
                    .collect();

                std::thread::scope(|s| {
                    for chunk in tile_chunks {
                        let splitter_ref = &splitter;
                        let info_ref = &info;
                        s.spawn(move || {
                            for (rect, id) in chunk {
                                let mut local = splitter_ref.get_local_buffers(
                                    info_ref,
                                    rect,
                                    false,
                                    (width, height),
                                    (width, height),
                                    (0, 0),
                                );

                                for buf in local.iter_mut().flatten() {
                                    let (_, bh) = buf.byte_size();
                                    for ry in 0..bh {
                                        let row = buf.row_mut(ry);
                                        row.fill(id);
                                    }
                                }
                            }
                        });
                    }
                });

                assert_eq!(splitter.into_changed_regions().len(), tiles.len());
            }

            Ok(())
        });
    }
}
