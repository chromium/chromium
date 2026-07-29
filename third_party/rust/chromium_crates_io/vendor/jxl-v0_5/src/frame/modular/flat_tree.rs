// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::fmt::Debug;

use super::{Predictor, predict::WeightedPredictorState};
use crate::{
    error::Result,
    frame::modular::{
        Tree,
        predict::PredictionData,
        tree::{PredictionResult, TreeNode, compute_properties},
    },
    image::Image,
    util::NewWithCapacity,
};

/// Flattened tree node for optimized traversal.
/// Stores parent + info about both children to evaluate 3 nodes per iteration.
#[derive(Debug, Clone, Copy)]
pub(super) enum FlatTreeNode {
    Split {
        properties: [u8; 3],
        splitvals: [i32; 3],
        child_id: u32,
    },
    Leaf {
        predictor: Predictor,
        multiplier: u32,
        context: u32,
        offset: i32,
    },
}

#[inline(always)]
#[allow(clippy::too_many_arguments, unsafe_code)]
pub(super) fn predict_flat(
    flat_tree: &[FlatTreeNode],
    prediction_data: PredictionData,
    wp_state: Option<&mut WeightedPredictorState>,
    pos: (usize, usize),
    references: &Image<i32>,
    property_buffer: &mut [i32; 256],
) -> PredictionResult {
    let wp_pred = compute_properties(
        prediction_data,
        wp_state,
        pos.0,
        pos.1,
        references,
        property_buffer,
    );

    let mut pos = 0;
    loop {
        // Removing this bound check doesn't seem to have a significant effect.
        let node = flat_tree[pos];
        match node {
            FlatTreeNode::Split {
                properties,
                splitvals,
                child_id,
            } => {
                // This bound check is elided by virtue of `property_buffer` having 256 elements.
                let props = properties.map(|x| property_buffer[x as usize]);
                let p0 = props[0] <= splitvals[0];
                let p1 = props[1] <= splitvals[1];
                let p2 = props[2] <= splitvals[2];
                pos = child_id as usize + if p0 { 2 | p2 as usize } else { p1 as usize };
            }
            FlatTreeNode::Leaf {
                predictor,
                multiplier,
                context,
                offset,
            } => {
                let pred = predictor.predict_one(prediction_data, wp_pred);
                return PredictionResult {
                    guess: pred + offset as i64,
                    multiplier,
                    context,
                };
            }
        };
    }
}

impl Tree {
    /// Build flat tree using BFS traversal.
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
            match nodes[cur_idx] {
                TreeNode::Leaf {
                    predictor,
                    offset,
                    multiplier,
                    id,
                } => {
                    flat_nodes.push(FlatTreeNode::Leaf {
                        predictor,
                        offset,
                        multiplier,
                        context: id,
                    });
                }
                TreeNode::Split {
                    property,
                    val,
                    left,
                    right,
                } => {
                    // childID points to first of 4 grandchildren in output
                    let child_id = (flat_nodes.len() + queue.len() + 1) as u32;

                    let mut splitvals = [val, 0, 0];
                    let mut properties = [property, 0, 0];

                    // Process left (i=0) and right (i=1) children
                    for (i, &child_idx) in [left as usize, right as usize].iter().enumerate() {
                        match &nodes[child_idx] {
                            TreeNode::Leaf { .. } => {
                                // Child is leaf: enqueue leaf twice
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
                                properties[i + 1] = *cp;
                                splitvals[i + 1] = *cv;
                                queue.push_back(*cl as usize);
                                queue.push_back(*cr as usize);
                            }
                        }
                    }

                    flat_nodes.push(FlatTreeNode::Split {
                        properties,
                        splitvals,
                        child_id,
                    });
                }
            }
        }

        Ok(flat_nodes)
    }
}
