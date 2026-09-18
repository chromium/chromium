//! A concurrent insert only hash map.
//!
//! This crate implements a "memo map" which is in many ways similar to a
//! [`HashMap`] with some crucial differences:
//!
//! * Unlike a regular hash map, a memo map is thread safe and synchronized.
//! * Adding or retrieving keys works through a shared reference, removing only
//!   through a mutable reference.
//! * Retrieving a value from a memo map returns a plain old reference.
//!
//! Together these purposes allow one to use this type of structure to
//! implement something similar to lazy loading in places where the API
//! has been constrained to references before.
//!
//! The entries in the map are individually boxed up so that resizing of the
//! map retains the previously issued references.
//!
//! ```
//! use memo_map::MemoMap;
//!
//! let memo = MemoMap::new();
//! let one = memo.get_or_insert(&1, || "one".to_string());
//! let one2 = memo.get_or_insert(&1, || "not one".to_string());
//! assert_eq!(one, "one");
//! assert_eq!(one2, "one");
//! ```
//!
//! # Notes on Iteration
//!
//! Because the memo map internally uses a mutex it needs to be held during
//! iteration.  This is potentially dangerous as it means you can easily
//! deadlock yourself when trying to use the memo map while iterating.  The
//! iteration functionality thus has to be used with great care.
//!
//! # Notes on Removal
//!
//! Items can be removed from a memo map but this operation requires a mutable
//! reference to the memo map.  This is so that it can ensure that there are no
//! borrows outstanding that would be invalidated through the removal of the item.
use std::borrow::Borrow;
use std::cell::UnsafeCell;
use std::collections::hash_map::{self, Entry, RandomState};
use std::collections::HashMap;
use std::convert::Infallible;
use std::hash::{BuildHasher, Hash};
use std::marker::PhantomPinned;
use std::mem::{transmute, ManuallyDrop};
use std::pin::Pin;
use std::sync::{Mutex, MutexGuard};

macro_rules! lock {
    ($mutex:expr) => {
        match $mutex.lock() {
            Ok(guard) => guard,
            Err(poisoned) => poisoned.into_inner(),
        }
    };
}

macro_rules! get_mut {
    (let $target:ident, $mutex:expr) => {
        let mut $target = $mutex.get_mut();
        let $target = match $target {
            Ok(guard) => guard,
            Err(ref mut poisoned) => poisoned.get_mut(),
        };
    };
}

struct StableEntryInner<K, V> {
    key: K,
    value: UnsafeCell<V>,
    _pin: PhantomPinned,
}

struct StableEntry<K, V>(Pin<Box<StableEntryInner<K, V>>>);

impl<K, V> StableEntry<K, V> {
    fn new(key: K, value: V) -> Self {
        StableEntry(Box::pin(StableEntryInner {
            key,
            value: UnsafeCell::new(value),
            _pin: PhantomPinned,
        }))
    }

    fn key(&self) -> &K {
        &self.0.key
    }

    fn value(&self) -> &V {
        // SAFETY: Values are only mutated through an exclusive borrow of the
        // MemoMap. Shared access to a MemoMap never changes existing values.
        unsafe { &*self.0.value.get() }
    }

    fn value_ptr(&self) -> *mut V {
        self.0.value.get()
    }

    fn into_value(self) -> V {
        // SAFETY: The entry has been removed from the map, and an exclusive
        // borrow of the MemoMap ensures no references into it are outstanding.
        unsafe { Pin::into_inner_unchecked(self.0) }
            .value
            .into_inner()
    }
}

impl<K: Clone, V: Clone> Clone for StableEntry<K, V> {
    fn clone(&self) -> Self {
        StableEntry::new(self.key().clone(), self.value().clone())
    }
}

impl<K: Hash, V> Hash for StableEntry<K, V> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.key().hash(state);
    }
}

impl<K: PartialEq, V> PartialEq for StableEntry<K, V> {
    fn eq(&self, other: &Self) -> bool {
        self.key().eq(other.key())
    }
}

impl<K: Eq, V> Eq for StableEntry<K, V> {}

impl<K, V, Q: ?Sized> Borrow<BorrowedKey<Q>> for StableEntry<K, V>
where
    K: Borrow<Q>,
{
    fn borrow(&self) -> &BorrowedKey<Q> {
        BorrowedKey::from_ref(self.key().borrow())
    }
}

