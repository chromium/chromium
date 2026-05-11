//! [`HashIndex`] is a read-optimized concurrent and asynchronous hash map.

use super::ebr::{AtomicShared, Guard, Shared};
use super::hash_table::bucket::{Bucket, EntryPtr, Locker, OPTIMISTIC};
use super::hash_table::bucket_array::BucketArray;
use super::hash_table::{HashTable, LockedEntry};
use super::wait_queue::AsyncWait;
use super::Equivalent;
use std::collections::hash_map::RandomState;
use std::fmt::{self, Debug};
use std::hash::{BuildHasher, Hash};
use std::iter::FusedIterator;
use std::ops::{Deref, RangeInclusive};
use std::panic::UnwindSafe;
use std::pin::Pin;
use std::ptr;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::{Acquire, Relaxed};

/// Scalable concurrent hash index.
///
/// [`HashIndex`] is a concurrent and asynchronous hash map data structure optimized for parallel
/// read operations. The key characteristics of [`HashIndex`] are similar to that of
/// [`HashMap`](super::HashMap) except its read operations are lock-free.
///
/// ## The key differences between [`HashIndex`] and [`HashMap`](crate::HashMap).
///
/// * Lock-free-read: read and scan operations are never blocked and do not modify shared data.
/// * Immutability: the data in the container is immutable until it becomes unreachable.
/// * Linearizability: linearizability of read operations relies on the CPU architecture.
///
/// ## The key statistics for [`HashIndex`]
///
/// * The expected size of metadata for a single key-value pair: 2-byte.
/// * The expected number of atomic write operations required for an operation on a single key: 2.
/// * The expected number of atomic variables accessed during a single key operation: 2.
/// * The number of entries managed by a single bucket without a linked list: 32.
/// * The expected maximum linked list length when resize is triggered: log(capacity) / 8.
///
/// ## Unwind safety
///
/// [`HashIndex`] is impervious to out-of-memory errors and panics in user-specified code on one
/// condition; `H::Hasher::hash`, `K::drop` and `V::drop` must not panic.
pub struct HashIndex<K, V, H = RandomState>
where
    H: BuildHasher,
{
    array: AtomicShared<BucketArray<K, V, (), OPTIMISTIC>>,
    minimum_capacity: AtomicUsize,
    build_hasher: H,
}

