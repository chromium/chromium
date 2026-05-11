use super::bucket::{Bucket, DataBlock, LruList, BUCKET_LEN, OPTIMISTIC};
use crate::ebr::{AtomicShared, Guard, Ptr, Tag};
use std::alloc::{alloc, alloc_zeroed, dealloc, Layout};
use std::mem::{align_of, needs_drop, size_of};
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::Relaxed;

/// [`BucketArray`] is a special purpose array to manage [`Bucket`] and [`DataBlock`].
pub struct BucketArray<K, V, L: LruList, const TYPE: char> {
    bucket_ptr: *mut Bucket<K, V, L, TYPE>,
    data_block_ptr: *mut DataBlock<K, V, BUCKET_LEN>,
    array_len: usize,
    hash_offset: u32,
    sample_size_mask: u16,
    bucket_ptr_offset: u16,
    old_array: AtomicShared<BucketArray<K, V, L, TYPE>>,
    num_cleared_buckets: AtomicUsize,
}

impl<K, V, L: LruList, const TYPE: char> BucketArray<K, V, L, TYPE> {
    /// Returns the number of [`Bucket`] instances in the [`BucketArray`].
    #[inline]
    pub(crate) const fn num_buckets(&self) -> usize {
        self.array_len
    }

    /// Returns a reference to a [`Bucket`] at the given position.
    #[inline]
    pub(crate) fn bucket(&self, index: usize) -> &Bucket<K, V, L, TYPE> {
        debug_assert!(index < self.num_buckets());
        unsafe { &(*(self.bucket_ptr.add(index))) }
    }

    /// Returns a mutable reference to a [`Bucket`] at the given position.
    #[allow(clippy::mut_from_ref)]
    #[inline]
    pub(crate) fn bucket_mut(&self, index: usize) -> &mut Bucket<K, V, L, TYPE> {
        debug_assert!(index < self.num_buckets());
        unsafe { &mut *self.bucket_ptr.add(index) }
    }

    /// Returns a mutable reference to a [`DataBlock`] at the given position.
    #[inline]
    pub(crate) fn data_block(&self, index: usize) -> &DataBlock<K, V, BUCKET_LEN> {
        debug_assert!(index < self.num_buckets());
        unsafe { &*self.data_block_ptr.add(index) }
    }

    /// Returns a mutable reference to a [`DataBlock`] at the given position.
    #[allow(clippy::mut_from_ref)]
    #[inline]
    pub(crate) fn data_block_mut(&self, index: usize) -> &mut DataBlock<K, V, BUCKET_LEN> {
        debug_assert!(index < self.num_buckets());
        unsafe { &mut *self.data_block_ptr.add(index) }
    }

    /// Calculates the layout of the memory block for an array of `T`.
    const fn calculate_memory_layout<T: Sized>(array_len: usize) -> (usize, usize, Layout) {
        let size_of_t = size_of::<T>();
        let aligned_size = size_of_t.next_power_of_two();
        let allocation_size = aligned_size + array_len * size_of_t;
        (size_of_t, allocation_size, unsafe {
            // Intentionally misaligned in order to take full advantage of demand paging.
            Layout::from_size_align_unchecked(allocation_size, 1)
        })
    }
}