#[repr(transparent)]
struct BorrowedKey<Q: ?Sized>(Q);

impl<Q: ?Sized> BorrowedKey<Q> {
    fn from_ref(key: &Q) -> &Self {
        // SAFETY: BorrowedKey is transparent over Q, so their references have
        // identical layouts and metadata.
        unsafe { &*(key as *const Q as *const BorrowedKey<Q>) }
    }
}

impl<Q: Hash + ?Sized> Hash for BorrowedKey<Q> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}

impl<Q: PartialEq + ?Sized> PartialEq for BorrowedKey<Q> {
    fn eq(&self, other: &Self) -> bool {
        self.0.eq(&other.0)
    }
}

impl<Q: Eq + ?Sized> Eq for BorrowedKey<Q> {}

type InnerMap<K, V, S> = HashMap<StableEntry<K, V>, (), S>;

struct DebugMap<'a, K, V, S>(&'a InnerMap<K, V, S>);

impl<K: std::fmt::Debug, V: std::fmt::Debug, S> std::fmt::Debug for DebugMap<'_, K, V, S> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut map = f.debug_map();
        for entry in self.0.keys() {
            map.entry(entry.key(), entry.value());
        }
        map.finish()
    }
}

/// An insert only, thread safe hash map to memoize values.
///
/// Keys and values need to be [`Sync`] for the map itself to be [`Sync`],
/// because references to both can be returned after the internal lock is
/// released.
///
/// ```compile_fail
/// use memo_map::MemoMap;
/// use std::cell::Cell;
///
/// fn assert_sync<T: Sync>() {}
/// assert_sync::<MemoMap<u32, Cell<u32>>>();
/// ```
///
/// ```compile_fail
/// use memo_map::MemoMap;
/// use std::cell::Cell;
///
/// fn assert_sync<T: Sync>() {}
/// assert_sync::<MemoMap<Cell<u32>, u32>>();
/// ```
pub struct MemoMap<K, V, S = RandomState> {
    inner: Mutex<InnerMap<K, V, S>>,
}

impl<K: std::fmt::Debug, V: std::fmt::Debug, S> std::fmt::Debug for MemoMap<K, V, S> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = lock!(self.inner);
        f.debug_struct("MemoMap")
            .field("inner", &DebugMap(&inner))
            .finish()
    }
}

// SAFETY: Moving a MemoMap between threads moves all stored keys, values, and
// the hash builder, so each of them must be Send.
unsafe impl<K: Send, V: Send, S: Send> Send for MemoMap<K, V, S> {}

// SAFETY: Access to the hash map is synchronized, and the keys and values are
// additionally Sync because shared references to them can outlive the lock.
unsafe impl<K: Send + Sync, V: Send + Sync, S: Send> Sync for MemoMap<K, V, S> {}

impl<K: Clone, V: Clone, S: Clone> Clone for MemoMap<K, V, S> {
    fn clone(&self) -> Self {
        Self {
            inner: Mutex::new(lock!(self.inner).clone()),
        }
    }
}

impl<K, V, S: Default> Default for MemoMap<K, V, S> {
    fn default() -> Self {
        MemoMap {
            inner: Mutex::new(HashMap::default()),
        }
    }
}

impl<K, V> MemoMap<K, V, RandomState> {
    /// Creates an empty `MemoMap`.
    pub fn new() -> MemoMap<K, V, RandomState> {
        MemoMap {
            inner: Mutex::default(),
        }
    }
}

impl<K, V, S> MemoMap<K, V, S> {
    /// Creates an empty `MemoMap` which will use the given hash builder to hash
    /// keys.
    pub fn with_hasher(hash_builder: S) -> MemoMap<K, V, S> {
        MemoMap {
            inner: Mutex::new(HashMap::with_hasher(hash_builder)),
        }
    }
}

