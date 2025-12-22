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

/// Flattened tree node for optimized traversal (matches C++ FlatDecisionNode).
/// Stores parent + info about both children to evaluate 3 nodes per iteration.
// TODO(hjanuschka): investigate performance of using a Rust enum here, and whether
// separating internal nodes and leaves into two arrays could save a branch.
#[derive(Debug, Clone, Copy)]
pub(super) struct FlatTreeNode {
    property0: i32,                    // Property to test, -1 if leaf
    splitval0_or_predictor: i32,       // Split value, or predictor if leaf
    splitvals_or_multiplier: [i32; 2], // Child splitvals, or multiplier if leaf
    child_id: u32,                     // Index to first grandchild, or context if leaf
    properties_or_offset: [i16; 2],    // Child properties, or offset if leaf
}

impl FlatTreeNode {
    #[inline]
    fn leaf(predictor: Predictor, offset: i32, multiplier: u32, context: u32) -> Self {
        Self {
            property0: -1,
            splitval0_or_predictor: predictor as i32,
            splitvals_or_multiplier: [multiplier as i32, 0],
            child_id: context,
            properties_or_offset: [offset as i16, 0],
        }
    }
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

/// Computes properties for tree traversal. Shared between flat and non-flat prediction.
/// Returns the weighted predictor prediction value.
#[inline]
fn compute_properties(
    prediction_data: PredictionData,
    xsize: usize,
    wp_state: Option<&mut WeightedPredictorState>,
    x: usize,
    y: usize,
    references: &Image<i32>,
    property_buffer: &mut [i32],
) -> i64 {
    let PredictionData {
        left,
        top,
        toptop,
        topleft,
        topright,
        leftleft,
        toprightright: _,
    } = prediction_data;

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
    let (wp_pred, wp_prop) = wp_state
        .map(|wp_state| wp_state.predict_and_property((x, y), xsize, &prediction_data))
        .unwrap_or((0, 0));
    property_buffer[15] = wp_prop;

    // Reference properties.
    let num_refs = references.size().0;
    if num_refs != 0 {
        let ref_properties = &mut property_buffer[NUM_NONREF_PROPERTIES..];
        ref_properties[..num_refs].copy_from_slice(&references.row(x)[..num_refs]);
    }

    wp_pred
}

/// Prediction using standard tree traversal.
/// Used for small channels where building a flat tree isn't worth it.
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
    let wp_pred = compute_properties(
        prediction_data,
        xsize,
        wp_state,
        x,
        y,
        references,
        property_buffer,
    );

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

/// Optimized prediction using flat tree (matches C++ context_predict.h:351-371).
#[inline]
#[allow(clippy::too_many_arguments)]
pub(super) fn predict_flat(
    flat_tree: &[FlatTreeNode],
    prediction_data: PredictionData,
    xsize: usize,
    wp_state: Option<&mut WeightedPredictorState>,
    x: usize,
    y: usize,
    references: &Image<i32>,
    property_buffer: &mut [i32],
) -> PredictionResult {
    let wp_pred = compute_properties(
        prediction_data,
        xsize,
        wp_state,
        x,
        y,
        references,
        property_buffer,
    );

    // Flat tree traversal
    let mut pos = 0;
    loop {
        let node = &flat_tree[pos];

        if node.property0 < 0 {
            // Leaf node
            let predictor = Predictor::try_from(node.splitval0_or_predictor as u32).unwrap();
            let offset = node.properties_or_offset[0] as i32;
            let multiplier = node.splitvals_or_multiplier[0] as u32;
            let context = node.child_id;

            let pred = predictor.predict_one(prediction_data, wp_pred);

            return PredictionResult {
                guess: pred + offset as i64,
                multiplier,
                context,
            };
        }

        // Split node: C++ logic from context_predict.h:361-365
        let p0 = property_buffer[node.property0 as usize] <= node.splitval0_or_predictor;
        let off0 = if property_buffer[node.properties_or_offset[0] as usize]
            <= node.splitvals_or_multiplier[0]
        {
            1
        } else {
            0
        };
        let off1 = if property_buffer[node.properties_or_offset[1] as usize]
            <= node.splitvals_or_multiplier[1]
        {
            3
        } else {
            2
        };

        pos = (node.child_id + if p0 { off1 } else { off0 }) as usize;
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

    /// Build flat tree using BFS traversal (matches C++ encoding.cc:81-144).
    /// Each flat node stores parent + both children info to reduce branches.
    pub(super) fn build_flat_tree(nodes: &[TreeNode]) -> Result<Vec<FlatTreeNode>> {
        use std::collections::VecDeque;

        if nodes.is_empty() {
            return Ok(vec![]);
        }

        let mut flat_nodes = Vec::new_with_capacity(nodes.len())?;
        let mut queue: VecDeque<usize> = VecDeque::new();
        queue.push_back(0); // Start with root

        while let Some(cur_idx) = queue.pop_front() {
            match &nodes[cur_idx] {
                TreeNode::Leaf {
                    predictor,
                    offset,
                    multiplier,
                    id,
                } => {
                    flat_nodes.push(FlatTreeNode::leaf(*predictor, *offset, *multiplier, *id));
                }
                TreeNode::Split {
                    property,
                    val,
                    left,
                    right,
                } => {
                    // childID points to first of 4 grandchildren in output
                    let child_id = (flat_nodes.len() + queue.len() + 1) as u32;

                    let mut flat = FlatTreeNode {
                        property0: *property as i32,
                        splitval0_or_predictor: *val,
                        splitvals_or_multiplier: [0, 0],
                        child_id,
                        properties_or_offset: [0, 0],
                    };

                    // Process left (i=0) and right (i=1) children
                    for (i, &child_idx) in [*left as usize, *right as usize].iter().enumerate() {
                        match &nodes[child_idx] {
                            TreeNode::Leaf { .. } => {
                                // Child is leaf: set property=0 and enqueue leaf twice
                                flat.properties_or_offset[i] = 0;
                                flat.splitvals_or_multiplier[i] = 0;
                                queue.push_back(child_idx);
                                queue.push_back(child_idx);
                            }
                            TreeNode::Split {
                                property: cp,
                                val: cv,
                                left: cl,
                                right: cr,
                            } => {
                                // Child is split: store property/splitval and enqueue grandchildren
                                flat.properties_or_offset[i] = *cp as i16;
                                flat.splitvals_or_multiplier[i] = *cv;
                                queue.push_back(*cl as usize);
                                queue.push_back(*cr as usize);
                            }
                        }
                    }

                    flat_nodes.push(flat);
                }
            }
        }

        Ok(flat_nodes)
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