impl<K, V, L: LruList, const TYPE: char> BucketArray<K, V, L, TYPE> {
    /// Creates a new [`BucketArray`] of the given capacity.
    ///
    /// `capacity` is the desired number entries, not the number of [`Bucket`] instances.
    pub(crate) fn new(
        capacity: usize,
        old_array: AtomicShared<BucketArray<K, V, L, TYPE>>,
    ) -> Self {
        let log2_array_len = Self::calculate_log2_array_size(capacity);
        assert_ne!(log2_array_len, 0);

        let array_len = 1_usize << log2_array_len;
        unsafe {
            let (bucket_size, bucket_array_allocation_size, bucket_array_layout) =
                Self::calculate_memory_layout::<Bucket<K, V, L, TYPE>>(array_len);
            let bucket_array_ptr = alloc_zeroed(bucket_array_layout);
            assert!(
                !bucket_array_ptr.is_null(),
                "memory allocation failure: {bucket_array_allocation_size} bytes",
            );
            let bucket_array_ptr_offset = bucket_size.next_power_of_two()
                - (bucket_array_ptr as usize % bucket_size.next_power_of_two());
            assert!(
                bucket_array_ptr_offset + bucket_size * array_len <= bucket_array_allocation_size,
            );
            assert_eq!(
                (bucket_array_ptr as usize + bucket_array_ptr_offset)
                    % bucket_size.next_power_of_two(),
                0
            );

            #[allow(clippy::cast_ptr_alignment)]
            let bucket_array_ptr =
                bucket_array_ptr
                    .add(bucket_array_ptr_offset)
                    .cast::<Bucket<K, V, L, TYPE>>();
            #[allow(clippy::cast_possible_truncation)]
            let bucket_array_ptr_offset = bucket_array_ptr_offset as u16;

            let data_block_array_layout = Layout::from_size_align(
                size_of::<DataBlock<K, V, BUCKET_LEN>>() * array_len,
                align_of::<[DataBlock<K, V, BUCKET_LEN>; 0]>(),
            )
            .unwrap();

            let data_block_array_ptr =
                alloc(data_block_array_layout).cast::<DataBlock<K, V, BUCKET_LEN>>();
            assert!(
                !data_block_array_ptr.is_null(),
                "memory allocation failure: {} bytes",
                data_block_array_layout.size(),
            );

            let sample_size_mask = u16::from(log2_array_len).next_power_of_two() - 1;

            Self {
                bucket_ptr: bucket_array_ptr,
                data_block_ptr: data_block_array_ptr,
                array_len,
                hash_offset: 64 - u32::from(log2_array_len),
                sample_size_mask,
                bucket_ptr_offset: bucket_array_ptr_offset,
                old_array,
                num_cleared_buckets: AtomicUsize::new(0),
            }
        }
    }

    /// Returns the number of total entries.
    #[inline]
    pub(crate) const fn num_entries(&self) -> usize {
        self.array_len * BUCKET_LEN
    }

    /// Calculates the [`Bucket`] index for the hash value.
    #[allow(clippy::cast_possible_truncation)]
    #[inline]
    pub(crate) const fn calculate_bucket_index(&self, hash: u64) -> usize {
        // Take the upper n-bits to make sure that a single bucket is spread across a few adjacent
        // buckets when the hash table is resized.
        (hash >> self.hash_offset) as usize
    }

    /// Returns the minimum capacity.
    #[inline]
    pub const fn minimum_capacity() -> usize {
        BUCKET_LEN << 1
    }

    /// Returns the partial hash value of the given hash.
    #[allow(clippy::cast_possible_truncation)]
    #[inline]
    pub(crate) const fn partial_hash(hash: u64) -> u8 {
        (hash & (u8::MAX as u64)) as u8
    }

    /// Returns a reference to its rehashing metadata.
    #[inline]
    pub(crate) const fn rehashing_metadata(&self) -> &AtomicUsize {
        &self.num_cleared_buckets
    }

    /// Checks if the bucket is allowed to initiate sampling.
    #[inline]
    pub(crate) const fn initiate_sampling(&self, index: usize) -> bool {
        (index & self.sample_size_mask as usize) == 0
    }

    /// Returns the recommended sampling size.
    #[inline]
    pub(crate) const fn sample_size(&self) -> usize {
        (self.sample_size_mask + 1) as usize
    }

    /// Returns the recommended sampling size.
    #[inline]
    pub(crate) fn full_sample_size(&self) -> usize {
        (self.sample_size() * self.sample_size()).min(self.num_buckets())
    }

    /// Returns a [`Ptr`] to the old array.
    #[inline]
    pub(crate) fn has_old_array(&self) -> bool {
        !self.old_array.is_null(Relaxed)
    }

