//! [`HashSet`] is a concurrent and asynchronous hash set.

use super::hash_table::HashTable;
use super::{Equivalent, HashMap};
use std::collections::hash_map::RandomState;
use std::fmt::{self, Debug};
use std::hash::{BuildHasher, Hash};
use std::ops::RangeInclusive;

/// Scalable concurrent hash set.
///
/// [`HashSet`] is a concurrent and asynchronous hash set based on [`HashMap`].
pub struct HashSet<K, H = RandomState>
where
    H: BuildHasher,
{
    map: HashMap<K, (), H>,
}

/// [`Reserve`] keeps the capacity of the associated [`HashSet`] higher than a certain level.
///
/// The [`HashSet`] does not shrink the capacity below the reserved capacity.
pub type Reserve<'h, K, H = RandomState> = super::hash_map::Reserve<'h, K, (), H>;

impl<K, H> HashSet<K, H>
where
    H: BuildHasher,
{
    /// Creates an empty [`HashSet`] with the given [`BuildHasher`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    /// use std::collections::hash_map::RandomState;
    ///
    /// let hashset: HashSet<u64, RandomState> = HashSet::with_hasher(RandomState::new());
    /// ```
    #[cfg(not(feature = "loom"))]
    #[inline]
    pub const fn with_hasher(build_hasher: H) -> Self {
        Self {
            map: HashMap::with_hasher(build_hasher),
        }
    }

    /// Creates an empty [`HashSet`] with the given [`BuildHasher`].
    #[cfg(feature = "loom")]
    #[inline]
    pub fn with_hasher(build_hasher: H) -> Self {
        Self {
            map: HashMap::with_hasher(build_hasher),
        }
    }

    /// Creates an empty [`HashSet`] with the specified capacity and [`BuildHasher`].
    ///
    /// The actual capacity is equal to or greater than the specified capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    /// use std::collections::hash_map::RandomState;
    ///
    /// let hashset: HashSet<u64, RandomState> =
    ///     HashSet::with_capacity_and_hasher(1000, RandomState::new());
    ///
    /// let result = hashset.capacity();
    /// assert_eq!(result, 1024);
    /// ```
    #[inline]
    pub fn with_capacity_and_hasher(capacity: usize, build_hasher: H) -> Self {
        Self {
            map: HashMap::with_capacity_and_hasher(capacity, build_hasher),
        }
    }
}

