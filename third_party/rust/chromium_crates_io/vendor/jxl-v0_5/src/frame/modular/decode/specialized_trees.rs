// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{collections::VecDeque, ops::Range};

use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::{Histograms, SymbolReader, unpack_signed},
    error::Result,
    frame::modular::{
        ModularChannel, Predictor, Tree,
        decode::{
            channel::ModularChannelDecoder,
            common::{make_pixel, precompute_references},
        },
        flat_tree::{FlatTreeNode, predict_flat},
        predict::{PredictionData, WeightedPredictorState, clamped_gradient},
        tree::{NUM_NONREF_PROPERTIES, PROPERTIES_PER_PREVCHAN, PredictionResult, TreeNode},
    },
    headers::modular::GroupHeader,
    image::Image,
};

trait MaybeWeightedPredictor: Sized {
    fn predict(
        &mut self,
        nodes: &[FlatTreeNode],
        prediction_data: PredictionData,
        pos: (usize, usize),
        references: &Image<i32>,
        prop_buffer: &mut [i32; 256],
    ) -> PredictionResult;
    fn update_errors(&mut self, _val: i32, _pos: (usize, usize)) {}
}

impl MaybeWeightedPredictor for () {
    #[inline(always)]
    fn predict(
        &mut self,
        nodes: &[FlatTreeNode],
        prediction_data: PredictionData,
        pos: (usize, usize),
        references: &Image<i32>,
        prop_buffer: &mut [i32; 256],
    ) -> PredictionResult {
        predict_flat(nodes, prediction_data, None, pos, references, prop_buffer)
    }
}

impl MaybeWeightedPredictor for WeightedPredictorState {
    #[inline(always)]
    fn predict(
        &mut self,
        nodes: &[FlatTreeNode],
        prediction_data: PredictionData,
        pos: (usize, usize),
        references: &Image<i32>,
        prop_buffer: &mut [i32; 256],
    ) -> PredictionResult {
        predict_flat(
            nodes,
            prediction_data,
            Some(self),
            pos,
            references,
            prop_buffer,
        )
    }
    #[inline(always)]
    fn update_errors(&mut self, val: i32, pos: (usize, usize)) {
        self.update_errors(val, pos);
    }
}

