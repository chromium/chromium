//! Cycle detection for depth first traversals of a graph.

use core::ops::{Deref, DerefMut};

#[derive(Copy, Clone, Debug)]
pub(crate) enum DecyclerError {
    DepthLimitExceeded,
    CycleDetected,
}

/// Cycle detector for a depth first traversal of a graph.
///
/// The graph is expected to have unique node identifiers of type `T`, and
/// the traversal is limited to a depth of `D`.
///
/// Based on `hb_decycler_t` in HarfBuzz
/// (<https://github.com/harfbuzz/harfbuzz/blob/a2ea5d28cb5387f4de2049802474b817be15ad5b/src/hb-decycler.hh>),
/// an extension of Floyd's tortoise and hare algorithm
/// (<https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_tortoise_and_hare>)
/// to depth first traversals.
///
/// Unlike HarfBuzz, which supports traversals of any depth with a linked
/// list on the stack, this stores the chain in a fixed array indexed by
/// depth. This imposes the depth limit, which is also needed to prevent
/// stack overflow.
pub(crate) struct Decycler<T, const D: usize> {
    node_ids: [T; D],
    depth: usize,
}

impl<T, const D: usize> Decycler<T, D>
where
    T: Copy + PartialEq + Default,
{
    pub(super) fn new() -> Self {
        Self {
            node_ids: [T::default(); D],
            depth: 0,
        }
    }

    /// Enters the node uniquely identified by `node_id`.
    ///
    /// Returns an error if a cycle is detected or the depth limit is reached.
    /// Otherwise, increases the depth and returns a guard that decreases it
    /// when dropped.
    ///
    /// The guard dereferences to the decycler, so a recursive traversal can
    /// pass it down to check child nodes.
    pub(super) fn enter(&mut self, node_id: T) -> Result<DecyclerGuard<'_, T, D>, DecyclerError> {
        if self.depth < D {
            if self.depth == 0 || self.node_ids[self.depth / 2] != node_id {
                self.node_ids[self.depth] = node_id;
                self.depth += 1;
                Ok(DecyclerGuard { decycler: self })
            } else {
                Err(DecyclerError::CycleDetected)
            }
        } else {
            Err(DecyclerError::DepthLimitExceeded)
        }
    }
}

impl<T, const D: usize> Default for Decycler<T, D>
where
    T: Copy + PartialEq + Default,
{
    fn default() -> Self {
        Self::new()
    }
}

pub(crate) struct DecyclerGuard<'a, T, const D: usize> {
    decycler: &'a mut Decycler<T, D>,
}

impl<T, const D: usize> Deref for DecyclerGuard<'_, T, D> {
    type Target = Decycler<T, D>;

    fn deref(&self) -> &Self::Target {
        self.decycler
    }
}

impl<T, const D: usize> DerefMut for DecyclerGuard<'_, T, D> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.decycler
    }
}

impl<T, const D: usize> Drop for DecyclerGuard<'_, T, D> {
    fn drop(&mut self) {
        self.decycler.depth -= 1;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::vec::Vec;

    #[test]
    fn graph_with_cycles() {
        let tree = Tree {
            nodes: alloc::vec![
                Node::new(alloc::vec![1, 2]),
                Node::new(alloc::vec![2, 3]),
                Node::new(alloc::vec![]),
                Node::new(alloc::vec![0, 1]),
            ],
        };
        let result = tree.traverse(&mut TestDecycler::new());
        assert!(matches!(result, Err(DecyclerError::CycleDetected)));
    }

    #[test]
    fn exceeds_max_depth() {
        let mut nodes = (0..MAX_DEPTH)
            .map(|ix| Node::new(alloc::vec![ix + 1]))
            .collect::<Vec<_>>();
        nodes.push(Node::new(alloc::vec![]));
        let tree = Tree { nodes };
        let result = tree.traverse(&mut TestDecycler::new());
        assert!(matches!(result, Err(DecyclerError::DepthLimitExceeded)));
    }

    #[test]
    fn well_formed_tree() {
        let mut nodes = (0..MAX_DEPTH - 1)
            .map(|ix| Node::new(alloc::vec![ix + 1]))
            .collect::<Vec<_>>();
        nodes.push(Node::new(alloc::vec![]));
        let tree = Tree { nodes };
        let result = tree.traverse(&mut TestDecycler::new());
        assert!(result.is_ok());
    }

    const MAX_DEPTH: usize = 64;
    type TestDecycler = Decycler<usize, MAX_DEPTH>;

    struct Node {
        child_ids: Vec<usize>,
    }

    impl Node {
        fn new(child_ids: Vec<usize>) -> Self {
            Self { child_ids }
        }
    }

    struct Tree {
        nodes: Vec<Node>,
    }

    impl Tree {
        fn traverse(&self, decycler: &mut TestDecycler) -> Result<(), DecyclerError> {
            self.traverse_impl(decycler, 0)
        }

        fn traverse_impl(
            &self,
            decycler: &mut TestDecycler,
            node_id: usize,
        ) -> Result<(), DecyclerError> {
            let mut cycle_guard = decycler.enter(node_id)?;
            let node = &self.nodes[node_id];
            for child_id in &node.child_ids {
                self.traverse_impl(&mut cycle_guard, *child_id)?;
            }
            Ok(())
        }
    }
}