impl<K, V, S> MemoMap<K, V, S>
where
    K: Eq + Hash,
    S: BuildHasher,
{
    /// Inserts a value into the memo map.
    ///
    /// This inserts a value for a specific key into the memo map.  If the
    /// key already exists, this method does nothing and instead returns `false`.
    /// Otherwise the value is inserted and `true` is returned.  It's generally
    /// recommended to instead use [`get_or_insert`](Self::get_or_insert) or
    /// it's sibling [`get_or_try_insert`](Self::get_or_try_insert).
    pub fn insert(&self, key: K, value: V) -> bool {
        let mut inner = lock!(self.inner);
        match inner.entry(StableEntry::new(key, value)) {
            Entry::Occupied(_) => false,
            Entry::Vacant(vacant) => {
                vacant.insert(());
                true
            }
        }
    }

    /// Inserts a value into the memo map replacing the old value.
    ///
    /// This has the same restrictions as [`remove`](Self::remove) and
    /// [`clear`](Self::clear) in that it requires a mutable reference to
    /// the map.
    pub fn replace(&mut self, key: K, value: V) {
        let mut inner = lock!(self.inner);
        if let Some((entry, _)) = inner.get_key_value(BorrowedKey::from_ref(&key)) {
            // SAFETY: replace requires an exclusive borrow of the MemoMap, so
            // no references to a stored value can be outstanding.
            let old_value = unsafe { std::mem::replace(&mut *entry.value_ptr(), value) };
            drop(old_value);
        } else {
            inner.insert(StableEntry::new(key, value), ());
        }
    }

    /// Returns true if the map contains a value for the specified key.
    ///
    /// The key may be any borrowed form of the map's key type, but [`Hash`] and
    /// [`Eq`] on the borrowed form must match those for the key type.
    pub fn contains_key<Q>(&self, key: &Q) -> bool
    where
        Q: Hash + Eq + ?Sized,
        K: Borrow<Q>,
    {
        lock!(self.inner).contains_key(BorrowedKey::from_ref(key))
    }

    /// Returns a reference to the value corresponding to the key.
    ///
    /// The key may be any borrowed form of the map's key type, but [`Hash`] and
    /// [`Eq`] on the borrowed form must match those for the key type.
    pub fn get<Q>(&self, key: &Q) -> Option<&V>
    where
        Q: Hash + Eq + ?Sized,
        K: Borrow<Q>,
    {
        let inner = lock!(self.inner);
        let value = inner.get_key_value(BorrowedKey::from_ref(key))?.0.value();
        Some(unsafe { transmute::<&V, &V>(value) })
    }

    /// Returns a mutable reference to the value corresponding to the key.
    ///
    /// The key may be any borrowed form of the map's key type, but [`Hash`] and
    /// [`Eq`] on the borrowed form must match those for the key type.
    #[allow(clippy::mutable_key_type)] // Only the non-hashed value is mutable.
    pub fn get_mut<Q>(&mut self, key: &Q) -> Option<&mut V>
    where
        Q: Hash + Eq + ?Sized,
        K: Borrow<Q>,
    {
        get_mut!(let map, self.inner);
        let entry = map.get_key_value(BorrowedKey::from_ref(key))?.0;
        // SAFETY: get_mut requires an exclusive borrow of the MemoMap, so no
        // other references to the value can be outstanding.
        Some(unsafe { &mut *entry.value_ptr() })
    }

    /// Returns a reference to the value corresponding to the key or inserts.
    ///
    /// This is the preferred way to work with a memo map: if the value has not
    /// been in the map yet the creator function is invoked to create the value,
    /// otherwise the already stored value is returned.  The creator function itself
    /// can be falliable and the error is passed through.
    ///
    /// If the creator is infallible, [`get_or_insert`](Self::get_or_insert) can be used.
    pub fn get_or_try_insert<Q, F, E>(&self, key: &Q, creator: F) -> Result<&V, E>
    where
        Q: Hash + Eq + ToOwned<Owned = K> + ?Sized,
        K: Borrow<Q>,
        F: FnOnce() -> Result<V, E>,
    {
        let mut inner = lock!(self.inner);
        let key = BorrowedKey::from_ref(key);
        if let Some((entry, _)) = inner.get_key_value(key) {
            return Ok(unsafe { transmute::<&V, &V>(entry.value()) });
        }

        let entry = StableEntry::new(key.0.to_owned(), creator()?);
        let value_ptr = match inner.entry(entry) {
            Entry::Occupied(entry) => entry.key().value_ptr(),
            Entry::Vacant(entry) => {
                let value_ptr = entry.key().value_ptr();
                entry.insert(());
                value_ptr
            }
        };
        // SAFETY: the entry is individually boxed and cannot be removed while
        // the returned reference keeps the MemoMap borrowed.
        Ok(unsafe { &*value_ptr })
    }

    /// Like [`get_or_insert`](Self::get_or_insert) but with an owned key.
    pub fn get_or_insert_owned<F>(&self, key: K, creator: F) -> &V
    where
        F: FnOnce() -> V,
    {
        self.get_or_try_insert_owned(key, || Ok::<_, Infallible>(creator()))
            .unwrap()
    }

    /// Like [`get_or_try_insert`](Self::get_or_try_insert) but with an owned key.
    ///
    /// If the creator is infallible, [`get_or_insert_owned`](Self::get_or_insert_owned) can be used.
    pub fn get_or_try_insert_owned<F, E>(&self, key: K, creator: F) -> Result<&V, E>
    where
        F: FnOnce() -> Result<V, E>,
    {
        let mut inner = lock!(self.inner);
        if let Some((entry, _)) = inner.get_key_value(BorrowedKey::from_ref(&key)) {
            return Ok(unsafe { transmute::<&V, &V>(entry.value()) });
        }

        let entry = StableEntry::new(key, creator()?);
        let value_ptr = match inner.entry(entry) {
            Entry::Occupied(entry) => entry.key().value_ptr(),
            Entry::Vacant(entry) => {
                let value_ptr = entry.key().value_ptr();
                entry.insert(());
                value_ptr
            }
        };
        // SAFETY: the entry is individually boxed and cannot be removed while
        // the returned reference keeps the MemoMap borrowed.
        Ok(unsafe { &*value_ptr })
    }

    /// Returns a reference to the value corresponding to the key or inserts.
    ///
    /// This is the preferred way to work with a memo map: if the value has not
    /// been in the map yet the creator function is invoked to create the value,
    /// otherwise the already stored value is returned.
    ///
    /// If the creator is fallible, [`get_or_try_insert`](Self::get_or_try_insert) can be used.
    ///
    /// # Example
    ///
    /// ```
    /// # use memo_map::MemoMap;
    /// let memo = MemoMap::new();
    ///
    /// // first time inserts
    /// let value = memo.get_or_insert("key", || "23");
    /// assert_eq!(*value, "23");
    ///
    /// // second time returns old value
    /// let value = memo.get_or_insert("key", || "24");
    /// assert_eq!(*value, "23");
    /// ```
    pub fn get_or_insert<Q, F>(&self, key: &Q, creator: F) -> &V
    where
        Q: Hash + Eq + ToOwned<Owned = K> + ?Sized,
        K: Borrow<Q>,
        F: FnOnce() -> V,
    {
        self.get_or_try_insert(key, || Ok::<_, Infallible>(creator()))
            .unwrap()
    }

    /// Removes a key from the memo map, returning the value at the key if the key
    /// was previously in the map.
    ///
    /// A key can only be removed if a mutable reference to the memo map exists.
    /// In other words a key can not be removed if there can be borrows to the item.
    pub fn remove<Q>(&mut self, key: &Q) -> Option<V>
    where
        Q: Hash + Eq + ?Sized,
        K: Borrow<Q>,
    {
        lock!(self.inner)
            .remove_entry(BorrowedKey::from_ref(key))
            .map(|(entry, ())| entry.into_value())
    }

    /// Clears the map, removing all elements.
    pub fn clear(&mut self) {
        lock!(self.inner).clear();
    }

    /// Returns the number of items in the map.
    ///
    /// # Example
    ///
    /// ```
    /// # use memo_map::MemoMap;
    /// let memo = MemoMap::new();
    ///
    /// assert_eq!(memo.len(), 0);
    /// memo.insert(1, "a");
    /// memo.insert(2, "b");
    /// memo.insert(2, "not b");
    /// assert_eq!(memo.len(), 2);
    /// ```
    pub fn len(&self) -> usize {
        lock!(self.inner).len()
    }

    /// Returns `true` if the memo map contains no items.
    pub fn is_empty(&self) -> bool {
        lock!(self.inner).is_empty()
    }

    /// An iterator visiting all key-value pairs in arbitrary order. The
    /// iterator element type is `(&'a K, &'a V)`.
    ///
    /// Important note: during iteration the map is locked!  This means that you
    /// must not perform calls to the map or you will run into deadlocks.  This
    /// makes the iterator rather useless in practice for a lot of operations.
    pub fn iter(&self) -> Iter<'_, K, V, S> {
        let guard = lock!(self.inner);
        let iter = guard.iter();
        Iter {
            iter: ManuallyDrop::new(unsafe {
                transmute::<
                    hash_map::Iter<'_, StableEntry<K, V>, ()>,
                    hash_map::Iter<'_, StableEntry<K, V>, ()>,
                >(iter)
            }),
            guard: ManuallyDrop::new(guard),
        }
    }

    /// An iterator visiting all key-value pairs in arbitrary order, with mutable
    /// references to the values.  The iterator element type is `(&'a K, &'a mut V)`.
    ///
    /// This iterator requires a mutable reference to the map.
    #[allow(clippy::mutable_key_type)] // Only the non-hashed values are mutable.
    pub fn iter_mut(&mut self) -> IterMut<'_, K, V> {
        get_mut!(let map, self.inner);
        IterMut {
            iter: unsafe {
                transmute::<
                    hash_map::IterMut<'_, StableEntry<K, V>, ()>,
                    hash_map::IterMut<'_, StableEntry<K, V>, ()>,
                >(map.iter_mut())
            },
        }
    }

    /// An iterator visiting all values mutably in arbitrary order.  The iterator
    /// element type is `&'a mut V`.
    ///
    /// This iterator requires a mutable reference to the map.
    #[allow(clippy::mutable_key_type)] // Only the non-hashed values are mutable.
    pub fn values_mut(&mut self) -> ValuesMut<'_, K, V> {
        get_mut!(let map, self.inner);
        ValuesMut {
            iter: unsafe {
                transmute::<
                    hash_map::IterMut<'_, StableEntry<K, V>, ()>,
                    hash_map::IterMut<'_, StableEntry<K, V>, ()>,
                >(map.iter_mut())
            },
        }
    }

    /// An iterator visiting all keys in arbitrary order. The iterator element
    /// type is `&'a K`.
    pub fn keys(&self) -> Keys<'_, K, V, S> {
        Keys { iter: self.iter() }
    }
}

