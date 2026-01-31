// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{collections::VecDeque, ops::Range};

use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::{Histograms, SymbolReader},
    error::Result,
    frame::modular::{
        ModularChannel, Predictor, Tree,
        decode::{
            channel::ModularChannelDecoder,
            common::{make_pixel, precompute_references},
        },
        predict::{PredictionData, WeightedPredictorState, clamped_gradient},
        tree::{
            FlatTreeNode, NUM_NONREF_PROPERTIES, PROPERTIES_PER_PREVCHAN, TreeNode, predict_flat,
        },
    },
    headers::modular::GroupHeader,
    image::Image,
};

pub struct NoWpTree {
    flat_nodes: Vec<FlatTreeNode>,
    references: Image<i32>,
    property_buffer: Vec<i32>,
}

impl NoWpTree {
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
        let num_properties = NUM_NONREF_PROPERTIES + num_ref_props;
        let mut property_buffer: Vec<i32> = vec![0; num_properties];

        property_buffer[0] = channel as i32;
        property_buffer[1] = stream as i32;

        let flat_nodes = Tree::build_flat_tree(&nodes)?;

        Ok(Self {
            flat_nodes,
            references,
            property_buffer,
        })
    }
}

impl ModularChannelDecoder for NoWpTree {
    const NEEDS_TOP: bool = true;
    const NEEDS_TOPTOP: bool = true;

    fn init_row(&mut self, buffers: &mut [&mut ModularChannel], chan: usize, y: usize) {
        precompute_references(buffers, chan, y, &mut self.references);
        self.property_buffer[2..].fill(0);
    }

    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        pos: (usize, usize),
        xsize: usize,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let prediction_result = predict_flat(
            &self.flat_nodes,
            prediction_data,
            xsize,
            None,
            pos.0,
            pos.1,
            &self.references,
            &mut self.property_buffer,
        );
        let dec = reader.read_signed(histograms, br, prediction_result.context as usize);
        make_pixel(dec, prediction_result.multiplier, prediction_result.guess)
    }
}

pub struct GeneralTree {
    no_wp_tree: NoWpTree,
    wp_state: WeightedPredictorState,
}

impl GeneralTree {
    fn new(
        nodes: Vec<TreeNode>,
        max_property_count: usize,
        header: &GroupHeader,
        channel: usize,
        stream: usize,
        xsize: usize,
    ) -> Result<Self> {
        let wp_state = WeightedPredictorState::new(&header.wp_header, xsize);
        Ok(Self {
            no_wp_tree: NoWpTree::new(nodes, max_property_count, channel, stream, xsize)?,
            wp_state,
        })
    }
}

impl ModularChannelDecoder for GeneralTree {
    const NEEDS_TOP: bool = true;
    const NEEDS_TOPTOP: bool = true;

    fn init_row(&mut self, buffers: &mut [&mut ModularChannel], chan: usize, y: usize) {
        self.no_wp_tree.init_row(buffers, chan, y);
    }

    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        pos: (usize, usize),
        xsize: usize,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let prediction_result = predict_flat(
            &self.no_wp_tree.flat_nodes,
            prediction_data,
            xsize,
            Some(&mut self.wp_state),
            pos.0,
            pos.1,
            &self.no_wp_tree.references,
            &mut self.no_wp_tree.property_buffer,
        );
        let dec = reader.read_signed(histograms, br, prediction_result.context as usize);
        let val = make_pixel(dec, prediction_result.multiplier, prediction_result.guess);
        self.wp_state.update_errors(val, pos, xsize);
        val
    }
}

const LUT_MAX_SPLITVAL: i32 = 1023;
const LUT_MIN_SPLITVAL: i32 = -1024;
const LUT_TABLE_SIZE: usize = (LUT_MAX_SPLITVAL - LUT_MIN_SPLITVAL + 1) as usize;
const _: () = assert!(LUT_TABLE_SIZE.is_power_of_two());

pub struct WpOnlyLookup {
    lut: [u8; LUT_TABLE_SIZE], // Lookup (wp value -> *clustered* context id)
    wp_state: WeightedPredictorState,
}

fn make_lut(tree: &[TreeNode], histograms: &Histograms) -> Option<[u8; LUT_TABLE_SIZE]> {
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
                ans[start as usize..end as usize]
                    .fill(histograms.map_context_to_cluster(id as usize) as u8);
            }
        }
    }

    Some(ans)
}

impl WpOnlyLookup {
    fn new(
        tree: &[TreeNode],
        histograms: &Histograms,
        header: &GroupHeader,
        xsize: usize,
    ) -> Option<Self> {
        let wp_state = WeightedPredictorState::new(&header.wp_header, xsize);
        let lut = make_lut(tree, histograms)?;
        Some(Self { lut, wp_state })
    }
}

impl ModularChannelDecoder for WpOnlyLookup {
    const NEEDS_TOP: bool = true;
    const NEEDS_TOPTOP: bool = true;