trait Reader: Sized {
    fn read(
        &self,
        reader: &mut SymbolReader,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> i32;
}

impl Reader for i32 {
    #[inline(always)]
    fn read(&self, _: &mut SymbolReader, _: &Histograms, _: &mut BitReader, _: usize) -> i32 {
        *self
    }
}

struct Reader420NoLz;
impl Reader for Reader420NoLz {
    #[inline(always)]
    fn read(
        &self,
        reader: &mut SymbolReader,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> i32 {
        reader.read_signed_clustered_config_420(histograms, br, cluster)
    }
}

struct ReaderGeneric;
impl Reader for ReaderGeneric {
    #[inline(always)]
    fn read(
        &self,
        reader: &mut SymbolReader,
        histograms: &Histograms,
        br: &mut BitReader,
        cluster: usize,
    ) -> i32 {
        reader.read_signed_clustered_inline(histograms, br, cluster)
    }
}

struct FlatTreeInner {
    nodes: Vec<FlatTreeNode>,
    references: Image<i32>,
    property_buffer: Box<[i32; 256]>,
}

impl FlatTreeInner {
    fn new(
        nodes: Vec<TreeNode>,
        max_property_count: usize,
        channel: usize,
        stream: usize,
        xsize: usize,
    ) -> Result<Self> {
        let num_ref_props = max_property_count
            .saturating_sub(NUM_NONREF_PROPERTIES)
            .next_multiple_of(PROPERTIES_PER_PREVCHAN);
        let references = Image::<i32>::new((num_ref_props, xsize))?;
        let mut property_buffer = Box::new([0; 256]);

        property_buffer[0] = channel as i32;
        property_buffer[1] = stream as i32;

        Ok(Self {
            nodes: Tree::build_flat_tree(&nodes)?,
            references,
            property_buffer,
        })
    }
}

struct FlatTree<WP, R> {
    inner: FlatTreeInner,
    reader: R,
    wp_state: WP,
}

impl<WP: MaybeWeightedPredictor, R: Reader> FlatTree<WP, R> {
    fn new(inner: FlatTreeInner, reader: R, wp_state: WP) -> Self {
        Self {
            inner,
            reader,
            wp_state,
        }
    }
}

impl<WP: MaybeWeightedPredictor, R: Reader> ModularChannelDecoder for FlatTree<WP, R> {
    fn init_row(&mut self, buffers: &mut [&mut ModularChannel], chan: usize, y: usize) {
        precompute_references(buffers, chan, y, &mut self.inner.references);
        self.inner.property_buffer[GRADIENT_PROPERTY as usize] = 0;
    }

    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        pos: (usize, usize),
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let prediction_result = self.wp_state.predict(
            &self.inner.nodes,
            prediction_data,
            pos,
            &self.inner.references,
            &mut self.inner.property_buffer,
        );
        let dec = self
            .reader
            .read(reader, histograms, br, prediction_result.context as usize);
        let val = make_pixel(dec, prediction_result.multiplier, prediction_result.guess);
        self.wp_state.update_errors(val, pos);
        val
    }
}

const LUT_MAX_SPLITVAL: i32 = 1023;
const LUT_MIN_SPLITVAL: i32 = -1024;
const LUT_TABLE_SIZE: usize = (LUT_MAX_SPLITVAL - LUT_MIN_SPLITVAL + 1) as usize;
const _: () = assert!(LUT_TABLE_SIZE.is_power_of_two());

fn make_lut(tree: &[TreeNode]) -> Option<[u8; LUT_TABLE_SIZE]> {
    struct RangeAndNode {
        range: Range<i32>,
        node: u32,
    }
    let mut stack = vec![RangeAndNode {
        range: LUT_MIN_SPLITVAL..LUT_MAX_SPLITVAL + 1,
        node: 0,
    }];

    let mut ans = [0u8; LUT_TABLE_SIZE];
    while let Some(RangeAndNode { range, node }) = stack.pop() {
        let v = tree[node as usize];
        match v {
            TreeNode::Split {
                val, left, right, ..
            } => {
                let first_left = val + 1;
                if first_left >= range.end || first_left <= range.start {
                    return None;
                }
                stack.push(RangeAndNode {
                    range: first_left..range.end,
                    node: left,
                });
                stack.push(RangeAndNode {
                    range: range.start..first_left,
                    node: right,
                });
            }
            TreeNode::Leaf {
                offset,
                multiplier,
                id,
                ..
            } => {
                if offset != 0 || multiplier != 1 {
                    return None;
                }
                let start = range.start - LUT_MIN_SPLITVAL;
                let end = range.end - LUT_MIN_SPLITVAL;
                ans[start as usize..end as usize].fill(id as u8);
            }
        }
    }

    Some(ans)
}

struct WpOnly<R> {
    lut: [u8; LUT_TABLE_SIZE],
    wp_state: WeightedPredictorState,
    reader: R,
}

impl<R: Reader> WpOnly<R> {
    fn new(tree: &[TreeNode], header: &GroupHeader, xsize: usize, reader: R) -> Option<Self> {
        let wp_state = WeightedPredictorState::new(&header.wp_header, xsize);
        let lut = make_lut(tree)?;
        Some(Self {
            lut,
            wp_state,
            reader,
        })
    }
}

impl<R: Reader> ModularChannelDecoder for WpOnly<R> {
    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        pos: (usize, usize),
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let (wp_pred, property) = self.wp_state.predict_and_property(pos, &prediction_data);
        let ctx = self.lut[(property as i64 - LUT_MIN_SPLITVAL as i64)
            .clamp(0, LUT_TABLE_SIZE as i64 - 1) as usize];
        let dec = self.reader.read(reader, histograms, br, ctx as usize);
        let val = dec.wrapping_add(wp_pred as i32);
        self.wp_state.update_errors(val, pos);
        val
    }
}

