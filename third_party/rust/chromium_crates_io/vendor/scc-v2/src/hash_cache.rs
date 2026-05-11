//! [`HashCache`] is a concurrent and asynchronous 32-way associative cache backed by
//! [`HashMap`](super::HashMap).

use super::ebr::{AtomicShared, Guard, Shared, Tag};
use super::hash_table::bucket::{DoublyLinkedList, EntryPtr, Locker, Reader, CACHE};
use super::hash_table::bucket_array::BucketArray;
use super::hash_table::{HashTable, LockedEntry};
use super::wait_queue::AsyncWait;
use super::Equivalent;
use std::collections::hash_map::RandomState;
use std::fmt::{self, Debug};
use std::hash::{BuildHasher, Hash};
use std::mem::replace;
use std::ops::{Deref, DerefMut, RangeInclusive};
use std::pin::Pin;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::{Acquire, Relaxed};

/// Scalable concurrent 32-way associative cache backed by [`HashMap`](super::HashMap).
///
/// [`HashCache`] is a concurrent 32-way associative cache that is based on the
/// [`HashMap`](super::HashMap) implementation. [`HashCache`] does not keep track of the least
/// recently used entry in the entire cache. Instead, each bucket maintains a doubly linked list of
/// occupied entries, updated on access to entries to keep track of the least recently used entries
/// within the bucket. Therefore, entries can be evicted before the cache is full.
///
/// [`HashCache`] and [`HashMap`](super::HashMap) share the same runtime characteristic, except that
/// each entry in a [`HashCache`] additionally uses 2-byte space for a doubly linked list and a
/// [`HashCache`] starts evicting least recently used entries if the bucket is full instead of
/// allocating linked list of entries.
///
/// ## Unwind safety
///
/// [`HashCache`] is impervious to out-of-memory errors and panics in user-specified code on one
/// condition; `H::Hasher::hash`, `K::drop` and `V::drop` must not panic.
pub struct HashCache<K, V, H = RandomState>
where
    H: BuildHasher,
{
    array: AtomicShared<BucketArray<K, V, DoublyLinkedList, CACHE>>,
    minimum_capacity: AtomicUsize,
    maximum_capacity: usize,
    build_hasher: H,
}

/// The default maximum capacity of a [`HashCache`] is `256`.
pub const DEFAULT_MAXIMUM_CAPACITY: usize = 256;

/// [`EvictedEntry`] is a type alias for `Option<(K, V)>`.
pub type EvictedEntry<K, V> = Option<(K, V)>;

