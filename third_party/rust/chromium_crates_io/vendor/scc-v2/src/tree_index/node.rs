use super::internal_node::{self, InternalNode};
use super::leaf::{InsertResult, Leaf, RemoveResult, Scanner};
use super::leaf_node::{self, LeafNode};
use crate::ebr::{AtomicShared, Guard, Ptr, Shared, Tag};
use crate::wait_queue::DeriveAsyncWait;
use crate::Comparable;
use std::fmt::{self, Debug};
use std::ops::RangeBounds;
use std::sync::atomic::Ordering::{AcqRel, Acquire, Relaxed};

/// [`Node`] is either [`Self::Internal`] or [`Self::Leaf`].
pub enum Node<K, V> {
    /// Internal node.
    Internal(InternalNode<K, V>),

    /// Leaf node.
    Leaf(LeafNode<K, V>),
}

impl<K, V> Node<K, V> {
    /// Creates a new [`InternalNode`].
    #[inline]
    pub(super) fn new_internal_node() -> Self {
        Self::Internal(InternalNode::new())
    }

    /// Creates a new [`LeafNode`].
    #[inline]
    pub(super) fn new_leaf_node() -> Self {
        Self::Leaf(LeafNode::new())
    }

    /// Clears the node.
    #[inline]
    pub(super) fn clear(&self, guard: &Guard) {
        match &self {
            Self::Internal(internal_node) => internal_node.clear(guard),
            Self::Leaf(leaf_node) => leaf_node.clear(guard),
        }
    }

    /// Returns the depth of the node.
    #[inline]
    pub(super) fn depth(&self, depth: usize, guard: &Guard) -> usize {
        match &self {
            Self::Internal(internal_node) => internal_node.depth(depth, guard),
            Self::Leaf(_) => depth,
        }
    }

    /// Checks if the node has retired.
    #[inline]
    pub(super) fn retired(&self) -> bool {
        match &self {
            Self::Internal(internal_node) => internal_node.retired(),
            Self::Leaf(leaf_node) => leaf_node.retired(),
        }
    }
}