/// [`Entry`] represents a single entry in a [`HashIndex`].
pub enum Entry<'h, K, V, H = RandomState>
where
    H: BuildHasher,
{
    /// An occupied entry.
    Occupied(OccupiedEntry<'h, K, V, H>),

    /// A vacant entry.
    Vacant(VacantEntry<'h, K, V, H>),
}

/// [`OccupiedEntry`] is a view into an occupied entry in a [`HashIndex`].
pub struct OccupiedEntry<'h, K, V, H = RandomState>
where
    H: BuildHasher,
{
    hashindex: &'h HashIndex<K, V, H>,
    locked_entry: LockedEntry<'h, K, V, (), OPTIMISTIC>,
}

/// [`VacantEntry`] is a view into a vacant entry in a [`HashIndex`].
pub struct VacantEntry<'h, K, V, H = RandomState>
where
    H: BuildHasher,
{
    hashindex: &'h HashIndex<K, V, H>,
    key: K,
    hash: u64,
    locked_entry: LockedEntry<'h, K, V, (), OPTIMISTIC>,
}

/// [`Reserve`] keeps the capacity of the associated [`HashIndex`] higher than a certain level.
///
/// The [`HashIndex`] does not shrink the capacity below the reserved capacity.
pub struct Reserve<'h, K, V, H = RandomState>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    hashindex: &'h HashIndex<K, V, H>,
    additional: usize,
}

/// An iterator over the entries of a [`HashIndex`].
///
/// An [`Iter`] iterates over all the entries that survive the [`Iter`].
pub struct Iter<'h, 'g, K, V, H = RandomState>
where
    H: BuildHasher,
{
    hashindex: &'h HashIndex<K, V, H>,
    current_array: Option<&'g BucketArray<K, V, (), OPTIMISTIC>>,
    current_index: usize,
    current_bucket: Option<&'g Bucket<K, V, (), OPTIMISTIC>>,
    current_entry_ptr: EntryPtr<'g, K, V, OPTIMISTIC>,
    guard: &'g Guard,
}

impl<K, V, H> HashIndex<K, V, H>
where
    H: BuildHasher,
{
    /// Creates an empty [`HashIndex`] with the given [`BuildHasher`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use std::collections::hash_map::RandomState;
    ///
    /// let hashindex: HashIndex<u64, u32, RandomState> =
    ///     HashIndex::with_hasher(RandomState::new());
    /// ```
    #[cfg(not(feature = "loom"))]
    #[inline]
    pub const fn with_hasher(build_hasher: H) -> Self {
        Self {
            array: AtomicShared::null(),
            minimum_capacity: AtomicUsize::new(0),
            build_hasher,
        }
    }

    /// Creates an empty [`HashIndex`] with the given [`BuildHasher`].
    #[cfg(feature = "loom")]
    #[inline]
    pub fn with_hasher(build_hasher: H) -> Self {
        Self {
            array: AtomicShared::null(),
            minimum_capacity: AtomicUsize::new(0),
            build_hasher,
        }
    }

    /// Creates an empty [`HashIndex`] with the specified capacity and [`BuildHasher`].
    ///
    /// The actual capacity is equal to or greater than the specified capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use std::collections::hash_map::RandomState;
    ///
    /// let hashindex: HashIndex<u64, u32, RandomState> =
    ///     HashIndex::with_capacity_and_hasher(1000, RandomState::new());
    ///
    /// let result = hashindex.capacity();
    /// assert_eq!(result, 1024);
    /// ```
    #[inline]
    pub fn with_capacity_and_hasher(capacity: usize, build_hasher: H) -> Self {
        let (array, minimum_capacity) = if capacity == 0 {
            (AtomicShared::null(), AtomicUsize::new(0))
        } else {
            let array = unsafe {
                Shared::new_unchecked(BucketArray::<K, V, (), OPTIMISTIC>::new(
                    capacity,
                    AtomicShared::null(),
                ))
            };
            let minimum_capacity = array.num_entries();
            (
                AtomicShared::from(array),
                AtomicUsize::new(minimum_capacity),
            )
        };
        Self {
            array,
            minimum_capacity,
            build_hasher,
        }
    }
}

impl<K, V, H> HashIndex<K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    /// Temporarily increases the minimum capacity of the [`HashIndex`].
    ///
    /// A [`Reserve`] is returned if the [`HashIndex`] could increase the minimum capacity while
    /// the increased capacity is not exclusively owned by the returned [`Reserve`], allowing
    /// others to benefit from it. The memory for the additional space may not be immediately
    /// allocated if the [`HashIndex`] is empty or currently being resized, however once the memory
    /// is reserved eventually, the capacity will not shrink below the additional capacity until
    /// the returned [`Reserve`] is dropped.
    ///
    /// # Errors
    ///
    /// Returns `None` if a too large number is given.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<usize, usize> = HashIndex::with_capacity(1000);
    /// assert_eq!(hashindex.capacity(), 1024);
    ///
    /// let reserved = hashindex.reserve(10000);
    /// assert!(reserved.is_some());
    /// assert_eq!(hashindex.capacity(), 16384);
    ///
    /// assert!(hashindex.reserve(usize::MAX).is_none());
    /// assert_eq!(hashindex.capacity(), 16384);
    ///
    /// for i in 0..16 {
    ///     assert!(hashindex.insert(i, i).is_ok());
    /// }
    /// drop(reserved);
    ///
    /// assert_eq!(hashindex.capacity(), 1024);
    /// ```
    #[inline]
    pub fn reserve(&self, additional_capacity: usize) -> Option<Reserve<'_, K, V, H>> {
        let additional = self.reserve_capacity(additional_capacity);
        if additional == 0 {
            None
        } else {
            Some(Reserve {
                hashindex: self,
                additional,
            })
        }
    }

    /// Gets the entry associated with the given key in the map for in-place manipulation.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<char, u32> = HashIndex::default();
    ///
    /// for ch in "a short treatise on fungi".chars() {
    ///     unsafe {
    ///         hashindex.entry(ch).and_modify(|counter| *counter += 1).or_insert(1);
    ///     }
    /// }
    ///
    /// assert_eq!(hashindex.peek_with(&'s', |_, v| *v), Some(2));
    /// assert_eq!(hashindex.peek_with(&'t', |_, v| *v), Some(3));
    /// assert!(hashindex.peek_with(&'y', |_, v| *v).is_none());
    /// ```
    #[inline]
    pub fn entry(&self, key: K) -> Entry<'_, K, V, H> {
        let guard = Guard::new();
        let hash = self.hash(&key);
        let locked_entry = unsafe {
            self.reserve_entry(&key, hash, &mut (), self.prolonged_guard_ref(&guard))
                .ok()
                .unwrap_unchecked()
        };
        if locked_entry.entry_ptr.is_valid() {
            Entry::Occupied(OccupiedEntry {
                hashindex: self,
                locked_entry,
            })
        } else {
            Entry::Vacant(VacantEntry {
                hashindex: self,
                key,
                hash,
                locked_entry,
            })
        }
    }

    /// Gets the entry associated with the given key in the map for in-place manipulation.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<char, u32> = HashIndex::default();
    ///
    /// let future_entry = hashindex.entry_async('b');
    /// ```
    #[inline]
    pub async fn entry_async(&self, key: K) -> Entry<'_, K, V, H> {
        let hash = self.hash(&key);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            {
                let guard = Guard::new();
                if let Ok(locked_entry) = self.reserve_entry(
                    &key,
                    hash,
                    &mut async_wait_pinned,
                    self.prolonged_guard_ref(&guard),
                ) {
                    if locked_entry.entry_ptr.is_valid() {
                        return Entry::Occupied(OccupiedEntry {
                            hashindex: self,
                            locked_entry,
                        });
                    }
                    return Entry::Vacant(VacantEntry {
                        hashindex: self,
                        key,
                        hash,
                        locked_entry,
                    });
                }
            }
            async_wait_pinned.await;
        }
    }

    /// Tries to get the entry associated with the given key in the map for in-place manipulation.
    ///
    /// Returns `None` if the entry could not be locked.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<usize, usize> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(0, 1).is_ok());
    /// assert!(hashindex.try_entry(0).is_some());
    /// ```
    #[inline]
    pub fn try_entry(&self, key: K) -> Option<Entry<'_, K, V, H>> {
        let guard = Guard::new();
        let hash = self.hash(&key);
        let locked_entry = self.try_reserve_entry(&key, hash, self.prolonged_guard_ref(&guard))?;
        if locked_entry.entry_ptr.is_valid() {
            Some(Entry::Occupied(OccupiedEntry {
                hashindex: self,
                locked_entry,
            }))
        } else {
            Some(Entry::Vacant(VacantEntry {
                hashindex: self,
                key,
                hash,
                locked_entry,
            }))
        }
    }

    /// Gets the first occupied entry for in-place manipulation.
    ///
    /// The returned [`OccupiedEntry`] in combination with [`OccupiedEntry::next`] or
    /// [`OccupiedEntry::next_async`] can act as a mutable iterator over entries.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    ///
    /// let mut first_entry = hashindex.first_entry().unwrap();
    /// unsafe {
    ///     *first_entry.get_mut() = 2;
    /// }
    ///
    /// assert!(first_entry.next().is_none());
    /// assert_eq!(hashindex.peek_with(&1, |_, v| *v), Some(2));
    /// ```
    #[inline]
    pub fn first_entry(&self) -> Option<OccupiedEntry<'_, K, V, H>> {
        let guard = Guard::new();
        let prolonged_guard = self.prolonged_guard_ref(&guard);
        if let Some(locked_entry) = self.lock_first_entry(prolonged_guard) {
            return Some(OccupiedEntry {
                hashindex: self,
                locked_entry,
            });
        }
        None
    }

    /// Gets the first occupied entry for in-place manipulation.
    ///
    /// The returned [`OccupiedEntry`] in combination with [`OccupiedEntry::next`] or
    /// [`OccupiedEntry::next_async`] can act as a mutable iterator over entries.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<char, u32> = HashIndex::default();
    ///
    /// let future_entry = hashindex.first_entry_async();
    /// ```
    #[inline]
    pub async fn first_entry_async(&self) -> Option<OccupiedEntry<'_, K, V, H>> {
        if let Some(locked_entry) = LockedEntry::first_entry_async(self).await {
            return Some(OccupiedEntry {
                hashindex: self,
                locked_entry,
            });
        }
        None
    }

    /// Finds any entry satisfying the supplied predicate for in-place manipulation.
    ///
    /// The returned [`OccupiedEntry`] in combination with [`OccupiedEntry::next`] or
    /// [`OccupiedEntry::next_async`] can act as a mutable iterator over entries.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(hashindex.insert(2, 3).is_ok());
    ///
    /// let mut entry = hashindex.any_entry(|k, _| *k == 2).unwrap();
    /// assert_eq!(*entry.get(), 3);
    /// ```
    #[inline]
    pub fn any_entry<P: FnMut(&K, &V) -> bool>(
        &self,
        pred: P,
    ) -> Option<OccupiedEntry<'_, K, V, H>> {
        let guard = Guard::new();
        let prolonged_guard = self.prolonged_guard_ref(&guard);
        let locked_entry = self.find_entry(pred, prolonged_guard)?;
        Some(OccupiedEntry {
            hashindex: self,
            locked_entry,
        })
    }

    /// Finds any entry satisfying the supplied predicate for in-place manipulation.
    ///
    /// The returned [`OccupiedEntry`] in combination with [`OccupiedEntry::next`] or
    /// [`OccupiedEntry::next_async`] can act as a mutable iterator over entries.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// let future_entry = hashindex.any_entry_async(|k, _| *k == 2);
    /// ```
    #[inline]
    pub async fn any_entry_async<P: FnMut(&K, &V) -> bool>(
        &self,
        mut pred: P,
    ) -> Option<OccupiedEntry<'_, K, V, H>> {
        if let Some(locked_entry) = LockedEntry::first_entry_async(self).await {
            let mut entry = OccupiedEntry {
                hashindex: self,
                locked_entry,
            };
            loop {
                if pred(entry.key(), entry.get()) {
                    return Some(entry);
                }
                entry = entry.next()?;
            }
        }
        None
    }

    /// Inserts a key-value pair into the [`HashIndex`].
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key-value pair if the key exists.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert_eq!(hashindex.insert(1, 1).unwrap_err(), (1, 1));
    /// ```
    #[inline]
    pub fn insert(&self, key: K, val: V) -> Result<(), (K, V)> {
        let guard = Guard::new();
        let hash = self.hash(&key);
        if let Ok(Some((k, v))) = self.insert_entry(key, val, hash, &mut (), &guard) {
            Err((k, v))
        } else {
            Ok(())
        }
    }

    /// Inserts a key-value pair into the [`HashIndex`].
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
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// let future_insert = hashindex.insert_async(11, 17);
    /// ```
    #[inline]
    pub async fn insert_async(&self, mut key: K, mut val: V) -> Result<(), (K, V)> {
        let hash = self.hash(&key);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            match self.insert_entry(key, val, hash, &mut async_wait_pinned, &Guard::new()) {
                Ok(Some(returned)) => return Err(returned),
                Ok(None) => return Ok(()),
                Err(returned) => {
                    key = returned.0;
                    val = returned.1;
                }
            }
            async_wait_pinned.await;
        }
    }

    /// Removes a key-value pair if the key exists.
    ///
    /// Returns `false` if the key does not exist.
    ///
    /// Returns `true` if the key existed and the condition was met after marking the entry
    /// unreachable; the memory will be reclaimed later.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(!hashindex.remove(&1));
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(hashindex.remove(&1));
    /// ```
    #[inline]
    pub fn remove<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.remove_if(key, |_| true)
    }

    /// Removes a key-value pair if the key exists.
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
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// let future_insert = hashindex.insert_async(11, 17);
    /// let future_remove = hashindex.remove_async(&11);
    /// ```
    #[inline]
    pub async fn remove_async<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.remove_if_async(key, |_| true).await
    }

    /// Removes a key-value pair if the key exists and the given condition is met.
    ///
    /// Returns `false` if the key does not exist or the condition was not met.
    ///
    /// Returns `true` if the key existed and the condition was met after marking the entry
    /// unreachable; the memory will be reclaimed later.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(!hashindex.remove_if(&1, |v| *v == 1));
    /// assert!(hashindex.remove_if(&1, |v| *v == 0));
    /// ```
    #[inline]
    pub fn remove_if<Q, F: FnOnce(&V) -> bool>(&self, key: &Q, condition: F) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.remove_entry(
            key,
            self.hash(key),
            |v: &mut V| condition(v),
            |r| r.is_some(),
            &mut (),
            &Guard::new(),
        )
        .ok()
        .map_or(false, |r| r)
    }

    /// Removes a key-value pair if the key exists and the given condition is met.
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
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// let future_insert = hashindex.insert_async(11, 17);
    /// let future_remove = hashindex.remove_if_async(&11, |_| true);
    /// ```
    #[inline]
    pub async fn remove_if_async<Q, F: FnOnce(&V) -> bool>(&self, key: &Q, condition: F) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let hash = self.hash(key);
        let mut condition = |v: &mut V| condition(v);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            match self.remove_entry(
                key,
                hash,
                condition,
                |r| r.is_some(),
                &mut async_wait_pinned,
                &Guard::new(),
            ) {
                Ok(r) => return r,
                Err(c) => condition = c,
            }
            async_wait_pinned.await;
        }
    }

    /// Gets an [`OccupiedEntry`] corresponding to the key for in-place modification.
    ///
    /// [`OccupiedEntry`] exclusively owns the entry, preventing others from gaining access to it:
    /// use [`peek`](Self::peek) if read-only access is sufficient.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.get(&1).is_none());
    /// assert!(hashindex.insert(1, 10).is_ok());
    /// assert_eq!(*hashindex.get(&1).unwrap().get(), 10);
    /// assert_eq!(*hashindex.get(&1).unwrap(), 10);
    /// ```
    #[inline]
    pub fn get<Q>(&self, key: &Q) -> Option<OccupiedEntry<'_, K, V, H>>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let guard = Guard::new();
        let locked_entry = self
            .get_entry(
                key,
                self.hash(key),
                &mut (),
                self.prolonged_guard_ref(&guard),
            )
            .ok()
            .flatten()?;
        Some(OccupiedEntry {
            hashindex: self,
            locked_entry,
        })
    }

    /// Gets an [`OccupiedEntry`] corresponding to the key for in-place modification.
    ///
    /// [`OccupiedEntry`] exclusively owns the entry, preventing others from gaining access to it:
    /// use [`peek`](Self::peek) if read-only access is sufficient.
    ///
    /// Returns `None` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// let future_insert = hashindex.insert_async(11, 17);
    /// let future_get = hashindex.get_async(&11);
    /// ```
    #[inline]
    pub async fn get_async<Q>(&self, key: &Q) -> Option<OccupiedEntry<'_, K, V, H>>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let hash = self.hash(key);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            if let Ok(result) = self.get_entry(
                key,
                hash,
                &mut async_wait_pinned,
                self.prolonged_guard_ref(&Guard::new()),
            ) {
                if let Some(locked_entry) = result {
                    return Some(OccupiedEntry {
                        hashindex: self,
                        locked_entry,
                    });
                }
                return None;
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
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 10).is_ok());
    ///
    /// let guard = Guard::new();
    /// let value_ref = hashindex.peek(&1, &guard).unwrap();
    /// assert_eq!(*value_ref, 10);
    /// ```
    #[inline]
    pub fn peek<'g, Q>(&self, key: &Q, guard: &'g Guard) -> Option<&'g V>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.peek_entry(key, self.hash(key), guard).map(|(_, v)| v)
    }

    /// Peeks a key-value pair without acquiring locks.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.peek_with(&1, |_, v| *v).is_none());
    /// assert!(hashindex.insert(1, 10).is_ok());
    /// assert_eq!(hashindex.peek_with(&1, |_, v| *v).unwrap(), 10);
    /// ```
    #[inline]
    pub fn peek_with<Q, R, F: FnOnce(&K, &V) -> R>(&self, key: &Q, reader: F) -> Option<R>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let guard = Guard::new();
        self.peek_entry(key, self.hash(key), &guard)
            .map(|(k, v)| reader(k, v))
    }

    /// Returns `true` if the [`HashIndex`] contains a value for the specified key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(!hashindex.contains(&1));
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(hashindex.contains(&1));
    /// ```
    #[inline]
    pub fn contains<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.peek_with(key, |_, _| ()).is_some()
    }

    /// Retains the entries specified by the predicate.
    ///
    /// Entries that have existed since the invocation of the method are guaranteed to be visited
    /// if they are not removed, however the same entry can be visited more than once if the
    /// [`HashIndex`] gets resized by another thread.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(hashindex.insert(2, 1).is_ok());
    /// assert!(hashindex.insert(3, 2).is_ok());
    ///
    /// hashindex.retain(|k, v| *k == 1 && *v == 0);
    ///
    /// assert!(hashindex.contains(&1));
    /// assert!(!hashindex.contains(&2));
    /// assert!(!hashindex.contains(&3));
    /// ```
    #[inline]
    pub fn retain<F: FnMut(&K, &V) -> bool>(&self, mut pred: F) {
        self.retain_entries(|k, v| pred(k, v));
    }

    /// Retains the entries specified by the predicate.
    ///
    /// Entries that have existed since the invocation of the method are guaranteed to be visited
    /// if they are not removed, however the same entry can be visited more than once if the
    /// [`HashIndex`] gets resized by another thread.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// let future_insert = hashindex.insert_async(1, 0);
    /// let future_retain = hashindex.retain_async(|k, v| *k == 1);
    /// ```
    #[inline]
    pub async fn retain_async<F: FnMut(&K, &V) -> bool>(&self, mut pred: F) {
        let mut removed = false;
        let mut current_array_holder = self.array.get_shared(Acquire, &Guard::new());
        while let Some(current_array) = current_array_holder.take() {
            self.cleanse_old_array_async(&current_array).await;
            for index in 0..current_array.num_buckets() {
                loop {
                    let mut async_wait = AsyncWait::default();
                    let mut async_wait_pinned = Pin::new(&mut async_wait);
                    {
                        let guard = Guard::new();
                        let bucket = current_array.bucket_mut(index);
                        if let Ok(locker) =
                            Locker::try_lock_or_wait(bucket, &mut async_wait_pinned, &guard)
                        {
                            if let Some(mut locker) = locker {
                                let data_block_mut = current_array.data_block_mut(index);
                                let mut entry_ptr = EntryPtr::new(&guard);
                                while entry_ptr.move_to_next(&locker, &guard) {
                                    let (k, v) = entry_ptr.get(data_block_mut);
                                    if !pred(k, v) {
                                        locker.mark_removed(&mut entry_ptr, &guard);
                                        removed = true;
                                    }
                                }
                            }
                            break;
                        };
                    }
                    async_wait_pinned.await;
                }
            }

            if let Some(new_current_array) = self.array.get_shared(Acquire, &Guard::new()) {
                if new_current_array.as_ptr() == current_array.as_ptr() {
                    break;
                }
                current_array_holder.replace(new_current_array);
                continue;
            }
            break;
        }

        if removed {
            self.try_resize(0, &Guard::new());
        }
    }

    /// Clears the [`HashIndex`] by removing all key-value pairs.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// hashindex.clear();
    ///
    /// assert!(!hashindex.contains(&1));
    /// ```
    pub fn clear(&self) {
        self.retain(|_, _| false);
    }

    /// Clears the [`HashIndex`] by removing all key-value pairs.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// let future_insert = hashindex.insert_async(1, 0);
    /// let future_retain = hashindex.clear_async();
    /// ```
    pub async fn clear_async(&self) {
        self.retain_async(|_, _| false).await;
    }

    /// Returns the number of entries in the [`HashIndex`].
    ///
    /// It reads the entire metadata area of the bucket array to calculate the number of valid
    /// entries, making its time complexity `O(N)`. Furthermore, it may overcount entries if an old
    /// bucket array has yet to be dropped.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert_eq!(hashindex.len(), 1);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        self.num_entries(&Guard::new())
    }

    /// Returns `true` if the [`HashIndex`] is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.is_empty());
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(!hashindex.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        !self.has_entry(&Guard::new())
    }

    /// Returns the capacity of the [`HashIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex_default: HashIndex<u64, u32> = HashIndex::default();
    /// assert_eq!(hashindex_default.capacity(), 0);
    ///
    /// assert!(hashindex_default.insert(1, 0).is_ok());
    /// assert_eq!(hashindex_default.capacity(), 64);
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::with_capacity(1000);
    /// assert_eq!(hashindex.capacity(), 1024);
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize {
        self.num_slots(&Guard::new())
    }

    /// Returns the current capacity range of the [`HashIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert_eq!(hashindex.capacity_range(), 0..=(1_usize << (usize::BITS - 1)));
    ///
    /// let reserved = hashindex.reserve(1000);
    /// assert_eq!(hashindex.capacity_range(), 1000..=(1_usize << (usize::BITS - 1)));
    /// ```
    #[inline]
    pub fn capacity_range(&self) -> RangeInclusive<usize> {
        self.minimum_capacity.load(Relaxed)..=self.maximum_capacity()
    }

    /// Returns the index of the bucket that may contain the key.
    ///
    /// The method returns the index of the bucket associated with the key. The number of buckets
    /// can be calculated by dividing `32` into the capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::with_capacity(1024);
    ///
    /// let bucket_index = hashindex.bucket_index(&11);
    /// assert!(bucket_index < hashindex.capacity() / 32);
    /// ```
    #[inline]
    pub fn bucket_index<Q>(&self, key: &Q) -> usize
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.calculate_bucket_index(key)
    }

    /// Returns an [`Iter`].
    ///
    /// It is guaranteed to go through all the key-value pairs pertaining in the [`HashIndex`]
    /// at the moment, however the same key-value pair can be visited more than once if the
    /// [`HashIndex`] is being resized.
    ///
    /// It requires the user to supply a reference to a [`Guard`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::ebr::Guard;
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    ///
    /// let guard = Guard::new();
    ///
    /// let mut iter = hashindex.iter(&guard);
    /// let entry_ref = iter.next().unwrap();
    /// assert_eq!(iter.next(), None);
    ///
    /// for iter in hashindex.iter(&guard) {
    ///     assert_eq!(iter, (&1, &0));
    /// }
    ///
    /// drop(hashindex);
    ///
    /// assert_eq!(entry_ref, (&1, &0));
    /// ```
    #[inline]
    pub fn iter<'h, 'g>(&'h self, guard: &'g Guard) -> Iter<'h, 'g, K, V, H> {
        Iter {
            hashindex: self,
            current_array: None,
            current_index: 0,
            current_bucket: None,
            current_entry_ptr: EntryPtr::new(guard),
            guard,
        }
    }

    /// Clears the old array asynchronously.
    async fn cleanse_old_array_async(&self, current_array: &BucketArray<K, V, (), OPTIMISTIC>) {
        while current_array.has_old_array() {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            if self.incremental_rehash::<K, _, false>(
                current_array,
                &mut async_wait_pinned,
                &Guard::new(),
            ) == Ok(true)
            {
                break;
            }
            async_wait_pinned.await;
        }
    }
}