/// Property 9 is the "gradient property": left + top - topleft
const GRADIENT_PROPERTY: u8 = 9;
const WEIGHTED_PROPERTY: u8 = 15;

struct GradientOnly<R> {
    lut: [u8; LUT_TABLE_SIZE],
    reader: R,
}

impl<R: Reader> GradientOnly<R> {
    fn new(tree: &[TreeNode], reader: R) -> Option<Self> {
        let lut = make_lut(tree)?;
        Some(Self { lut, reader })
    }
}

impl<R: Reader> ModularChannelDecoder for GradientOnly<R> {
    #[inline(always)]
    fn needs_toptop(&self) -> bool {
        false
    }

    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        _: (usize, usize),
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let prop9 = prediction_data
            .left
            .wrapping_add(prediction_data.top)
            .wrapping_sub(prediction_data.topleft);

        let index =
            (prop9 as i64 - LUT_MIN_SPLITVAL as i64).clamp(0, LUT_TABLE_SIZE as i64 - 1) as usize;
        let cluster = self.lut[index];

        let pred = clamped_gradient(
            prediction_data.left as i64,
            prediction_data.top as i64,
            prediction_data.topleft as i64,
        );

        let dec = self.reader.read(reader, histograms, br, cluster as usize);
        dec.wrapping_add(pred as i32)
    }
}

struct SingleGradientOnly<R> {
    clustered_ctx: usize,
    reader: R,
}

impl<R: Reader> ModularChannelDecoder for SingleGradientOnly<R> {
    #[inline(always)]
    fn needs_toptop(&self) -> bool {
        false
    }

    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        _: (usize, usize),
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let pred = clamped_gradient(
            prediction_data.left as i64,
            prediction_data.top as i64,
            prediction_data.topleft as i64,
        );
        let dec = self.reader.read(reader, histograms, br, self.clustered_ctx);
        dec.wrapping_add(pred as i32)
    }
}

struct NoTreeZero {
    clustered_ctx: usize,
    single_value: Option<i32>,
}

impl ModularChannelDecoder for NoTreeZero {
    #[inline(never)]
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
    ) {
        let row = buffers[chan].data.row_mut(y);
        debug_assert_eq!(row.len(), xsize);
        if let Some(sym) = self.single_value {
            row.fill(sym);
        } else {
            for r in row.iter_mut() {
                *r = reader.read_signed_clustered_inline(histograms, br, self.clustered_ctx);
            }
        }
    }
}

