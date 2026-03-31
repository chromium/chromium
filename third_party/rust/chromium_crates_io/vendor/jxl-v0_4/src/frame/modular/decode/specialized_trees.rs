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
        let dec = reader.read_signed_clustered(histograms, br, prediction_result.context as usize);
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
        let dec = reader.read_signed_clustered(histograms, br, prediction_result.context as usize);
        let val = make_pixel(dec, prediction_result.multiplier, prediction_result.guess);
        self.wp_state.update_errors(val, pos, xsize);
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

/// Specialized WpOnlyLookup for when all HybridUint configs are 420
/// This allows using the fast-path entropy decoder
pub struct WpOnlyLookupConfig420 {
    lut: [u8; LUT_TABLE_SIZE],
    wp_state: WeightedPredictorState,
}

impl WpOnlyLookupConfig420 {
    fn new(
        tree: &[TreeNode],
        histograms: &Histograms,
        header: &GroupHeader,
        xsize: usize,
    ) -> Option<Self> {
        if !histograms.can_use_config_420_fast_path() {
            return None;
        }
        let wp_state = WeightedPredictorState::new(&header.wp_header, xsize);
        let lut = make_lut(tree)?;
        Some(Self { lut, wp_state })
    }
}

impl ModularChannelDecoder for WpOnlyLookupConfig420 {
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
        // Use the specialized 420 fast path
        let dec = reader.read_signed_clustered_config_420(histograms, br, ctx as usize);
        let val = dec.wrapping_add(wp_pred as i32);
        self.wp_state.update_errors(val, pos, xsize);
        val
    }
}

/// Property 9 is the "gradient property": left + top - topleft
const GRADIENT_PROPERTY: u8 = 9;

/// Config 420 specialized version of gradient lookup for trees that split only on property 9.
/// This uses the specialized entropy decoder for config 420 + no LZ77.
pub struct GradientLookupConfig420 {
    lut: [u8; LUT_TABLE_SIZE],
}

fn make_gradient_lut_config_420(
    tree: &[TreeNode],
    histograms: &Histograms,
) -> Option<GradientLookupConfig420> {
    if !histograms.can_use_config_420_fast_path() {
        return None;
    }
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

    let lut = make_lut(tree)?;
    Some(GradientLookupConfig420 { lut })
}

impl ModularChannelDecoder for GradientLookupConfig420 {
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

        // Use the specialized config 420 fast path
        let dec = reader.read_signed_clustered_config_420(histograms, br, cluster as usize);
        dec.wrapping_add(pred as i32)
    }
}

pub struct SingleGradientOnly {
    clustered_ctx: usize,
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
        let dec = reader.read_signed_clustered_inline(histograms, br, self.clustered_ctx);
        make_pixel(dec, 1, pred)
    }
}

pub struct NoTree {
    clustered_ctx: usize,
}

impl ModularChannelDecoder for NoTree {
    const NEEDS_TOP: bool = false;
    const NEEDS_TOPTOP: bool = false;

    fn init_row(&mut self, _: &mut [&mut ModularChannel], _: usize, _: usize) {}

    #[inline(always)]
    fn decode_one(
        &mut self,
        _: PredictionData,
        _: (usize, usize),
        _: usize,
        reader: &mut SymbolReader,
        br: &mut BitReader,
        histograms: &Histograms,
    ) -> i32 {
        let dec = reader.read_signed_clustered_inline(histograms, br, self.clustered_ctx);
        make_pixel(dec, 1, 0)
    }
}

#[allow(clippy::large_enum_variant)]
pub enum TreeSpecialCase {
    NoTree(NoTree),
    NoWp(NoWpTree),
    WpOnlyConfig420(WpOnlyLookupConfig420),
    GradientLookupConfig420(GradientLookupConfig420),
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
                let TreeNode::Leaf { id, .. } = &mut node else {
                    unreachable!()
                };
                *id = tree.histograms.map_context_to_cluster(*id as usize) as u32;
                pruned_tree.push(node);
            }
        }
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
        return Ok(TreeSpecialCase::NoTree(NoTree {
            clustered_ctx: *id as usize,
        }));
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
            clustered_ctx: *id as usize,
        }));
    }

    if !uses_non_wp {
        // Try the specialized 420 config version (fast path for effort 3 encoded images)
        if let Some(wp) = WpOnlyLookupConfig420::new(&pruned_tree, &tree.histograms, header, xsize)
        {
            return Ok(TreeSpecialCase::WpOnlyConfig420(wp));
        }
    }

    // Non-WP trees (includes effort 2 encoding and some groups in effort > 3)
    if !uses_wp {
        // Try config 420 specialized gradient LUT version (fast path for effort 2 encoded images)
        if let Some(gl) = make_gradient_lut_config_420(&pruned_tree, &tree.histograms) {
            return Ok(TreeSpecialCase::GradientLookupConfig420(gl));
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
