// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

use super::{Predictor, predict::WeightedPredictorState};
use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::Histograms,
    entropy_coding::decode::SymbolReader,
    error::{Error, Result},
    frame::modular::predict::PredictionData,
    image::Image,
    util::{NewWithCapacity, tracing_wrappers::*},
};

#[derive(Debug, Clone, Copy)]
pub enum TreeNode {
    Split {
        property: u8,
        val: i32,
        left: u32,
        right: u32,
    },
    Leaf {
        predictor: Predictor,
        offset: i32,
        multiplier: u32,
        id: u32,
    },
}

pub struct Tree {
    pub nodes: Vec<TreeNode>,
    pub histograms: Histograms,
}

impl Debug for Tree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Tree[{:?}]", self.nodes)
    }
}

#[derive(Debug)]
pub struct PredictionResult {
    pub guess: i64,
    pub multiplier: u32,
    pub context: u32,
}

pub const NUM_NONREF_PROPERTIES: usize = 16;
pub const PROPERTIES_PER_PREVCHAN: usize = 4;

const SPLIT_VAL_CONTEXT: usize = 0;
const PROPERTY_CONTEXT: usize = 1;
const PREDICTOR_CONTEXT: usize = 2;
const OFFSET_CONTEXT: usize = 3;
const MULTIPLIER_LOG_CONTEXT: usize = 4;
const MULTIPLIER_BITS_CONTEXT: usize = 5;
const NUM_TREE_CONTEXTS: usize = 6;

// Note: `property_buffer` is passed as input because this implementation relies on having the
// previous values available for computing the local gradient property.
// Also, the first two properties (the static properties) should be already set by the caller.
// All other properties should be 0 on the first call in a row.
#[inline]
#[instrument(level = "trace", ret)]
#[allow(clippy::too_many_arguments)]
pub(super) fn predict(
    tree: &[TreeNode],
    prediction_data: PredictionData,
    xsize: usize,
    wp_state: Option<&mut WeightedPredictorState>,
    x: usize,
    y: usize,
    references: &Image<i32>,
    property_buffer: &mut [i32],
) -> PredictionResult {
    let PredictionData {
        left,
        top,
        toptop,
        topleft,
        topright,
        leftleft,
        toprightright: _toprightright,
    } = prediction_data;

    trace!(
        left,
        top, topleft, topright, leftleft, toptop, _toprightright
    );

    // Position
    property_buffer[2] = y as i32;
    property_buffer[3] = x as i32;

    // Neighbours
    property_buffer[4] = top.wrapping_abs();
    property_buffer[5] = left.wrapping_abs();
    property_buffer[6] = top;
    property_buffer[7] = left;

    // Local gradient
    property_buffer[8] = left.wrapping_sub(property_buffer[9]);
    property_buffer[9] = left.wrapping_add(top).wrapping_sub(topleft);

    // FFV1 context properties
    property_buffer[10] = left.wrapping_sub(topleft);
    property_buffer[11] = topleft.wrapping_sub(top);
    property_buffer[12] = top.wrapping_sub(topright);
    property_buffer[13] = top.wrapping_sub(toptop);
    property_buffer[14] = left.wrapping_sub(leftleft);

    // Weighted predictor property.
    let wp_pred;
    (wp_pred, property_buffer[15]) = wp_state
        .map(|wp_state| wp_state.predict_and_property((x, y), xsize, &prediction_data))
        .unwrap_or((0, 0));

    // Reference properties.
    let num_refs = references.size().0;
    if num_refs != 0 {
        let ref_properties = &mut property_buffer[NUM_NONREF_PROPERTIES..];
        ref_properties[..num_refs].copy_from_slice(&references.row(x)[..num_refs]);
    }

    trace!(?property_buffer, "new properties");

    let mut tree_node = 0;
    while let TreeNode::Split {
        property,
        val,
        left,
        right,
    } = tree[tree_node]
    {
        if property_buffer[property as usize] > val {
            trace!(
                "left at node {tree_node} [{} > {val}]",
                property_buffer[property as usize]
            );
            tree_node = left as usize;
        } else {
            trace!(
                "right at node {tree_node} [{} <= {val}]",
                property_buffer[property as usize]
            );
            tree_node = right as usize;
        }
    }

    trace!(leaf = ?tree[tree_node]);

    let TreeNode::Leaf {
        predictor,
        offset,
        multiplier,
        id,
    } = tree[tree_node]
    else {
        unreachable!();
    };

    let pred = predictor.predict_one(prediction_data, wp_pred);

    PredictionResult {
        guess: pred + offset as i64,
        multiplier,
        context: id,
    }
}