impl<K, V, H> Clone for HashIndex<K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher + Clone,
{
    #[inline]
    fn clone(&self) -> Self {
        let self_clone = Self::with_capacity_and_hasher(self.capacity(), self.hasher().clone());
        for (k, v) in self.iter(&Guard::new()) {
            let _result = self_clone.insert(k.clone(), v.clone());
        }
        self_clone
    }
}

impl<K, V, H> Debug for HashIndex<K, V, H>
where
    K: 'static + Clone + Debug + Eq + Hash,
    V: 'static + Clone + Debug,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let guard = Guard::new();
        f.debug_map().entries(self.iter(&guard)).finish()
    }
}

impl<K, V> HashIndex<K, V, RandomState>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
{
    /// Creates an empty default [`HashIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::new();
    ///
    /// let result = hashindex.capacity();
    /// assert_eq!(result, 0);
    /// ```
    #[inline]
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Creates an empty [`HashIndex`] with the specified capacity.
    ///
    /// The actual capacity is equal to or greater than the specified capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::with_capacity(1000);
    ///
    /// let result = hashindex.capacity();
    /// assert_eq!(result, 1024);
    /// ```
    #[inline]
    #[must_use]
    pub fn with_capacity(capacity: usize) -> Self {
        Self::with_capacity_and_hasher(capacity, RandomState::new())
    }
}