    /// Returns a [`Ptr`] to the old array.
    #[inline]
    pub(crate) fn old_array<'g>(&self, guard: &'g Guard) -> Ptr<'g, BucketArray<K, V, L, TYPE>> {
        self.old_array.load(Relaxed, guard)
    }

    /// Drops the old array.
    #[inline]
    pub(crate) fn drop_old_array(&self) {
        self.old_array.swap((None, Tag::None), Relaxed).0.map(|a| {
            // It is OK to pass the old array instance to the garbage collector, deferring destruction.
            debug_assert_eq!(
                a.num_cleared_buckets.load(Relaxed),
                a.array_len.max(BUCKET_LEN)
            );
            a.release()
        });
    }

    /// Calculates `log_2` of the array size from the given capacity.
    ///
    /// Returns a non-zero `u8`, even when `capacity < 2 * BUCKET_LEN`.
    #[allow(clippy::cast_possible_truncation)]
    fn calculate_log2_array_size(capacity: usize) -> u8 {
        let adjusted_capacity = capacity.clamp(64, (usize::MAX / 2) - (BUCKET_LEN - 1));
        let required_buckets =
            ((adjusted_capacity + BUCKET_LEN - 1) / BUCKET_LEN).next_power_of_two();
        let log2_capacity = usize::BITS as usize - (required_buckets.leading_zeros() as usize) - 1;

        // `2^log2_capacity * BUCKET_LEN >= capacity`.
        debug_assert!(log2_capacity < (usize::BITS as usize));
        debug_assert!((1_usize << log2_capacity) * BUCKET_LEN >= adjusted_capacity);
        log2_capacity as u8
    }
}

impl<K, V, L: LruList, const TYPE: char> Drop for BucketArray<K, V, L, TYPE> {
    fn drop(&mut self) {
        if TYPE != OPTIMISTIC && !self.old_array.is_null(Relaxed) {
            // The `BucketArray` should be dropped immediately.
            unsafe {
                self.old_array
                    .swap((None, Tag::None), Relaxed)
                    .0
                    .map(|a| a.drop_in_place());
            }
        }

        let num_cleared_buckets = if TYPE == OPTIMISTIC && needs_drop::<(K, V)>() {
            // No instances are dropped when the array is reachable.
            0
        } else {
            // `LinkedBucket` instances should be cleaned up.
            self.num_cleared_buckets.load(Relaxed)
        };

        if num_cleared_buckets < self.array_len {
            for index in num_cleared_buckets..self.array_len {
                self.bucket_mut(index)
                    .drop_entries(self.data_block_mut(index));
            }
        }

        unsafe {
            dealloc(
                self.bucket_ptr
                    .cast::<u8>()
                    .sub(self.bucket_ptr_offset as usize),
                Self::calculate_memory_layout::<Bucket<K, V, L, TYPE>>(self.array_len).2,
            );
            dealloc(
                self.data_block_ptr.cast::<u8>(),
                Layout::from_size_align(
                    size_of::<DataBlock<K, V, BUCKET_LEN>>() * self.array_len,
                    align_of::<[DataBlock<K, V, BUCKET_LEN>; 0]>(),
                )
                .unwrap(),
            );
        }
    }
}

unsafe impl<K: Send, V: Send, L: LruList, const TYPE: char> Send for BucketArray<K, V, L, TYPE> {}
unsafe impl<K: Send + Sync, V: Send + Sync, L: LruList, const TYPE: char> Sync
    for BucketArray<K, V, L, TYPE>
{
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod test {
    use super::*;

    use std::time::Instant;

    #[test]
    fn alloc() {
        let start = Instant::now();
        let array: BucketArray<usize, usize, (), OPTIMISTIC> =
            BucketArray::new(1024 * 1024 * 32, AtomicShared::default());
        assert_eq!(array.num_buckets(), 1024 * 1024);
        let after_alloc = Instant::now();
        println!("allocation took {:?}", after_alloc - start);
        array.num_cleared_buckets.store(array.array_len, Relaxed);
        drop(array);
        let after_dealloc = Instant::now();
        println!("de-allocation took {:?}", after_dealloc - after_alloc);
    }

    #[test]
    fn array() {
        for s in 0..BUCKET_LEN * 4 {
            let array: BucketArray<usize, usize, (), OPTIMISTIC> =
                BucketArray::new(s, AtomicShared::default());
            assert!(
                array.num_buckets() >= (s.max(1) + BUCKET_LEN - 1) / BUCKET_LEN,
                "{s} {}",
                array.num_buckets()
            );
            assert!(
                array.num_buckets() <= 2 * (s.max(1) + BUCKET_LEN - 1) / BUCKET_LEN,
                "{s} {}",
                array.num_buckets()
            );
            assert!(
                array.full_sample_size() <= array.num_buckets(),
                "{} {}",
                array.full_sample_size(),
                array.num_buckets()
            );
            assert!(array.num_entries() >= s, "{s} {}", array.num_entries());
            array.num_cleared_buckets.store(array.array_len, Relaxed);
        }
    }
}