impl Tree {
    #[instrument(level = "debug", skip(br), err)]
    pub fn read(br: &mut BitReader, size_limit: usize) -> Result<Tree> {
        assert!(size_limit <= u32::MAX as usize);
        trace!(pos = br.total_bits_read());
        let tree_histograms = Histograms::decode(NUM_TREE_CONTEXTS, br, true)?;
        let mut tree_reader = SymbolReader::new(&tree_histograms, br, None)?;
        // TODO(veluca): consider early-exiting for trees known to be infinite.
        let mut tree: Vec<TreeNode> = vec![];
        let mut to_decode = 1;
        let mut leaf_id = 0;
        let mut max_property = 0;
        while to_decode > 0 {
            if tree.len() > size_limit {
                return Err(Error::TreeTooLarge(tree.len(), size_limit));
            }
            if tree.len() >= tree.capacity() {
                tree.try_reserve(tree.len() * 2 + 1)?;
            }
            to_decode -= 1;
            let property = tree_reader.read_unsigned(&tree_histograms, br, PROPERTY_CONTEXT);
            trace!(property);
            if let Some(property) = property.checked_sub(1) {
                // inner node.
                if property > 255 {
                    return Err(Error::InvalidProperty(property));
                }
                max_property = max_property.max(property);
                let splitval = tree_reader.read_signed(&tree_histograms, br, SPLIT_VAL_CONTEXT);
                let left_child = (tree.len() + to_decode + 1) as u32;
                let node = TreeNode::Split {
                    property: property as u8,
                    val: splitval,
                    left: left_child,
                    right: left_child + 1,
                };
                trace!("split node {:?}", node);
                to_decode += 2;
                tree.push(node);
            } else {
                let predictor = Predictor::try_from(tree_reader.read_unsigned(
                    &tree_histograms,
                    br,
                    PREDICTOR_CONTEXT,
                ))?;
                let offset = tree_reader.read_signed(&tree_histograms, br, OFFSET_CONTEXT);
                let mul_log =
                    tree_reader.read_unsigned(&tree_histograms, br, MULTIPLIER_LOG_CONTEXT);
                if mul_log >= 31 {
                    return Err(Error::TreeMultiplierTooLarge(mul_log, 31));
                }
                let mul_bits =
                    tree_reader.read_unsigned(&tree_histograms, br, MULTIPLIER_BITS_CONTEXT);
                let multiplier = (mul_bits as u64 + 1) << mul_log;
                if multiplier > (u32::MAX as u64) {
                    return Err(Error::TreeMultiplierBitsTooLarge(mul_bits, mul_log));
                }
                let node = TreeNode::Leaf {
                    predictor,
                    offset,
                    id: leaf_id,
                    multiplier: multiplier as u32,
                };
                leaf_id += 1;
                trace!("leaf node {:?}", node);
                tree.push(node);
            }
        }
        tree_reader.check_final_state(&tree_histograms, br)?;

        let num_properties = max_property as usize + 1;
        let mut property_ranges = Vec::new_with_capacity(num_properties * tree.len())?;
        property_ranges.resize(num_properties * tree.len(), (i32::MIN, i32::MAX));
        let mut height = Vec::new_with_capacity(tree.len())?;
        height.resize(tree.len(), 0);
        for i in 0..tree.len() {
            const HEIGHT_LIMIT: usize = 2048;
            if height[i] > HEIGHT_LIMIT {
                return Err(Error::TreeTooLarge(height[i], HEIGHT_LIMIT));
            }
            if let TreeNode::Split {
                property,
                val,
                left,
                right,
            } = tree[i]
            {
                height[left as usize] = height[i] + 1;
                height[right as usize] = height[i] + 1;
                for p in 0..num_properties {
                    if p == property as usize {
                        let (l, u) = property_ranges[i * num_properties + p];
                        if l > val || u <= val {
                            return Err(Error::TreeSplitOnEmptyRange(p as u8, val, l, u));
                        }
                        trace!(
                            "splitting at node {i} on property {p}, range [{l}, {u}] at position {val}"
                        );
                        property_ranges[left as usize * num_properties + p] = (val + 1, u);
                        property_ranges[right as usize * num_properties + p] = (l, val);
                    } else {
                        property_ranges[left as usize * num_properties + p] =
                            property_ranges[i * num_properties + p];
                        property_ranges[right as usize * num_properties + p] =
                            property_ranges[i * num_properties + p];
                    }
                }
            } else {
                #[cfg(feature = "tracing")]
                {
                    for p in 0..num_properties {
                        let (l, u) = property_ranges[i * num_properties + p];
                        trace!("final range at node {i} property {p}: [{l}, {u}]");
                    }
                }
            }
        }

        let histograms = Histograms::decode(tree.len().div_ceil(2), br, true)?;

        Ok(Tree {
            nodes: tree,
            histograms,
        })
    }

    pub fn max_property_count(&self) -> usize {
        self.nodes
            .iter()
            .map(|x| match x {
                TreeNode::Leaf { .. } => 0,
                TreeNode::Split { property, .. } => *property,
            })
            .max()
            .unwrap_or_default() as usize
            + 1
    }

    pub fn num_prev_channels(&self) -> usize {
        self.max_property_count()
            .saturating_sub(NUM_NONREF_PROPERTIES)
            .div_ceil(PROPERTIES_PER_PREVCHAN)
    }
}
