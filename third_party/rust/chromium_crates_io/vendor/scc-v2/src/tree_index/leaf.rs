use crate::ebr::{AtomicShared, Guard, Shared};
use crate::maybe_std::AtomicUsize;
use crate::LinkedList;
use crate::{range_helper, Comparable};
use std::cell::UnsafeCell;
use std::cmp::Ordering;
use std::fmt::{self, Debug};
use std::mem::{needs_drop, MaybeUninit};
use std::ops::RangeBounds;
use std::sync::atomic::Ordering::{AcqRel, Acquire, Relaxed, Release};

/// [`Leaf`] is an ordered array of key-value pairs.
///
/// A constructed key-value pair entry is never dropped until the entire [`Leaf`] instance is
/// dropped.
pub struct Leaf<K, V> {
    /// The metadata containing information about the [`Leaf`] and individual entries.
    ///
    /// The state of each entry is as follows.
    /// * `0`: `uninit`.
    /// * `1-ARRAY_SIZE`: `rank`.
    /// * `ARRAY_SIZE + 1`: `removed`.
    ///
    /// The entry state transitions as follows.
    /// * `uninit -> removed -> rank -> removed`.
    metadata: AtomicUsize,

    /// The array of key-value pairs.
    entry_array: UnsafeCell<EntryArray<K, V>>,

    /// A pointer that points to the next adjacent [`Leaf`].
    link: AtomicShared<Leaf<K, V>>,
}

/// The number of entries and number of state bits per entry.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Dimension {
    pub num_entries: usize,
    pub num_bits_per_entry: usize,
}

/// The result of insertion.
pub enum InsertResult<K, V> {
    /// Insertion succeeded.
    Success,

    /// Duplicate key found.
    Duplicate(K, V),

    /// No vacant slot for the key.
    Full(K, V),

    /// The [`Leaf`] is frozen.
    ///
    /// It is not a terminal state that a frozen [`Leaf`] can be unfrozen.
    Frozen(K, V),

    /// Insertion failed as the [`Leaf`] has retired.
    ///
    /// It is a terminal state.
    Retired(K, V),

    /// The operation can be retried.
    Retry(K, V),
}

/// The result of removal.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RemoveResult {
    /// Remove succeeded.
    Success,

    /// Remove succeeded and cleanup required.
    Cleanup,

    /// Remove succeeded and the [`Leaf`] has retired without usable entries left.
    Retired,

    /// Remove failed.
    Fail,

    /// The [`Leaf`] is frozen.
    Frozen,
}

impl<K, V> Leaf<K, V> {
    /// Creates a new [`Leaf`].
    #[cfg(not(feature = "loom"))]
    #[inline]
    pub(super) const fn new() -> Leaf<K, V> {
        #[allow(clippy::uninit_assumed_init)]
        Leaf {
            metadata: AtomicUsize::new(0),
            entry_array: UnsafeCell::new(unsafe { MaybeUninit::uninit().assume_init() }),
            link: AtomicShared::null(),
        }
    }

    #[cfg(feature = "loom")]
    #[inline]
    pub(super) fn new() -> Leaf<K, V> {
        #[allow(clippy::uninit_assumed_init)]
        Leaf {
            metadata: AtomicUsize::new(0),
            entry_array: UnsafeCell::new(unsafe { MaybeUninit::uninit().assume_init() }),
            link: AtomicShared::null(),
        }
    }

    /// Thaws the [`Leaf`].
    #[inline]
    pub(super) fn thaw(&self) -> bool {
        self.metadata
            .fetch_update(Release, Relaxed, |p| {
                if Dimension::frozen(p) {
                    Some(Dimension::thaw(p))
                } else {
                    None
                }
            })
            .is_ok()
    }

    /// Returns `true` if the [`Leaf`] has retired.
    #[inline]
    pub(super) fn is_retired(&self) -> bool {
        Dimension::retired(self.metadata.load(Acquire))
    }