impl<K, V, H> Default for HashIndex<K, V, H>
where
    K: 'static,
    V: 'static,
    H: BuildHasher + Default,
{
    /// Creates an empty default [`HashIndex`].
    ///
    /// The default capacity is `64`.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// let result = hashindex.capacity();
    /// assert_eq!(result, 0);
    /// ```
    #[inline]
    fn default() -> Self {
        Self::with_hasher(H::default())
    }
}

impl<K, V, H> FromIterator<(K, V)> for HashIndex<K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher + Default,
{
    #[inline]
    fn from_iter<T: IntoIterator<Item = (K, V)>>(iter: T) -> Self {
        let into_iter = iter.into_iter();
        let hashindex = Self::with_capacity_and_hasher(
            Self::capacity_from_size_hint(into_iter.size_hint()),
            H::default(),
        );
        into_iter.for_each(|e| {
            let _result = hashindex.insert(e.0, e.1);
        });
        hashindex
    }
}

impl<K, V, H> HashTable<K, V, H, (), OPTIMISTIC> for HashIndex<K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    #[inline]
    fn hasher(&self) -> &H {
        &self.build_hasher
    }
    #[inline]
    fn try_clone(entry: &(K, V)) -> Option<(K, V)> {
        Some((entry.0.clone(), entry.1.clone()))
    }
    #[inline]
    fn bucket_array(&self) -> &AtomicShared<BucketArray<K, V, (), OPTIMISTIC>> {
        &self.array
    }
    #[inline]
    fn minimum_capacity(&self) -> &AtomicUsize {
        &self.minimum_capacity
    }
    #[inline]
    fn maximum_capacity(&self) -> usize {
        1_usize << (usize::BITS - 1)
    }
}

