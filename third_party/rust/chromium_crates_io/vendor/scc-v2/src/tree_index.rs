//! [`TreeIndex`] is a read-optimized concurrent and asynchronous B-plus tree.

mod internal_node;
mod leaf;
mod leaf_node;
mod node;

use crate::ebr::{AtomicShared, Guard, Ptr, Shared, Tag};
use crate::wait_queue::AsyncWait;
use crate::Comparable;
use leaf::{InsertResult, Leaf, RemoveResult, Scanner};
use node::Node;
use std::fmt::{self, Debug};
use std::iter::FusedIterator;
use std::marker::PhantomData;
use std::ops::Bound::{Excluded, Included, Unbounded};
use std::ops::RangeBounds;
use std::panic::UnwindSafe;
use std::pin::Pin;
use std::sync::atomic::Ordering::{AcqRel, Acquire};

/// Scalable concurrent B-plus tree.
///
/// [`TreeIndex`] is a concurrent and asynchronous B-plus tree variant optimized for read
/// operations. Read operations, such as read iteration over entries, are neither blocked nor
/// interrupted by other threads or tasks. Write operations, such as insert and remove, do not block
/// if structural changes are not required.
///
/// ## Notes
///
/// [`TreeIndex`] methods are linearizable. However, its iterator methods are not; [`Iter`] and
/// [`Range`] are only guaranteed to observe events that happened before the first call to
/// [`Iterator::next`].
///
/// ## The key features of [`TreeIndex`]
///
/// * Lock-free-read: read and scan operations are never blocked and do not modify shared data.
/// * Near lock-free write: write operations do not block unless a structural change is needed.
/// * No busy waiting: each node has a wait queue to avoid spinning.
/// * Immutability: the data in the container is immutable until it becomes unreachable.
///
/// ## The key statistics for [`TreeIndex`]
///
/// * The maximum number of entries that a leaf can contain: 14.
/// * The maximum number of leaves or child nodes a node can contain: 15.
///
/// ## Locking behavior
///
/// Read access is always lock-free and non-blocking. Write access to an entry is also lock-free and
/// non-blocking as long as no structural changes are required. However, when nodes are split or
/// merged by a write operation, other write operations on keys in the affected range are blocked.
///
/// ### Unwind safety
///
/// [`TreeIndex`] is impervious to out-of-memory errors and panics in user-specified code on one
/// condition; `K::drop` and `V::drop` must not panic.
pub struct TreeIndex<K, V> {
    root: AtomicShared<Node<K, V>>,
}

/// An iterator over the entries of a [`TreeIndex`].
///
/// An [`Iter`] iterates over all the entries that survive the [`Iter`] in monotonically increasing
/// order.
pub struct Iter<'t, 'g, K, V> {
    root: &'t AtomicShared<Node<K, V>>,
    leaf_scanner: Option<Scanner<'g, K, V>>,
    guard: &'g Guard,
}

/// An iterator over a sub-range of entries in a [`TreeIndex`].
pub struct Range<'t, 'g, K, V, Q: ?Sized, R: RangeBounds<Q>> {
    root: &'t AtomicShared<Node<K, V>>,
    leaf_scanner: Option<Scanner<'g, K, V>>,
    bounds: R,
    check_lower_bound: bool,
    check_upper_bound: bool,
    guard: &'g Guard,
    query: PhantomData<fn() -> Q>,
}

impl<K, V> TreeIndex<K, V> {
    /// Creates an empty [`TreeIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    /// ```
    #[cfg(not(feature = "loom"))]
    #[inline]
    #[must_use]
    pub const fn new() -> Self {
        Self {
            root: AtomicShared::null(),
        }
    }

    /// Creates an empty [`TreeIndex`].
    #[cfg(feature = "loom")]
    #[inline]
    #[must_use]
    pub fn new() -> Self {
        Self {
            root: AtomicShared::null(),
        }
    }

    /// Clears the [`TreeIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// treeindex.clear();
    /// assert_eq!(treeindex.len(), 0);
    /// ```
    #[inline]
    pub fn clear(&self) {
        if let (Some(root), _) = self.root.swap((None, Tag::None), Acquire) {
            root.clear(&Guard::new());
        }
    }

