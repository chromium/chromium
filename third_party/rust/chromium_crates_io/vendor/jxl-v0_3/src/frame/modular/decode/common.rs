// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    frame::{
        modular::{ModularChannel, predict::clamped_gradient},
        quantizer::NUM_QUANT_TABLES,
    },
    headers::frame_header::FrameHeader,
    image::Image,
};
use num_traits::abs;

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

pub(super) fn precompute_references(
    buffers: &mut [&mut ModularChannel],
    chan: usize,
    y: usize,
    references: &mut Image<i32>,
) {
    references.fill(0);
    let mut offset = 0;
    let num_extra_props = references.size().0;
    for i in 0..chan {
        if offset >= num_extra_props {
            break;
        }
        let j = chan - i - 1;
        if buffers[j].data.size() != buffers[chan].data.size()
            || buffers[j].shift != buffers[chan].shift
        {
            continue;
        }
        let ref_chan_row = buffers[j].data.row(y);
        let ref_chan_prev = buffers[j].data.row(y.saturating_sub(1));
        for x in 0..buffers[chan].data.size().0 {
            let ref_row = references.row_mut(x);
            let v = ref_chan_row[x];
            ref_row[offset] = abs(v);
            ref_row[offset + 1] = v;
            let vleft = if x > 0 { ref_chan_row[x - 1] } else { 0 };
            let vtop = if y > 0 { ref_chan_prev[x] } else { vleft };
            let vtopleft = if x > 0 && y > 0 {
                ref_chan_prev[x - 1]
            } else {
                vleft
            };
            let vpredicted = clamped_gradient(vleft as i64, vtop as i64, vtopleft as i64);
            ref_row[offset + 2] = abs(v as i64 - vpredicted) as i32;
            ref_row[offset + 3] = (v as i64 - vpredicted) as i32;
        }
        offset += 4;
    }
}

pub(super) fn make_pixel(dec: i32, mul: u32, guess: i64) -> i32 {
    (guess + (mul as i64) * (dec as i64)) as i32
}