    fn init_row(&mut self, _buffers: &mut [&mut ModularChannel], _chan: usize, _y: usize) {
        // nothing to do
    }

    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        pos: (usize, usize),
        xsize: usize,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let (wp_pred, property) = self
            .wp_state
            .predict_and_property(pos, xsize, &prediction_data);
        let ctx = self.lut[(property as i64 - LUT_MIN_SPLITVAL as i64)
            .clamp(0, LUT_TABLE_SIZE as i64 - 1) as usize];
        let dec = reader.read_signed_clustered(histograms, br, ctx as usize);
        let val = dec.wrapping_add(wp_pred as i32);
        self.wp_state.update_errors(val, pos, xsize);
        val
    }
}

/// Fast path for trees that split only on property 9 (gradient: left + top - topleft)
/// with Gradient predictor, offset=0, multiplier=1.
/// Maps property 9 values directly to cluster IDs via a LUT.
/// This targets libjxl effort 2 encoding.
pub struct GradientLookup {
    lut: [u8; LUT_TABLE_SIZE],
}

/// Property 9 is the "gradient property": left + top - topleft
const GRADIENT_PROPERTY: u8 = 9;

fn make_gradient_lut(tree: &[TreeNode], histograms: &Histograms) -> Option<GradientLookup> {
    // Verify all splits are on property 9 and all leaves have Gradient predictor
    for node in tree {
        match node {
            TreeNode::Split { property, .. } => {
                if *property != GRADIENT_PROPERTY {
                    return None;
                }
            }
            TreeNode::Leaf { predictor, .. } => {
                if *predictor != Predictor::Gradient {
                    return None;
                }
            }
        }
    }

    // Use existing make_lut which handles offset=0, multiplier=1 checks
    let lut = make_lut(tree, histograms)?;
    Some(GradientLookup { lut })
}

impl ModularChannelDecoder for GradientLookup {
    const NEEDS_TOP: bool = true;
    const NEEDS_TOPTOP: bool = false;

    fn init_row(&mut self, _: &mut [&mut ModularChannel], _: usize, _: usize) {}

    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        _: (usize, usize),
        _: usize,
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

        let dec = reader.read_signed_clustered(histograms, br, cluster as usize);
        dec.wrapping_add(pred as i32)
    }
}

pub struct SingleGradientOnly {
    ctx: usize,
}

impl ModularChannelDecoder for SingleGradientOnly {
    const NEEDS_TOP: bool = true;
    const NEEDS_TOPTOP: bool = false;

    fn init_row(&mut self, _: &mut [&mut ModularChannel], _: usize, _: usize) {}

    #[inline(always)]
    fn decode_one(
        &mut self,
        prediction_data: PredictionData,
        _: (usize, usize),
        _: usize,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let pred = Predictor::Gradient.predict_one(prediction_data, 0);
        let dec = reader.read_signed(histograms, br, self.ctx);
        make_pixel(dec, 1, pred)
    }
}

#[allow(clippy::large_enum_variant)]
pub enum TreeSpecialCase {
    NoWp(NoWpTree),
    WpOnly(WpOnlyLookup),
    GradientLookup(GradientLookup),
    SingleGradientOnly(SingleGradientOnly),
    General(GeneralTree),
}

pub fn specialize_tree(
    tree: &Tree,
    channel: usize,
    stream: usize,
    xsize: usize,
    header: &GroupHeader,
) -> Result<TreeSpecialCase> {
    // TODO(veluca): consider skipping the pruning if header.uses_global_tree is true.
    let mut pruned_tree = Vec::new();
    let mut queue = VecDeque::new();
    pruned_tree.try_reserve(tree.nodes.len())?;
    queue.try_reserve(tree.nodes.len())?;
    queue.push_front(0);

    let mut uses_wp = false;
    let mut uses_non_wp = false;

    // Obtain a pruned tree without nodes that are not relevant in the current channel and stream.
    // Proceed in BFS order, so that we know that the children of  anode will be adjacent.
    while let Some(v) = queue.pop_front() {
        let node = tree.nodes[v as usize];
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
                // WeightedPredictor property.
                uses_wp |= property == 15;
                uses_non_wp |= property != 15;
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
                pruned_tree.push(node);
            }
        }
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
        return Ok(TreeSpecialCase::SingleGradientOnly(SingleGradientOnly {
            ctx: *id as usize,
        }));
    }

    if !uses_non_wp
        && let Some(wp) = WpOnlyLookup::new(&pruned_tree, &tree.histograms, header, xsize)
    {
        return Ok(TreeSpecialCase::WpOnly(wp));
    }

    // Try gradient LUT for non-WP trees (targets effort 2 encoding)
    if !uses_wp {
        if let Some(gl) = make_gradient_lut(&pruned_tree, &tree.histograms) {
            return Ok(TreeSpecialCase::GradientLookup(gl));
        }
        return Ok(TreeSpecialCase::NoWp(NoWpTree::new(
            pruned_tree,
            tree.max_property_count(),
            channel,
            stream,
            xsize,
        )?));
    }

    Ok(TreeSpecialCase::General(GeneralTree::new(
        pruned_tree,
        tree.max_property_count(),
        header,
        channel,
        stream,
        xsize,
    )?))
}