/// An iterator over the items of a [`MemoMap`].
///
/// This struct is created by the [`iter`](MemoMap::iter) method on [`MemoMap`].
/// See its documentation for more information.
pub struct Iter<'a, K, V, S> {
    iter: ManuallyDrop<hash_map::Iter<'a, StableEntry<K, V>, ()>>,
    guard: ManuallyDrop<MutexGuard<'a, InnerMap<K, V, S>>>,
}

impl<K, V, S> Drop for Iter<'_, K, V, S> {
    fn drop(&mut self) {
        unsafe {
            // The iterator borrows data protected by the guard, so it must be
            // dropped before the guard unlocks the map.
            ManuallyDrop::drop(&mut self.iter);
            ManuallyDrop::drop(&mut self.guard);
        }
    }
}

impl<'a, K, V, S> Iterator for Iter<'a, K, V, S> {
    type Item = (&'a K, &'a V);

    fn next(&mut self) -> Option<Self::Item> {
        self.iter
            .next()
            .map(|(entry, ())| (entry.key(), entry.value()))
    }
}

/// An iterator over the keys of a [`MemoMap`].
///
/// This struct is created by the [`keys`](MemoMap::keys) method on [`MemoMap`].
/// See its documentation for more information.
pub struct Keys<'a, K, V, S> {
    iter: Iter<'a, K, V, S>,
}

