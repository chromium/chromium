pub mod bucket;
pub mod bucket_array;

use super::ebr::{AtomicShared, Guard, Ptr, Shared, Tag};
use super::exit_guard::ExitGuard;
use super::wait_queue::{AsyncWait, DeriveAsyncWait};
use super::Equivalent;
use bucket::{DataBlock, EntryPtr, Locker, LruList, Reader, BUCKET_LEN, CACHE, OPTIMISTIC};
use bucket_array::BucketArray;
use std::hash::{BuildHasher, Hash, Hasher};
use std::pin::Pin;
use std::sync::atomic::Ordering::{AcqRel, Acquire, Relaxed, Release};
use std::sync::atomic::{fence, AtomicUsize};

/// The maximum resize factor.
const MAX_RESIZE_FACTOR: usize = (usize::BITS / 2) as usize;

/// `HashTable` defines common functions for hash table implementations.
pub(super) trait HashTable<K, V, H, L: LruList, const TYPE: char>
where
    K: Eq + Hash,
    H: BuildHasher,
{
    /// Returns the hash value of the key.
    #[allow(clippy::manual_hash_one)] // Stabilized in Rust 1.71.0.
    #[inline]
    fn hash<Q>(&self, key: &Q) -> u64
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        let mut hasher = self.hasher().build_hasher();
        key.hash(&mut hasher);
        hasher.finish()
    }

    /// Returns a reference to its [`BuildHasher`].
    fn hasher(&self) -> &H;

    /// Tries to clone the instances pointed by `entry`.
    ///
    /// It does not clone unless `TYPE` is `OPTIMISTIC` thus `K` and `V` both being `Clone`.
    fn try_clone(entry: &(K, V)) -> Option<(K, V)>;

    /// Returns a reference to the [`BucketArray`] pointer.
    fn bucket_array(&self) -> &AtomicShared<BucketArray<K, V, L, TYPE>>;

    /// Calculates the bucket index from the supplied key.
    #[inline]
    fn calculate_bucket_index<Q>(&self, key: &Q) -> usize
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        self.bucket_array()
            .load(Acquire, &Guard::new())
            .as_ref()
            .map_or(0, |a| a.calculate_bucket_index(self.hash(key)))
    }

    /// Returns the minimum allowed capacity.
    fn minimum_capacity(&self) -> &AtomicUsize;

    /// Returns the maximum capacity.
    ///
    /// The maximum capacity must be a power of `2`.
    fn maximum_capacity(&self) -> usize;

    /// Reserves the specified capacity.
    ///
    /// Returns the actually allocated capacity.
    #[inline]
    fn reserve_capacity(&self, additional_capacity: usize) -> usize {
        let mut current_minimum_capacity = self.minimum_capacity().load(Relaxed);
        loop {
            let Some(new_minimum_capacity) =
                current_minimum_capacity.checked_add(additional_capacity)
            else {
                return 0;
            };

            match self.minimum_capacity().compare_exchange_weak(
                current_minimum_capacity,
                new_minimum_capacity,
                Relaxed,
                Relaxed,
            ) {
                Ok(_) => {
                    self.try_resize(0, &Guard::new());
                    return additional_capacity;
                }
                Err(actual) => current_minimum_capacity = actual,
            }
        }
    }

    /// Returns a reference to the current array.
    ///
    /// If no array has been allocated, it allocates a new one and returns it.
    #[inline]
    fn get_current_array<'g>(&self, guard: &'g Guard) -> &'g BucketArray<K, V, L, TYPE> {
        // An acquire fence is required to correctly load the contents of the array.
        let current_array_ptr = self.bucket_array().load(Acquire, guard);
        if let Some(current_array) = current_array_ptr.as_ref() {
            return current_array;
        }

        unsafe {
            let current_array_ptr = match self.bucket_array().compare_exchange(
                Ptr::null(),
                (
                    Some(Shared::new_unchecked(BucketArray::<K, V, L, TYPE>::new(
                        self.minimum_capacity().load(Relaxed),
                        AtomicShared::null(),
                    ))),
                    Tag::None,
                ),
                AcqRel,
                Acquire,
                guard,
            ) {
                Ok((_, ptr)) | Err((_, ptr)) => ptr,
            };
            current_array_ptr.as_ref().unwrap_unchecked()
        }
    }

    /// Returns the number of entries.
    #[inline]
    fn num_entries(&self, guard: &Guard) -> usize {
        let mut num_entries = 0;
        if let Some(current_array) = self.bucket_array().load(Acquire, guard).as_ref() {
            let old_array_ptr = current_array.old_array(guard);
            if let Some(old_array) = old_array_ptr.as_ref() {
                for i in 0..old_array.num_buckets() {
                    num_entries += old_array.bucket(i).num_entries();
                }
            }
            for i in 0..current_array.num_buckets() {
                num_entries += current_array.bucket(i).num_entries();
            }
            if num_entries == 0 && self.minimum_capacity().load(Relaxed) == 0 {
                self.try_resize(0, guard);
            }
        }
        num_entries
    }

    /// Returns `true` if the number of entries is non-zero.
    #[inline]
    fn has_entry(&self, guard: &Guard) -> bool {
        if let Some(current_array) = self.bucket_array().load(Acquire, guard).as_ref() {
            let old_array_ptr = current_array.old_array(guard);
            if let Some(old_array) = old_array_ptr.as_ref() {
                for i in 0..old_array.num_buckets() {
                    if old_array.bucket(i).num_entries() != 0 {
                        return true;
                    }
                }
            }
            for i in 0..current_array.num_buckets() {
                if current_array.bucket(i).num_entries() != 0 {
                    return true;
                }
            }
            if self.minimum_capacity().load(Relaxed) == 0 {
                self.try_resize(0, guard);
            }
        }
        false
    }

    /// Returns the number of slots.
    #[inline]
    fn num_slots(&self, guard: &Guard) -> usize {
        if let Some(current_array) = self.bucket_array().load(Acquire, guard).as_ref() {
            current_array.num_entries()
        } else {
            0
        }
    }

    /// Estimates the number of entries by sampling the specified number of buckets.
    #[inline]
    fn sample(
        current_array: &BucketArray<K, V, L, TYPE>,
        sampling_index: usize,
        sample_size: usize,
    ) -> usize {
        let mut num_entries = 0;
        for i in sampling_index..(sampling_index + sample_size) {
            num_entries += current_array
                .bucket(i % current_array.num_buckets())
                .num_entries();
        }
        num_entries * (current_array.num_buckets() / sample_size)
    }

    /// Checks whether rebuilding the entire hash table is required.
    #[inline]
    fn check_rebuild(
        current_array: &BucketArray<K, V, L, TYPE>,
        sampling_index: usize,
        sample_size: usize,
    ) -> bool {
        let mut num_buckets_to_rebuild = 0;
        for i in sampling_index..(sampling_index + sample_size) {
            if current_array
                .bucket(i % current_array.num_buckets())
                .need_rebuild()
            {
                num_buckets_to_rebuild += 1;
                if num_buckets_to_rebuild >= sample_size / 2 {
                    return true;
                }
            }
        }
        false
    }

    /// Inserts an entry into the [`HashTable`].
    #[inline]
    fn insert_entry<D: DeriveAsyncWait>(
        &self,
        key: K,
        val: V,
        hash: u64,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<Option<(K, V)>, (K, V)> {
        match self.reserve_entry(&key, hash, async_wait, guard) {
            Ok(LockedEntry {
                mut locker,
                data_block_mut,
                entry_ptr,
                index: _,
            }) => {
                if entry_ptr.is_valid() {
                    return Ok(Some((key, val)));
                }
                locker.insert_with(
                    data_block_mut,
                    BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                    || (key, val),
                    guard,
                );
                Ok(None)
            }
            Err(()) => Err((key, val)),
        }
    }

    /// Returns a [`LockedEntry`] pointing to the first occupied entry.
    #[inline]
    fn lock_first_entry<'g>(&self, guard: &'g Guard) -> Option<LockedEntry<'g, K, V, L, TYPE>> {
        let mut current_array_ptr = self.bucket_array().load(Acquire, guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            self.clear_old_array(current_array, guard);
            for index in 0..current_array.num_buckets() {
                let bucket = current_array.bucket_mut(index);
                let lock_result = Locker::lock(bucket, guard);
                if let Some(locker) = lock_result {
                    let data_block_mut = current_array.data_block_mut(index);
                    let mut entry_ptr = EntryPtr::new(guard);
                    if entry_ptr.move_to_next(&locker, guard) {
                        return Some(LockedEntry::new(
                            locker,
                            data_block_mut,
                            entry_ptr,
                            index,
                            guard,
                        ));
                    }
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, guard);
            if current_array_ptr.without_tag() == new_current_array_ptr.without_tag() {
                break;
            }
            current_array_ptr = new_current_array_ptr;
        }
        None
    }

    /// Peeks an entry from the [`HashTable`].
    #[inline]
    fn peek_entry<'g, Q>(&self, key: &Q, hash: u64, guard: &'g Guard) -> Option<&'g (K, V)>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        debug_assert_eq!(TYPE, OPTIMISTIC);
        let mut current_array_ptr = self.bucket_array().load(Acquire, guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            if let Some(old_array) = current_array.old_array(guard).as_ref() {
                if self.incremental_rehash::<Q, (), true>(current_array, &mut (), guard) != Ok(true)
                {
                    let index = old_array.calculate_bucket_index(hash);
                    if let Some(entry) = old_array.bucket(index).search_entry(
                        old_array.data_block(index),
                        key,
                        BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                        guard,
                    ) {
                        return Some(entry);
                    }
                }
            }

            let index = current_array.calculate_bucket_index(hash);
            let bucket = current_array.bucket(index);
            if let Some(entry) = bucket.search_entry(
                current_array.data_block(index),
                key,
                BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                guard,
            ) {
                return Some(entry);
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, guard);
            if current_array_ptr == new_current_array_ptr {
                break;
            }

            // A new array has been allocated.
            current_array_ptr = new_current_array_ptr;
        }

        None
    }

    /// Reads an entry from the [`HashTable`].
    #[inline]
    fn read_entry<Q, D, R, F: FnOnce(&K, &V) -> R>(
        &self,
        key: &Q,
        hash: u64,
        f: F,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<Option<R>, F>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        let mut current_array_ptr = self.bucket_array().load(Acquire, guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            if let Some(old_array) = current_array.old_array(guard).as_ref() {
                if self
                    .move_entry::<Q, D>(current_array, old_array, hash, async_wait, guard)
                    .is_err()
                {
                    return Err(f);
                }
            }

            let index = current_array.calculate_bucket_index(hash);
            let bucket = current_array.bucket(index);
            let lock_result = if let Some(async_wait) = async_wait.derive() {
                match Reader::try_lock_or_wait(bucket, async_wait, guard) {
                    Ok(result) => result,
                    Err(()) => return Err(f),
                }
            } else {
                Reader::lock(bucket, guard)
            };
            if let Some(reader) = lock_result {
                if let Some(entry) = reader.search_entry(
                    current_array.data_block(index),
                    key,
                    BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                    guard,
                ) {
                    return Ok(Some(f(&entry.0, &entry.1)));
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, guard);
            if current_array_ptr == new_current_array_ptr {
                break;
            }

            // A new array has been allocated.
            current_array_ptr = new_current_array_ptr;
        }

        Ok(None)
    }

    /// Gets the occupied entry corresponding to the key.
    ///
    /// Returns an error if locking failed.
    #[inline]
    fn get_entry<'g, Q, D>(
        &self,
        key: &Q,
        hash: u64,
        async_wait: &mut D,
        guard: &'g Guard,
    ) -> Result<Option<LockedEntry<'g, K, V, L, TYPE>>, ()>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        let mut current_array_ptr = self.bucket_array().load(Acquire, guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            if let Some(old_array) = current_array.old_array(guard).as_ref() {
                self.move_entry::<Q, D>(current_array, old_array, hash, async_wait, guard)?;
            }

            let index = current_array.calculate_bucket_index(hash);
            let bucket = current_array.bucket_mut(index);
            let lock_result = if let Some(async_wait) = async_wait.derive() {
                Locker::try_lock_or_wait(bucket, async_wait, guard)?
            } else {
                Locker::lock(bucket, guard)
            };
            if let Some(locker) = lock_result {
                let data_block_mut = current_array.data_block_mut(index);
                let entry_ptr = locker.get_entry_ptr(
                    data_block_mut,
                    key,
                    BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                    guard,
                );
                if entry_ptr.is_valid() {
                    return Ok(Some(LockedEntry::new(
                        locker,
                        data_block_mut,
                        entry_ptr,
                        index,
                        guard,
                    )));
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, guard);
            if current_array_ptr == new_current_array_ptr {
                break;
            }

            // A new array has been allocated.
            current_array_ptr = new_current_array_ptr;
        }

        Ok(None)
    }

    /// Removes the entry containing the key if the condition is met.
    ///
    /// Returns an error if locking failed.
    #[inline]
    fn remove_entry<Q, F: FnOnce(&mut V) -> bool, D, R, P: FnOnce(Option<Option<(K, V)>>) -> R>(
        &self,
        key: &Q,
        hash: u64,
        condition: F,
        post_processor: P,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<R, F>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        while let Some(current_array) = self.bucket_array().load(Acquire, guard).as_ref() {
            // The reasoning behind this loop can be found in `acquire_entry`.
            let shrinkable = if let Some(old_array) = current_array.old_array(guard).as_ref() {
                match self.move_entry::<Q, D>(current_array, old_array, hash, async_wait, guard) {
                    Ok(r) => r,
                    Err(()) => return Err(condition),
                }
            } else {
                true
            };

            let index = current_array.calculate_bucket_index(hash);
            let bucket = current_array.bucket_mut(index);
            let lock_result = if let Some(async_wait) = async_wait.derive() {
                match Locker::try_lock_or_wait(bucket, async_wait, guard) {
                    Ok(l) => l,
                    Err(()) => return Err(condition),
                }
            } else {
                Locker::lock(bucket, guard)
            };
            if let Some(mut locker) = lock_result {
                let data_block_mut = current_array.data_block_mut(index);
                let mut entry_ptr = locker.get_entry_ptr(
                    data_block_mut,
                    key,
                    BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                    guard,
                );
                if entry_ptr.is_valid()
                    && condition(&mut entry_ptr.get_mut(data_block_mut, &mut locker).1)
                {
                    let result = if TYPE == OPTIMISTIC {
                        locker.mark_removed(&mut entry_ptr, guard);
                        None
                    } else {
                        Some(locker.remove(data_block_mut, &mut entry_ptr, guard))
                    };
                    if shrinkable
                        && (locker.num_entries() <= 1 || locker.need_rebuild())
                        && current_array.initiate_sampling(index)
                    {
                        drop(locker);
                        self.try_shrink_or_rebuild(current_array, index, guard);
                    }
                    return Ok(post_processor(Some(result)));
                }
                break;
            }
        }
        Ok(post_processor(None))
    }

    /// Checks if there is any entry that satisfies the specified predicate.
    #[inline]
    fn contains_entry<P: FnMut(&K, &V) -> bool>(&self, mut pred: P) -> bool {
        let guard = Guard::new();
        let mut current_array_ptr = self.bucket_array().load(Acquire, &guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            self.clear_old_array(current_array, &guard);
            for index in 0..current_array.num_buckets() {
                let bucket = current_array.bucket(index);
                if let Some(locker) = Reader::lock(bucket, &guard) {
                    let data_block = current_array.data_block(index);
                    let mut entry_ptr = EntryPtr::new(&guard);
                    while entry_ptr.move_to_next(*locker, &guard) {
                        let (k, v) = entry_ptr.get(data_block);
                        if pred(k, v) {
                            return true;
                        }
                    }
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, &guard);
            if current_array_ptr.without_tag() == new_current_array_ptr.without_tag() {
                break;
            }
            current_array_ptr = new_current_array_ptr;
        }
        false
    }

    /// Finds the first entry that satisfies the specified predicate.
    #[inline]
    fn find_entry<'g, P: FnMut(&K, &V) -> bool>(
        &self,
        mut pred: P,
        guard: &'g Guard,
    ) -> Option<LockedEntry<'g, K, V, L, TYPE>> {
        let mut current_array_ptr = self.bucket_array().load(Acquire, guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            self.clear_old_array(current_array, guard);
            for index in 0..current_array.num_buckets() {
                let bucket = current_array.bucket_mut(index);
                if let Some(locker) = Locker::lock(bucket, guard) {
                    let data_block_mut = current_array.data_block_mut(index);
                    let mut entry_ptr = EntryPtr::new(guard);
                    while entry_ptr.move_to_next(&locker, guard) {
                        let (k, v) = entry_ptr.get(data_block_mut);
                        if pred(k, v) {
                            return Some(LockedEntry::new(
                                locker,
                                data_block_mut,
                                entry_ptr,
                                index,
                                guard,
                            ));
                        }
                    }
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, guard);
            if current_array_ptr.without_tag() == new_current_array_ptr.without_tag() {
                break;
            }
            current_array_ptr = new_current_array_ptr;
        }
        None
    }

    /// Retains entries that satisfy the specified predicate.
    #[inline]
    fn retain_entries<F: FnMut(&K, &mut V) -> bool>(&self, mut pred: F) {
        let guard = Guard::new();
        let mut removed = false;
        let mut current_array_ptr = self.bucket_array().load(Acquire, &guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            self.clear_old_array(current_array, &guard);
            for index in 0..current_array.num_buckets() {
                let bucket = current_array.bucket_mut(index);
                if let Some(mut locker) = Locker::lock(bucket, &guard) {
                    let data_block_mut = current_array.data_block_mut(index);
                    let mut entry_ptr = EntryPtr::new(&guard);
                    while entry_ptr.move_to_next(&locker, &guard) {
                        let (k, v) = entry_ptr.get_mut(data_block_mut, &mut locker);
                        if !pred(k, v) {
                            if TYPE == OPTIMISTIC {
                                locker.mark_removed(&mut entry_ptr, &guard);
                            } else {
                                locker.remove(data_block_mut, &mut entry_ptr, &guard);
                            }
                            removed = true;
                        }
                    }
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, &guard);
            if current_array_ptr.without_tag() == new_current_array_ptr.without_tag() {
                break;
            }
            current_array_ptr = new_current_array_ptr;
        }

        if removed {
            self.try_resize(0, &guard);
        }
    }

    /// Prunes entries satisfying the predicate.
    #[inline]
    fn prune_entries<F: FnMut(&K, V) -> Option<V>>(&self, mut pred: F) {
        let guard = Guard::new();
        let mut removed = false;
        let mut current_array_ptr = self.bucket_array().load(Acquire, &guard);
        while let Some(current_array) = current_array_ptr.as_ref() {
            self.clear_old_array(current_array, &guard);
            for index in 0..current_array.num_buckets() {
                let bucket = current_array.bucket_mut(index);
                if let Some(mut locker) = Locker::lock(bucket, &guard) {
                    let data_block_mut = current_array.data_block_mut(index);
                    let mut entry_ptr = EntryPtr::new(&guard);
                    while entry_ptr.move_to_next(&locker, &guard) {
                        if locker.keep_or_consume(data_block_mut, &mut entry_ptr, &mut pred, &guard)
                        {
                            removed = true;
                        }
                    }
                }
            }

            let new_current_array_ptr = self.bucket_array().load(Acquire, &guard);
            if current_array_ptr.without_tag() == new_current_array_ptr.without_tag() {
                break;
            }
            current_array_ptr = new_current_array_ptr;
        }

        if removed {
            self.try_resize(0, &guard);
        }
    }

    /// Reserves an entry and returns a [`Locker`] and [`EntryPtr`] corresponding to the key.
    ///
    /// The returned [`EntryPtr`] may point to an occupied entry if the key exists.
    ///
    /// Returns an error if locking failed.
    #[inline]
    fn reserve_entry<'g, Q, D>(
        &self,
        key: &Q,
        hash: u64,
        async_wait: &mut D,
        guard: &'g Guard,
    ) -> Result<LockedEntry<'g, K, V, L, TYPE>, ()>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        // It is guaranteed that the thread reads a consistent snapshot of the current and old
        // array pair by a release memory guard in the resize function, hence the following
        // procedure is correct.
        //  - The thread reads `self.array`, and it kills the target bucket in the old array if
        //    there is one attached to it, and inserts the key into `self.array`.
        // There are two cases.
        //  1. The thread reads an old version of `self.array`.
        //    If there is another thread having read the latest version of `self.array`,
        //    trying to insert the same key, it will try to kill the bucket in the old version
        //    of `self.array`, thus competing with each other.
        //  2. The thread reads the latest version of `self.array`.
        //    If the array is deprecated while inserting the key, it falls into case 1.
        loop {
            let current_array = self.get_current_array(guard);
            let resizable = if let Some(old_array) = current_array.old_array(guard).as_ref() {
                self.move_entry::<Q, D>(current_array, old_array, hash, async_wait, guard)?;
                false
            } else {
                true
            };

            let index = current_array.calculate_bucket_index(hash);
            let mut bucket = current_array.bucket_mut(index);

            // Try to resize the array.
            if resizable
                && (TYPE != CACHE || current_array.num_entries() < self.maximum_capacity())
                && current_array.initiate_sampling(index)
                && bucket.num_entries() >= BUCKET_LEN - 1
            {
                self.try_enlarge(current_array, index, bucket.num_entries(), guard);
                bucket = current_array.bucket_mut(index);
            }

            let lock_result = if let Some(async_wait) = async_wait.derive() {
                Locker::try_lock_or_wait(bucket, async_wait, guard)?
            } else {
                Locker::lock(bucket, guard)
            };
            if let Some(locker) = lock_result {
                let data_block_mut = current_array.data_block_mut(index);
                let entry_ptr = locker.get_entry_ptr(
                    data_block_mut,
                    key,
                    BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                    guard,
                );
                return Ok(LockedEntry::new(
                    locker,
                    data_block_mut,
                    entry_ptr,
                    index,
                    guard,
                ));
            }

            // Reaching here means that `self.bucket_array()` has been updated.
        }
    }

    /// Tries to reserve an entry and returns a [`Locker`] and [`EntryPtr`] corresponding to the
    /// key.
    ///
    /// The returned [`EntryPtr`] may point to an occupied entry if the key exists.
    ///
    /// Returns `None` if locking failed.
    #[inline]
    fn try_reserve_entry<'g, Q>(
        &self,
        key: &Q,
        hash: u64,
        guard: &'g Guard,
    ) -> Option<LockedEntry<'g, K, V, L, TYPE>>
    where
        Q: Equivalent<K> + Hash + ?Sized,
    {
        // See `Self::reserve_entry`.
        loop {
            let current_array = self.get_current_array(guard);
            let resizable = if let Some(old_array) = current_array.old_array(guard).as_ref() {
                self.move_entry::<Q, _>(current_array, old_array, hash, &mut (), guard)
                    .ok()?;
                false
            } else {
                true
            };

            let index = current_array.calculate_bucket_index(hash);
            let mut bucket = current_array.bucket_mut(index);

            // Try to resize the array.
            if resizable
                && (TYPE != CACHE || current_array.num_entries() < self.maximum_capacity())
                && current_array.initiate_sampling(index)
                && bucket.num_entries() >= BUCKET_LEN - 1
            {
                self.try_enlarge(current_array, index, bucket.num_entries(), guard);
                bucket = current_array.bucket_mut(index);
            }

            let lock_result = Locker::try_lock(bucket, guard).ok()?;
            if let Some(locker) = lock_result {
                let data_block_mut = current_array.data_block_mut(index);
                let entry_ptr = locker.get_entry_ptr(
                    data_block_mut,
                    key,
                    BucketArray::<K, V, L, TYPE>::partial_hash(hash),
                    guard,
                );
                return Some(LockedEntry::new(
                    locker,
                    data_block_mut,
                    entry_ptr,
                    index,
                    guard,
                ));
            }

            // Reaching here means that `self.bucket_array()` has been updated.
        }
    }

    /// Moves an entry in the old array to the current one.
    ///
    /// Returns `true` if no old array is attached to the current one.
    #[inline]
    fn move_entry<Q, D>(
        &self,
        current_array: &BucketArray<K, V, L, TYPE>,
        old_array: &BucketArray<K, V, L, TYPE>,
        hash: u64,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<bool, ()>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        if !self.incremental_rehash::<Q, D, false>(current_array, async_wait, guard)? {
            let index = old_array.calculate_bucket_index(hash);
            let bucket = old_array.bucket_mut(index);
            let lock_result = if let Some(async_wait) = async_wait.derive() {
                Locker::try_lock_or_wait(bucket, async_wait, guard)?
            } else {
                Locker::lock(bucket, guard)
            };
            if let Some(mut locker) = lock_result {
                self.relocate_bucket::<Q, _, false>(
                    current_array,
                    old_array,
                    index,
                    &mut locker,
                    async_wait,
                    guard,
                )?;
            }
            return Ok(false);
        }
        Ok(true)
    }

    /// Relocates the bucket to the current bucket array.
    ///
    /// Returns an error if locking failed.
    #[inline]
    fn relocate_bucket<Q, D, const TRY_LOCK: bool>(
        &self,
        current_array: &BucketArray<K, V, L, TYPE>,
        old_array: &BucketArray<K, V, L, TYPE>,
        old_index: usize,
        old_locker: &mut Locker<K, V, L, TYPE>,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<(), ()>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        debug_assert!(!old_locker.killed());
        if old_locker.num_entries() != 0 {
            let target_index = if old_array.num_buckets() >= current_array.num_buckets() {
                let ratio = old_array.num_buckets() / current_array.num_buckets();
                old_index / ratio
            } else {
                let ratio = current_array.num_buckets() / old_array.num_buckets();
                debug_assert!(ratio <= BUCKET_LEN);
                old_index * ratio
            };

            let mut target_buckets: [Option<Locker<K, V, L, TYPE>>; MAX_RESIZE_FACTOR] =
                Default::default();
            let mut max_index = 0;
            let mut entry_ptr = EntryPtr::new(guard);
            let old_data_block_mut = old_array.data_block_mut(old_index);
            while entry_ptr.move_to_next(old_locker, guard) {
                let old_entry = entry_ptr.get(old_data_block_mut);
                let (new_index, partial_hash) =
                    if old_array.num_buckets() >= current_array.num_buckets() {
                        debug_assert_eq!(
                            current_array.calculate_bucket_index(self.hash(&old_entry.0)),
                            target_index
                        );
                        (target_index, entry_ptr.partial_hash(&*old_locker))
                    } else {
                        let hash = self.hash(&old_entry.0);
                        let new_index = current_array.calculate_bucket_index(hash);
                        debug_assert!(
                            new_index - target_index
                                < (current_array.num_buckets() / old_array.num_buckets())
                        );
                        let partial_hash = BucketArray::<K, V, L, TYPE>::partial_hash(hash);
                        (new_index, partial_hash)
                    };

                while max_index <= new_index - target_index {
                    let target_bucket = current_array.bucket_mut(max_index + target_index);
                    let locker = unsafe {
                        if TRY_LOCK {
                            Locker::try_lock(target_bucket, guard)?.unwrap_unchecked()
                        } else if let Some(async_wait) = async_wait.derive() {
                            Locker::try_lock_or_wait(target_bucket, async_wait, guard)?
                                .unwrap_unchecked()
                        } else {
                            Locker::lock(target_bucket, guard).unwrap_unchecked()
                        }
                    };
                    target_buckets[max_index].replace(locker);
                    max_index += 1;
                }

                let target_bucket = unsafe {
                    target_buckets[new_index - target_index]
                        .as_mut()
                        .unwrap_unchecked()
                };

                let entry_clone = Self::try_clone(old_entry);
                target_bucket.insert_with(
                    current_array.data_block_mut(new_index),
                    partial_hash,
                    || {
                        // Stack unwinding during a call to `insert` will result in the entry being
                        // removed from the map, any map entry modification should take place after all
                        // the memory is reserved.
                        entry_clone.unwrap_or_else(|| {
                            old_locker.extract(old_data_block_mut, &mut entry_ptr, guard)
                        })
                    },
                    guard,
                );

                if TYPE == OPTIMISTIC {
                    // In order for readers that have observed the following erasure to see the above
                    // insertion, a `Release` fence is needed.
                    fence(Release);
                    old_locker.mark_removed(&mut entry_ptr, guard);
                }
            }
        }
        old_locker.kill();
        Ok(())
    }

    /// Clears the old array.
    fn clear_old_array(&self, current_array: &BucketArray<K, V, L, TYPE>, guard: &Guard) {
        while current_array.has_old_array() {
            if self.incremental_rehash::<K, _, false>(current_array, &mut (), guard) == Ok(true) {
                break;
            }
        }
    }

    /// Relocates a fixed number of buckets from the old array to the current array.
    ///
    /// Returns `true` if `old_array` is null.
    fn incremental_rehash<Q, D, const TRY_LOCK: bool>(
        &self,
        current_array: &BucketArray<K, V, L, TYPE>,
        async_wait: &mut D,
        guard: &Guard,
    ) -> Result<bool, ()>
    where
        Q: Equivalent<K> + Hash + ?Sized,
        D: DeriveAsyncWait,
    {
        if let Some(old_array) = current_array.old_array(guard).as_ref() {
            // Assign itself a range of `Bucket` instances to rehash.
            //
            // Aside from the range, it increments the implicit reference counting field in
            // `old_array.rehashing`.
            let rehashing_metadata = old_array.rehashing_metadata();
            let mut current = rehashing_metadata.load(Relaxed);
            loop {
                if current >= old_array.num_buckets()
                    || (current & (BUCKET_LEN - 1)) == BUCKET_LEN - 1
                {
                    // Only `BUCKET_LEN - 1` threads are allowed to rehash a `Bucket` at a moment.
                    return Ok(!current_array.has_old_array());
                }
                match rehashing_metadata.compare_exchange_weak(
                    current,
                    current + BUCKET_LEN + 1,
                    Relaxed,
                    Relaxed,
                ) {
                    Ok(_) => {
                        current &= !(BUCKET_LEN - 1);
                        break;
                    }
                    Err(result) => current = result,
                }
            }

            // The guard ensures dropping one reference in `old_array.rehashing`.
            let mut rehashing_guard = ExitGuard::new((current, false), |(prev, success)| {
                if success {
                    // Keep the index as it is.
                    let current = rehashing_metadata.fetch_sub(1, Relaxed) - 1;
                    if (current & (BUCKET_LEN - 1) == 0) && current >= old_array.num_buckets() {
                        // The last one trying to relocate old entries gets rid of the old array.
                        current_array.drop_old_array();
                    }
                } else {
                    // On failure, `rehashing` reverts to its previous state.
                    let mut current = rehashing_metadata.load(Relaxed);
                    loop {
                        let new = if current <= prev {
                            current - 1
                        } else {
                            let ref_cnt = current & (BUCKET_LEN - 1);
                            prev | (ref_cnt - 1)
                        };
                        match rehashing_metadata
                            .compare_exchange_weak(current, new, Relaxed, Relaxed)
                        {
                            Ok(_) => break,
                            Err(actual) => current = actual,
                        }
                    }
                }
            });

            for index in current..(current + BUCKET_LEN).min(old_array.num_buckets()) {
                let old_bucket = old_array.bucket_mut(index);
                let lock_result = if TRY_LOCK {
                    Locker::try_lock(old_bucket, guard)?
                } else if let Some(async_wait) = async_wait.derive() {
                    Locker::try_lock_or_wait(old_bucket, async_wait, guard)?
                } else {
                    Locker::lock(old_bucket, guard)
                };
                if let Some(mut locker) = lock_result {
                    self.relocate_bucket::<Q, D, TRY_LOCK>(
                        current_array,
                        old_array,
                        index,
                        &mut locker,
                        async_wait,
                        guard,
                    )?;
                }
            }

            // Successfully rehashed all the assigned buckets.
            rehashing_guard.1 = true;
        }
        Ok(!current_array.has_old_array())
    }

    /// Tries to enlarge the array if the estimated load factor is greater than `7/8`.
    #[inline]
    fn try_enlarge(
        &self,
        current_array: &BucketArray<K, V, L, TYPE>,
        index: usize,
        mut num_entries: usize,
        guard: &Guard,
    ) {
        let sample_size = current_array.sample_size();
        let threshold = sample_size * (BUCKET_LEN / 8) * 7;
        if num_entries > threshold
            || (1..sample_size).any(|i| {
                num_entries += current_array
                    .bucket((index + i) % current_array.num_buckets())
                    .num_entries();
                num_entries > threshold
            })
        {
            self.try_resize(index, guard);
        }
    }

    /// Tries to shrink the hash table to fit the estimated number of entries, or rebuild it to
    /// optimize the storage.
    #[inline]
    fn try_shrink_or_rebuild(
        &self,
        current_array: &BucketArray<K, V, L, TYPE>,
        index: usize,
        guard: &Guard,
    ) {
        debug_assert!(!current_array.has_old_array());

        if !cfg!(miri)
            && (current_array.num_entries()
                > self.minimum_capacity().load(Relaxed).next_power_of_two()
                || TYPE == OPTIMISTIC)
        {
            let sample_size = current_array.sample_size();
            let shrink_threshold = sample_size * BUCKET_LEN / 16;
            let rebuild_threshold = sample_size / 2;
            let mut num_entries = 0;
            let mut num_buckets_to_rebuild = 0;
            for i in 0..sample_size {
                let bucket = current_array.bucket((index + i) % current_array.num_buckets());
                num_entries += bucket.num_entries();
                if num_entries >= shrink_threshold
                    && (TYPE != OPTIMISTIC
                        || num_buckets_to_rebuild + (sample_size - i) < rebuild_threshold)
                {
                    // Early exit.
                    return;
                }
                if TYPE == OPTIMISTIC && bucket.need_rebuild() {
                    if num_buckets_to_rebuild >= rebuild_threshold {
                        self.try_resize(index, guard);
                        return;
                    }
                    num_buckets_to_rebuild += 1;
                }
            }
            if TYPE != OPTIMISTIC || num_entries < shrink_threshold {
                self.try_resize(index, guard);
            }
        }
    }

    /// Tries to resize the array.
    fn try_resize(&self, sampling_index: usize, guard: &Guard) {
        let current_array_ptr = self.bucket_array().load(Acquire, guard);
        if current_array_ptr.tag() != Tag::None {
            // Another thread is currently allocating a new bucket array.
            return;
        }

        if let Some(current_array) = current_array_ptr.as_ref() {
            if current_array.has_old_array() {
                // The hash table cannot be resized with an old array attached to it.
                return;
            }

            // The resizing policies are as follows.
            //  - `The estimated load factor >= 7/8`, then the hash table grows up to `32x`.
            //  - `The estimated load factor <= 1/16`, then the hash table shrinks to fit.
            let minimum_capacity = self.minimum_capacity().load(Relaxed);
            let capacity = current_array.num_entries();
            let sample_size = current_array.full_sample_size();
            let estimated_num_entries = Self::sample(current_array, sampling_index, sample_size);
            let new_capacity = if estimated_num_entries >= (capacity / 8) * 7 {
                if capacity == self.maximum_capacity() {
                    // Do not resize if the capacity cannot be increased.
                    capacity
                } else {
                    let mut new_capacity = capacity;
                    while new_capacity <= (estimated_num_entries / 8) * 15 {
                        // Double `new_capacity` until the expected load factor is below 0.5.
                        if new_capacity == self.maximum_capacity() {
                            break;
                        }
                        if new_capacity / capacity == MAX_RESIZE_FACTOR {
                            break;
                        }
                        new_capacity *= 2;
                    }
                    new_capacity
                }
            } else if estimated_num_entries <= capacity / 16 {
                // Shrink to fit.
                estimated_num_entries
                    .max(minimum_capacity)
                    .max(BucketArray::<K, V, L, TYPE>::minimum_capacity())
                    .next_power_of_two()
            } else {
                capacity
            };

            let try_resize = new_capacity != capacity;
            let try_drop_table = estimated_num_entries == 0 && minimum_capacity == 0;
            let try_rebuild = TYPE == OPTIMISTIC
                && !try_resize
                && Self::check_rebuild(current_array, sampling_index, sample_size);

            if try_resize || try_drop_table || try_rebuild {
                // Mark that the thread may allocate a new array to prevent multiple threads from
                // allocating bucket arrays at the same time.
                if !self.bucket_array().update_tag_if(
                    Tag::First,
                    |ptr| ptr == current_array_ptr,
                    Relaxed,
                    Relaxed,
                ) {
                    // The bucket array is being replaced with a new one.
                    return;
                }

                if try_drop_table {
                    // Try to drop the hash table with all the buckets read-locked if empty.
                    let mut reader_guard = ExitGuard::new(
                        (current_array.num_buckets(), true),
                        |(num_locked_buckets, success): (usize, bool)| {
                            for i in 0..num_locked_buckets {
                                let bucket = current_array.bucket_mut(i);
                                if success {
                                    bucket.kill();
                                }
                                Reader::release(bucket);
                            }
                        },
                    );

                    if !(0..current_array.num_buckets()).any(|i| {
                        if let Ok(Some(reader)) = Reader::try_lock(current_array.bucket(i), guard) {
                            if reader.num_entries() == 0 {
                                // The bucket will be unlocked later.
                                std::mem::forget(reader);
                                return false;
                            }
                        }
                        reader_guard.0 = i;
                        reader_guard.1 = false;
                        true
                    }) {
                        // All the buckets are empty and locked.
                        self.bucket_array().swap((None, Tag::None), Relaxed);
                        return;
                    }
                }

                let allocated_array: Option<Shared<BucketArray<K, V, L, TYPE>>> = None;
                let mut mutex_guard = ExitGuard::new(allocated_array, |allocated_array| {
                    if let Some(allocated_array) = allocated_array {
                        // A new array was allocated.
                        self.bucket_array()
                            .swap((Some(allocated_array), Tag::None), Release);
                    } else {
                        // Release the lock.
                        self.bucket_array()
                            .update_tag_if(Tag::None, |_| true, Relaxed, Relaxed);
                    }
                });
                if try_resize || try_rebuild {
                    mutex_guard.replace(unsafe {
                        Shared::new_unchecked(BucketArray::<K, V, L, TYPE>::new(
                            new_capacity,
                            self.bucket_array().clone(Relaxed, guard),
                        ))
                    });
                }
            }
        }
    }

    // Returns an estimated required size of the container based on the size hint.
    fn capacity_from_size_hint(size_hint: (usize, Option<usize>)) -> usize {
        // A resize can be triggered when the load factor reaches ~80%.
        (size_hint
            .1
            .unwrap_or(size_hint.0)
            .min(1_usize << (usize::BITS - 2))
            / 4)
            * 5
    }

    /// Returns a reference to the specified [`Guard`] whose lifetime matches that of `self`.
    fn prolonged_guard_ref<'h>(&'h self, guard: &Guard) -> &'h Guard {
        let _: &Self = self;
        unsafe { std::mem::transmute::<&Guard, &'h Guard>(guard) }
    }
}