/// [`Entry`] represents a single cache entry in a [`HashCache`].
pub enum Entry<'h, K, V, H = RandomState>
where
    H: BuildHasher,
{
    /// An occupied entry.
    Occupied(OccupiedEntry<'h, K, V, H>),

    /// A vacant entry.
    Vacant(VacantEntry<'h, K, V, H>),
}

/// [`OccupiedEntry`] is a view into an occupied cache entry in a [`HashCache`].
pub struct OccupiedEntry<'h, K, V, H = RandomState>
where
    H: BuildHasher,
{
    hashcache: &'h HashCache<K, V, H>,
    locked_entry: LockedEntry<'h, K, V, DoublyLinkedList, CACHE>,
}

/// [`VacantEntry`] is a view into a vacant cache entry in a [`HashCache`].
pub struct VacantEntry<'h, K, V, H = RandomState>
where
    H: BuildHasher,
{
    hashcache: &'h HashCache<K, V, H>,
    key: K,
    hash: u64,
    locked_entry: LockedEntry<'h, K, V, DoublyLinkedList, CACHE>,
}

impl<K, V, H> HashCache<K, V, H>
where
    H: BuildHasher,
{
    /// Creates an empty [`HashCache`] with the given [`BuildHasher`].
    ///
    /// The maximum capacity is set to [`DEFAULT_MAXIMUM_CAPACITY`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use std::collections::hash_map::RandomState;
    ///
    /// let hashcache: HashCache<u64, u32, RandomState> = HashCache::with_hasher(RandomState::new());
    /// ```
    #[cfg(not(feature = "loom"))]
    #[inline]
    pub const fn with_hasher(build_hasher: H) -> Self {
        HashCache {
            array: AtomicShared::null(),
            minimum_capacity: AtomicUsize::new(0),
            maximum_capacity: DEFAULT_MAXIMUM_CAPACITY,
            build_hasher,
        }
    }

    /// Creates an empty [`HashCache`] with the given [`BuildHasher`].
    #[cfg(feature = "loom")]
    #[inline]
    pub fn with_hasher(build_hasher: H) -> Self {
        Self {
            array: AtomicShared::null(),
            minimum_capacity: AtomicUsize::new(0),
            maximum_capacity: DEFAULT_MAXIMUM_CAPACITY,
            build_hasher,
        }
    }

    /// Creates an empty [`HashCache`] with the specified capacity and [`BuildHasher`].
    ///
    /// The actual capacity is equal to or greater than the specified capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use std::collections::hash_map::RandomState;
    ///
    /// let hashcache: HashCache<u64, u32, RandomState> =
    ///     HashCache::with_capacity_and_hasher(1000, 2000, RandomState::new());
    ///
    /// let result = hashcache.capacity();
    /// assert_eq!(result, 1024);
    /// ```
    #[inline]
    pub fn with_capacity_and_hasher(
        minimum_capacity: usize,
        maximum_capacity: usize,
        build_hasher: H,
    ) -> Self {
        let (array, minimum_capacity) = if minimum_capacity == 0 {
            (AtomicShared::null(), AtomicUsize::new(0))
        } else {
            let array = unsafe {
                Shared::new_unchecked(BucketArray::<K, V, DoublyLinkedList, CACHE>::new(
                    minimum_capacity,
                    AtomicShared::null(),
                ))
            };
            let minimum_capacity = array.num_entries();
            (
                AtomicShared::from(array),
                AtomicUsize::new(minimum_capacity),
            )
        };
        let maximum_capacity = maximum_capacity
            .max(minimum_capacity.load(Relaxed))
            .max(BucketArray::<K, V, DoublyLinkedList, CACHE>::minimum_capacity())
            .min(1_usize << (usize::BITS - 1))
            .next_power_of_two();
        HashCache {
            array,
            minimum_capacity,
            maximum_capacity,
            build_hasher,
        }
    }
}

impl<K, V, H> HashCache<K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Gets the entry associated with the given key in the map for in-place manipulation.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<char, u32> = HashCache::default();
    ///
    /// for ch in "a short treatise on fungi".chars() {
    ///     hashcache.entry(ch).and_modify(|counter| *counter += 1).or_put(1);
    /// }
    ///
    /// assert_eq!(*hashcache.get(&'s').unwrap().get(), 2);
    /// assert_eq!(*hashcache.get(&'t').unwrap().get(), 3);
    /// assert!(hashcache.get(&'y').is_none());
    /// ```
    #[inline]
    pub fn entry(&self, key: K) -> Entry<'_, K, V, H> {
        let guard = Guard::new();
        let hash = self.hash(&key);
        let mut locked_entry = unsafe {
            self.reserve_entry(&key, hash, &mut (), self.prolonged_guard_ref(&guard))
                .ok()
                .unwrap_unchecked()
        };
        if locked_entry.entry_ptr.is_valid() {
            locked_entry.locker.update_lru_tail(&locked_entry.entry_ptr);
            Entry::Occupied(OccupiedEntry {
                hashcache: self,
                locked_entry,
            })
        } else {
            Entry::Vacant(VacantEntry {
                hashcache: self,
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
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<char, u32> = HashCache::default();
    ///
    /// let future_entry = hashcache.entry_async('b');
    /// ```
    #[inline]
    pub async fn entry_async(&self, key: K) -> Entry<'_, K, V, H> {
        let hash = self.hash(&key);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            {
                let guard = Guard::new();
                if let Ok(mut locked_entry) = self.reserve_entry(
                    &key,
                    hash,
                    &mut async_wait_pinned,
                    self.prolonged_guard_ref(&guard),
                ) {
                    if locked_entry.entry_ptr.is_valid() {
                        locked_entry.locker.update_lru_tail(&locked_entry.entry_ptr);
                        return Entry::Occupied(OccupiedEntry {
                            hashcache: self,
                            locked_entry,
                        });
                    }
                    return Entry::Vacant(VacantEntry {
                        hashcache: self,
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
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<usize, usize> = HashCache::default();
    ///
    /// assert!(hashcache.put(0, 1).is_ok());
    /// assert!(hashcache.try_entry(0).is_some());
    /// ```
    #[inline]
    pub fn try_entry(&self, key: K) -> Option<Entry<'_, K, V, H>> {
        let guard = Guard::new();
        let hash = self.hash(&key);
        let locked_entry = self.try_reserve_entry(&key, hash, self.prolonged_guard_ref(&guard))?;
        if locked_entry.entry_ptr.is_valid() {
            Some(Entry::Occupied(OccupiedEntry {
                hashcache: self,
                locked_entry,
            }))
        } else {
            Some(Entry::Vacant(VacantEntry {
                hashcache: self,
                key,
                hash,
                locked_entry,
            }))
        }
    }

    /// Puts a key-value pair into the [`HashCache`].
    ///
    /// Returns `Some` if an entry was evicted for the new key-value pair.
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key-value pair if the key exists.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert_eq!(hashcache.put(1, 1).unwrap_err(), (1, 1));
    /// ```
    #[inline]
    pub fn put(&self, key: K, val: V) -> Result<EvictedEntry<K, V>, (K, V)> {
        let guard = Guard::new();
        let hash = self.hash(&key);
        let result = match self.reserve_entry(&key, hash, &mut (), &guard) {
            Ok(LockedEntry {
                mut locker,
                data_block_mut,
                entry_ptr,
                index: _,
            }) => {
                if entry_ptr.is_valid() {
                    return Err((key, val));
                }
                let evicted = locker.evict_lru_head(data_block_mut);
                let entry_ptr = locker.insert_with(
                    data_block_mut,
                    BucketArray::<K, V, DoublyLinkedList, CACHE>::partial_hash(hash),
                    || (key, val),
                    &guard,
                );
                locker.update_lru_tail(&entry_ptr);
                Ok(evicted)
            }
            Err(()) => Err((key, val)),
        };
        result
    }

    /// Puts a key-value pair into the [`HashCache`].
    ///
    /// Returns `Some` if an entry was evicted for the new key-value pair. It is an asynchronous
    /// method returning an `impl Future` for the caller to await.
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key-value pair if the key exists.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// let future_put = hashcache.put_async(11, 17);
    /// ```
    #[inline]
    pub async fn put_async(&self, key: K, val: V) -> Result<EvictedEntry<K, V>, (K, V)> {
        let hash = self.hash(&key);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            {
                let guard = Guard::new();
                if let Ok(LockedEntry {
                    mut locker,
                    data_block_mut,
                    entry_ptr,
                    index: _,
                }) = self.reserve_entry(&key, hash, &mut async_wait_pinned, &guard)
                {
                    if entry_ptr.is_valid() {
                        return Err((key, val));
                    }
                    let evicted = locker.evict_lru_head(data_block_mut);
                    let entry_ptr = locker.insert_with(
                        data_block_mut,
                        BucketArray::<K, V, DoublyLinkedList, CACHE>::partial_hash(hash),
                        || (key, val),
                        &guard,
                    );
                    locker.update_lru_tail(&entry_ptr);
                    return Ok(evicted);
                };
            }
            async_wait_pinned.await;
        }
    }

    /// Gets an [`OccupiedEntry`] corresponding to the key for in-place modification.
    ///
    /// [`OccupiedEntry`] exclusively owns the entry, preventing others from gaining access to it:
    /// use [`read`](Self::read) if read-only access is sufficient.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.get(&1).is_none());
    /// assert!(hashcache.put(1, 10).is_ok());
    /// assert_eq!(*hashcache.get(&1).unwrap().get(), 10);
    ///
    /// *hashcache.get(&1).unwrap() = 11;
    /// assert_eq!(*hashcache.get(&1).unwrap(), 11);
    /// ```
    #[inline]
    pub fn get<Q>(&self, key: &Q) -> Option<OccupiedEntry<'_, K, V, H>>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let guard = Guard::new();
        let mut locked_entry = self
            .get_entry(
                key,
                self.hash(key),
                &mut (),
                self.prolonged_guard_ref(&guard),
            )
            .ok()
            .flatten()?;
        locked_entry.locker.update_lru_tail(&locked_entry.entry_ptr);
        Some(OccupiedEntry {
            hashcache: self,
            locked_entry,
        })
    }

    /// Gets an [`OccupiedEntry`] corresponding to the key for in-place modification.
    ///
    /// [`OccupiedEntry`] exclusively owns the entry, preventing others from gaining access to it:
    /// use [`read_async`](Self::read_async) if read-only access is sufficient.
    ///
    /// Returns `None` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// let future_put = hashcache.put_async(11, 17);
    /// let future_get = hashcache.get_async(&11);
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
                if let Some(mut locked_entry) = result {
                    locked_entry.locker.update_lru_tail(&locked_entry.entry_ptr);
                    return Some(OccupiedEntry {
                        hashcache: self,
                        locked_entry,
                    });
                }
                return None;
            }
            async_wait_pinned.await;
        }
    }

    /// Reads a key-value pair.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.read(&1, |_, v| *v).is_none());
    /// assert!(hashcache.put(1, 10).is_ok());
    /// assert_eq!(hashcache.read(&1, |_, v| *v).unwrap(), 10);
    /// ```
    #[inline]
    pub fn read<Q, R, F: FnOnce(&K, &V) -> R>(&self, key: &Q, reader: F) -> Option<R>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.read_entry(key, self.hash(key), reader, &mut (), &Guard::new())
            .ok()
            .flatten()
    }

    /// Reads a key-value pair.
    ///
    /// Returns `None` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// let future_put = hashcache.put_async(11, 17);
    /// let future_read = hashcache.read_async(&11, |_, v| *v);
    /// ```
    #[inline]
    pub async fn read_async<Q, R, F: FnOnce(&K, &V) -> R>(
        &self,
        key: &Q,
        mut reader: F,
    ) -> Option<R>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let hash = self.hash(key);
        loop {
            let mut async_wait = AsyncWait::default();
            let mut async_wait_pinned = Pin::new(&mut async_wait);
            match self.read_entry(key, hash, reader, &mut async_wait_pinned, &Guard::new()) {
                Ok(result) => return result,
                Err(f) => reader = f,
            }
            async_wait_pinned.await;
        }
    }

    /// Returns `true` if the [`HashCache`] contains a value for the specified key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(!hashcache.contains(&1));
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert!(hashcache.contains(&1));
    /// ```
    #[inline]
    pub fn contains<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.read(key, |_, _| ()).is_some()
    }

    /// Returns `true` if the [`HashCache`] contains a value for the specified key.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// let future_contains = hashcache.contains_async(&1);
    /// ```
    #[inline]
    pub async fn contains_async<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.read_async(key, |_, _| ()).await.is_some()
    }

    /// Removes a key-value pair if the key exists.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.remove(&1).is_none());
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert_eq!(hashcache.remove(&1).unwrap(), (1, 0));
    /// ```
    #[inline]
    pub fn remove<Q>(&self, key: &Q) -> Option<(K, V)>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.remove_if(key, |_| true)
    }

    /// Removes a key-value pair if the key exists.
    ///
    /// Returns `None` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// let future_put = hashcache.put_async(11, 17);
    /// let future_remove = hashcache.remove_async(&11);
    /// ```
    #[inline]
    pub async fn remove_async<Q>(&self, key: &Q) -> Option<(K, V)>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.remove_if_async(key, |_| true).await
    }

    /// Removes a key-value pair if the key exists and the given condition is met.
    ///
    /// Returns `None` if the key does not exist or the condition was not met.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert!(hashcache.remove_if(&1, |v| { *v += 1; false }).is_none());
    /// assert_eq!(hashcache.remove_if(&1, |v| *v == 1).unwrap(), (1, 1));
    /// ```
    #[inline]
    pub fn remove_if<Q, F: FnOnce(&mut V) -> bool>(&self, key: &Q, condition: F) -> Option<(K, V)>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.get(key).and_then(|mut o| {
            if condition(o.get_mut()) {
                Some(o.remove_entry())
            } else {
                None
            }
        })
    }

    /// Removes a key-value pair if the key exists and the given condition is met.
    ///
    /// Returns `None` if the key does not exist or the condition was not met. It is an
    /// asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// let future_put = hashcache.put_async(11, 17);
    /// let future_remove = hashcache.remove_if_async(&11, |_| true);
    /// ```
    #[inline]
    pub async fn remove_if_async<Q, F: FnOnce(&mut V) -> bool>(
        &self,
        key: &Q,
        condition: F,
    ) -> Option<(K, V)>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        if let Some(mut occupied_entry) = self.get_async(key).await {
            if condition(occupied_entry.get_mut()) {
                return Some(occupied_entry.remove_entry());
            }
        }
        None
    }

    /// Scans all the entries.
    ///
    /// This method does not affect the LRU information in each bucket.
    ///
    /// Key-value pairs that have existed since the invocation of the method are guaranteed to be
    /// visited if they are not removed, however the same key-value pair can be visited more than
    /// once if the [`HashCache`] gets resized by another thread.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<usize, usize> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert!(hashcache.put(2, 1).is_ok());
    ///
    /// let mut sum = 0;
    /// hashcache.scan(|k, v| { sum += *k + *v; });
    /// assert_eq!(sum, 4);
    /// ```
    #[inline]
    pub fn scan<F: FnMut(&K, &V)>(&self, mut scanner: F) {
        let guard = Guard::new();
        let mut current_array_ptr = self.array.load(Acquire, &guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            self.clear_old_array(current_array, &guard);
            for index in 0..current_array.num_buckets() {
                let bucket = current_array.bucket(index);
                if let Some(locker) = Reader::lock(bucket, &guard) {
                    let data_block = current_array.data_block(index);
                    let mut entry_ptr = EntryPtr::new(&guard);
                    while entry_ptr.move_to_next(*locker, &guard) {
                        let (k, v) = entry_ptr.get(data_block);
                        scanner(k, v);
                    }
                }
            }
            let new_current_array_ptr = self.array.load(Acquire, &guard);
            if current_array_ptr.without_tag() == new_current_array_ptr.without_tag() {
                break;
            }
            current_array_ptr = new_current_array_ptr;
        }
    }

    /// Scans all the entries.
    ///
    /// This method does not affect the LRU information in each bucket.
    ///
    /// Key-value pairs that have existed since the invocation of the method are guaranteed to be
    /// visited if they are not removed, however the same key-value pair can be visited more than
    /// once if the [`HashCache`] gets resized by another task.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<usize, usize> = HashCache::default();
    ///
    /// let future_put = hashcache.put_async(1, 0);
    /// let future_scan = hashcache.scan_async(|k, v| println!("{k} {v}"));
    /// ```
    #[inline]
    pub async fn scan_async<F: FnMut(&K, &V)>(&self, mut scanner: F) {
        let mut current_array_holder = self.array.get_shared(Acquire, &Guard::new());
        while let Some(current_array) = current_array_holder.take() {
            self.cleanse_old_array_async(&current_array).await;
            for index in 0..current_array.num_buckets() {
                loop {
                    let mut async_wait = AsyncWait::default();
                    let mut async_wait_pinned = Pin::new(&mut async_wait);
                    {
                        let guard = Guard::new();
                        let bucket = current_array.bucket(index);
                        if let Ok(reader) =
                            Reader::try_lock_or_wait(bucket, &mut async_wait_pinned, &guard)
                        {
                            if let Some(reader) = reader {
                                let data_block = current_array.data_block(index);
                                let mut entry_ptr = EntryPtr::new(&guard);
                                while entry_ptr.move_to_next(*reader, &guard) {
                                    let (k, v) = entry_ptr.get(data_block);
                                    scanner(k, v);
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
    }

    /// Searches for any entry that satisfies the given predicate.
    ///
    /// Key-value pairs that have existed since the invocation of the method are guaranteed to be
    /// visited if they are not removed, however the same key-value pair can be visited more than
    /// once if the [`HashCache`] gets resized by another thread.
    ///
    /// Returns `true` as soon as an entry satisfying the predicate is found.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert!(hashcache.put(2, 1).is_ok());
    /// assert!(hashcache.put(3, 2).is_ok());
    ///
    /// assert!(hashcache.any(|k, v| *k == 1 && *v == 0));
    /// assert!(!hashcache.any(|k, v| *k == 2 && *v == 0));
    /// ```
    #[inline]
    pub fn any<P: FnMut(&K, &V) -> bool>(&self, mut pred: P) -> bool {
        self.contains_entry(|k, v| pred(k, v))
    }

    /// Searches for any entry that satisfies the given predicate.
    ///
    /// Key-value pairs that have existed since the invocation of the method are guaranteed to be
    /// visited if they are not removed, however the same key-value pair can be visited more than
    /// once if the [`HashCache`] gets resized by another task.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// Returns `true` as soon as an entry satisfying the predicate is found.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// let future_put = hashcache.put_async(1, 0);
    /// let future_any = hashcache.any_async(|k, _| *k == 1);
    /// ```
    #[inline]
    pub async fn any_async<P: FnMut(&K, &V) -> bool>(&self, mut pred: P) -> bool {
        let mut current_array_holder = self.array.get_shared(Acquire, &Guard::new());
        while let Some(current_array) = current_array_holder.take() {
            self.cleanse_old_array_async(&current_array).await;
            for index in 0..current_array.num_buckets() {
                loop {
                    let mut async_wait = AsyncWait::default();
                    let mut async_wait_pinned = Pin::new(&mut async_wait);
                    {
                        let guard = Guard::new();
                        let bucket = current_array.bucket(index);
                        if let Ok(reader) =
                            Reader::try_lock_or_wait(bucket, &mut async_wait_pinned, &guard)
                        {
                            if let Some(reader) = reader {
                                let data_block = current_array.data_block(index);
                                let mut entry_ptr = EntryPtr::new(&guard);
                                while entry_ptr.move_to_next(*reader, &guard) {
                                    let (k, v) = entry_ptr.get(data_block);
                                    if pred(k, v) {
                                        // Found one entry satisfying the predicate.
                                        return true;
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

        false
    }

    /// Retains the entries specified by the predicate.
    ///
    /// This method allows the predicate closure to modify the value field.
    ///
    /// Entries that have existed since the invocation of the method are guaranteed to be visited
    /// if they are not removed, however the same entry can be visited more than once if the
    /// [`HashCache`] gets resized by another thread.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert!(hashcache.put(2, 1).is_ok());
    /// assert!(hashcache.put(3, 2).is_ok());
    ///
    /// hashcache.retain(|k, v| *k == 1 && *v == 0);
    ///
    /// assert!(hashcache.contains(&1));
    /// assert!(!hashcache.contains(&2));
    /// assert!(!hashcache.contains(&3));
    /// ```
    #[inline]
    pub fn retain<F: FnMut(&K, &mut V) -> bool>(&self, mut pred: F) {
        #[allow(clippy::redundant_closure_for_method_calls)]
        self.retain_entries(|k, v| pred(k, v));
    }

    /// Retains the entries specified by the predicate.
    ///
    /// This method allows the predicate closure to modify the value field.
    ///
    /// Entries that have existed since the invocation of the method are guaranteed to be visited
    /// if they are not removed, however the same entry can be visited more than once if the
    /// [`HashCache`] gets resized by another thread.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// let future_put = hashcache.put_async(1, 0);
    /// let future_retain = hashcache.retain_async(|k, v| *k == 1);
    /// ```
    #[inline]
    pub async fn retain_async<F: FnMut(&K, &mut V) -> bool>(&self, mut filter: F) {
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
                                    let (k, v) = entry_ptr.get_mut(data_block_mut, &mut locker);
                                    if !filter(k, v) {
                                        locker.remove(data_block_mut, &mut entry_ptr, &guard);
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

    /// Clears the [`HashCache`] by removing all key-value pairs.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// hashcache.clear();
    ///
    /// assert!(!hashcache.contains(&1));
    /// ```
    #[inline]
    pub fn clear(&self) {
        self.retain(|_, _| false);
    }

    /// Clears the [`HashCache`] by removing all key-value pairs.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// let future_put = hashcache.put_async(1, 0);
    /// let future_clear = hashcache.clear_async();
    /// ```
    #[inline]
    pub async fn clear_async(&self) {
        self.retain_async(|_, _| false).await;
    }

    /// Returns the number of entries in the [`HashCache`].
    ///
    /// It reads the entire metadata area of the bucket array to calculate the number of valid
    /// entries, making its time complexity `O(N)`. Furthermore, it may overcount entries if an old
    /// bucket array has yet to be dropped.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert_eq!(hashcache.len(), 1);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        self.num_entries(&Guard::new())
    }

    /// Returns `true` if the [`HashCache`] is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert!(hashcache.is_empty());
    /// assert!(hashcache.put(1, 0).is_ok());
    /// assert!(!hashcache.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        !self.has_entry(&Guard::new())
    }

    /// Returns the capacity of the [`HashCache`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache_default: HashCache<u64, u32> = HashCache::default();
    /// assert_eq!(hashcache_default.capacity(), 0);
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::with_capacity(1000, 2000);
    /// assert_eq!(hashcache.capacity(), 1024);
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize {
        self.num_slots(&Guard::new())
    }

    /// Returns the current capacity range of the [`HashCache`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert_eq!(hashcache.capacity_range(), 0..=256);
    /// ```
    #[inline]
    pub fn capacity_range(&self) -> RangeInclusive<usize> {
        self.minimum_capacity.load(Relaxed)..=self.maximum_capacity()
    }

    /// Clears the old array asynchronously.
    async fn cleanse_old_array_async(
        &self,
        current_array: &BucketArray<K, V, DoublyLinkedList, CACHE>,
    ) {
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

impl<K, V> HashCache<K, V, RandomState>
where
    K: Eq + Hash,
{
    /// Creates an empty default [`HashCache`].
    ///
    /// The maximum capacity is set to [`DEFAULT_MAXIMUM_CAPACITY`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::new();
    ///
    /// let result = hashcache.capacity();
    /// assert_eq!(result, 0);
    /// ```
    #[inline]
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Creates an empty [`HashCache`] with the specified capacity.
    ///
    /// The supplied minimum and maximum capacity values are adjusted to power-of-two values equal
    /// to or larger than the provided values and `64` with one exception; if `0` is specified as
    /// the minimum capacity, the minimum capacity of the `HashCache` becomes `0`.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::with_capacity(1000, 2000);
    ///
    /// let result = hashcache.capacity();
    /// assert_eq!(result, 1024);
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::with_capacity(0, 0);
    /// let result = hashcache.capacity_range();
    /// assert_eq!(result, 0..=64);
    /// ```
    #[inline]
    #[must_use]
    pub fn with_capacity(minimum_capacity: usize, maximum_capacity: usize) -> Self {
        Self::with_capacity_and_hasher(minimum_capacity, maximum_capacity, RandomState::new())
    }
}

impl<K, V, H> Default for HashCache<K, V, H>
where
    H: BuildHasher + Default,
{
    /// Creates an empty default [`HashCache`].
    ///
    /// The maximum capacity is set to [`DEFAULT_MAXIMUM_CAPACITY`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// let result = hashcache.capacity();
    /// assert_eq!(result, 0);
    /// ```
    #[inline]
    fn default() -> Self {
        Self::with_hasher(H::default())
    }
}

impl<K, V, H> Debug for HashCache<K, V, H>
where
    K: Debug + Eq + Hash,
    V: Debug,
    H: BuildHasher,
{
    /// Iterates over all the entries in the [`HashCache`] to print them.
    ///
    /// ## Locking behavior
    ///
    /// Shared locks on buckets are acquired during iteration, therefore any [`Entry`],
    /// [`OccupiedEntry`] or [`VacantEntry`] owned by the current thread will lead to a deadlock.
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut d = f.debug_map();
        self.scan(|k, v| {
            d.entry(k, v);
        });
        d.finish()
    }
}

impl<K, V, H> Drop for HashCache<K, V, H>
where
    H: BuildHasher,
{
    #[inline]
    fn drop(&mut self) {
        self.array
            .swap((None, Tag::None), Relaxed)
            .0
            .map(|a| unsafe {
                // The entire array does not need to wait for an epoch change as no references will
                // remain outside the lifetime of the `HashCache`.
                a.drop_in_place()
            });
    }
}

impl<K, V, H> FromIterator<(K, V)> for HashCache<K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher + Default,
{
    #[inline]
    fn from_iter<T: IntoIterator<Item = (K, V)>>(iter: T) -> Self {
        let into_iter = iter.into_iter();
        let size_hint = into_iter.size_hint();
        let hashcache = Self::with_capacity_and_hasher(
            size_hint.0,
            Self::capacity_from_size_hint(size_hint),
            H::default(),
        );
        into_iter.for_each(|e| {
            let _result = hashcache.put(e.0, e.1);
        });
        hashcache
    }
}

impl<K, V, H> HashTable<K, V, H, DoublyLinkedList, CACHE> for HashCache<K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    #[inline]
    fn hasher(&self) -> &H {
        &self.build_hasher
    }
    #[inline]
    fn try_clone(_entry: &(K, V)) -> Option<(K, V)> {
        None
    }
    #[inline]
    fn bucket_array(&self) -> &AtomicShared<BucketArray<K, V, DoublyLinkedList, CACHE>> {
        &self.array
    }
    #[inline]
    fn minimum_capacity(&self) -> &AtomicUsize {
        &self.minimum_capacity
    }
    #[inline]
    fn maximum_capacity(&self) -> usize {
        self.maximum_capacity
    }
}

impl<K, V, H> PartialEq for HashCache<K, V, H>
where
    K: Eq + Hash,
    V: PartialEq,
    H: BuildHasher,
{
    /// Compares two [`HashCache`] instances.
    ///
    /// ## Locking behavior
    ///
    /// Shared locks on buckets are acquired when comparing two instances of [`HashCache`], therefore
    /// it may lead to a deadlock if the instances are being modified by another thread.
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        if !self.any(|k, v| other.read(k, |_, ov| v == ov) != Some(true)) {
            return !other.any(|k, v| self.read(k, |_, sv| v == sv) != Some(true));
        }
        false
    }
}

impl<'h, K, V, H> Entry<'h, K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Ensures a value is in the entry by putting the supplied instance if empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(3).or_put(7);
    /// assert_eq!(*hashcache.get(&3).unwrap().get(), 7);
    /// ```
    #[inline]
    pub fn or_put(self, val: V) -> (EvictedEntry<K, V>, OccupiedEntry<'h, K, V, H>) {
        self.or_put_with(|| val)
    }

    /// Ensures a value is in the entry by putting the result of the supplied closure if empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(19).or_put_with(|| 5);
    /// assert_eq!(*hashcache.get(&19).unwrap().get(), 5);
    /// ```
    #[inline]
    pub fn or_put_with<F: FnOnce() -> V>(
        self,
        constructor: F,
    ) -> (EvictedEntry<K, V>, OccupiedEntry<'h, K, V, H>) {
        self.or_put_with_key(|_| constructor())
    }

    /// Ensures a value is in the entry by putting the result of the supplied closure if empty.
    ///
    /// The reference to the moved key is provided, therefore cloning or copying the key is
    /// unnecessary.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(11).or_put_with_key(|k| if *k == 11 { 7 } else { 3 });
    /// assert_eq!(*hashcache.get(&11).unwrap().get(), 7);
    /// ```
    #[inline]
    pub fn or_put_with_key<F: FnOnce(&K) -> V>(
        self,
        constructor: F,
    ) -> (EvictedEntry<K, V>, OccupiedEntry<'h, K, V, H>) {
        match self {
            Self::Occupied(o) => (None, o),
            Self::Vacant(v) => {
                let val = constructor(v.key());
                v.put_entry(val)
            }
        }
    }

    /// Returns a reference to the key of this entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// assert_eq!(hashcache.entry(31).key(), &31);
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
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(37).and_modify(|v| { *v += 1 }).or_put(47);
    /// assert_eq!(*hashcache.get(&37).unwrap().get(), 47);
    ///
    /// hashcache.entry(37).and_modify(|v| { *v += 1 }).or_put(3);
    /// assert_eq!(*hashcache.get(&37).unwrap().get(), 48);
    /// ```
    #[inline]
    #[must_use]
    pub fn and_modify<F>(self, f: F) -> Self
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

    /// Sets the value of the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// let entry = hashcache.entry(11).put_entry(17).1;
    /// assert_eq!(entry.key(), &11);
    /// ```
    #[inline]
    pub fn put_entry(self, val: V) -> (EvictedEntry<K, V>, OccupiedEntry<'h, K, V, H>) {
        match self {
            Self::Occupied(mut o) => {
                o.put(val);
                (None, o)
            }
            Self::Vacant(v) => v.put_entry(val),
        }
    }
}

impl<'h, K, V, H> Entry<'h, K, V, H>
where
    K: Eq + Hash,
    V: Default,
    H: BuildHasher,
{
    /// Ensures a value is in the entry by putting the default value if empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// hashcache.entry(11).or_default();
    /// assert_eq!(*hashcache.get(&11).unwrap().get(), 0);
    /// ```
    #[inline]
    pub fn or_default(self) -> (EvictedEntry<K, V>, OccupiedEntry<'h, K, V, H>) {
        match self {
            Self::Occupied(o) => (None, o),
            Self::Vacant(v) => v.put_entry(Default::default()),
        }
    }
}

impl<K, V, H> Debug for Entry<'_, K, V, H>
where
    K: Debug + Eq + Hash,
    V: Debug,
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

impl<K, V, H> OccupiedEntry<'_, K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Gets a reference to the key in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// assert_eq!(hashcache.entry(29).or_default().1.key(), &29);
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

    /// Takes ownership of the key and value from the [`HashCache`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(11).or_put(17);
    ///
    /// if let Entry::Occupied(o) = hashcache.entry(11) {
    ///     assert_eq!(o.remove_entry(), (11, 17));
    /// };
    /// ```
    #[inline]
    #[must_use]
    pub fn remove_entry(mut self) -> (K, V) {
        let guard = Guard::new();
        let (k, v) = self.locked_entry.locker.remove(
            self.locked_entry.data_block_mut,
            &mut self.locked_entry.entry_ptr,
            self.hashcache.prolonged_guard_ref(&guard),
        );
        if self.locked_entry.locker.num_entries() <= 1 || self.locked_entry.locker.need_rebuild() {
            let hashcache = self.hashcache;
            if let Some(current_array) = hashcache.bucket_array().load(Acquire, &guard).as_ref() {
                if !current_array.has_old_array() {
                    let index = self.locked_entry.index;
                    if current_array.initiate_sampling(index) {
                        drop(self);
                        hashcache.try_shrink_or_rebuild(current_array, index, &guard);
                    }
                }
            }
        }
        (k, v)
    }

    /// Gets a reference to the value in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(19).or_put(11);
    ///
    /// if let Entry::Occupied(o) = hashcache.entry(19) {
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
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(37).or_put(11);
    ///
    /// if let Entry::Occupied(mut o) = hashcache.entry(37) {
    ///     *o.get_mut() += 18;
    ///     assert_eq!(*o.get(), 29);
    /// }
    ///
    /// assert_eq!(*hashcache.get(&37).unwrap().get(), 29);
    /// ```
    #[inline]
    pub fn get_mut(&mut self) -> &mut V {
        &mut self
            .locked_entry
            .entry_ptr
            .get_mut(
                self.locked_entry.data_block_mut,
                &mut self.locked_entry.locker,
            )
            .1
    }

    /// Sets the value of the entry, and returns the old value.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(37).or_put(11);
    ///
    /// if let Entry::Occupied(mut o) = hashcache.entry(37) {
    ///     assert_eq!(o.put(17), 11);
    /// }
    ///
    /// assert_eq!(*hashcache.get(&37).unwrap().get(), 17);
    /// ```
    #[inline]
    pub fn put(&mut self, val: V) -> V {
        replace(self.get_mut(), val)
    }

    /// Takes the value out of the entry, and returns it.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// hashcache.entry(11).or_put(17);
    ///
    /// if let Entry::Occupied(o) = hashcache.entry(11) {
    ///     assert_eq!(o.remove(), 17);
    /// };
    /// ```
    #[inline]
    #[must_use]
    pub fn remove(self) -> V {
        self.remove_entry().1
    }
}

impl<K, V, H> Debug for OccupiedEntry<'_, K, V, H>
where
    K: Debug + Eq + Hash,
    V: Debug,
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
    K: Eq + Hash,
    H: BuildHasher,
{
    type Target = V;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

impl<K, V, H> DerefMut for OccupiedEntry<'_, K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.get_mut()
    }
}

impl<'h, K, V, H> VacantEntry<'h, K, V, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Gets a reference to the key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    /// assert_eq!(hashcache.entry(11).key(), &11);
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
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// if let Entry::Vacant(v) = hashcache.entry(17) {
    ///     assert_eq!(v.into_key(), 17);
    /// };
    /// ```
    #[inline]
    pub fn into_key(self) -> K {
        self.key
    }

    /// Sets the value of the entry with its key, and returns an [`OccupiedEntry`].
    ///
    /// Returns a key-value pair if an entry was evicted for the new key-value pair.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashCache;
    /// use scc::hash_cache::Entry;
    ///
    /// let hashcache: HashCache<u64, u32> = HashCache::default();
    ///
    /// if let Entry::Vacant(o) = hashcache.entry(19) {
    ///     o.put_entry(29);
    /// }
    ///
    /// assert_eq!(*hashcache.get(&19).unwrap().get(), 29);
    /// ```
    #[inline]
    pub fn put_entry(mut self, val: V) -> (EvictedEntry<K, V>, OccupiedEntry<'h, K, V, H>) {
        let evicted = self
            .locked_entry
            .locker
            .evict_lru_head(self.locked_entry.data_block_mut);
        let entry_ptr = self.locked_entry.locker.insert_with(
            self.locked_entry.data_block_mut,
            BucketArray::<K, V, DoublyLinkedList, CACHE>::partial_hash(self.hash),
            || (self.key, val),
            self.hashcache.prolonged_guard_ref(&Guard::new()),
        );
        self.locked_entry.locker.update_lru_tail(&entry_ptr);
        let occupied = OccupiedEntry {
            hashcache: self.hashcache,
            locked_entry: LockedEntry {
                index: self.locked_entry.index,
                data_block_mut: self.locked_entry.data_block_mut,
                locker: self.locked_entry.locker,
                entry_ptr,
            },
        };

        (evicted, occupied)
    }
}

impl<K, V, H> Debug for VacantEntry<'_, K, V, H>
where
    K: Debug + Eq + Hash,
    V: Debug,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("VacantEntry").field(self.key()).finish()
    }
}