impl<'a, K, V, S> Iterator for Keys<'a, K, V, S> {
    type Item = &'a K;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|(k, _)| k)
    }
}

/// A mutable iterator over a [`MemoMap`].
pub struct IterMut<'a, K, V> {
    iter: hash_map::IterMut<'a, StableEntry<K, V>, ()>,
}

impl<'a, K, V> Iterator for IterMut<'a, K, V> {
    type Item = (&'a K, &'a mut V);

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|(entry, ())| {
            // SAFETY: IterMut holds an exclusive borrow of the MemoMap.
            (entry.key(), unsafe { &mut *entry.value_ptr() })
        })
    }
}

/// A mutable iterator over a [`MemoMap`].
pub struct ValuesMut<'a, K, V> {
    iter: hash_map::IterMut<'a, StableEntry<K, V>, ()>,
}

impl<'a, K, V> Iterator for ValuesMut<'a, K, V> {
    type Item = &'a mut V;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|(entry, ())| {
            // SAFETY: ValuesMut holds an exclusive borrow of the MemoMap.
            unsafe { &mut *entry.value_ptr() }
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_insert() {
        let memo = MemoMap::new();
        assert!(memo.insert(23u32, Box::new(1u32)));
        assert!(!memo.insert(23u32, Box::new(2u32)));
        assert_eq!(memo.get(&23u32).cloned(), Some(Box::new(1)));
    }