    /// Returns `true` if the [`Leaf`] has no reachable entry.
    #[inline]
    pub(super) fn is_empty(&self) -> bool {
        let mut mutable_metadata = self.metadata.load(Acquire);
        for _ in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank != Dimension::uninit_rank() && rank != DIMENSION.removed_rank() {
                return false;
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
        true
    }

    /// Returns a reference to the max key.
    #[inline]
    pub(super) fn max_key(&self) -> Option<&K> {
        self.max_entry().map(|(k, _)| k)
    }

    /// Returns a reference to the max entry.
    #[inline]
    pub(super) fn max_entry(&self) -> Option<(&K, &V)> {
        let mut mutable_metadata = self.metadata.load(Acquire);
        let mut max_rank = 0;
        let mut max_index = DIMENSION.num_entries;
        for i in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank > max_rank && rank != DIMENSION.removed_rank() {
                max_rank = rank;
                max_index = i;
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
        if max_index != DIMENSION.num_entries {
            return Some((self.key_at(max_index), self.value_at(max_index)));
        }
        None
    }

    /// Inserts a key value pair at the specified position without checking the metadata.
    ///
    /// `rank` is calculated as `index + 1`.
    #[inline]
    pub(super) fn insert_unchecked(&self, key: K, val: V, index: usize) {
        debug_assert!(index < DIMENSION.num_entries);
        let metadata = self.metadata.load(Relaxed);
        let new_metadata = DIMENSION.augment(metadata, index, index + 1);
        self.write(index, key, val);
        self.metadata.store(new_metadata, Release);
    }

    /// Compares the given metadata value with the current one.
    #[inline]
    pub(super) fn validate(&self, metadata: usize) -> bool {
        // `Relaxed` is sufficient as long as the caller has load-acquired its contents.
        self.metadata.load(Relaxed) == metadata
    }

    /// Freezes the [`Leaf`] temporarily.
    ///
    /// A frozen [`Leaf`] cannot store more entries, and on-going insertion is canceled.
    #[inline]
    pub(super) fn freeze(&self) -> bool {
        self.metadata
            .fetch_update(AcqRel, Acquire, |p| {
                if Dimension::frozen(p) {
                    None
                } else {
                    Some(Dimension::freeze(p))
                }
            })
            .is_ok()
    }

    /// Returns the recommended number of entries that the left-side node shall store when a
    /// [`Leaf`] is split.
    ///
    /// Returns a number in `[1, len(leaf))` that represents the recommended number of entries in
    /// the left-side node. The number is calculated as, for each adjacent slots,
    /// - Initial `score = len(leaf)`.
    /// - Rank increased: `score -= 1`.
    /// - Rank decreased: `score += 1`.
    /// - Clamp `score` in `[len(leaf) / 2 + 1, len(leaf) / 2 + len(leaf) - 1)`.
    /// - Take `score - len(leaf) / 2`.
    ///
    /// For instance, when the length of a [`Leaf`] is 7,
    /// - Returns 6 for `rank = [1, 2, 3, 4, 5, 6, 7]`.
    /// - Returns 1 for `rank = [7, 6, 5, 4, 3, 2, 1]`.
    #[inline]
    pub(super) fn optimal_boundary(mut mutable_metadata: usize) -> usize {
        let mut boundary: usize = DIMENSION.num_entries;
        let mut prev_rank = 0;
        for _ in 0..DIMENSION.num_entries {
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank != 0 && rank != DIMENSION.removed_rank() {
                if prev_rank >= rank {
                    boundary -= 1;
                } else if prev_rank != 0 {
                    boundary += 1;
                }
                prev_rank = rank;
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
        boundary.clamp(
            DIMENSION.num_entries / 2 + 1,
            DIMENSION.num_entries + DIMENSION.num_entries / 2 - 1,
        ) - DIMENSION.num_entries / 2
    }

    fn key_at(&self, index: usize) -> &K {
        unsafe { &*(*self.entry_array.get()).0[index].as_ptr() }
    }

    fn value_at(&self, index: usize) -> &V {
        unsafe { &*(*self.entry_array.get()).1[index].as_ptr() }
    }

    fn rollback(&self, index: usize) -> InsertResult<K, V> {
        let (key, val) = self.take(index);
        let result = self
            .metadata
            .fetch_and(!DIMENSION.rank_mask(index), Release)
            & (!DIMENSION.rank_mask(index));
        if Dimension::retired(result) {
            InsertResult::Retired(key, val)
        } else if Dimension::frozen(result) {
            InsertResult::Frozen(key, val)
        } else {
            InsertResult::Duplicate(key, val)
        }
    }

    fn take(&self, index: usize) -> (K, V) {
        unsafe {
            (
                (*self.entry_array.get()).0[index].as_ptr().read(),
                (*self.entry_array.get()).1[index].as_ptr().read(),
            )
        }
    }

    fn write(&self, index: usize, key: K, val: V) {
        unsafe {
            (*self.entry_array.get()).0[index].as_mut_ptr().write(key);
            (*self.entry_array.get()).1[index].as_mut_ptr().write(val);
        }
    }

    /// Returns the index of the corresponding entry of the next higher ranked entry.
    fn next(index: usize, mut mutable_metadata: usize) -> usize {
        debug_assert_ne!(index, usize::MAX);
        let current_entry_rank = if index == DIMENSION.num_entries {
            0
        } else {
            DIMENSION.rank(mutable_metadata, index)
        };
        let mut next_index = DIMENSION.num_entries;
        if current_entry_rank < DIMENSION.num_entries {
            let mut next_rank = DIMENSION.removed_rank();
            for i in 0..DIMENSION.num_entries {
                if mutable_metadata == 0 {
                    break;
                }
                if i != index {
                    let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
                    if rank != Dimension::uninit_rank() && rank < next_rank {
                        if rank == current_entry_rank + 1 {
                            return i;
                        } else if rank > current_entry_rank {
                            next_rank = rank;
                            next_index = i;
                        }
                    }
                }
                mutable_metadata >>= DIMENSION.num_bits_per_entry;
            }
        }
        next_index
    }
}

impl<K, V> Leaf<K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
    /// Inserts a key value pair.
    #[inline]
    pub(super) fn insert(&self, key: K, val: V) -> InsertResult<K, V> {
        let mut metadata = self.metadata.load(Acquire);
        'after_read_metadata: loop {
            if Dimension::retired(metadata) {
                return InsertResult::Retired(key, val);
            } else if Dimension::frozen(metadata) {
                return InsertResult::Frozen(key, val);
            }

            let mut mutable_metadata = metadata;
            for i in 0..DIMENSION.num_entries {
                let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
                if rank == Dimension::uninit_rank() {
                    let interim_metadata = DIMENSION.augment(metadata, i, DIMENSION.removed_rank());

                    // Reserve the slot.
                    //
                    // It doesn't have to be a release-store.
                    if let Err(actual) =
                        self.metadata
                            .compare_exchange(metadata, interim_metadata, Acquire, Acquire)
                    {
                        metadata = actual;
                        continue 'after_read_metadata;
                    }

                    self.write(i, key, val);
                    return self.post_insert(i, interim_metadata);
                }
                mutable_metadata >>= DIMENSION.num_bits_per_entry;
            }

            if self.search_slot(&key, metadata).is_some() {
                return InsertResult::Duplicate(key, val);
            }
            return InsertResult::Full(key, val);
        }
    }

    /// Removes the key if the condition is met.
    #[inline]
    pub(super) fn remove_if<Q, F: FnMut(&V) -> bool>(
        &self,
        key: &Q,
        condition: &mut F,
    ) -> RemoveResult
    where
        Q: Comparable<K> + ?Sized,
    {
        let mut metadata = self.metadata.load(Acquire);
        if Dimension::frozen(metadata) {
            return RemoveResult::Frozen;
        }
        let mut min_max_rank = DIMENSION.removed_rank();
        let mut max_min_rank = 0;
        let mut mutable_metadata = metadata;
        for i in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank < min_max_rank && rank > max_min_rank {
                match self.compare(i, key) {
                    Ordering::Less => {
                        if max_min_rank < rank {
                            max_min_rank = rank;
                        }
                    }
                    Ordering::Greater => {
                        if min_max_rank > rank {
                            min_max_rank = rank;
                        }
                    }
                    Ordering::Equal => {
                        // Found the key.
                        loop {
                            if !condition(self.value_at(i)) {
                                // The given condition is not met.
                                return RemoveResult::Fail;
                            }
                            let mut empty = true;
                            mutable_metadata = metadata;
                            for j in 0..DIMENSION.num_entries {
                                if mutable_metadata == 0 {
                                    break;
                                }
                                if i != j {
                                    let rank = mutable_metadata
                                        % (1_usize << DIMENSION.num_bits_per_entry);
                                    if rank != Dimension::uninit_rank()
                                        && rank != DIMENSION.removed_rank()
                                    {
                                        empty = false;
                                        break;
                                    }
                                }
                                mutable_metadata >>= DIMENSION.num_bits_per_entry;
                            }

                            let mut new_metadata = metadata | DIMENSION.rank_mask(i);
                            if empty {
                                new_metadata = Dimension::retire(new_metadata);
                            }
                            match self.metadata.compare_exchange(
                                metadata,
                                new_metadata,
                                AcqRel,
                                Acquire,
                            ) {
                                Ok(_) => {
                                    if empty {
                                        return RemoveResult::Retired;
                                    }
                                    return RemoveResult::Success;
                                }
                                Err(actual) => {
                                    if DIMENSION.rank(actual, i) == DIMENSION.removed_rank() {
                                        return RemoveResult::Fail;
                                    }
                                    if Dimension::frozen(actual) {
                                        return RemoveResult::Frozen;
                                    }
                                    metadata = actual;
                                }
                            }
                        }
                    }
                }
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }

        RemoveResult::Fail
    }

    /// Removes a range of entries.
    ///
    /// Returns the number of remaining children.
    #[inline]
    pub(super) fn remove_range<Q, R: RangeBounds<Q>>(&self, range: &R)
    where
        Q: Comparable<K> + ?Sized,
    {
        let mut mutable_metadata = self.metadata.load(Acquire);
        for i in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank != Dimension::uninit_rank() && rank != DIMENSION.removed_rank() {
                let k = self.key_at(i);
                if range_helper::contains(range, k) {
                    self.remove_if(k, &mut |_| true);
                }
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
    }

    /// Returns an entry containing the specified key.
    #[inline]
    pub(super) fn search_entry<Q>(&self, key: &Q) -> Option<(&K, &V)>
    where
        Q: Comparable<K> + ?Sized,
    {
        let metadata = self.metadata.load(Acquire);
        self.search_slot(key, metadata)
            .map(|i| (self.key_at(i), self.value_at(i)))
    }

    /// Returns the value associated with the specified key.
    #[inline]
    pub(super) fn search_value<Q>(&self, key: &Q) -> Option<&V>
    where
        Q: Comparable<K> + ?Sized,
    {
        let metadata = self.metadata.load(Acquire);
        self.search_slot(key, metadata).map(|i| self.value_at(i))
    }

    /// Returns the index of the key-value pair that is smaller than the given key.
    #[inline]
    pub(super) fn max_less<Q>(&self, mut mutable_metadata: usize, key: &Q) -> usize
    where
        Q: Comparable<K> + ?Sized,
    {
        let mut min_max_rank = DIMENSION.removed_rank();
        let mut max_min_rank = 0;
        let mut max_min_index = DIMENSION.num_entries;
        for i in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank < min_max_rank && rank > max_min_rank {
                match self.compare(i, key) {
                    Ordering::Less => {
                        if max_min_rank < rank {
                            max_min_rank = rank;
                            max_min_index = i;
                        }
                    }
                    Ordering::Greater => {
                        if min_max_rank > rank {
                            min_max_rank = rank;
                        }
                    }
                    Ordering::Equal => {
                        min_max_rank = rank;
                    }
                }
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
        max_min_index
    }

    /// Returns the minimum entry among those that are not `Ordering::Less` than the given key.
    ///
    /// It additionally returns the current version of its metadata in order for the caller to
    /// validate the sanity of the result.
    #[inline]
    pub(super) fn min_greater_equal<Q>(&self, key: &Q) -> (Option<(&K, &V)>, usize)
    where
        Q: Comparable<K> + ?Sized,
    {
        let metadata = self.metadata.load(Acquire);
        let mut min_max_rank = DIMENSION.removed_rank();
        let mut max_min_rank = 0;
        let mut min_max_index = DIMENSION.num_entries;
        let mut mutable_metadata = metadata;
        for i in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank < min_max_rank && rank > max_min_rank {
                let k = self.key_at(i);
                match key.compare(k) {
                    Ordering::Greater => {
                        if max_min_rank < rank {
                            max_min_rank = rank;
                        }
                    }
                    Ordering::Less => {
                        if min_max_rank > rank {
                            min_max_rank = rank;
                            min_max_index = i;
                        }
                    }
                    Ordering::Equal => {
                        return (Some((k, self.value_at(i))), metadata);
                    }
                }
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
        if min_max_index != DIMENSION.num_entries {
            return (
                Some((self.key_at(min_max_index), self.value_at(min_max_index))),
                metadata,
            );
        }
        (None, metadata)
    }

    /// Freezes the [`Leaf`] and distribute entries to two new leaves.
    #[inline]
    pub(super) fn freeze_and_distribute(
        &self,
        low_key_leaf: &mut Option<Shared<Leaf<K, V>>>,
        high_key_leaf: &mut Option<Shared<Leaf<K, V>>>,
    ) {
        let metadata = unsafe {
            self.metadata
                .fetch_update(AcqRel, Acquire, |p| {
                    if Dimension::frozen(p) {
                        None
                    } else {
                        Some(Dimension::freeze(p))
                    }
                })
                .unwrap_unchecked()
        };

        let boundary = Self::optimal_boundary(metadata);
        let scanner = Scanner {
            leaf: self,
            metadata,
            entry_index: DIMENSION.num_entries,
        };
        for (i, (k, v)) in scanner.enumerate() {
            if i < boundary {
                low_key_leaf
                    .get_or_insert_with(|| Shared::new(Leaf::new()))
                    .insert_unchecked(k.clone(), v.clone(), i);
            } else {
                high_key_leaf
                    .get_or_insert_with(|| Shared::new(Leaf::new()))
                    .insert_unchecked(k.clone(), v.clone(), i - boundary);
            }
        }
    }

    /// Post-processing after reserving a free slot.
    fn post_insert(&self, free_slot_index: usize, mut prev_metadata: usize) -> InsertResult<K, V> {
        let key = self.key_at(free_slot_index);
        loop {
            let mut min_max_rank = DIMENSION.removed_rank();
            let mut max_min_rank = 0;
            let mut new_metadata = prev_metadata;
            let mut mutable_metadata = prev_metadata;
            for i in 0..DIMENSION.num_entries {
                if mutable_metadata == 0 {
                    break;
                }
                let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
                if rank < min_max_rank && rank > max_min_rank {
                    match self.compare(i, key) {
                        Ordering::Less => {
                            if max_min_rank < rank {
                                max_min_rank = rank;
                            }
                        }
                        Ordering::Greater => {
                            if min_max_rank > rank {
                                min_max_rank = rank;
                            }
                            new_metadata = DIMENSION.augment(new_metadata, i, rank + 1);
                        }
                        Ordering::Equal => {
                            // Duplicate key.
                            return self.rollback(free_slot_index);
                        }
                    }
                } else if rank != DIMENSION.removed_rank() && rank > min_max_rank {
                    new_metadata = DIMENSION.augment(new_metadata, i, rank + 1);
                }
                mutable_metadata >>= DIMENSION.num_bits_per_entry;
            }

            // Make the newly inserted value reachable.
            let final_metadata = DIMENSION.augment(new_metadata, free_slot_index, max_min_rank + 1);
            if let Err(actual) =
                self.metadata
                    .compare_exchange(prev_metadata, final_metadata, AcqRel, Acquire)
            {
                if Dimension::frozen(actual) || Dimension::retired(actual) {
                    return self.rollback(free_slot_index);
                }
                prev_metadata = actual;
                continue;
            }

            return InsertResult::Success;
        }
    }

    /// Searches for a slot in which the key is stored.
    fn search_slot<Q>(&self, key: &Q, mut mutable_metadata: usize) -> Option<usize>
    where
        Q: Comparable<K> + ?Sized,
    {
        let mut min_max_rank = DIMENSION.removed_rank();
        let mut max_min_rank = 0;
        for i in 0..DIMENSION.num_entries {
            if mutable_metadata == 0 {
                break;
            }
            let rank = mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry);
            if rank < min_max_rank && rank > max_min_rank {
                match self.compare(i, key) {
                    Ordering::Less => {
                        if max_min_rank < rank {
                            max_min_rank = rank;
                        }
                    }
                    Ordering::Greater => {
                        if min_max_rank > rank {
                            min_max_rank = rank;
                        }
                    }
                    Ordering::Equal => {
                        return Some(i);
                    }
                }
            }
            mutable_metadata >>= DIMENSION.num_bits_per_entry;
        }
        None
    }

    fn compare<Q>(&self, index: usize, key: &Q) -> Ordering
    where
        Q: Comparable<K> + ?Sized,
    {
        key.compare(self.key_at(index)).reverse()
    }
}

impl<K, V> Drop for Leaf<K, V> {
    #[inline]
    fn drop(&mut self) {
        if needs_drop::<(K, V)>() {
            let mut mutable_metadata = self.metadata.load(Acquire);
            for i in 0..DIMENSION.num_entries {
                if mutable_metadata == 0 {
                    break;
                }
                if mutable_metadata % (1_usize << DIMENSION.num_bits_per_entry)
                    != Dimension::uninit_rank()
                {
                    self.take(i);
                }
                mutable_metadata >>= DIMENSION.num_bits_per_entry;
            }
        }
    }
}

/// [`LinkedList`] implementation for [`Leaf`].
impl<K, V> LinkedList for Leaf<K, V> {
    #[inline]
    fn link_ref(&self) -> &AtomicShared<Leaf<K, V>> {
        &self.link
    }
}

unsafe impl<K: Send, V: Send> Send for Leaf<K, V> {}

unsafe impl<K: Sync, V: Sync> Sync for Leaf<K, V> {}

impl Dimension {
    /// Checks if the [`Leaf`] is frozen.
    const fn frozen(metadata: usize) -> bool {
        metadata & (1_usize << (usize::BITS - 2)) != 0
    }

    /// Makes the metadata represent a frozen state.
    const fn freeze(metadata: usize) -> usize {
        metadata | (1_usize << (usize::BITS - 2))
    }

    /// Updates the metadata to represent a non-frozen state.
    const fn thaw(metadata: usize) -> usize {
        metadata & (!(1_usize << (usize::BITS - 2)))
    }

    /// Checks if the [`Leaf`] is retired.
    const fn retired(metadata: usize) -> bool {
        metadata & (1_usize << (usize::BITS - 1)) != 0
    }

    /// Makes the metadata represent a retired state.
    const fn retire(metadata: usize) -> usize {
        metadata | (1_usize << (usize::BITS - 1))
    }

    /// Returns a bit mask for an entry.
    const fn rank_mask(&self, index: usize) -> usize {
        ((1_usize << self.num_bits_per_entry) - 1) << (index * self.num_bits_per_entry)
    }

    /// Returns the rank of an entry.
    const fn rank(&self, metadata: usize, index: usize) -> usize {
        (metadata >> (index * self.num_bits_per_entry)) % (1_usize << self.num_bits_per_entry)
    }

    /// Returns the uninitialized rank value which is smaller than all the valid rank values.
    const fn uninit_rank() -> usize {
        0
    }

    /// Returns the removed rank value which is greater than all the valid rank values.
    const fn removed_rank(&self) -> usize {
        (1_usize << self.num_bits_per_entry) - 1
    }

    /// Augments the rank to the given metadata.
    const fn augment(&self, metadata: usize, index: usize, rank: usize) -> usize {
        (metadata & (!self.rank_mask(index))) | (rank << (index * self.num_bits_per_entry))
    }
}

/// The maximum number of entries and the number of metadata bits per entry in a [`Leaf`].
///
/// * `M`: The maximum number of entries.
/// * `B`: The minimum number of bits to express the state of an entry.
/// * `2`: The number of special states of an entry: uninitialized, removed.
/// * `2`: The number of special states of a [`Leaf`]: frozen, retired.
/// * `U`: `usize::BITS`.
/// * `Eq1 = M + 2 <= 2^B`: `B` bits represent at least `M + 2` states.
/// * `Eq2 = B * M + 2 <= U`: `M entries + 2` special state.
/// * `Eq3 = Ceil(Log2(M + 2)) * M + 2 <= U`: derived from `Eq1` and `Eq2`.
///
/// Therefore, when `U = 64 => M = 14 / B = 4`, and `U = 32 => M = 7 / B = 4`.
pub const DIMENSION: Dimension = match usize::BITS / 8 {
    1 => Dimension {
        num_entries: 2,
        num_bits_per_entry: 2,
    },
    2 => Dimension {
        num_entries: 4,
        num_bits_per_entry: 3,
    },
    4 => Dimension {
        num_entries: 7,
        num_bits_per_entry: 4,
    },
    8 => Dimension {
        num_entries: 14,
        num_bits_per_entry: 4,
    },
    _ => Dimension {
        num_entries: 25,
        num_bits_per_entry: 5,
    },
};

/// Each constructed entry in an `EntryArray` is never dropped until the [`Leaf`] is dropped.
pub type EntryArray<K, V> = (
    [MaybeUninit<K>; DIMENSION.num_entries],
    [MaybeUninit<V>; DIMENSION.num_entries],
);

/// Leaf scanner.
pub struct Scanner<'l, K, V> {
    leaf: &'l Leaf<K, V>,
    metadata: usize,
    entry_index: usize,
}

impl<'l, K, V> Scanner<'l, K, V> {
    /// Creates a new [`Scanner`].
    #[inline]
    pub(super) fn new(leaf: &'l Leaf<K, V>) -> Scanner<'l, K, V> {
        Scanner {
            leaf,
            metadata: leaf.metadata.load(Acquire),
            entry_index: DIMENSION.num_entries,
        }
    }

    /// Returns the metadata that the [`Scanner`] is currently using.
    #[inline]
    pub(super) const fn metadata(&self) -> usize {
        self.metadata
    }

    /// Returns a reference to the entry that the scanner is currently pointing to
    #[inline]
    pub(super) fn get(&self) -> Option<(&'l K, &'l V)> {
        if self.entry_index >= DIMENSION.num_entries {
            return None;
        }
        Some((
            self.leaf.key_at(self.entry_index),
            self.leaf.value_at(self.entry_index),
        ))
    }

    /// Returns a reference to the max key.
    #[inline]
    pub(super) fn max_key(&self) -> Option<&'l K> {
        self.leaf.max_key()
    }

    /// Traverses the linked list.
    #[inline]
    pub(super) fn jump<'g>(
        &self,
        min_allowed_key: Option<&K>,
        guard: &'g Guard,
    ) -> Option<Scanner<'g, K, V>>
    where
        K: Ord,
    {
        let mut next_leaf_ptr = self.leaf.next_ptr(Acquire, guard);
        while let Some(next_leaf_ref) = next_leaf_ptr.as_ref() {
            let mut leaf_scanner = Scanner::new(next_leaf_ref);
            if let Some(key) = min_allowed_key {
                if !self.leaf.is_clear(Relaxed) {
                    // Data race resolution: compare keys if the current leaf has been deleted.
                    //
                    // There is a chance that the current leaf has been deleted, and smaller
                    // keys have been inserted into the next leaf.
                    while let Some((k, _)) = leaf_scanner.next() {
                        if key.cmp(k) == Ordering::Less {
                            return Some(leaf_scanner);
                        }
                    }
                    next_leaf_ptr = next_leaf_ref.next_ptr(Acquire, guard);
                    continue;
                }
            }
            if leaf_scanner.next().is_some() {
                return Some(leaf_scanner);
            }
            next_leaf_ptr = next_leaf_ref.next_ptr(Acquire, guard);
        }
        None
    }

    fn proceed(&mut self) {
        if self.entry_index == usize::MAX {
            return;
        }
        let index = Leaf::<K, V>::next(self.entry_index, self.metadata);
        if index == DIMENSION.num_entries {
            // Fuse the iterator.
            self.entry_index = usize::MAX;
        } else {
            self.entry_index = index;
        }
    }
}

impl<'l, K, V> Scanner<'l, K, V>
where
    K: 'static + Clone + Ord,
    V: 'static + Clone,
{
    /// Returns a [`Scanner`] pointing to the max-less entry if there is one.
    #[inline]
    pub(super) fn max_less<Q>(leaf: &'l Leaf<K, V>, key: &Q) -> Option<Scanner<'l, K, V>>
    where
        Q: Comparable<K> + ?Sized,
    {
        let metadata = leaf.metadata.load(Acquire);
        let index = leaf.max_less(metadata, key);
        if index == DIMENSION.num_entries {
            None
        } else {
            Some(Scanner {
                leaf,
                metadata,
                entry_index: index,
            })
        }
    }
}

impl<K, V> Debug for Scanner<'_, K, V> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Scanner")
            .field("metadata", &self.metadata)
            .field("entry_index", &self.entry_index)
            .finish()
    }
}

impl<'l, K, V> Iterator for Scanner<'l, K, V> {
    type Item = (&'l K, &'l V);

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        self.proceed();
        self.get()
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod test {
    use super::*;
    use proptest::prelude::*;
    use std::sync::atomic::AtomicBool;
    use tokio::sync::Barrier;

    #[test]
    fn basic() {
        let leaf: Leaf<String, String> = Leaf::new();
        assert!(matches!(
            leaf.insert("MY GOODNESS!".to_owned(), "OH MY GOD!!".to_owned()),
            InsertResult::Success
        ));
        assert!(matches!(
            leaf.insert("GOOD DAY".to_owned(), "OH MY GOD!!".to_owned()),
            InsertResult::Success
        ));
        assert_eq!(leaf.search_entry("MY GOODNESS!").unwrap().1, "OH MY GOD!!");
        assert_eq!(leaf.search_entry("GOOD DAY").unwrap().1, "OH MY GOD!!");

        for i in 0..DIMENSION.num_entries {
            if let InsertResult::Full(k, v) = leaf.insert(i.to_string(), i.to_string()) {
                assert_eq!(i + 2, DIMENSION.num_entries);
                assert_eq!(k, i.to_string());
                assert_eq!(v, i.to_string());
                break;
            }
            assert_eq!(
                leaf.search_entry(&i.to_string()).unwrap(),
                (&i.to_string(), &i.to_string())
            );
        }

        for i in 0..DIMENSION.num_entries {
            let result = leaf.remove_if(&i.to_string(), &mut |_| i >= 10);
            if i >= 10 && i + 2 < DIMENSION.num_entries {
                assert_eq!(result, RemoveResult::Success);
            } else {
                assert_eq!(result, RemoveResult::Fail);
            }
        }

        assert_eq!(
            leaf.remove_if("GOOD DAY", &mut |v| v == "OH MY"),
            RemoveResult::Fail
        );
        assert_eq!(
            leaf.remove_if("GOOD DAY", &mut |v| v == "OH MY GOD!!"),
            RemoveResult::Success
        );
        assert!(leaf.search_entry("GOOD DAY").is_none());
        assert_eq!(
            leaf.remove_if("MY GOODNESS!", &mut |_| true),
            RemoveResult::Success
        );
        assert!(leaf.search_entry("MY GOODNESS!").is_none());
        assert!(leaf.search_entry("1").is_some());
        assert!(matches!(
            leaf.insert("1".to_owned(), "1".to_owned()),
            InsertResult::Duplicate(..)
        ));
        assert!(matches!(
            leaf.insert("100".to_owned(), "100".to_owned()),
            InsertResult::Full(..)
        ));

        let mut scanner = Scanner::new(&leaf);
        for i in 0..DIMENSION.num_entries {
            if let Some(e) = scanner.next() {
                assert_eq!(e.0, &i.to_string());
                assert_eq!(e.1, &i.to_string());
                assert_ne!(
                    leaf.remove_if(&i.to_string(), &mut |_| true),
                    RemoveResult::Fail
                );
            } else {
                break;
            }
        }

        assert!(matches!(
            leaf.insert("200".to_owned(), "200".to_owned()),
            InsertResult::Retired(..)
        ));
    }

    #[test]
    fn calculate_boundary() {
        let leaf: Leaf<usize, usize> = Leaf::new();
        for i in 0..DIMENSION.num_entries {
            assert!(matches!(leaf.insert(i, i), InsertResult::Success));
        }
        assert_eq!(
            Leaf::<usize, usize>::optimal_boundary(leaf.metadata.load(Relaxed)),
            DIMENSION.num_entries - 1
        );

        let leaf: Leaf<usize, usize> = Leaf::new();
        for i in (0..DIMENSION.num_entries).rev() {
            assert!(matches!(leaf.insert(i, i), InsertResult::Success));
        }
        assert_eq!(
            Leaf::<usize, usize>::optimal_boundary(leaf.metadata.load(Relaxed)),
            1
        );

        let leaf: Leaf<usize, usize> = Leaf::new();
        for i in 0..DIMENSION.num_entries {
            if i < DIMENSION.num_entries / 2 {
                assert!(matches!(
                    leaf.insert(usize::MAX - i, usize::MAX - i),
                    InsertResult::Success
                ));
            } else {
                assert!(matches!(leaf.insert(i, i), InsertResult::Success));
            }
        }
        if usize::BITS == 32 {
            assert_eq!(
                Leaf::<usize, usize>::optimal_boundary(leaf.metadata.load(Relaxed)),
                4
            );
        } else {
            assert_eq!(
                Leaf::<usize, usize>::optimal_boundary(leaf.metadata.load(Relaxed)),
                6
            );
        }
    }

    #[test]
    fn special() {
        let leaf: Leaf<usize, usize> = Leaf::new();
        assert!(matches!(leaf.insert(11, 17), InsertResult::Success));
        assert!(matches!(leaf.insert(17, 11), InsertResult::Success));

        let mut leaf1 = None;
        let mut leaf2 = None;
        leaf.freeze_and_distribute(&mut leaf1, &mut leaf2);
        assert_eq!(
            leaf1.as_ref().and_then(|l| l.search_entry(&11)),
            Some((&11, &17))
        );
        assert_eq!(
            leaf1.as_ref().and_then(|l| l.search_entry(&17)),
            Some((&17, &11))
        );
        assert!(leaf2.is_none());
        assert!(matches!(leaf.insert(1, 7), InsertResult::Frozen(..)));
        assert_eq!(leaf.remove_if(&17, &mut |_| true), RemoveResult::Frozen);
        assert!(matches!(leaf.insert(3, 5), InsertResult::Frozen(..)));

        assert!(leaf.thaw());
        assert!(matches!(leaf.insert(1, 7), InsertResult::Success));

        assert_eq!(leaf.remove_if(&1, &mut |_| true), RemoveResult::Success);
        assert_eq!(leaf.remove_if(&17, &mut |_| true), RemoveResult::Success);
        assert_eq!(leaf.remove_if(&11, &mut |_| true), RemoveResult::Retired);

        assert!(matches!(leaf.insert(5, 3), InsertResult::Retired(..)));
    }

    proptest! {
        #[cfg_attr(miri, ignore)]
        #[test]
        fn general(insert in 0_usize..DIMENSION.num_entries, remove in 0_usize..DIMENSION.num_entries) {
            let leaf: Leaf<usize, usize> = Leaf::new();
            assert!(leaf.is_empty());
            for i in 0..insert {
                assert!(matches!(leaf.insert(i, i), InsertResult::Success));
                if i != 0 {
                    let result = leaf.max_less(leaf.metadata.load(Relaxed), &i);
                    assert_eq!(*leaf.key_at(result), i - 1);
                    assert_eq!(*leaf.value_at(result), i - 1);
                }
            }
            if insert == 0 {
                assert_eq!(leaf.max_key(), None);
                assert!(leaf.is_empty());
            } else {
                assert_eq!(leaf.max_key(), Some(&(insert - 1)));
                assert!(!leaf.is_empty());
            }
            for i in 0..insert {
                assert!(matches!(leaf.insert(i, i), InsertResult::Duplicate(..)));
                assert!(!leaf.is_empty());
                let result = leaf.min_greater_equal(&i);
                assert_eq!(result.0, Some((&i, &i)));
            }
            for i in 0..insert {
                assert_eq!(leaf.search_entry(&i).unwrap(), (&i, &i));
            }
            if insert == DIMENSION.num_entries {
                assert!(matches!(leaf.insert(usize::MAX, usize::MAX), InsertResult::Full(..)));
            }
            for i in 0..remove {
                if i < insert {
                    if i == insert - 1 {
                        assert!(matches!(leaf.remove_if(&i, &mut |_| true), RemoveResult::Retired));
                        for i in 0..insert {
                            assert!(matches!(leaf.insert(i, i), InsertResult::Retired(..)));
                        }
                    } else {
                        assert!(matches!(leaf.remove_if(&i, &mut |_| true), RemoveResult::Success));
                    }
                } else {
                    assert!(matches!(leaf.remove_if(&i, &mut |_| true), RemoveResult::Fail));
                    assert!(leaf.is_empty());
                }
            }
        }

        #[cfg_attr(miri, ignore)]
        #[test]
        fn range(start in 0_usize..DIMENSION.num_entries, end in 0_usize..DIMENSION.num_entries) {
            let leaf: Leaf<usize, usize> = Leaf::new();
            for i in 1..DIMENSION.num_entries - 1 {
                prop_assert!(matches!(leaf.insert(i, i), InsertResult::Success));
            }
            leaf.remove_range(&(start..end));
            for i in 1..DIMENSION.num_entries - 1 {
                prop_assert!(leaf.search_entry(&i).is_none() == (start..end).contains(&i));
            }
            prop_assert!(leaf.search_entry(&0).is_none());
            prop_assert!(leaf.search_entry(&(DIMENSION.num_entries - 1)).is_none());
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn update() {
        let num_excess = 3;
        let num_tasks = DIMENSION.num_entries + num_excess;
        for _ in 0..256 {
            let barrier = Shared::new(Barrier::new(num_tasks));
            let leaf: Shared<Leaf<usize, usize>> = Shared::new(Leaf::new());
            let full: Shared<AtomicUsize> = Shared::new(AtomicUsize::new(0));
            let retire: Shared<AtomicUsize> = Shared::new(AtomicUsize::new(0));
            let mut task_handles = Vec::with_capacity(num_tasks);
            for t in 1..=num_tasks {
                let barrier_clone = barrier.clone();
                let leaf_clone = leaf.clone();
                let full_clone = full.clone();
                let retire_clone = retire.clone();
                task_handles.push(tokio::spawn(async move {
                    barrier_clone.wait().await;
                    let inserted = match leaf_clone.insert(t, t) {
                        InsertResult::Success => {
                            assert_eq!(leaf_clone.search_entry(&t).unwrap(), (&t, &t));
                            true
                        }
                        InsertResult::Duplicate(_, _)
                        | InsertResult::Frozen(_, _)
                        | InsertResult::Retired(_, _)
                        | InsertResult::Retry(_, _) => {
                            unreachable!();
                        }
                        InsertResult::Full(k, v) => {
                            assert_eq!(k, v);
                            assert_eq!(k, t);
                            full_clone.fetch_add(1, Relaxed);
                            false
                        }
                    };
                    {
                        let mut prev = 0;
                        let mut scanner = Scanner::new(&leaf_clone);
                        for e in scanner.by_ref() {
                            assert_eq!(e.0, e.1);
                            assert!(*e.0 > prev);
                            prev = *e.0;
                        }
                    }

                    barrier_clone.wait().await;
                    assert_eq!((*full_clone).load(Relaxed), num_excess);
                    if inserted {
                        assert_eq!(leaf_clone.search_entry(&t).unwrap(), (&t, &t));
                    }
                    {
                        let scanner = Scanner::new(&leaf_clone);
                        assert_eq!(scanner.count(), DIMENSION.num_entries);
                    }

                    barrier_clone.wait().await;
                    match leaf_clone.remove_if(&t, &mut |_| true) {
                        RemoveResult::Success => assert!(inserted),
                        RemoveResult::Fail => assert!(!inserted),
                        RemoveResult::Frozen | RemoveResult::Cleanup => unreachable!(),
                        RemoveResult::Retired => {
                            assert!(inserted);
                            assert_eq!(retire_clone.swap(1, Relaxed), 0);
                        }
                    }
                }));
            }
            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn durability() {
        let num_tasks = 16_usize;
        let workload_size = 8_usize;
        for _ in 0..16 {
            for k in 0..=workload_size {
                let barrier = Shared::new(Barrier::new(num_tasks));
                let leaf: Shared<Leaf<usize, usize>> = Shared::new(Leaf::new());
                let inserted: Shared<AtomicBool> = Shared::new(AtomicBool::new(false));
                let mut task_handles = Vec::with_capacity(num_tasks);
                for _ in 0..num_tasks {
                    let barrier_clone = barrier.clone();
                    let leaf_clone = leaf.clone();
                    let inserted_clone = inserted.clone();
                    task_handles.push(tokio::spawn(async move {
                        {
                            barrier_clone.wait().await;
                            if let InsertResult::Success = leaf_clone.insert(k, k) {
                                assert!(!inserted_clone.swap(true, Relaxed));
                            }
                        }
                        {
                            barrier_clone.wait().await;
                            for i in 0..workload_size {
                                if i != k {
                                    let _result = leaf_clone.insert(i, i);
                                }
                                assert!(!leaf_clone.is_retired());
                                assert_eq!(leaf_clone.search_entry(&k).unwrap(), (&k, &k));
                            }
                            for i in 0..workload_size {
                                let _result = leaf_clone.remove_if(&i, &mut |v| *v != k);
                                assert_eq!(leaf_clone.search_entry(&k).unwrap(), (&k, &k));
                            }
                        }
                    }));
                }
                for r in futures::future::join_all(task_handles).await {
                    assert!(r.is_ok());
                }
                assert!((*inserted).load(Relaxed));
            }
        }
    }
}