impl<K, V> Node<K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
    /// Searches for an entry containing the specified key.
    #[inline]
    pub(super) fn search_entry<'g, Q>(&self, key: &Q, guard: &'g Guard) -> Option<(&'g K, &'g V)>
    where
        K: 'g,
        Q: Comparable<K> + ?Sized,
    {
        match &self {
            Self::Internal(internal_node) => internal_node.search_entry(key, guard),
            Self::Leaf(leaf_node) => leaf_node.search_entry(key, guard),
        }
    }

    /// Searches for the value associated with the specified key.
    #[inline]
    pub(super) fn search_value<'g, Q>(&self, key: &Q, guard: &'g Guard) -> Option<&'g V>
    where
        K: 'g,
        Q: Comparable<K> + ?Sized,
    {
        match &self {
            Self::Internal(internal_node) => internal_node.search_value(key, guard),
            Self::Leaf(leaf_node) => leaf_node.search_value(key, guard),
        }
    }

    /// Returns the minimum key-value pair.
    ///
    /// This method is not linearizable.
    #[inline]
    pub(super) fn min<'g>(&self, guard: &'g Guard) -> Option<Scanner<'g, K, V>> {
        match &self {
            Self::Internal(internal_node) => internal_node.min(guard),
            Self::Leaf(leaf_node) => leaf_node.min(guard),
        }
    }

    /// Returns a [`Scanner`] pointing to an entry that is close enough to the entry with the
    /// maximum key among those keys smaller than or equal to the given key.
    ///
    /// This method is not linearizable.
    #[inline]
    pub(super) fn max_le_appr<'g, Q>(&self, key: &Q, guard: &'g Guard) -> Option<Scanner<'g, K, V>>
    where
        K: 'g,
        Q: Comparable<K> + ?Sized,
    {
        match &self {
            Self::Internal(internal_node) => internal_node.max_le_appr(key, guard),
            Self::Leaf(leaf_node) => leaf_node.max_le_appr(key, guard),
        }
    }

    /// Inserts a key-value pair.
    #[inline]
    pub(super) fn insert<D: DeriveAsyncWait>(
        &self,
        key: K,
        val: V,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<InsertResult<K, V>, (K, V)> {
        match &self {
            Self::Internal(internal_node) => internal_node.insert(key, val, async_wait, guard),
            Self::Leaf(leaf_node) => leaf_node.insert(key, val, async_wait, guard),
        }
    }

    /// Removes an entry associated with the given key.
    #[inline]
    pub(super) fn remove_if<Q, F: FnMut(&V) -> bool, D>(
        &self,
        key: &Q,
        condition: &mut F,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<RemoveResult, ()>
    where
        Q: Comparable<K> + ?Sized,
        D: DeriveAsyncWait,
    {
        match &self {
            Self::Internal(internal_node) => {
                internal_node.remove_if::<_, _, _>(key, condition, async_wait, guard)
            }
            Self::Leaf(leaf_node) => {
                leaf_node.remove_if::<_, _, _>(key, condition, async_wait, guard)
            }
        }
    }

    /// Removes a range of entries.
    ///
    /// Returns the number of remaining children.
    #[inline]
    pub(super) fn remove_range<'g, Q, R: RangeBounds<Q>, D: DeriveAsyncWait>(
        &self,
        range: &R,
        start_unbounded: bool,
        valid_lower_max_leaf: Option<&'g Leaf<K, V>>,
        valid_upper_min_node: Option<&'g Node<K, V>>,
        async_wait: &mut D,
        guard: &'g Guard,
    ) -> Result<usize, ()>
    where
        Q: Comparable<K> + ?Sized,
    {
        match &self {
            Self::Internal(internal_node) => internal_node.remove_range(
                range,
                start_unbounded,
                valid_lower_max_leaf,
                valid_upper_min_node,
                async_wait,
                guard,
            ),
            Self::Leaf(leaf_node) => leaf_node.remove_range(
                range,
                start_unbounded,
                valid_lower_max_leaf,
                valid_upper_min_node,
                async_wait,
                guard,
            ),
        }
    }

    /// Splits the current root node.
    #[inline]
    pub(super) fn split_root(
        root_ptr: Ptr<Node<K, V>>,
        root: &AtomicShared<Node<K, V>>,
        key: K,
        val: V,
        guard: &Guard,
    ) -> (K, V) {
        // The fact that the `TreeIndex` calls this function means the root is full and locked.
        let mut new_root = Shared::new(Node::new_internal_node());
        if let (Some(Self::Internal(internal_node)), Some(old_root)) = (
            unsafe { new_root.get_mut() },
            root.get_shared(Relaxed, guard),
        ) {
            internal_node.unbounded_child = AtomicShared::from(old_root);
            let result = internal_node.split_node(
                key,
                val,
                None,
                root_ptr,
                &internal_node.unbounded_child,
                true,
                &mut (),
                guard,
            );
            let Ok(InsertResult::Retry(key, val)) = result else {
                unreachable!()
            };

            // Updates the pointer before unlocking the root.
            match root.compare_exchange(
                root_ptr,
                (Some(new_root), Tag::None),
                AcqRel,
                Acquire,
                guard,
            ) {
                Ok((old_root, new_root_ptr)) => {
                    if let Some(Self::Internal(internal_node)) = new_root_ptr.as_ref() {
                        internal_node.finish_split();
                    }
                    if let Some(old_root) = old_root {
                        old_root.commit(guard);
                    }
                }
                Err((new_root, old_root_ptr)) => {
                    // The root has been cleared.
                    if let Some(Self::Internal(internal_node)) = new_root.as_deref() {
                        internal_node.finish_split();
                    }
                    if let Some(old_root) = old_root_ptr.as_ref() {
                        old_root.rollback(guard);
                    }
                }
            }
            (key, val)
        } else {
            // The root has been cleared.
            if let Some(old_root) = root_ptr.as_ref() {
                old_root.rollback(guard);
            }
            (key, val)
        }
    }

    /// Cleans up or removes the current root node.
    ///
    /// If the root is empty, the root is removed from the tree, or if the root has only a single
    /// child, the root is replaced with the child.
    ///
    /// Returns `false` if a conflict is detected.
    #[inline]
    pub(super) fn cleanup_root<D: DeriveAsyncWait>(
        root: &AtomicShared<Node<K, V>>,
        async_wait: &mut D,
        guard: &Guard,
    ) -> bool {
        let mut root_ptr = root.load(Acquire, guard);
        while let Some(root_ref) = root_ptr.as_ref() {
            let mut internal_node_locker = None;
            let mut leaf_node_locker = None;
            match root_ref {
                Self::Internal(internal_node) => {
                    if !internal_node.retired() && !internal_node.children.is_empty() {
                        // The internal node is still usable.
                        break;
                    } else if let Some(locker) = internal_node::Locker::try_lock(internal_node) {
                        internal_node_locker.replace(locker);
                    } else {
                        internal_node.wait(async_wait);
                    }
                }
                Self::Leaf(leaf_node) => {
                    if !leaf_node.retired() {
                        // The leaf node is still usable.
                        break;
                    } else if let Some(locker) = leaf_node::Locker::try_lock(leaf_node) {
                        leaf_node_locker.replace(locker);
                    } else {
                        leaf_node.wait(async_wait);
                    }
                }
            }

            if internal_node_locker.is_none() && leaf_node_locker.is_none() {
                // The root node is locked by another thread.
                return false;
            }

            let new_root = match root_ref {
                Node::Internal(internal_node) => {
                    if internal_node.retired() {
                        // The internal node is empty, therefore the entire tree can be emptied.
                        None
                    } else if internal_node.children.is_empty() {
                        // Replace the root with the unbounded child.
                        internal_node.unbounded_child.get_shared(Acquire, guard)
                    } else {
                        // The internal node is not empty.
                        break;
                    }
                }
                Node::Leaf(leaf_node) => {
                    if leaf_node.retired() {
                        // The leaf node is empty, therefore the entire tree can be emptied.
                        None
                    } else {
                        // The leaf node is not empty.
                        break;
                    }
                }
            };

            match root.compare_exchange(root_ptr, (new_root, Tag::None), AcqRel, Acquire, guard) {
                Ok((_, new_root_ptr)) => {
                    root_ptr = new_root_ptr;
                    if let Some(internal_node_locker) = internal_node_locker {
                        internal_node_locker.unlock_retire();
                    }
                }
                Err((_, new_root_ptr)) => {
                    // The root node has been changed.
                    root_ptr = new_root_ptr;
                }
            }
        }

        true
    }

    /// Commits an on-going structural change.
    #[inline]
    pub(super) fn commit(&self, guard: &Guard) {
        match &self {
            Self::Internal(internal_node) => internal_node.commit(guard),
            Self::Leaf(leaf_node) => leaf_node.commit(guard),
        }
    }

    /// Rolls back an on-going structural change.
    #[inline]
    pub(super) fn rollback(&self, guard: &Guard) {
        match &self {
            Self::Internal(internal_node) => internal_node.rollback(guard),
            Self::Leaf(leaf_node) => leaf_node.rollback(guard),
        }
    }

    /// Cleans up logically deleted [`LeafNode`] instances in the linked list.
    ///
    /// If the target leaf node does not exist in the sub-tree, returns `false`.
    #[inline]
    pub(super) fn cleanup_link<'g, Q>(&self, key: &Q, traverse_max: bool, guard: &'g Guard) -> bool
    where
        K: 'g,
        Q: Comparable<K> + ?Sized,
    {
        match &self {
            Self::Internal(internal_node) => internal_node.cleanup_link(key, traverse_max, guard),
            Self::Leaf(leaf_node) => leaf_node.cleanup_link(key, traverse_max, guard),
        }
    }
}

impl<K, V> Debug for Node<K, V> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Internal(_) => f.debug_tuple("Internal").finish(),
            Self::Leaf(_) => f.debug_tuple("Leaf").finish(),
        }
    }
}