    /// Returns the depth of the [`TreeIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    /// assert_eq!(treeindex.depth(), 0);
    /// ```
    #[inline]
    #[must_use]
    pub fn depth(&self) -> usize {
        let guard = Guard::new();
        self.root
            .load(Acquire, &guard)
            .as_ref()
            .map_or(0, |root_ref| root_ref.depth(1, &guard))
    }
}

impl<K, V> TreeIndex<K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
    /// Inserts a key-value pair.
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key-value pair if the key exists.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// assert!(treeindex.insert(1, 10).is_ok());
    /// assert_eq!(treeindex.insert(1, 11).err().unwrap(), (1, 11));
    /// assert_eq!(treeindex.peek_with(&1, |k, v| *v).unwrap(), 10);
    /// ```
    #[inline]
    pub fn insert(&self, mut key: K, mut val: V) -> Result<(), (K, V)> {
        let mut new_root = None;
        loop {
            let guard = Guard::new();
            let root_ptr = self.root.load(Acquire, &guard);
            if let Some(root_ref) = root_ptr.as_ref() {
                match root_ref.insert(key, val, &mut (), &guard) {
                    Ok(r) => match r {
                        InsertResult::Success => return Ok(()),
                        InsertResult::Frozen(k, v) | InsertResult::Retry(k, v) => {
                            key = k;
                            val = v;
                            root_ref.cleanup_link(&key, false, &guard);
                        }
                        InsertResult::Duplicate(k, v) => return Err((k, v)),
                        InsertResult::Full(k, v) => {
                            let (k, v) = Node::split_root(root_ptr, &self.root, k, v, &guard);
                            key = k;
                            val = v;
                            continue;
                        }
                        InsertResult::Retired(k, v) => {
                            key = k;
                            val = v;
                            let _result = Node::cleanup_root(&self.root, &mut (), &guard);
                        }
                    },
                    Err((k, v)) => {
                        key = k;
                        val = v;
                    }
                }
            }

            let node = if let Some(new_root) = new_root.take() {
                new_root
            } else {
                Shared::new(Node::new_leaf_node())
            };
            if let Err((node, _)) = self.root.compare_exchange(
                Ptr::null(),
                (Some(node), Tag::None),
                AcqRel,
                Acquire,
                &guard,
            ) {
                new_root = node;
            }
        }
    }

    /// Inserts a key-value pair.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key-value pair if the key exists.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    /// let future_insert = treeindex.insert_async(1, 10);
    /// ```
    #[inline]
    pub async fn insert_async(&self, mut key: K, mut val: V) -> Result<(), (K, V)> {
        let mut new_root = None;
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);

            let need_await = {
                let guard = Guard::new();
                let root_ptr = self.root.load(Acquire, &guard);
                if let Some(root_ref) = root_ptr.as_ref() {
                    match root_ref.insert(key, val, &mut async_wait_pinned, &guard) {
                        Ok(r) => match r {
                            InsertResult::Success => return Ok(()),
                            InsertResult::Frozen(k, v) | InsertResult::Retry(k, v) => {
                                key = k;
                                val = v;
                                root_ref.cleanup_link(&key, false, &guard);
                                true
                            }
                            InsertResult::Duplicate(k, v) => return Err((k, v)),
                            InsertResult::Full(k, v) => {
                                let (k, v) = Node::split_root(root_ptr, &self.root, k, v, &guard);
                                key = k;
                                val = v;
                                continue;
                            }
                            InsertResult::Retired(k, v) => {
                                key = k;
                                val = v;
                                !Node::cleanup_root(&self.root, &mut async_wait_pinned, &guard)
                            }
                        },
                        Err((k, v)) => {
                            key = k;
                            val = v;
                            true
                        }
                    }
                } else {
                    false
                }
            };

            if need_await {
                async_wait_pinned.await;
            }

            let node = if let Some(new_root) = new_root.take() {
                new_root
            } else {
                Shared::new(Node::new_leaf_node())
            };
            if let Err((node, _)) = self.root.compare_exchange(
                Ptr::null(),
                (Some(node), Tag::None),
                AcqRel,
                Acquire,
                &Guard::new(),
            ) {
                new_root = node;
            }
        }
    }

    /// Removes a key-value pair.
    ///
    /// Returns `false` if the key does not exist.
    ///
    /// Returns `true` if the key existed and the condition was met after marking the entry
    /// unreachable; the memory will be reclaimed later.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// assert!(!treeindex.remove(&1));
    /// assert!(treeindex.insert(1, 10).is_ok());
    /// assert!(treeindex.remove(&1));
    /// ```
    #[inline]
    pub fn remove<Q>(&self, key: &Q) -> bool
    where
        Q: Comparable<K> + ?Sized,
    {
        self.remove_if(key, |_| true)
    }

    /// Removes a key-value pair.
    ///
    /// Returns `false` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// Returns `true` if the key existed and the condition was met after marking the entry
    /// unreachable; the memory will be reclaimed later.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    /// let future_remove = treeindex.remove_async(&1);
    /// ```
    #[inline]
    pub async fn remove_async<Q>(&self, key: &Q) -> bool
    where
        Q: Comparable<K> + ?Sized,
    {
        self.remove_if_async(key, |_| true).await
    }

    /// Removes a key-value pair if the given condition is met.
    ///
    /// Returns `false` if the key does not exist or the condition was not met.
    ///
    /// Returns `true` if the key existed and the condition was met after marking the entry
    /// unreachable; the memory will be reclaimed later.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// assert!(treeindex.insert(1, 10).is_ok());
    /// assert!(!treeindex.remove_if(&1, |v| *v == 0));
    /// assert!(treeindex.remove_if(&1, |v| *v == 10));
    /// ```
    #[inline]
    pub fn remove_if<Q, F: FnMut(&V) -> bool>(&self, key: &Q, mut condition: F) -> bool
    where
        Q: Comparable<K> + ?Sized,
    {
        let mut removed = false;
        loop {
            let guard = Guard::new();
            if let Some(root_ref) = self.root.load(Acquire, &guard).as_ref() {
                if let Ok(result) =
                    root_ref.remove_if::<_, _, _>(key, &mut condition, &mut (), &guard)
                {
                    if matches!(result, RemoveResult::Cleanup) {
                        root_ref.cleanup_link(key, false, &guard);
                    }
                    match result {
                        RemoveResult::Success => return true,
                        RemoveResult::Cleanup | RemoveResult::Retired => {
                            if Node::cleanup_root(&self.root, &mut (), &guard) {
                                return true;
                            }
                            removed = true;
                        }
                        RemoveResult::Fail => {
                            if removed {
                                if Node::cleanup_root(&self.root, &mut (), &guard) {
                                    return true;
                                }
                            } else {
                                return false;
                            }
                        }
                        RemoveResult::Frozen => (),
                    }
                }
            } else {
                return removed;
            }
        }
    }

    /// Removes a key-value pair if the given condition is met.
    ///
    /// Returns `false` if the key does not exist or the condition was not met. It is an
    /// asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// Returns `true` if the key existed and the condition was met after marking the entry
    /// unreachable; the memory will be reclaimed later.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    /// let future_remove = treeindex.remove_if_async(&1, |v| *v == 0);
    /// ```
    #[inline]
    pub async fn remove_if_async<Q, F: FnMut(&V) -> bool>(&self, key: &Q, mut condition: F) -> bool
    where
        Q: Comparable<K> + ?Sized,
    {
        let mut removed = false;
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            {
                let guard = Guard::new();
                if let Some(root_ref) = self.root.load(Acquire, &guard).as_ref() {
                    if let Ok(result) = root_ref.remove_if::<_, _, _>(
                        key,
                        &mut condition,
                        &mut async_wait_pinned,
                        &guard,
                    ) {
                        if matches!(result, RemoveResult::Cleanup) {
                            root_ref.cleanup_link(key, false, &guard);
                        }
                        match result {
                            RemoveResult::Success => return true,
                            RemoveResult::Cleanup | RemoveResult::Retired => {
                                if Node::cleanup_root(&self.root, &mut async_wait_pinned, &guard) {
                                    return true;
                                }
                                removed = true;
                            }
                            RemoveResult::Fail => {
                                if removed {
                                    if Node::cleanup_root(
                                        &self.root,
                                        &mut async_wait_pinned,
                                        &guard,
                                    ) {
                                        return true;
                                    }
                                } else {
                                    return false;
                                }
                            }
                            RemoveResult::Frozen => (),
                        }
                    }
                } else {
                    return removed;
                }
            }
            async_wait_pinned.await;
        }
    }

    /// Removes keys in the specified range.
    ///
    /// This method removes internal nodes that are definitely contained in the specified range
    /// first, and then removes remaining entries individually.
    ///
    /// # Notes
    ///
    /// Internally, multiple internal node locks need to be acquired, thus making this method
    /// susceptible to lock starvation.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// for k in 2..8 {
    ///     assert!(treeindex.insert(k, 1).is_ok());
    /// }
    ///
    /// treeindex.remove_range(3..8);
    ///
    /// assert!(treeindex.contains(&2));
    /// assert!(!treeindex.contains(&3));
    /// ```
    #[inline]
    pub fn remove_range<Q, R: RangeBounds<Q>>(&self, range: R)
    where
        Q: Comparable<K> + ?Sized,
    {
        let start_unbounded = matches!(range.start_bound(), Unbounded);
        let guard = Guard::new();

        // Remove internal nodes, and individual entries in affected leaves.
        //
        // It takes O(N) to traverse sub-trees on the range border.
        while let Some(root_ref) = self.root.load(Acquire, &guard).as_ref() {
            if let Ok(num_children) =
                root_ref.remove_range(&range, start_unbounded, None, None, &mut (), &guard)
            {
                if num_children < 2 && !Node::cleanup_root(&self.root, &mut (), &guard) {
                    continue;
                }
                break;
            }
        }
    }

    /// Removes keys in the specified range.
    ///
    /// This method removes internal nodes that are definitely contained in the specified range
    /// first, and then removes remaining entries individually.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Notes
    ///
    /// Internally, multiple internal node locks need to be acquired, thus making this method
    /// susceptible to lock starvation.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// for k in 2..8 {
    ///     assert!(treeindex.insert(k, 1).is_ok());
    /// }
    ///
    /// let future_remove_range = treeindex.remove_range_async(3..8);
    /// ```
    #[inline]
    pub async fn remove_range_async<Q, R: RangeBounds<Q>>(&self, range: R)
    where
        Q: Comparable<K> + ?Sized,
    {
        let start_unbounded = matches!(range.start_bound(), Unbounded);

        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            {
                let guard = Guard::new();

                // Remove internal nodes, and individual entries in affected leaves.
                //
                // It takes O(N) to traverse sub-trees on the range border.
                if let Some(root_ref) = self.root.load(Acquire, &guard).as_ref() {
                    if let Ok(num_children) = root_ref.remove_range(
                        &range,
                        start_unbounded,
                        None,
                        None,
                        &mut async_wait_pinned,
                        &guard,
                    ) {
                        if num_children >= 2
                            || Node::cleanup_root(&self.root, &mut async_wait_pinned, &guard)
                        {
                            // Completed removal and cleaning up the root.
                            return;
                        }
                    }
                } else {
                    // Nothing to remove.
                    return;
                }
            }
            async_wait_pinned.await;
        }
    }

    /// Returns a guarded reference to the value for the specified key without acquiring locks.
    ///
    /// Returns `None` if the key does not exist. The returned reference can survive as long as the
    /// associated [`Guard`] is alive.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::ebr::Guard;
    /// use std::sync::Arc;
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<Arc<str>, u32> = TreeIndex::new();
    ///
    /// let guard = Guard::new();
    /// assert!(treeindex.peek("foo", &guard).is_none());
    ///
    /// treeindex.insert("foo".into(), 1).expect("insert in empty TreeIndex");
    /// ```
    #[inline]
    pub fn peek<'g, Q>(&self, key: &Q, guard: &'g Guard) -> Option<&'g V>
    where
        Q: Comparable<K> + ?Sized,
    {
        if let Some(root_ref) = self.root.load(Acquire, guard).as_ref() {
            return root_ref.search_value(key, guard);
        }
        None
    }

    /// Peeks a key-value pair without acquiring locks.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::sync::Arc;
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<Arc<str>, u32> = TreeIndex::new();
    ///
    /// assert!(treeindex.peek_with("foo", |k, v| *v).is_none());
    ///
    /// treeindex.insert("foo".into(), 1).expect("insert in empty TreeIndex");
    ///
    /// let key: Arc<str> = treeindex
    ///     .peek_with("foo", |k, _v| Arc::clone(k))
    ///     .expect("peek_with by borrowed key");
    /// ```
    #[inline]
    pub fn peek_with<Q, R, F: FnOnce(&K, &V) -> R>(&self, key: &Q, reader: F) -> Option<R>
    where
        Q: Comparable<K> + ?Sized,
    {
        let guard = Guard::new();
        self.peek_entry(key, &guard).map(|(k, v)| reader(k, v))
    }

    /// Returns a guarded reference to the key-value pair for the specified key without acquiring locks.
    ///
    /// Returns `None` if the key does not exist. The returned reference can survive as long as the
    /// associated [`Guard`] is alive.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::ebr::Guard;
    /// use std::sync::Arc;
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<Arc<str>, u32> = TreeIndex::new();
    ///
    /// let guard = Guard::new();
    /// assert!(treeindex.peek_entry("foo", &guard).is_none());
    ///
    /// treeindex.insert("foo".into(), 1).expect("insert in empty TreeIndex");
    ///
    /// let key: Arc<str> = treeindex
    ///     .peek_entry("foo", &guard)
    ///     .map(|(k, _v)| Arc::clone(k))
    ///     .expect("peek_entry by borrowed key");
    /// ```
    #[inline]
    pub fn peek_entry<'g, Q>(&self, key: &Q, guard: &'g Guard) -> Option<(&'g K, &'g V)>
    where
        Q: Comparable<K> + ?Sized,
    {
        if let Some(root_ref) = self.root.load(Acquire, guard).as_ref() {
            return root_ref.search_entry(key, guard);
        }
        None
    }

    /// Returns `true` if the [`TreeIndex`] contains the key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::default();
    ///
    /// assert!(!treeindex.contains(&1));
    /// assert!(treeindex.insert(1, 0).is_ok());
    /// assert!(treeindex.contains(&1));
    /// ```
    #[inline]
    pub fn contains<Q>(&self, key: &Q) -> bool
    where
        Q: Comparable<K> + ?Sized,
    {
        self.peek(key, &Guard::new()).is_some()
    }

    /// Returns the size of the [`TreeIndex`].
    ///
    /// It internally scans all the leaf nodes, and therefore the time complexity is O(N).
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    /// assert_eq!(treeindex.len(), 0);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        let guard = Guard::new();
        self.iter(&guard).count()
    }

    /// Returns `true` if the [`TreeIndex`] is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// assert!(treeindex.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        let guard = Guard::new();
        !self.iter(&guard).any(|_| true)
    }

    /// Returns an [`Iter`].
    ///
    /// The returned [`Iter`] starts scanning from the minimum key-value pair. Key-value pairs
    /// are scanned in ascending order, and key-value pairs that have existed since the invocation
    /// of the method are guaranteed to be visited if they are not removed. However, it is possible
    /// to visit removed key-value pairs momentarily.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    /// use scc::ebr::Guard;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// let guard = Guard::new();
    /// let mut iter = treeindex.iter(&guard);
    /// assert!(iter.next().is_none());
    /// ```
    #[inline]
    pub fn iter<'t, 'g>(&'t self, guard: &'g Guard) -> Iter<'t, 'g, K, V> {
        Iter::new(&self.root, guard)
    }

    /// Returns a [`Range`] that scans keys in the given range.
    ///
    /// Key-value pairs in the range are scanned in ascending order, and key-value pairs that have
    /// existed since the invocation of the method are guaranteed to be visited if they are not
    /// removed. However, it is possible to visit removed key-value pairs momentarily.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    /// use scc::ebr::Guard;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::new();
    ///
    /// let guard = Guard::new();
    /// assert_eq!(treeindex.range(4..=8, &guard).count(), 0);
    /// ```
    #[inline]
    pub fn range<'t, 'g, Q, R: RangeBounds<Q>>(
        &'t self,
        range: R,
        guard: &'g Guard,
    ) -> Range<'t, 'g, K, V, Q, R>
    where
        Q: Comparable<K> + ?Sized,
    {
        Range::new(&self.root, range, guard)
    }
}