impl<K, V, H> PartialEq for HashIndex<K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone + PartialEq,
    H: BuildHasher,
{
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        let guard = Guard::new();
        if !self
            .iter(&guard)
            .any(|(k, v)| other.peek_with(k, |_, ov| v == ov) != Some(true))
        {
            return !other
                .iter(&guard)
                .any(|(k, v)| self.peek_with(k, |_, sv| v == sv) != Some(true));
        }
        false
    }
}

impl<'h, K, V, H> Entry<'h, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    /// Ensures a value is in the entry by inserting the supplied instance if empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(3).or_insert(7);
    /// assert_eq!(hashindex.peek_with(&3, |_, v| *v), Some(7));
    /// ```
    #[inline]
    pub fn or_insert(self, val: V) -> OccupiedEntry<'h, K, V, H> {
        self.or_insert_with(|| val)
    }

    /// Ensures a value is in the entry by inserting the result of the supplied closure if empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(19).or_insert_with(|| 5);
    /// assert_eq!(hashindex.peek_with(&19, |_, v| *v), Some(5));
    /// ```
    #[inline]
    pub fn or_insert_with<F: FnOnce() -> V>(self, constructor: F) -> OccupiedEntry<'h, K, V, H> {
        self.or_insert_with_key(|_| constructor())
    }

    /// Ensures a value is in the entry by inserting the result of the supplied closure if empty.
    ///
    /// The reference to the moved key is provided, therefore cloning or copying the key is
    /// unnecessary.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(11).or_insert_with_key(|k| if *k == 11 { 7 } else { 3 });
    /// assert_eq!(hashindex.peek_with(&11, |_, v| *v), Some(7));
    /// ```
    #[inline]
    pub fn or_insert_with_key<F: FnOnce(&K) -> V>(
        self,
        constructor: F,
    ) -> OccupiedEntry<'h, K, V, H> {
        match self {
            Self::Occupied(o) => o,
            Self::Vacant(v) => {
                let val = constructor(v.key());
                v.insert_entry(val)
            }
        }
    }

    /// Returns a reference to the key of this entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// assert_eq!(hashindex.entry(31).key(), &31);
    /// ```
    #[inline]
    pub fn key(&self) -> &K {
        match self {
            Self::Occupied(o) => o.key(),
            Self::Vacant(v) => v.key(),
        }
    }

    /// Provides in-place mutable access to an occupied entry.
    ///
    /// # Safety
    ///
    /// The caller has to make sure that there are no readers of the entry, e.g., a reader keeping
    /// a reference to the entry via [`HashIndex::iter`], [`HashIndex::peek`], or
    /// [`HashIndex::peek_with`], unless an instance of `V` can be safely read when there is a
    /// single writer, e.g., `V = [u8; 32]`.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// unsafe {
    ///     hashindex.entry(37).and_modify(|v| { *v += 1 }).or_insert(47);
    /// }
    /// assert_eq!(hashindex.peek_with(&37, |_, v| *v), Some(47));
    ///
    /// unsafe {
    ///     hashindex.entry(37).and_modify(|v| { *v += 1 }).or_insert(3);
    /// }
    /// assert_eq!(hashindex.peek_with(&37, |_, v| *v), Some(48));
    /// ```
    #[inline]
    #[must_use]
    pub unsafe fn and_modify<F>(self, f: F) -> Self
    where
        F: FnOnce(&mut V),
    {
        match self {
            Self::Occupied(mut o) => {
                f(o.get_mut());
                Self::Occupied(o)
            }
            Self::Vacant(_) => self,
        }
    }
}

