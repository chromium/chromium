// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
use std::fmt::Debug;

use crate::{
    error::Result,
    frame::modular::{
        ChannelInfo,
        buffers::ModularChannel,
        transforms::{meta_apply::meta_apply_single_transform, step::TransformStep},
    },
    headers::modular::GroupHeader,
    image::Rect,
    util::tracing_wrappers::*,
};

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
    header: &GroupHeader,
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
            .zip(channels.iter_mut().zip(buf_tmp))
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
                TransformStep::HSqueeze {
                    buf_in, buf_out, ..
                }
                | TransformStep::VSqueeze {
                    buf_in, buf_out, ..
                } => {
                    for b in once(buf_out).chain(buf_in.iter_mut()) {
                        *b = buf_remap[*b];
                    }
                }
                TransformStep::Output { .. } => {
                    unreachable!()
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
                for (pos, buf) in buf_out.iter().zip(out_bufs) {
                    buffers[*pos] = buf;
                }
            }
            TransformStep::HSqueeze {
                buf_in, buf_out, ..
            } => {
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
            TransformStep::VSqueeze {
                buf_in, buf_out, ..
            } => {
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
            TransformStep::Output { .. } => {
                unreachable!()
            }
        };

        Ok(())
    }
}