impl<K, H> HashSet<K, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Temporarily increases the minimum capacity of the [`HashSet`].
    ///
    /// A [`Reserve`] is returned if the [`HashSet`] could increase the minimum capacity while the
    /// increased capacity is not exclusively owned by the returned [`Reserve`], allowing others to
    /// benefit from it. The memory for the additional space may not be immediately allocated if
    /// the [`HashSet`] is empty or currently being resized, however once the memory is reserved
    /// eventually, the capacity will not shrink below the additional capacity until the returned
    /// [`Reserve`] is dropped.
    ///
    /// # Errors
    ///
    /// Returns `None` if a too large number is given.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<usize> = HashSet::with_capacity(1000);
    /// assert_eq!(hashset.capacity(), 1024);
    ///
    /// let reserved = hashset.reserve(10000);
    /// assert!(reserved.is_some());
    /// assert_eq!(hashset.capacity(), 16384);
    ///
    /// assert!(hashset.reserve(usize::MAX).is_none());
    /// assert_eq!(hashset.capacity(), 16384);
    ///
    /// for i in 0..16 {
    ///     assert!(hashset.insert(i).is_ok());
    /// }
    /// drop(reserved);
    ///
    /// assert_eq!(hashset.capacity(), 1024);
    /// ```
    #[inline]
    pub fn reserve(&self, capacity: usize) -> Option<Reserve<'_, K, H>> {
        self.map.reserve(capacity)
    }

    /// Inserts a key into the [`HashSet`].
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key if the key exists.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// assert_eq!(hashset.insert(1).unwrap_err(), 1);
    /// ```
    #[inline]
    pub fn insert(&self, key: K) -> Result<(), K> {
        if let Err((k, ())) = self.map.insert(key, ()) {
            return Err(k);
        }
        Ok(())
    }

    /// Inserts a key into the [`HashSet`].
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Errors
    ///
    /// Returns an error along with the supplied key if the key exists.
    ///
    /// function.
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    /// let future_insert = hashset.insert_async(11);
    /// ```
    #[inline]
    pub async fn insert_async(&self, key: K) -> Result<(), K> {
        self.map.insert_async(key, ()).await.map_err(|(k, ())| k)
    }

    /// Removes a key if the key exists.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.remove(&1).is_none());
    /// assert!(hashset.insert(1).is_ok());
    /// assert_eq!(hashset.remove(&1).unwrap(), 1);
    /// ```
    #[inline]
    pub fn remove<Q>(&self, key: &Q) -> Option<K>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map.remove(key).map(|(k, ())| k)
    }

    /// Removes a key if the key exists.
    ///
    /// Returns `None` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    /// let future_insert = hashset.insert_async(11);
    /// let future_remove = hashset.remove_async(&11);
    /// ```
    #[inline]
    pub async fn remove_async<Q>(&self, key: &Q) -> Option<K>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map
            .remove_if_async(key, |()| true)
            .await
            .map(|(k, ())| k)
    }

    /// Removes a key if the key exists and the given condition is met.
    ///
    /// Returns `None` if the key does not exist or the condition was not met.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(hashset.remove_if(&1, || false).is_none());
    /// assert_eq!(hashset.remove_if(&1, || true).unwrap(), 1);
    /// ```
    #[inline]
    pub fn remove_if<Q, F: FnOnce() -> bool>(&self, key: &Q, condition: F) -> Option<K>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map.remove_if(key, |()| condition()).map(|(k, ())| k)
    }

    /// Removes a key if the key exists and the given condition is met.
    ///
    /// Returns `None` if the key does not exist or the condition was not met. It is an
    /// asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    /// let future_insert = hashset.insert_async(11);
    /// let future_remove = hashset.remove_if_async(&11, || true);
    /// ```
    #[inline]
    pub async fn remove_if_async<Q, F: FnOnce() -> bool>(&self, key: &Q, condition: F) -> Option<K>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map
            .remove_if_async(key, |()| condition())
            .await
            .map(|(k, ())| k)
    }

    /// Reads a key.
    ///
    /// Returns `None` if the key does not exist.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.read(&1, |_| true).is_none());
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(hashset.read(&1, |_| true).unwrap());
    /// ```
    #[inline]
    pub fn read<Q, R, F: FnOnce(&K) -> R>(&self, key: &Q, reader: F) -> Option<R>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map.read(key, |k, ()| reader(k))
    }

    /// Reads a key.
    ///
    /// Returns `None` if the key does not exist. It is an asynchronous method returning an
    /// `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    /// let future_insert = hashset.insert_async(11);
    /// let future_read = hashset.read_async(&11, |k| *k);
    /// ```
    #[inline]
    pub async fn read_async<Q, R, F: FnOnce(&K) -> R>(&self, key: &Q, reader: F) -> Option<R>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map.read_async(key, |k, ()| reader(k)).await
    }

    /// Returns `true` if the [`HashSet`] contains the specified key.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(!hashset.contains(&1));
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(hashset.contains(&1));
    /// ```
    #[inline]
    pub fn contains<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.read(key, |_| ()).is_some()
    }

    /// Returns `true` if the [`HashSet`] contains the specified key.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// let future_contains = hashset.contains_async(&1);
    /// ```
    #[inline]
    pub async fn contains_async<Q>(&self, key: &Q) -> bool
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map.contains_async(key).await
    }

    /// Scans all the keys.
    ///
    /// Keys that have existed since the invocation of the method are guaranteed to be visited if
    /// they are not removed, however the same key can be visited more than once if the [`HashSet`]
    /// gets resized by another thread.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<usize> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(hashset.insert(2).is_ok());
    ///
    /// let mut sum = 0;
    /// hashset.scan(|k| { sum += *k; });
    /// assert_eq!(sum, 3);
    /// ```
    #[inline]
    pub fn scan<F: FnMut(&K)>(&self, mut scanner: F) {
        self.map.scan(|k, ()| scanner(k));
    }

    /// Scans all the keys.
    ///
    /// Keys that have existed since the invocation of the method are guaranteed to be visited if
    /// they are not removed, however the same key can be visited more than once if the [`HashSet`]
    /// gets resized by another task.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<usize> = HashSet::default();
    ///
    /// let future_insert = hashset.insert_async(1);
    /// let future_scan = hashset.scan_async(|k| println!("{k}"));
    /// ```
    #[inline]
    pub async fn scan_async<F: FnMut(&K)>(&self, mut scanner: F) {
        self.map.scan_async(|k, ()| scanner(k)).await;
    }

    /// Searches for any key that satisfies the given predicate.
    ///
    /// Keys that have existed since the invocation of the method are guaranteed to be visited if
    /// they are not removed, however the same key can be visited more than once if the [`HashSet`]
    /// gets resized by another task.
    ///
    /// Returns `true` if a key satisfying the predicate is found.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(hashset.insert(2).is_ok());
    /// assert!(hashset.insert(3).is_ok());
    ///
    /// assert!(hashset.any(|k| *k == 1));
    /// assert!(!hashset.any(|k| *k == 4));
    /// ```
    #[inline]
    pub fn any<P: FnMut(&K) -> bool>(&self, mut pred: P) -> bool {
        self.map.any(|k, ()| pred(k))
    }

    /// Searches for any key that satisfies the given predicate.
    ///
    /// Keys that have existed since the invocation of the method are guaranteed to be visited if
    /// they are not removed, however the same key can be visited more than once if the [`HashSet`]
    /// gets resized by another task.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// Returns `true` if a key satisfying the predicate is found.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// let future_insert = hashset.insert(1);
    /// let future_any = hashset.any_async(|k| *k == 1);
    /// ```
    #[inline]
    pub async fn any_async<P: FnMut(&K) -> bool>(&self, mut pred: P) -> bool {
        self.map.any_async(|k, ()| pred(k)).await
    }

    /// Retains keys that satisfy the given predicate.
    ///
    /// Keys that have existed since the invocation of the method are guaranteed to be visited if
    /// they are not removed, however the same key can be visited more than once if the [`HashSet`]
    /// gets resized by another thread.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(hashset.insert(2).is_ok());
    /// assert!(hashset.insert(3).is_ok());
    ///
    /// hashset.retain(|k| *k == 1);
    ///
    /// assert!(hashset.contains(&1));
    /// assert!(!hashset.contains(&2));
    /// assert!(!hashset.contains(&3));
    /// ```
    #[inline]
    pub fn retain<F: FnMut(&K) -> bool>(&self, mut filter: F) {
        self.map.retain(|k, ()| filter(k));
    }

    /// Retains keys that satisfy the given predicate.
    ///
    /// Keys that have existed since the invocation of the method are guaranteed to be visited if
    /// they are not removed, however the same key can be visited more than once if the [`HashSet`]
    /// gets resized by another task.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// let future_insert = hashset.insert_async(1);
    /// let future_retain = hashset.retain_async(|k| *k == 1);
    /// ```
    #[inline]
    pub async fn retain_async<F: FnMut(&K) -> bool>(&self, mut filter: F) {
        self.map.retain_async(|k, ()| filter(k)).await;
    }

    /// Clears the [`HashSet`] by removing all keys.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// hashset.clear();
    ///
    /// assert!(!hashset.contains(&1));
    /// ```
    #[inline]
    pub fn clear(&self) {
        self.map.clear();
    }

    /// Clears the [`HashSet`] by removing all keys.
    ///
    /// It is an asynchronous method returning an `impl Future` for the caller to await.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// let future_insert = hashset.insert_async(1);
    /// let future_clear = hashset.clear_async();
    /// ```
    #[inline]
    pub async fn clear_async(&self) {
        self.map.clear_async().await;
    }

    /// Returns the number of entries in the [`HashSet`].
    ///
    /// It reads the entire metadata area of the bucket array to calculate the number of valid
    /// entries, making its time complexity `O(N)`. Furthermore, it may overcount entries if an old
    /// bucket array has yet to be dropped.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.insert(1).is_ok());
    /// assert_eq!(hashset.len(), 1);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        self.map.len()
    }

    /// Returns `true` if the [`HashSet`] is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert!(hashset.is_empty());
    /// assert!(hashset.insert(1).is_ok());
    /// assert!(!hashset.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.map.is_empty()
    }

    /// Returns the capacity of the [`HashSet`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset_default: HashSet<u64> = HashSet::default();
    /// assert_eq!(hashset_default.capacity(), 0);
    ///
    /// assert!(hashset_default.insert(1).is_ok());
    /// assert_eq!(hashset_default.capacity(), 64);
    ///
    /// let hashset: HashSet<u64> = HashSet::with_capacity(1000);
    /// assert_eq!(hashset.capacity(), 1024);
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize {
        self.map.capacity()
    }

    /// Returns the current capacity range of the [`HashSet`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// assert_eq!(hashset.capacity_range(), 0..=(1_usize << (usize::BITS - 1)));
    ///
    /// let reserved = hashset.reserve(1000);
    /// assert_eq!(hashset.capacity_range(), 1000..=(1_usize << (usize::BITS - 1)));
    /// ```
    #[inline]
    pub fn capacity_range(&self) -> RangeInclusive<usize> {
        self.map.capacity_range()
    }

    /// Returns the index of the bucket that may contain the key.
    ///
    /// The method returns the index of the bucket associated with the key. The number of buckets
    /// can be calculated by dividing `32` into the capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::with_capacity(1024);
    ///
    /// let bucket_index = hashset.bucket_index(&11);
    /// assert!(bucket_index < hashset.capacity() / 32);
    /// ```
    #[inline]
    pub fn bucket_index<Q>(&self, key: &Q) -> usize
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.map.bucket_index(key)
    }
}

