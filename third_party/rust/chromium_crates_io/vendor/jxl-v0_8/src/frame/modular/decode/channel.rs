// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::common::{
    References, init_properties_and_references, make_pixel, precompute_references,
};
use crate::bit_reader::BitReader;
use crate::entropy_coding::decode::{Histograms, SymbolReader};
use crate::error::Result;
use crate::frame::modular::decode::specialized_trees::run_on_specialized_tree;
use crate::frame::modular::predict::{PredictionData, WeightedPredictorState};
use crate::frame::modular::tree::predict;
use crate::frame::modular::{ModularChannel, ModularStorage, ScratchSpace, Tree, TreeScratch};
use crate::headers::modular::{GroupHeader, WeightedHeader};
use crate::image::ImageRectMut;
use crate::util::tracing_wrappers::*;

const SMALL_CHANNEL_THRESHOLD: usize = 64;

macro_rules! rows {
    ($row: ident, $row_top: ident, $row_toptop: ident, $buffers: expr, $y: expr, $scratch: expr, $xsize: expr) => {
        let mut rect;
        let ($row, $row_top, $row_toptop): (&mut [i32], &[i32], &[i32]) = if let Some(scratch) =
            $scratch.as_deref_mut()
        {
            let [row, row_top, row_toptop] = scratch;
            (
                &mut row[..$xsize],
                &row_top[..$xsize],
                if $y == 1 {
                    &row_top[..$xsize]
                } else {
                    &row_toptop[..$xsize]
                },
            )
        } else {
            rect = ImageRectMut::<i32>::from_raw($buffers.data.as_rect_mut());
            match $y {
                0 => (&mut rect.row(0)[..$xsize], &[], &[]),
                1 => {
                    let [row, row_top] = rect.distinct_rows_mut([1, 0]);
                    (&mut row[..$xsize], &row_top[..$xsize], &row_top[..$xsize])
                }
                _ => {
                    let [row, row_top, row_toptop] = rect.distinct_rows_mut([$y, $y - 1, $y - 2]);
                    (
                        &mut row[..$xsize],
                        &row_top[..$xsize],
                        &row_toptop[..$xsize],
                    )
                }
            }
        };
    };
}

// `scratch` is None for 32-bit decoding; for 16-bit decoding,
// it keeps a rolling window of the previous 3 rows as i32;
// in that case, this function updates the window and stores
// the current row back to the i16 channel.
pub(super) fn sync_scratch(
    buf: &mut ModularChannel,
    y: usize,
    scratch: Option<&mut [Vec<i32>; 3]>,
) {
    if let Some(scratch) = scratch {
        let xsize = buf.size(ModularStorage::I16).0;
        let mut dest_rect = ImageRectMut::<i16>::from_raw(buf.data.as_rect_mut());
        let dest = dest_rect.row(y);
        for (d, &s) in dest[..xsize].iter_mut().zip(&scratch[0][..xsize]) {
            *d = s as i16;
        }
        scratch.rotate_right(1);
    }
}

pub(super) trait ModularChannelDecoder {
    #[inline(always)]
    fn needs_toptop(&self) -> bool {
        true
    }

    fn init_row(&mut self, _buffers: &mut [&mut ModularChannel], _chan: usize, _y: usize) {}

    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        pos: (usize, usize),
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32;

    // Note: scratch is None iff modular buffers are 32-bit, otherwise buffers are 16-bit.
    #[allow(clippy::too_many_arguments)]
    #[inline(never)]
    fn decode_row(
        &mut self,
        buffers: &mut [&mut ModularChannel],
        chan: usize,
        histograms: &Histograms,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        y: usize,
        xsize: usize,
        mut scratch: Option<&mut [Vec<i32>; 3]>,
    ) {
        self.init_row(buffers, chan, y);

        rows!(row, row_top, row_toptop, buffers[chan], y, scratch, xsize);

        let do_decode_cold = {
            #[inline(never)]
            |decoder: &mut Self,
             row: &mut [i32],
             row_top: &[i32],
             row_toptop: &[i32],
             x_range: std::ops::Range<usize>,
             y: usize,
             reader: &mut SymbolReader,
             br: &mut BitReader|
             -> (PredictionData, i32) {
                let mut prediction_data = PredictionData::default();
                let mut val = 0;
                for x in x_range {
                    prediction_data = PredictionData::get_rows(row, row_top, row_toptop, x, y);
                    val = decoder.decode_one(prediction_data, (x, y), reader, br, histograms);
                    row[x] = val;
                }
                (prediction_data, val)
            }
        };

        let (x0, x1) = if y == 0 || xsize <= 4 {
            (0, 0)
        } else {
            (2, xsize - 2)
        };

        let mut last = 0;
        let mut prediction_data = PredictionData::default();
        if x0 > 0 {
            (prediction_data, last) =
                do_decode_cold(self, row, row_top, row_toptop, 0..x0, y, reader, br);
        }
        if x0 < x1 {
            let needs_toptop = self.needs_toptop();
            let row_mid = &mut row[x0..x1];
            let row_top_trr = &row_top[x0 + 2..x1 + 2];
            if needs_toptop {
                let row_toptop_mid = &row_toptop[x0..x1];
                for (i, ((r, &trr), &tt)) in row_mid
                    .iter_mut()
                    .zip(row_top_trr)
                    .zip(row_toptop_mid)
                    .enumerate()
                {
                    let x = x0 + i;
                    prediction_data = prediction_data.update_for_interior_row(last, trr, tt);
                    let val = self.decode_one(prediction_data, (x, y), reader, br, histograms);
                    *r = val;
                    last = val;
                }
            } else {
                for (i, (r, &trr)) in row_mid.iter_mut().zip(row_top_trr).enumerate() {
                    let x = x0 + i;
                    prediction_data = prediction_data.update_for_interior_row(last, trr, 0);
                    let val = self.decode_one(prediction_data, (x, y), reader, br, histograms);
                    *r = val;
                    last = val;
                }
            }
        }
        if x1 < xsize {
            do_decode_cold(self, row, row_top, row_toptop, x1..xsize, y, reader, br);
        }
        sync_scratch(buffers[chan], y, scratch);
    }
}