    #[test]
    fn test_iter() {
        let memo = MemoMap::new();
        memo.insert(1, "one");
        memo.insert(2, "two");
        memo.insert(3, "three");
        let mut values = memo.iter().map(|(k, v)| (*k, *v)).collect::<Vec<_>>();
        values.sort();
        assert_eq!(values, vec![(1, "one"), (2, "two"), (3, "three")]);
    }

    #[test]
    fn test_keys() {
        let memo = MemoMap::new();
        memo.insert(1, "one");
        memo.insert(2, "two");
        memo.insert(3, "three");
        let mut values = memo.keys().copied().collect::<Vec<_>>();
        values.sort();
        assert_eq!(values, vec![1, 2, 3]);
    }

    #[test]
    fn test_contains() {
        let memo = MemoMap::new();
        memo.insert(1, "one");
        assert!(memo.contains_key(&1));
        assert!(!memo.contains_key(&2));
    }

    #[test]
    fn test_remove() {
        let mut memo = MemoMap::new();
        memo.insert(1, "one");
        let value = memo.get(&1);
        assert!(value.is_some());
        let old_value = memo.remove(&1);
        assert_eq!(old_value, Some("one"));
        let value = memo.get(&1);
        assert!(value.is_none());
    }

    #[test]
    fn test_clear() {
        let mut memo = MemoMap::new();
        memo.insert(1, "one");
        memo.insert(2, "two");
        assert_eq!(memo.len(), 2);
        assert!(!memo.is_empty());
        memo.clear();
        assert_eq!(memo.len(), 0);
        assert!(memo.is_empty());
    }

    #[test]
    fn test_ref_after_resize() {
        let memo = MemoMap::new();
        let mut refs = Vec::new();

        let iterations = if cfg!(miri) { 100 } else { 10000 };

        for key in 0..iterations {
            refs.push((key, memo.get_or_insert(&key, || Box::new(key))));
        }
        for (key, val) in refs {
            dbg!(key, val);
            assert_eq!(memo.get(&key), Some(val));
        }
    }

    #[test]
    fn test_ref_after_resize_owned() {
        let memo = MemoMap::new();
        let mut refs = Vec::new();

        let iterations = if cfg!(miri) { 100 } else { 10000 };

        for key in 0..iterations {
            refs.push((
                key,
                memo.get_or_insert_owned(key.to_string(), || Box::new(key)),
            ));
        }
        for (key, val) in refs {
            dbg!(key, val);
            assert_eq!(memo.get(&key.to_string()), Some(val));
        }
    }

    #[test]
    fn test_key_ref_after_resize() {
        let memo = MemoMap::new();
        memo.insert(0usize, 0usize);

        let key = memo.keys().next().unwrap();
        let iterations = if cfg!(miri) { 100 } else { 10000 };
        for value in 1..iterations {
            memo.insert(value, value);
        }

        assert_eq!(*key, 0);
    }

    #[test]
    fn test_borrowed_key_lookup() {
        let mut memo = MemoMap::new();
        memo.insert("key".to_string(), 42);

        assert!(memo.contains_key("key"));
        assert_eq!(memo.get("key"), Some(&42));
        *memo.get_mut("key").unwrap() = 43;
        assert_eq!(memo.remove("key"), Some(43));
    }

    #[test]
    fn test_replace() {
        let mut memo = MemoMap::new();
        memo.insert("foo", "bar");
        memo.replace("foo", "bar2");
        assert_eq!(memo.get("foo"), Some(&"bar2"));
    }

    #[test]
    fn test_get_mut() {
        let mut memo = MemoMap::new();
        memo.insert("foo", "bar");
        *memo.get_mut("foo").unwrap() = "bar2";
        assert_eq!(memo.get("foo"), Some(&"bar2"));
    }

    #[test]
    fn test_iter_mut() {
        let mut memo = MemoMap::new();
        memo.insert("foo", "bar");
        for item in memo.iter_mut() {
            *item.1 = "bar2";
        }
        assert_eq!(memo.get("foo"), Some(&"bar2"));
    }

    #[test]
    fn test_values_mut() {
        let mut memo = MemoMap::new();
        memo.insert("foo", "bar");
        for item in memo.values_mut() {
            *item = "bar2";
        }
        assert_eq!(memo.get("foo"), Some(&"bar2"));
    }
}
