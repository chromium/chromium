use alloc::boxed::Box;
use core::cmp::{max, Ord};
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

use crate::interval::Interval;

#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub(crate) struct Node<T: Ord + Clone, V> {
    pub interval: Interval<T>,
    pub value: Option<V>,
    pub max: T,
    pub height: usize,
    pub size: usize,
    pub left_child: Option<Box<Node<T, V>>>,
    pub right_child: Option<Box<Node<T, V>>>,
}

impl<T: Ord + Clone, V> Node<T, V> {
    pub fn new<R: Into<Interval<T>>>(
        interval: R,
        value: V,
        max: T,
        height: usize,
        size: usize,
    ) -> Node<T, V> {
        Node {
            interval: interval.into(),
            value: Some(value),
            max,
            height,
            size,
            left_child: None,
            right_child: None,
        }
    }

    pub fn balance_factor(&self) -> i64 {
        Node::height(&self.left_child) - Node::height(&self.right_child)
    }

    // _max_height is at least -1, so +1 is a least 0 - and it can never be higher than usize
    #[allow(clippy::cast_sign_loss, clippy::cast_possible_truncation)]
    pub fn update_height(&mut self) {
        self.height = (1 + Node::max_height(&self.left_child, &self.right_child)) as usize;
    }

    pub fn update_size(&mut self) {
        self.size = 1 + Node::size(&self.left_child) + Node::size(&self.right_child);
    }

    pub fn update_max(&mut self) {
        self.max = match (&self.left_child, &self.right_child) {
            (Some(left_child), Some(right_child)) => max(
                self.interval.end.clone(),
                max(left_child.max.clone(), right_child.max.clone()),
            ),
            (Some(left_child), None) => max(self.interval.end.clone(), left_child.max.clone()),
            (None, Some(right_child)) => max(self.interval.end.clone(), right_child.max.clone()),
            (None, None) => self.interval.end.clone(),
        };
    }

    pub fn max_height(node1: &Option<Box<Node<T, V>>>, node2: &Option<Box<Node<T, V>>>) -> i64 {
        max(Node::height(node1), Node::height(node2))
    }

    pub fn height(node: &Option<Box<Node<T, V>>>) -> i64 {
        match node {
            Some(node) => node.height as i64,
            None => -1,
        }
    }

    pub fn size(node: &Option<Box<Node<T, V>>>) -> usize {
        match node {
            Some(node) => node.size,
            None => 0,
        }
    }
}