impl<K, V> Clone for TreeIndex<K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
    #[inline]
    fn clone(&self) -> Self {
        let self_clone = Self::default();
        for (k, v) in self.iter(&Guard::new()) {
            let _reuslt = self_clone.insert(k.clone(), v.clone());
        }
        self_clone
    }
}

impl<K, V> Debug for TreeIndex<K, V>
where
    K: 'static + Clone + Debug + Ord,
    V: 'static + Clone + Debug,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let guard = Guard::new();
        f.debug_map().entries(self.iter(&guard)).finish()
    }
}

impl<K, V> Default for TreeIndex<K, V> {
    /// Creates a [`TreeIndex`] with the default parameters.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::TreeIndex;
    ///
    /// let treeindex: TreeIndex<u64, u32> = TreeIndex::default();
    /// ```
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

impl<K, V> Drop for TreeIndex<K, V> {
    #[inline]
    fn drop(&mut self) {
        self.clear();
    }
}

impl<K, V> PartialEq for TreeIndex<K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone + PartialEq,
{
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        // The key order is preserved, therefore comparing iterators suffices.
        let guard = Guard::new();
        Iterator::eq(self.iter(&guard), other.iter(&guard))
    }
}

impl<K, V> UnwindSafe for TreeIndex<K, V> {}

