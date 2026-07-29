// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

use super::{Predictor, predict::WeightedPredictorState};
use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::{Histograms, SymbolReader},
    error::{Error, Result},
    frame::modular::predict::PredictionData,
    image::Image,
    util::tracing_wrappers::*,
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
    pub num_properties: usize,
}

fn validate_tree(tree: &[TreeNode], num_properties: usize) -> Result<()> {
    const HEIGHT_LIMIT: usize = 2048;

    if tree.is_empty() {
        return Ok(());
    }

    // This mirrors libjxl's ValidateTree(), but avoids allocating
    // `num_properties * tree.len()` entries.
    //
    // We do an explicit DFS and keep the property ranges only for the current root->node path.
    // When descending into a child we update exactly one property's range (the one we split on)
    // and store the previous range in the child frame; when returning from that child we restore
    // it. This makes memory O(num_properties + height) instead of O(num_properties * tree_size).

    #[derive(Clone, Copy, Debug)]
    enum Stage {
        Enter,
        AfterLeft,
        AfterRight,
    }

    struct Frame {
        node: usize,
        depth: usize,
        stage: Stage,
        restore: Option<(usize, (i32, i32))>,
    }

    let mut property_ranges: Vec<(i32, i32)> = vec![(i32::MIN, i32::MAX); num_properties];
    let mut stack = vec![Frame {
        node: 0,
        depth: 0,
        stage: Stage::Enter,
        restore: None,
    }];

    while let Some(mut frame) = stack.pop() {
        if frame.depth > HEIGHT_LIMIT {
            return Err(Error::TreeTooTall(frame.depth, HEIGHT_LIMIT));
        }

        match (frame.stage, tree[frame.node]) {
            (Stage::Enter, TreeNode::Leaf { .. }) => {
                if let Some((p, old)) = frame.restore {
                    property_ranges[p] = old;
                }
            }
            (
                Stage::Enter,
                TreeNode::Split {
                    property,
                    val,
                    left,
                    right: _,
                },
            ) => {
                let p = property as usize;
                let (l, u) = property_ranges[p];
                if l > val || u <= val {
                    return Err(Error::TreeSplitOnEmptyRange(property, val, l, u));
                }

                frame.stage = Stage::AfterLeft;
                let depth = frame.depth;
                stack.push(frame);

                // Descend into left child: range becomes (val+1, u).
                let old = property_ranges[p];
                property_ranges[p] = (val + 1, u);
                stack.push(Frame {
                    node: left as usize,
                    depth: depth + 1,
                    stage: Stage::Enter,
                    restore: Some((p, old)),
                });
            }
            (
                Stage::AfterLeft,
                TreeNode::Split {
                    property,
                    val,
                    left: _,
                    right,
                },
            ) => {
                let p = property as usize;
                let (l, u) = property_ranges[p];
                if l > val || u <= val {
                    return Err(Error::TreeSplitOnEmptyRange(property, val, l, u));
                }

                frame.stage = Stage::AfterRight;
                let depth = frame.depth;
                stack.push(frame);

                // Descend into right child: range becomes (l, val).
                let old = property_ranges[p];
                property_ranges[p] = (l, val);
                stack.push(Frame {
                    node: right as usize,
                    depth: depth + 1,
                    stage: Stage::Enter,
                    restore: Some((p, old)),
                });
            }
            (Stage::AfterRight, TreeNode::Split { .. }) => {
                if let Some((p, old)) = frame.restore {
                    property_ranges[p] = old;
                }
            }
            _ => unreachable!("invalid tree validation state"),
        }
    }

    Ok(())
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
#[inline(always)]
pub(super) fn compute_properties(
    prediction_data: PredictionData,
    wp_state: Option<&mut WeightedPredictorState>,
    x: usize,
    y: usize,
    references: &Image<i32>,
    property_buffer: &mut [i32],
) -> i64 {
    assert!(property_buffer.len() >= NUM_NONREF_PROPERTIES);

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
        .map(|wp_state| wp_state.predict_and_property((x, y), &prediction_data))
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
#[instrument(level = "trace", ret)]
#[allow(clippy::too_many_arguments)]
pub(super) fn predict(
    tree: &Tree,
    prediction_data: PredictionData,
    wp_state: Option<&mut WeightedPredictorState>,
    x: usize,
    y: usize,
    references: &Image<i32>,
    property_buffer: &mut [i32],
) -> PredictionResult {
    let wp_pred = compute_properties(prediction_data, wp_state, x, y, references, property_buffer);

    trace!(?property_buffer, "new properties");

    let TreeNode::Leaf {
        predictor,
        offset,
        multiplier,
        id,
    } = tree.walk(property_buffer)
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
        validate_tree(&tree, num_properties)?;

        let histograms = Histograms::decode(tree.len().div_ceil(2), br, true)?;

        Ok(Tree {
            nodes: tree,
            histograms,
            num_properties,
        })
    }

    pub(super) fn walk(&self, properties: &[i32]) -> TreeNode {
        let mut tree_node = 0;
        while let TreeNode::Split {
            property,
            val,
            left,
            right,
        } = self.nodes[tree_node]
        {
            if properties[property as usize] > val {
                trace!(
                    "left at node {tree_node} [{} > {val}]",
                    properties[property as usize]
                );
                tree_node = left as usize;
            } else {
                trace!(
                    "right at node {tree_node} [{} <= {val}]",
                    properties[property as usize]
                );
                tree_node = right as usize;
            }
        }

        trace!(leaf = ?self.nodes[tree_node]);
        self.nodes[tree_node]
    }
}