impl<'h, K, V, H> Entry<'h, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone + Default,
    H: BuildHasher,
{
    /// Ensures a value is in the entry by inserting the default value if empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// hashindex.entry(11).or_default();
    /// assert_eq!(hashindex.peek_with(&11, |_, v| *v), Some(0));
    /// ```
    #[inline]
    pub fn or_default(self) -> OccupiedEntry<'h, K, V, H> {
        match self {
            Self::Occupied(o) => o,
            Self::Vacant(v) => v.insert_entry(Default::default()),
        }
    }
}

impl<K, V, H> Debug for Entry<'_, K, V, H>
where
    K: 'static + Clone + Debug + Eq + Hash,
    V: 'static + Clone + Debug,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Vacant(v) => f.debug_tuple("Entry").field(v).finish(),
            Self::Occupied(o) => f.debug_tuple("Entry").field(o).finish(),
        }
    }
}

impl<'h, K, V, H> OccupiedEntry<'h, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    /// Gets a reference to the key in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert_eq!(hashindex.entry(29).or_default().key(), &29);
    /// ```
    #[inline]
    #[must_use]
    pub fn key(&self) -> &K {
        &self
            .locked_entry
            .entry_ptr
            .get(self.locked_entry.data_block_mut)
            .0
    }

    /// Marks that the entry is removed from the [`HashIndex`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(11).or_insert(17);
    ///
    /// if let Entry::Occupied(o) = hashindex.entry(11) {
    ///     o.remove_entry();
    /// };
    /// assert_eq!(hashindex.peek_with(&11, |_, v| *v), None);
    /// ```
    #[inline]
    pub fn remove_entry(mut self) {
        let guard = Guard::new();
        self.locked_entry.locker.mark_removed(
            &mut self.locked_entry.entry_ptr,
            self.hashindex.prolonged_guard_ref(&guard),
        );
        if self.locked_entry.locker.num_entries() <= 1 || self.locked_entry.locker.need_rebuild() {
            let hashindex = self.hashindex;
            if let Some(current_array) = hashindex.bucket_array().load(Acquire, &guard).as_ref() {
                if !current_array.has_old_array() {
                    let index = self.locked_entry.index;
                    if current_array.initiate_sampling(index) {
                        drop(self);
                        hashindex.try_shrink_or_rebuild(current_array, index, &guard);
                    }
                }
            }
        }
    }

    /// Gets a reference to the value in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(19).or_insert(11);
    ///
    /// if let Entry::Occupied(o) = hashindex.entry(19) {
    ///     assert_eq!(o.get(), &11);
    /// };
    /// ```
    #[inline]
    #[must_use]
    pub fn get(&self) -> &V {
        &self
            .locked_entry
            .entry_ptr
            .get(self.locked_entry.data_block_mut)
            .1
    }

    /// Gets a mutable reference to the value in the entry.
    ///
    /// # Safety
    ///
    /// The caller has to make sure that there are no readers of the entry, e.g., a reader keeping
    /// a reference to the entry via [`HashIndex::iter`], [`HashIndex::peek`], or
    /// [`HashIndex::peek_with`], unless an instance of `V` can be safely read when there is a
    /// single writer, e.g., `V = [u8; 32]`.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(37).or_insert(11);
    ///
    /// if let Entry::Occupied(mut o) = hashindex.entry(37) {
    ///     // Safety: `u32` can be safely read while being modified.
    ///     unsafe { *o.get_mut() += 18; }
    ///     assert_eq!(*o.get(), 29);
    /// }
    ///
    /// assert_eq!(hashindex.peek_with(&37, |_, v| *v), Some(29));
    /// ```
    #[inline]
    pub unsafe fn get_mut(&mut self) -> &mut V {
        &mut self
            .locked_entry
            .entry_ptr
            .get_mut(
                self.locked_entry.data_block_mut,
                &mut self.locked_entry.locker,
            )
            .1
    }

    /// Updates the entry by inserting a new entry and marking the existing entry removed.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// hashindex.entry(37).or_insert(11);
    ///
    /// if let Entry::Occupied(mut o) = hashindex.entry(37) {
    ///     o.update(29);
    /// }
    ///
    /// assert_eq!(hashindex.peek_with(&37, |_, v| *v), Some(29));
    /// ```
    #[inline]
    pub fn update(mut self, val: V) {
        let key = self.key().clone();
        let partial_hash = self
            .locked_entry
            .entry_ptr
            .partial_hash(&self.locked_entry.locker);
        let guard = Guard::new();
        self.locked_entry.locker.insert_with(
            self.locked_entry.data_block_mut,
            partial_hash,
            || (key, val),
            self.hashindex.prolonged_guard_ref(&guard),
        );
        self.locked_entry.locker.mark_removed(
            &mut self.locked_entry.entry_ptr,
            self.hashindex.prolonged_guard_ref(&guard),
        );
    }

    /// Gets the next closest occupied entry.
    ///
    /// [`HashIndex::first_entry`], [`HashIndex::first_entry_async`], and this method together
    /// enables the [`OccupiedEntry`] to effectively act as a mutable iterator over entries. The
    /// method never acquires more than one lock even when it searches other buckets for the next
    /// closest occupied entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(hashindex.insert(2, 0).is_ok());
    ///
    /// let first_entry = hashindex.first_entry().unwrap();
    /// let first_key = *first_entry.key();
    /// let second_entry = first_entry.next().unwrap();
    /// let second_key = *second_entry.key();
    ///
    /// assert!(second_entry.next().is_none());
    /// assert_eq!(first_key + second_key, 3);
    /// ```
    #[inline]
    #[must_use]
    pub fn next(self) -> Option<Self> {
        let hashindex = self.hashindex;
        if let Some(locked_entry) = self.locked_entry.next(hashindex) {
            return Some(OccupiedEntry {
                hashindex,
                locked_entry,
            });
        }
        None
    }

    /// Gets the next closest occupied entry.
    ///
    /// [`HashIndex::first_entry`], [`HashIndex::first_entry_async`], and this method together
    /// enables the [`OccupiedEntry`] to effectively act as a mutable iterator over entries. The
    /// method never acquires more than one lock even when it searches other buckets for the next
    /// closest occupied entry.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// assert!(hashindex.insert(1, 0).is_ok());
    /// assert!(hashindex.insert(2, 0).is_ok());
    ///
    /// let second_entry_future = hashindex.first_entry().unwrap().next_async();
    /// ```
    #[inline]
    pub async fn next_async(self) -> Option<OccupiedEntry<'h, K, V, H>> {
        let hashindex = self.hashindex;
        if let Some(locked_entry) = self.locked_entry.next_async(hashindex).await {
            return Some(OccupiedEntry {
                hashindex,
                locked_entry,
            });
        }
        None
    }
}