impl<'t, 'g, K, V> Iter<'t, 'g, K, V> {
    #[inline]
    fn new(root: &'t AtomicShared<Node<K, V>>, guard: &'g Guard) -> Iter<'t, 'g, K, V> {
        Iter::<'t, 'g, K, V> {
            root,
            leaf_scanner: None,
            guard,
        }
    }
}

impl<K, V> Debug for Iter<'_, '_, K, V> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Iter")
            .field("root", &self.root)
            .field("leaf_scanner", &self.leaf_scanner)
            .finish()
    }
}

impl<'g, K, V> Iterator for Iter<'_, 'g, K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
    type Item = (&'g K, &'g V);

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        // Starts scanning.
        if self.leaf_scanner.is_none() {
            let root_ptr = self.root.load(Acquire, self.guard);
            if let Some(root_ref) = root_ptr.as_ref() {
                if let Some(scanner) = root_ref.min(self.guard) {
                    self.leaf_scanner.replace(scanner);
                }
            } else {
                return None;
            }
        }

        // Go to the next entry.
        if let Some(mut scanner) = self.leaf_scanner.take() {
            let min_allowed_key = scanner.get().map(|(key, _)| key);
            if let Some(result) = scanner.next() {
                self.leaf_scanner.replace(scanner);
                return Some(result);
            }
            // Go to the next leaf node.
            if let Some(new_scanner) = scanner.jump(min_allowed_key, self.guard) {
                if let Some(entry) = new_scanner.get() {
                    self.leaf_scanner.replace(new_scanner);
                    return Some(entry);
                }
            }
        }
        None
    }
}