struct FullTree<'a> {
    tree: &'a Tree,
    references: References<'a>,
    property_buffer: &'a mut [i32; 256],
    wp_state: WeightedPredictorState<'a>,
    storage: ModularStorage,
}

impl<'a> FullTree<'a> {
    fn new(
        tree: &'a Tree,
        wp_header: &WeightedHeader,
        channel: usize,
        stream: usize,
        xsize: usize,
        storage: ModularStorage,
        tree_scratch: &'a mut TreeScratch,
    ) -> Self {
        let (references, property_buffer) = init_properties_and_references(
            &mut tree_scratch.references,
            &mut tree_scratch.property_buffer,
            channel,
            stream,
            xsize,
            tree.num_properties,
        );

        Self {
            tree,
            references,
            property_buffer,
            wp_state: WeightedPredictorState::new(wp_header, xsize, &mut tree_scratch.wp_scratch),
            storage,
        }
    }
}

impl<'a> ModularChannelDecoder for FullTree<'a> {
    fn init_row(&mut self, buffers: &mut [&mut ModularChannel], chan: usize, y: usize) {
        precompute_references(buffers, chan, y, &mut self.references, self.storage);
        self.property_buffer[9] = 0;
    }

    fn decode_one(
        &mut self,
        _prediction_data: PredictionData,
        _pos: (usize, usize),
        _reader: &mut SymbolReader,
        _br: &mut BitReader,
        _histograms: &Histograms,
    ) -> i32 {
        unreachable!()
    }

    fn decode_row(
        &mut self,
        buffers: &mut [&mut ModularChannel],
        chan: usize,
        histograms: &Histograms,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        y: usize,
        xsize: usize,
        mut scratch: Option<&mut [Vec<i32>; 3]>,
    ) {
        self.init_row(buffers, chan, y);

        rows!(row, row_top, row_toptop, buffers[chan], y, scratch, xsize);

        for x in 0..xsize {
            let prediction_data = PredictionData::get_rows(row, row_top, row_toptop, x, y);
            let prediction_result = predict(
                self.tree,
                prediction_data,
                Some(&mut self.wp_state),
                x,
                y,
                &self.references,
                &mut self.property_buffer[..],
            );
            let dec = reader.read_signed(histograms, br, prediction_result.context as usize);
            let val = make_pixel(dec, prediction_result.multiplier, prediction_result.guess);
            self.wp_state.update_errors(val, (x, y));
            row[x] = val;
        }
        sync_scratch(buffers[chan], y, scratch);
    }
}

#[allow(clippy::too_many_arguments)]
#[inline(never)]
fn decode_modular_channel_impl(
    t: &mut dyn ModularChannelDecoder,
    buffers: &mut [&mut ModularChannel],
    chan: usize,
    histo: &Histograms,
    reader: &mut SymbolReader,
    br: &mut BitReader,
    storage: ModularStorage,
    scratch: &mut [Vec<i32>; 3],
) -> Result<()> {
    let size = buffers[chan].size(storage);
    let xsize = size.0;
    if storage == ModularStorage::I16 {
        for r in scratch.iter_mut() {
            if r.len() < xsize {
                r.resize(xsize, 0);
            }
        }
    }
    for y in 0..size.1 {
        t.decode_row(
            buffers,
            chan,
            histo,
            reader,
            br,
            y,
            xsize,
            if storage == ModularStorage::I16 {
                Some(scratch)
            } else {
                None
            },
        );
    }
    Ok(())
}

#[allow(clippy::too_many_arguments)]
#[instrument(level = "debug", skip(buffers, reader, tree))]
pub(super) fn decode_modular_channel(
    buffers: &mut [&mut ModularChannel],
    chan: usize,
    stream_id: usize,
    header: &GroupHeader,
    tree: &Tree,
    reader: &mut SymbolReader,
    br: &mut BitReader,
    storage: ModularStorage,
    scratch_space: &mut ScratchSpace,
) -> Result<()> {
    debug!("reading channel");
    let size = buffers[chan].size(storage);
    let ScratchSpace {
        decode_row_scratch,
        tree_lut_scratch,
        tree_scratch,
        ..
    } = scratch_space;
    if size.0 * size.1 <= SMALL_CHANNEL_THRESHOLD {
        let mut decoder = FullTree::new(
            tree,
            &header.wp_header,
            chan,
            stream_id,
            size.0,
            storage,
            tree_scratch,
        );
        decode_modular_channel_impl(
            &mut decoder,
            buffers,
            chan,
            &tree.histograms,
            reader,
            br,
            storage,
            decode_row_scratch,
        )?;
        br.check_for_error()?;
        return Ok(());
    }

    run_on_specialized_tree(
        tree,
        chan,
        stream_id,
        size.0,
        header,
        storage,
        tree_lut_scratch,
        tree_scratch,
        |t| {
            decode_modular_channel_impl(
                t,
                buffers,
                chan,
                &tree.histograms,
                reader,
                br,
                storage,
                decode_row_scratch,
            )
        },
    )?;
    br.check_for_error()
}