impl<K, H> Clone for HashSet<K, H>
where
    K: Clone + Eq + Hash,
    H: BuildHasher + Clone,
{
    #[inline]
    fn clone(&self) -> Self {
        Self {
            map: self.map.clone(),
        }
    }
}

impl<K, H> Debug for HashSet<K, H>
where
    K: Debug + Eq + Hash,
    H: BuildHasher,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut d = f.debug_set();
        self.scan(|k| {
            d.entry(k);
        });
        d.finish()
    }
}

impl<K: Eq + Hash> HashSet<K, RandomState> {
    /// Creates an empty default [`HashSet`].
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::new();
    ///
    /// let result = hashset.capacity();
    /// assert_eq!(result, 0);
    /// ```
    #[inline]
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Creates an empty [`HashSet`] with the specified capacity.
    ///
    /// The actual capacity is equal to or greater than the specified capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::with_capacity(1000);
    ///
    /// let result = hashset.capacity();
    /// assert_eq!(result, 1024);
    /// ```
    #[inline]
    #[must_use]
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            map: HashMap::with_capacity(capacity),
        }
    }
}

impl<K, H> Default for HashSet<K, H>
where
    H: BuildHasher + Default,
{
    /// Creates an empty default [`HashSet`].
    ///
    /// The default capacity is `64`.
    ///
    /// # Examples
    ///
    /// ```
    /// use scc::HashSet;
    ///
    /// let hashset: HashSet<u64> = HashSet::default();
    ///
    /// let result = hashset.capacity();
    /// assert_eq!(result, 0);
    /// ```
    #[inline]
    fn default() -> Self {
        Self {
            map: HashMap::default(),
        }
    }
}

impl<K, H> FromIterator<K> for HashSet<K, H>
where
    K: Eq + Hash,
    H: BuildHasher + Default,
{
    #[inline]
    fn from_iter<T: IntoIterator<Item = K>>(iter: T) -> Self {
        let into_iter = iter.into_iter();
        let hashset = Self::with_capacity_and_hasher(
            HashMap::<K, (), H>::capacity_from_size_hint(into_iter.size_hint()),
            H::default(),
        );
        into_iter.for_each(|k| {
            let _result = hashset.insert(k);
        });
        hashset
    }
}

impl<K, H> PartialEq for HashSet<K, H>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Compares two [`HashSet`] instances.
    ///
    /// ## Locking behavior
    ///
    /// Shared locks on buckets are acquired when comparing two instances of [`HashSet`], therefore
    /// it may lead to a deadlock if the instances are being modified by another thread.
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        if !self.any(|k| !other.contains(k)) {
            return !other.any(|k| !self.contains(k));
        }
        false
    }
}