impl<K, V> FusedIterator for Iter<'_, '_, K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
}

impl<K, V> UnwindSafe for Iter<'_, '_, K, V> {}

impl<'t, 'g, K, V, Q: ?Sized, R: RangeBounds<Q>> Range<'t, 'g, K, V, Q, R> {
    #[inline]
    fn new(
        root: &'t AtomicShared<Node<K, V>>,
        range: R,
        guard: &'g Guard,
    ) -> Range<'t, 'g, K, V, Q, R> {
        Range::<'t, 'g, K, V, Q, R> {
            root,
            leaf_scanner: None,
            bounds: range,
            check_lower_bound: true,
            check_upper_bound: false,
            guard,
            query: PhantomData,
        }
    }
}

impl<'g, K, V, Q, R> Range<'_, 'g, K, V, Q, R>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
    Q: Comparable<K> + ?Sized,
    R: RangeBounds<Q>,
{
    #[inline]
    fn next_unbounded(&mut self) -> Option<(&'g K, &'g V)> {
        if self.leaf_scanner.is_none() {
            // Start scanning.
            let root_ptr = self.root.load(Acquire, self.guard);
            if let Some(root_ref) = root_ptr.as_ref() {
                let min_allowed_key = match self.bounds.start_bound() {
                    Excluded(key) | Included(key) => Some(key),
                    Unbounded => {
                        self.check_lower_bound = false;
                        None
                    }
                };
                let mut leaf_scanner = min_allowed_key
                    .and_then(|min_allowed_key| root_ref.max_le_appr(min_allowed_key, self.guard));
                if leaf_scanner.is_none() {
                    // No `min_allowed_key` is supplied, or no keys smaller than or equal to
                    // `min_allowed_key` found.
                    if let Some(mut scanner) = root_ref.min(self.guard) {
                        // It's possible that the leaf has just been emptied, so go to the next.
                        scanner.next();
                        while scanner.get().is_none() {
                            scanner = scanner.jump(None, self.guard)?;
                        }
                        leaf_scanner.replace(scanner);
                    }
                }
                if let Some(leaf_scanner) = leaf_scanner {
                    if let Some(result) = leaf_scanner.get() {
                        self.set_check_upper_bound(&leaf_scanner);
                        self.leaf_scanner.replace(leaf_scanner);
                        return Some(result);
                    }
                }
            }
        }

        // Go to the next entry.
        if let Some(mut leaf_scanner) = self.leaf_scanner.take() {
            let min_allowed_key = leaf_scanner.get().map(|(key, _)| key);
            if let Some(result) = leaf_scanner.next() {
                self.leaf_scanner.replace(leaf_scanner);
                return Some(result);
            }
            // Go to the next leaf node.
            if let Some(new_scanner) = leaf_scanner.jump(min_allowed_key, self.guard) {
                if let Some(entry) = new_scanner.get() {
                    self.set_check_upper_bound(&new_scanner);
                    self.leaf_scanner.replace(new_scanner);
                    return Some(entry);
                }
            }
        }

        None
    }

    #[inline]
    fn set_check_upper_bound(&mut self, scanner: &Scanner<K, V>) {
        self.check_upper_bound = match self.bounds.end_bound() {
            Excluded(key) => scanner
                .max_key()
                .map_or(false, |max_key| key.compare(max_key).is_le()),
            Included(key) => scanner
                .max_key()
                .map_or(false, |max_key| key.compare(max_key).is_lt()),
            Unbounded => false,
        };
    }
}