/// [`LockedEntry`] comprises pieces of data that are required for exclusive access to an entry.
pub(super) struct LockedEntry<'h, K, V, L: LruList, const TYPE: char> {
    /// The [`Locker`] holding the exclusive lock on the bucket.
    pub(super) locker: Locker<'h, K, V, L, TYPE>,

    /// The [`DataBlock`] that may contain desired entry data.
    pub(super) data_block_mut: &'h mut DataBlock<K, V, BUCKET_LEN>,

    /// [`EntryPtr`] pointing to the actual entry in the bucket.
    pub(super) entry_ptr: EntryPtr<'h, K, V, TYPE>,

    /// The index in the bucket array.
    pub(super) index: usize,
}

impl<'h, K: Eq + Hash + 'h, V: 'h, L: LruList, const TYPE: char> LockedEntry<'h, K, V, L, TYPE> {
    /// Creates a new [`LockedEntry`].
    pub(super) fn new(
        mut locker: Locker<'h, K, V, L, TYPE>,
        data_block_mut: &'h mut DataBlock<K, V, BUCKET_LEN>,
        entry_ptr: EntryPtr<'h, K, V, TYPE>,
        index: usize,
        guard: &Guard,
    ) -> LockedEntry<'h, K, V, L, TYPE> {
        if TYPE == OPTIMISTIC {
            locker.drop_removed_unreachable_entries(data_block_mut, guard);
        }
        LockedEntry {
            locker,
            data_block_mut,
            entry_ptr,
            index,
        }
    }

    /// Gets the first occupied entry.
    pub(super) async fn first_entry_async<H: BuildHasher, T: HashTable<K, V, H, L, TYPE>>(
        hash_table: &'h T,
    ) -> Option<LockedEntry<'h, K, V, L, TYPE>> {
        let mut current_array_holder = hash_table.bucket_array().get_shared(Acquire, &Guard::new());
        while let Some(current_array) = current_array_holder.take() {
            while current_array.has_old_array() {
                let mut async_wait = AsyncWait::default();
                let mut async_wait_pinned = Pin::new(&mut async_wait);
                if hash_table.incremental_rehash::<K, _, false>(
                    current_array.as_ref(),
                    &mut async_wait_pinned,
                    &Guard::new(),
                ) == Ok(true)
                {
                    break;
                }
                async_wait_pinned.await;
            }
            for index in 0..current_array.num_buckets() {
                loop {
                    let mut async_wait = AsyncWait::default();
                    let mut async_wait_pinned = Pin::new(&mut async_wait);
                    {
                        let guard = Guard::new();
                        let prolonged_guard = hash_table.prolonged_guard_ref(&guard);
                        let prolonged_current_array =
                            current_array.get_guarded_ref(prolonged_guard);
                        let bucket = prolonged_current_array.bucket_mut(index);
                        if let Ok(locker) = Locker::try_lock_or_wait(
                            bucket,
                            &mut async_wait_pinned,
                            prolonged_guard,
                        ) {
                            if let Some(locker) = locker {
                                let data_block_mut = prolonged_current_array.data_block_mut(index);
                                let mut entry_ptr = EntryPtr::new(prolonged_guard);
                                if entry_ptr.move_to_next(&locker, prolonged_guard) {
                                    return Some(LockedEntry::new(
                                        locker,
                                        data_block_mut,
                                        entry_ptr,
                                        index,
                                        &guard,
                                    ));
                                }
                            }
                            break;
                        };
                    }
                    async_wait_pinned.await;
                }
            }

            if let Some(new_current_array) =
                hash_table.bucket_array().get_shared(Acquire, &Guard::new())
            {
                if new_current_array.as_ptr() == current_array.as_ptr() {
                    break;
                }
                current_array_holder.replace(new_current_array);
                continue;
            }
            break;
        }

        None
    }

    /// Returns a [`LockedEntry`] owning the next entry.
    pub(super) fn next<H: BuildHasher, T: HashTable<K, V, H, L, TYPE>>(
        mut self,
        hash_table: &'h T,
    ) -> Option<Self> {
        let guard = Guard::new();
        let prolonged_guard = hash_table.prolonged_guard_ref(&guard);

        if self.entry_ptr.move_to_next(
            &self.locker,
            hash_table.prolonged_guard_ref(prolonged_guard),
        ) {
            return Some(self);
        }

        let current_array_ptr = hash_table.bucket_array().load(Acquire, prolonged_guard);
        if let Some(current_array) = current_array_ptr.as_ref() {
            if current_array.has_old_array() {
                drop(self);
                return hash_table.lock_first_entry(prolonged_guard);
            }

            let prev_index = self.index;
            let try_shrink_or_rebuild = (self.locker.num_entries() <= 1
                || self.locker.need_rebuild())
                && current_array.initiate_sampling(prev_index);
            drop(self);

            if try_shrink_or_rebuild {
                hash_table.try_shrink_or_rebuild(current_array, prev_index, &guard);
            }

            for index in (prev_index + 1)..current_array.num_buckets() {
                let bucket = current_array.bucket_mut(index);
                if let Some(locker) = Locker::lock(bucket, prolonged_guard) {
                    let data_block_mut = current_array.data_block_mut(index);
                    let mut entry_ptr = EntryPtr::new(prolonged_guard);
                    if entry_ptr.move_to_next(&locker, prolonged_guard) {
                        return Some(LockedEntry::new(
                            locker,
                            data_block_mut,
                            entry_ptr,
                            index,
                            &guard,
                        ));
                    }
                }
            }

            let new_current_array_ptr = hash_table.bucket_array().load(Relaxed, prolonged_guard);
            if current_array_ptr.without_tag() != new_current_array_ptr.without_tag() {
                return hash_table.lock_first_entry(prolonged_guard);
            }
        }

        None
    }

    /// Returns a [`LockedEntry`] owning the next entry.
    pub(super) async fn next_async<H: BuildHasher, T: HashTable<K, V, H, L, TYPE>>(
        mut self,
        hash_table: &'h T,
    ) -> Option<LockedEntry<'h, K, V, L, TYPE>> {
        if self
            .entry_ptr
            .move_to_next(&self.locker, hash_table.prolonged_guard_ref(&Guard::new()))
        {
            return Some(self);
        }

        let mut current_array_holder = hash_table.bucket_array().get_shared(Acquire, &Guard::new());
        if let Some(current_array) = current_array_holder {
            if current_array.has_old_array() {
                drop(self);
                return Self::first_entry_async(hash_table).await;
            }

            let prev_index = self.index;
            let try_shrink_or_rebuild = (self.locker.num_entries() <= 1
                || self.locker.need_rebuild())
                && current_array.initiate_sampling(prev_index);
            drop(self);

            if try_shrink_or_rebuild {
                hash_table.try_shrink_or_rebuild(&current_array, prev_index, &Guard::new());
            }

            for index in (prev_index + 1)..current_array.num_buckets() {
                loop {
                    let mut async_wait = AsyncWait::default();
                    let mut async_wait_pinned = Pin::new(&mut async_wait);
                    {
                        let guard = Guard::new();
                        let prolonged_guard = hash_table.prolonged_guard_ref(&guard);
                        let prolonged_current_array =
                            current_array.get_guarded_ref(prolonged_guard);
                        let bucket = prolonged_current_array.bucket_mut(index);
                        if let Ok(locker) = Locker::try_lock_or_wait(
                            bucket,
                            &mut async_wait_pinned,
                            prolonged_guard,
                        ) {
                            if let Some(locker) = locker {
                                let data_block_mut = prolonged_current_array.data_block_mut(index);
                                let mut entry_ptr = EntryPtr::new(prolonged_guard);
                                if entry_ptr.move_to_next(&locker, prolonged_guard) {
                                    return Some(Self {
                                        locker,
                                        data_block_mut,
                                        entry_ptr,
                                        index,
                                    });
                                }
                            }
                            break;
                        };
                    }
                    async_wait_pinned.await;
                }
            }

            current_array_holder = hash_table.bucket_array().get_shared(Relaxed, &Guard::new());
            if let Some(new_current_array) = current_array_holder {
                if new_current_array.as_ptr() != current_array.as_ptr() {
                    return Self::first_entry_async(hash_table).await;
                }
            }
        }
        None
    }
}

/// [`LockedEntry`] is safe to be sent across threads and awaits as long as the entry is.
unsafe impl<K: Eq + Hash + Send, V: Send, L: LruList, const TYPE: char> Send
    for LockedEntry<'_, K, V, L, TYPE>
{
}