impl<K, V, H> Debug for OccupiedEntry<'_, K, V, H>
where
    K: 'static + Clone + Debug + Eq + Hash,
    V: 'static + Clone + Debug,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("OccupiedEntry")
            .field("key", self.key())
            .field("value", self.get())
            .finish_non_exhaustive()
    }
}

impl<K, V, H> Deref for OccupiedEntry<'_, K, V, H>
where
    K: 'static + Clone + Debug + Eq + Hash,
    V: 'static + Clone + Debug,
    H: BuildHasher,
{
    type Target = V;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

impl<'h, K, V, H> VacantEntry<'h, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    /// Gets a reference to the key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    /// assert_eq!(hashindex.entry(11).key(), &11);
    /// ```
    #[inline]
    pub fn key(&self) -> &K {
        &self.key
    }

    /// Takes ownership of the key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// if let Entry::Vacant(v) = hashindex.entry(17) {
    ///     assert_eq!(v.into_key(), 17);
    /// };
    /// ```
    #[inline]
    pub fn into_key(self) -> K {
        self.key
    }

    /// Sets the value of the entry with its key, and returns an [`OccupiedEntry`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashIndex;
    /// use scc::hash_index::Entry;
    ///
    /// let hashindex: HashIndex<u64, u32> = HashIndex::default();
    ///
    /// if let Entry::Vacant(o) = hashindex.entry(19) {
    ///     o.insert_entry(29);
    /// }
    ///
    /// assert_eq!(hashindex.peek_with(&19, |_, v| *v), Some(29));
    /// ```
    #[inline]
    pub fn insert_entry(mut self, val: V) -> OccupiedEntry<'h, K, V, H> {
        let guard = Guard::new();
        let entry_ptr = self.locked_entry.locker.insert_with(
            self.locked_entry.data_block_mut,
            BucketArray::<K, V, (), OPTIMISTIC>::partial_hash(self.hash),
            || (self.key, val),
            self.hashindex.prolonged_guard_ref(&guard),
        );
        OccupiedEntry {
            hashindex: self.hashindex,
            locked_entry: LockedEntry {
                index: self.locked_entry.index,
                data_block_mut: self.locked_entry.data_block_mut,
                locker: self.locked_entry.locker,
                entry_ptr,
            },
        }
    }
}