impl<K, V, Q: ?Sized, R: RangeBounds<Q>> Debug for Range<'_, '_, K, V, Q, R> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Range")
            .field("root", &self.root)
            .field("leaf_scanner", &self.leaf_scanner)
            .field("check_lower_bound", &self.check_lower_bound)
            .field("check_upper_bound", &self.check_upper_bound)
            .finish()
    }
}

impl<'g, K, V, Q, R> Iterator for Range<'_, 'g, K, V, Q, R>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
    Q: Comparable<K> + ?Sized,
    R: RangeBounds<Q>,
{
    type Item = (&'g K, &'g V);

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        while let Some((k, v)) = self.next_unbounded() {
            if self.check_lower_bound {
                match self.bounds.start_bound() {
                    Excluded(key) => {
                        if key.compare(k).is_ge() {
                            continue;
                        }
                    }
                    Included(key) => {
                        if key.compare(k).is_gt() {
                            continue;
                        }
                    }
                    Unbounded => (),
                }
            }
            self.check_lower_bound = false;
            if self.check_upper_bound {
                match self.bounds.end_bound() {
                    Excluded(key) => {
                        if key.compare(k).is_gt() {
                            return Some((k, v));
                        }
                    }
                    Included(key) => {
                        if key.compare(k).is_ge() {
                            return Some((k, v));
                        }
                    }
                    Unbounded => {
                        return Some((k, v));
                    }
                }
                break;
            }
            return Some((k, v));
        }
        None
    }
}

impl<K, V, Q, R> FusedIterator for Range<'_, '_, K, V, Q, R>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
    Q: Comparable<K> + ?Sized,
    R: RangeBounds<Q>,
{
}

impl<K, V, Q, R> UnwindSafe for Range<'_, '_, K, V, Q, R>
where
    Q: ?Sized,
    R: RangeBounds<Q> + UnwindSafe,
{
}
