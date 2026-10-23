// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::frame::modular::predict::clamped_gradient;
use crate::frame::modular::tree::{NUM_NONREF_PROPERTIES, PROPERTIES_PER_PREVCHAN};
use crate::frame::modular::{ModularChannel, ModularStorage};
use crate::frame::quantizer::NUM_QUANT_TABLES;
use crate::headers::frame_header::FrameHeader;
use crate::image::ImageRect;

#[derive(Debug)]
pub(crate) struct References<'a> {
    pub(crate) data: &'a mut [i32],
    pub(crate) num_ref_props: usize,
}

impl<'a> References<'a> {
    pub(crate) fn new(scratch: &'a mut Vec<i32>, num_ref_props: usize, xsize: usize) -> Self {
        let len = num_ref_props * xsize;
        scratch.clear();
        scratch.resize(len, 0);
        Self {
            data: &mut scratch[..len],
            num_ref_props,
        }
    }
}

pub(crate) fn init_properties_and_references<'a>(
    references: &'a mut Vec<i32>,
    property_buffer: &'a mut Vec<i32>,
    channel: usize,
    stream: usize,
    xsize: usize,
    max_property_count: usize,
) -> (References<'a>, &'a mut [i32; 256]) {
    let num_ref_props = max_property_count
        .saturating_sub(NUM_NONREF_PROPERTIES)
        .next_multiple_of(PROPERTIES_PER_PREVCHAN)
        .min(channel * PROPERTIES_PER_PREVCHAN);
    let references = References::new(references, num_ref_props, xsize);
    property_buffer.clear();
    property_buffer.resize(256, 0);
    let property_buffer: &mut [i32; 256] = (&mut property_buffer[..256]).try_into().unwrap();
    property_buffer[0] = channel as i32;
    property_buffer[1] = stream as i32;
    (references, property_buffer)
}

#[derive(Debug)]
pub enum ModularStreamId {
    GlobalData,
    VarDCTLF(usize),
    ModularLF(usize),
    LFMeta(usize),
    QuantTable(usize),
    ModularHF { pass: usize, group: usize },
}

impl ModularStreamId {
    pub fn get_id(&self, frame_header: &FrameHeader) -> usize {
        match self {
            Self::GlobalData => 0,
            Self::VarDCTLF(g) => 1 + g,
            Self::ModularLF(g) => 1 + frame_header.num_lf_groups() + g,
            Self::LFMeta(g) => 1 + frame_header.num_lf_groups() * 2 + g,
            Self::QuantTable(q) => 1 + frame_header.num_lf_groups() * 3 + q,
            Self::ModularHF { pass, group } => {
                1 + frame_header.num_lf_groups() * 3
                    + NUM_QUANT_TABLES
                    + frame_header.num_groups() * *pass
                    + *group
            }
        }
    }
}

fn precompute_channel_refs<T: crate::image::ImageDataType + Into<i32> + Copy>(
    ref_rect: &ImageRect<T>,
    w: usize,
    y: usize,
    offset: usize,
    num_extra_props: usize,
    data: &mut [i32],
) {
    if w == 0 {
        return;
    }
    let ref_chan_row = &ref_rect.row(y)[..w];
    if y == 0 {
        let mut vleft: i64 = 0;
        for (&sample, ref_row) in ref_chan_row
            .iter()
            .zip(data.chunks_exact_mut(num_extra_props))
        {
            let v: i32 = sample.into();
            let diff = (v as i64 - vleft) as i32;
            let dst = &mut ref_row[offset..offset + 4];
            dst[0] = v.wrapping_abs();
            dst[1] = v;
            dst[2] = diff.wrapping_abs();
            dst[3] = diff;
            vleft = v as i64;
        }
    } else {
        let ref_chan_prev = &ref_rect.row(y - 1)[..w];
        let v0: i32 = ref_chan_row[0].into();
        let vtop0: i32 = ref_chan_prev[0].into();
        let diff0 = (v0 as i64 - vtop0 as i64) as i32;
        let dst0 = &mut data[offset..offset + 4];
        dst0[0] = v0.wrapping_abs();
        dst0[1] = v0;
        dst0[2] = diff0.wrapping_abs();
        dst0[3] = diff0;
        let mut vleft = v0 as i64;
        let mut vtopleft = vtop0 as i64;
        for ((&sample, &top_sample), ref_row) in ref_chan_row[1..]
            .iter()
            .zip(&ref_chan_prev[1..])
            .zip(data[num_extra_props..].chunks_exact_mut(num_extra_props))
        {
            let v: i32 = sample.into();
            let vtop: i32 = top_sample.into();
            let vpredicted = clamped_gradient(vleft, vtop as i64, vtopleft);
            let diff = (v as i64 - vpredicted) as i32;
            let dst = &mut ref_row[offset..offset + 4];
            dst[0] = v.wrapping_abs();
            dst[1] = v;
            dst[2] = diff.wrapping_abs();
            dst[3] = diff;
            vleft = v as i64;
            vtopleft = vtop as i64;
        }
    }
}

pub(super) fn precompute_references(
    buffers: &mut [&mut ModularChannel],
    chan: usize,
    y: usize,
    references: &mut References,
    storage: ModularStorage,
) {
    let num_extra_props = references.num_ref_props;
    if num_extra_props == 0 {
        return;
    }
    let chan_size = buffers[chan].size(storage);
    let chan_shift = buffers[chan].shift;
    let w = chan_size.0;
    let mut offset = 0;
    for i in 0..chan {
        if offset >= num_extra_props {
            break;
        }
        let j = chan - i - 1;
        if buffers[j].size(storage) != chan_size || buffers[j].shift != chan_shift {
            continue;
        }
        if storage == ModularStorage::I16 {
            let ref_rect = ImageRect::<i16>::from_raw(buffers[j].data.as_rect());
            precompute_channel_refs(&ref_rect, w, y, offset, num_extra_props, references.data);
        } else {
            let ref_rect = ImageRect::<i32>::from_raw(buffers[j].data.as_rect());
            precompute_channel_refs(&ref_rect, w, y, offset, num_extra_props, references.data);
        }
        offset += 4;
    }
}

#[inline(always)]
pub(super) fn make_pixel(dec: i32, mul: u32, guess: i64) -> i32 {
    (guess + (mul as i64) * (dec as i64)) as i32
}