impl<K, V, H> Debug for VacantEntry<'_, K, V, H>
where
    K: 'static + Clone + Debug + Eq + Hash,
    V: 'static + Clone + Debug,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("VacantEntry").field(self.key()).finish()
    }
}

impl<K, V, H> Reserve<'_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    /// Returns the number of reserved slots.
    #[inline]
    #[must_use]
    pub fn additional_capacity(&self) -> usize {
        self.additional
    }
}

impl<K, V, H> AsRef<HashIndex<K, V, H>> for Reserve<'_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    #[inline]
    fn as_ref(&self) -> &HashIndex<K, V, H> {
        self.hashindex
    }
}

impl<K, V, H> Debug for Reserve<'_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("Reserve").field(&self.additional).finish()
    }
}

impl<K, V, H> Deref for Reserve<'_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    type Target = HashIndex<K, V, H>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.hashindex
    }
}

impl<K, V, H> Drop for Reserve<'_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    #[inline]
    fn drop(&mut self) {
        let result = self
            .hashindex
            .minimum_capacity
            .fetch_sub(self.additional, Relaxed);
        self.hashindex.try_resize(0, &Guard::new());
        debug_assert!(result >= self.additional);
    }
}

impl<K, V, H> Debug for Iter<'_, '_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Iter")
            .field("current_index", &self.current_index)
            .field("current_entry_ptr", &self.current_entry_ptr)
            .finish()
    }
}

impl<'g, K, V, H> Iterator for Iter<'_, 'g, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
    type Item = (&'g K, &'g V);

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        let mut array = if let Some(array) = self.current_array.as_ref().copied() {
            array
        } else {
            // Start scanning.
            let current_array = self
                .hashindex
                .bucket_array()
                .load(Acquire, self.guard)
                .as_ref()?;
            let old_array_ptr = current_array.old_array(self.guard);
            let array = if let Some(old_array) = old_array_ptr.as_ref() {
                old_array
            } else {
                current_array
            };
            self.current_array.replace(array);
            self.current_bucket.replace(array.bucket(0));
            self.current_entry_ptr = EntryPtr::new(self.guard);
            array
        };

        // Go to the next bucket.
        loop {
            if let Some(bucket) = self.current_bucket.take() {
                // Go to the next entry in the bucket.
                if self.current_entry_ptr.move_to_next(bucket, self.guard) {
                    let (k, v) = self
                        .current_entry_ptr
                        .get(array.data_block(self.current_index));
                    self.current_bucket.replace(bucket);
                    return Some((k, v));
                }
            }
            self.current_index += 1;
            if self.current_index == array.num_buckets() {
                let current_array = self
                    .hashindex
                    .bucket_array()
                    .load(Acquire, self.guard)
                    .as_ref()?;
                if self
                    .current_array
                    .as_ref()
                    .copied()
                    .map_or(false, |a| ptr::eq(a, current_array))
                {
                    // Finished scanning the entire array.
                    break;
                }
                let old_array_ptr = current_array.old_array(self.guard);
                if self
                    .current_array
                    .as_ref()
                    .copied()
                    .map_or(false, |a| ptr::eq(a, old_array_ptr.as_ptr()))
                {
                    // Start scanning the current array.
                    array = current_array;
                    self.current_array.replace(array);
                    self.current_index = 0;
                    self.current_bucket.replace(array.bucket(0));
                    self.current_entry_ptr = EntryPtr::new(self.guard);
                    continue;
                }

                // Start from the very beginning.
                array = if let Some(old_array) = old_array_ptr.as_ref() {
                    old_array
                } else {
                    current_array
                };
                self.current_array.replace(array);
                self.current_index = 0;
                self.current_bucket.replace(array.bucket(0));
                self.current_entry_ptr = EntryPtr::new(self.guard);
                continue;
            }
            self.current_bucket
                .replace(array.bucket(self.current_index));
            self.current_entry_ptr = EntryPtr::new(self.guard);
        }
        None
    }
}

impl<K, V, H> FusedIterator for Iter<'_, '_, K, V, H>
where
    K: 'static + Clone + Eq + Hash,
    V: 'static + Clone,
    H: BuildHasher,
{
}

impl<K, V, H> UnwindSafe for Iter<'_, '_, K, V, H>
where
    K: 'static + Clone + Eq + Hash + UnwindSafe,
    V: 'static + Clone + UnwindSafe,
    H: BuildHasher + UnwindSafe,
{
}