pub fn run_on_specialized_tree<F: FnOnce(&mut dyn ModularChannelDecoder) -> Result<()>>(
    tree: &Tree,
    channel: usize,
    stream: usize,
    xsize: usize,
    header: &GroupHeader,
    run: F,
) -> Result<()> {
    // TODO(veluca): consider skipping the pruning if header.uses_global_tree is true.
    let mut pruned_tree = Vec::new();
    let mut queue = VecDeque::new();
    pruned_tree.try_reserve(tree.nodes.len())?;
    queue.try_reserve(tree.nodes.len())?;
    queue.push_front(0);

    let mut uses_wp = false;
    let mut uses_non_wp = false;
    let mut max_property_count = 0;
    let mut uses_non_gradient = false;

    // If, after pruning the tree, `is_single_symbol` is true, then `single_symbol` is the
    // only symbol that could possibly be decoded by this tree.
    // TODO(veluca): The single-symbol special case corrupts the lz77 window, so it is
    // disabled for now if lz77 is enabled. Figure out how to make them work together.
    let mut is_single_symbol = !tree.histograms.lz77_params().enabled;
    let mut single_symbol = None;

    // Obtain a pruned tree without nodes that are not relevant in the current channel and stream.
    // Proceed in BFS order, so that we know that the children of a node will be adjacent.
    // Also re-maps context IDs to cluster IDs.
    while let Some(v) = queue.pop_front() {
        let mut node = tree.nodes[v as usize];
        match node {
            TreeNode::Split {
                property,
                val,
                left,
                right,
            } if property < 2 => {
                // If the node splits on static properties, re-enqueue its correct child immediately.
                let vv = if property == 0 { channel } else { stream };
                queue.push_front(if vv as i32 > val { left } else { right });
                continue;
            }
            TreeNode::Split {
                property,
                val,
                left,
                right,
            } => {
                uses_wp |= property == WEIGHTED_PROPERTY;
                uses_non_wp |= property != WEIGHTED_PROPERTY;
                uses_non_gradient |= property != GRADIENT_PROPERTY;
                max_property_count = max_property_count.max(property as usize + 1);
                let base = (queue.len() + pruned_tree.len() + 1) as u32;
                pruned_tree.push(TreeNode::Split {
                    property,
                    val,
                    left: base,
                    right: base + 1,
                });
                queue.push_back(left);
                queue.push_back(right);
            }
            TreeNode::Leaf { predictor, .. } => {
                uses_wp |= predictor == Predictor::Weighted;
                uses_non_wp |= predictor != Predictor::Weighted;
                uses_non_gradient |= predictor != Predictor::Gradient;
                let TreeNode::Leaf { id, .. } = &mut node else {
                    unreachable!()
                };
                *id = tree.histograms.map_context_to_cluster(*id as usize) as u32;
                if is_single_symbol {
                    if let Some(sym) = tree.histograms.single_symbol(*id as usize) {
                        if sym >= tree.histograms.uint(*id as usize).split_token() {
                            // This symbol would need extra bits. This is rare enough, so disable
                            // the optimization.
                            is_single_symbol = false;
                        }
                        if single_symbol.is_none() {
                            single_symbol = Some(sym);
                        }
                        if single_symbol != Some(sym) {
                            is_single_symbol = false;
                        }
                    } else {
                        is_single_symbol = false;
                    }
                }
                pruned_tree.push(node);
            }
        }
    }

    if !is_single_symbol {
        single_symbol = None;
    }

    if let [
        TreeNode::Leaf {
            predictor: Predictor::Zero,
            multiplier: 1,
            offset: 0,
            id,
        },
    ] = &*pruned_tree
    {
        return run(&mut NoTreeZero {
            clustered_ctx: *id as usize,
            single_value: single_symbol.map(unpack_signed),
        });
    }

    if let [
        TreeNode::Leaf {
            predictor: Predictor::Gradient,
            multiplier: 1,
            offset: 0,
            id,
        },
    ] = &*pruned_tree
    {
        return run(&mut SingleGradientOnly {
            clustered_ctx: *id as usize,
            reader: ReaderGeneric,
        });
    }

    let uses_non420 = !tree.histograms.can_use_config_420_fast_path();

    if !uses_non_wp
        && !uses_non420
        && let Some(mut wp) = WpOnly::new(&pruned_tree, header, xsize, Reader420NoLz)
    {
        return run(&mut wp);
    }

    if !uses_non_gradient
        && !uses_non420
        && let Some(mut grad) = GradientOnly::new(&pruned_tree, Reader420NoLz)
    {
        return run(&mut grad);
    }

    let single_symbol = single_symbol.map(unpack_signed);

    let inner = FlatTreeInner::new(pruned_tree, max_property_count, channel, stream, xsize)?;

    // Non-WP trees (includes effort 2 encoding and some groups in effort > 3)
    if !uses_wp {
        if let Some(ss) = single_symbol {
            return run(&mut FlatTree::new(inner, ss, ()));
        }
        if !uses_non420 {
            return run(&mut FlatTree::new(inner, Reader420NoLz, ()));
        }
        return run(&mut FlatTree::new(inner, ReaderGeneric, ()));
    }

    let wp_state = WeightedPredictorState::new(&header.wp_header, xsize);

    if let Some(ss) = single_symbol {
        return run(&mut FlatTree::new(inner, ss, wp_state));
    }
    if !uses_non420 {
        return run(&mut FlatTree::new(inner, Reader420NoLz, wp_state));
    }
    run(&mut FlatTree::new(inner, ReaderGeneric, wp_state))
}
