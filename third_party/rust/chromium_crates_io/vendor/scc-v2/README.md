# Scalable Concurrent Containers

[![Cargo](https://img.shields.io/crates/v/scc)](https://crates.io/crates/scc)
![Crates.io](https://img.shields.io/crates/l/scc)
![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/wvwwvwwv/scalable-concurrent-containers/scc.yml?branch=main)

A collection of high-performance containers and utilities for concurrent and asynchronous programming.

#### Features

- Asynchronous counterparts of blocking and synchronous methods.
- [`Equivalent`](https://github.com/indexmap-rs/equivalent), [`Loom`](https://github.com/tokio-rs/loom) and [`Serde`](https://github.com/serde-rs/serde) support: `features = ["equivalent", "loom", "serde"]`.
- Near-linear scalability.
- No spin-locks and no busy loops.
- SIMD lookup to scan multiple entries in parallel: require `RUSTFLAGS='-C target_feature=+avx2'` on `x86_64`.

#### Concurrent and Asynchronous Containers

- [`HashMap`](#hashmap) is a concurrent and asynchronous hash map.
- [`HashSet`](#hashset) is a concurrent and asynchronous hash set.
- [`HashIndex`](#hashindex) is a read-optimized concurrent and asynchronous hash map.
- [`HashCache`](#hashcache) is a 32-way associative cache backed by [`HashMap`](#hashmap).
- [`TreeIndex`](#treeindex) is a read-optimized concurrent and asynchronous B-plus tree.

#### Utilities for Concurrent Programming

- [`LinkedList`](#linkedlist) is a type trait implementing a lock-free concurrent singly linked list.
- [`Queue`](#queue) is a concurrent lock-free first-in-first-out container.
- [`Stack`](#stack) is a concurrent lock-free last-in-first-out container.
- [`Bag`](#bag) is a concurrent lock-free unordered opaque container.

## `HashMap`

[`HashMap`](#hashmap) is a concurrent hash map optimized for highly parallel write-heavy workloads. [`HashMap`](#hashmap) is structured as a lock-free stack of entry bucket arrays. The entry bucket array is managed by [`sdd`](https://crates.io/crates/sdd), thus enabling lock-free access to it and non-blocking container resizing. Each bucket is a fixed-size array of entries, protected by a read-write lock that simultaneously provides blocking and asynchronous methods.

### Locking behavior

#### Entry access: fine-grained locking

Read/write access to an entry is serialized by the read-write lock in the bucket containing the entry. There are no container-level locks; therefore, the larger the container gets, the lower the chance of the bucket-level lock being contended.

#### Resize: lock-free

Resizing of a [`HashMap`](#hashmap) is entirely non-blocking and lock-free; resizing does not block any other read/write access to the container or resizing attempts. _Resizing is analogous to pushing a new bucket array into a lock-free stack_. Each entry in the old bucket array will be incrementally relocated to the new bucket array upon future access to the container, and the old bucket array will eventually be dropped after it becomes empty.

### Examples

If the key is unique, an entry can be inserted. The inserted entry can be updated, read, and removed synchronously or asynchronously.

```rust
use scc::HashMap;

let hashmap: HashMap<u64, u32> = HashMap::default();

assert!(hashmap.insert(1, 0).is_ok());
assert!(hashmap.insert(1, 1).is_err());
assert_eq!(hashmap.upsert(1, 1).unwrap(), 0);
assert_eq!(hashmap.update(&1, |_, v| { *v = 3; *v }).unwrap(), 3);
assert_eq!(hashmap.read(&1, |_, v| *v).unwrap(), 3);
assert_eq!(hashmap.remove(&1).unwrap(), (1, 3));

hashmap.entry(7).or_insert(17);
assert_eq!(hashmap.read(&7, |_, v| *v).unwrap(), 17);

let future_insert = hashmap.insert_async(2, 1);
let future_remove = hashmap.remove_async(&1);
```

The `Entry` API of [`HashMap`](#hashmap) is helpful if the workflow is complicated.

```rust
use scc::HashMap;

let hashmap: HashMap<u64, u32> = HashMap::default();

hashmap.entry(3).or_insert(7);
assert_eq!(hashmap.read(&3, |_, v| *v), Some(7));

hashmap.entry(4).and_modify(|v| { *v += 1 }).or_insert(5);
assert_eq!(hashmap.read(&4, |_, v| *v), Some(5));
```

[`HashMap`](#hashmap) does not provide an [`Iterator`](https://doc.rust-lang.org/std/iter/trait.Iterator.html) since it is impossible to confine the lifetime of [`Iterator::Item`](https://doc.rust-lang.org/std/iter/trait.Iterator.html#associatedtype.Item) to the [Iterator](https://doc.rust-lang.org/std/iter/trait.Iterator.html). The limitation can be circumvented by relying on interior mutability, e.g., letting the returned reference hold a lock. However, it may lead to a deadlock if not correctly used, and frequent acquisition of locks may impact performance. Therefore, [`Iterator`](https://doc.rust-lang.org/std/iter/trait.Iterator.html) is not implemented; instead, [`HashMap`](#hashmap) provides several methods to iterate over entries synchronously or asynchronously: `any`, `any_async`, `first_entry`, `first_entry_async`, `prune`, `prune_async`, `retain`, `retain_async`, `scan`, `scan_async`, `OccupiedEntry::next`, and `OccupiedEntry::next_async`.

```rust
use scc::HashMap;

let hashmap: HashMap<u64, u32> = HashMap::default();

assert!(hashmap.insert(1, 0).is_ok());
assert!(hashmap.insert(2, 1).is_ok());

// Entries can be modified or removed via `retain`.
let mut acc = 0;
hashmap.retain(|k, v_mut| { acc += *k; *v_mut = 2; true });
assert_eq!(acc, 3);
assert_eq!(hashmap.read(&1, |_, v| *v).unwrap(), 2);
assert_eq!(hashmap.read(&2, |_, v| *v).unwrap(), 2);

// `any` returns `true` when an entry satisfying the predicate is found.
assert!(hashmap.insert(3, 2).is_ok());
assert!(hashmap.any(|k, _| *k == 3));

// Multiple entries can be removed through `retain`.
hashmap.retain(|k, v| *k == 1 && *v == 2);

// `hash_map::OccupiedEntry` also can return the next closest occupied entry.
let first_entry = hashmap.first_entry();
assert_eq!(*first_entry.as_ref().unwrap().key(), 1);
let second_entry = first_entry.and_then(|e| e.next());
assert!(second_entry.is_none());

fn is_send<T: Send>(f: &T) -> bool {
    true
}

// Asynchronous iteration over entries using `scan_async`.
let future_scan = hashmap.scan_async(|k, v| println!("{k} {v}"));
assert!(is_send(&future_scan));

// Asynchronous iteration over entries using the `Entry` API.
let future_iter = async {
    let mut iter = hashmap.first_entry_async().await;
    while let Some(entry) = iter {
        // `OccupiedEntry` can be sent across awaits and threads.
        assert!(is_send(&entry));
        assert_eq!(*entry.key(), 1);
        iter = entry.next_async().await;
    }
};
assert!(is_send(&future_iter));
```

## `HashSet`

[`HashSet`](#hashset) is a special version of [`HashMap`](#hashmap) where the value type is `()`.

### Examples

Most [`HashSet`](#hashset) methods are identical to that of [`HashMap`](#hashmap) except that they do not receive a value argument, and some [`HashMap`](#hashmap) methods for value modification are not implemented for [`HashSet`](#hashset).

```rust
use scc::HashSet;

let hashset: HashSet<u64> = HashSet::default();

assert!(hashset.read(&1, |_| true).is_none());
assert!(hashset.insert(1).is_ok());
assert!(hashset.read(&1, |_| true).unwrap());

let future_insert = hashset.insert_async(2);
let future_remove = hashset.remove_async(&1);
```

## `HashIndex`

[`HashIndex`](#hashindex) is a read-optimized version of [`HashMap`](#hashmap). In a [`HashIndex`](#hashindex), not only is the memory of the bucket array managed by [`sdd`](https://crates.io/crates/sdd), but also that of entry buckets is protected by [`sdd`](https://crates.io/crates/sdd), enabling lock-free-read access to individual entries.

### Entry lifetime

`HashIndex` does not drop removed entries immediately; instead, they are dropped when one of the following conditions is met.

1. [`Epoch`](https://docs.rs/sdd/latest/sdd/struct.Epoch.html) reaches the next generation since the last entry was removed in a bucket, and the bucket is write-accessed.
2. `HashIndex` is cleared, or resized.
3. Buckets full of removed entries occupy 50% of the capacity.

Those conditions do not guarantee that the removed entry will be dropped within a definite period of time; therefore, `HashIndex` would not be an optimal choice if the workload is write-heavy and the entry size is large.

### Examples

The `peek` and `peek_with` methods are completely lock-free.

```rust
use scc::HashIndex;

let hashindex: HashIndex<u64, u32> = HashIndex::default();

assert!(hashindex.insert(1, 0).is_ok());

// `peek` and `peek_with` are lock-free.
assert_eq!(hashindex.peek_with(&1, |_, v| *v).unwrap(), 0);

let future_insert = hashindex.insert_async(2, 1);
let future_remove = hashindex.remove_if_async(&1, |_| true);
```

The `Entry` API of [`HashIndex`](#hashindex) can update an existing entry.

```rust
use scc::HashIndex;

let hashindex: HashIndex<u64, u32> = HashIndex::default();
assert!(hashindex.insert(1, 1).is_ok());

if let Some(mut o) = hashindex.get(&1) {
    // Create a new version of the entry.
    o.update(2);
};

if let Some(mut o) = hashindex.get(&1) {
    // Update the entry in place.
    unsafe { *o.get_mut() = 3; }
};
```

An [`Iterator`](https://doc.rust-lang.org/std/iter/trait.Iterator.html) is implemented for [`HashIndex`](#hashindex) because any derived references can survive as long as the associated `ebr::Guard` lives.

```rust
use scc::ebr::Guard;
use scc::HashIndex;

let hashindex: HashIndex<u64, u32> = HashIndex::default();

assert!(hashindex.insert(1, 0).is_ok());

// Existing values can be replaced with a new one.
hashindex.get(&1).unwrap().update(1);

let guard = Guard::new();

// An `Guard` has to be supplied to `iter`.
let mut iter = hashindex.iter(&guard);

// The derived reference can live as long as `guard`.
let entry_ref = iter.next().unwrap();
assert_eq!(iter.next(), None);

drop(hashindex);

// The entry can be read after `hashindex` is dropped.
assert_eq!(entry_ref, (&1, &1));
```

## `HashCache`

[`HashCache`](#hashcache) is a 32-way associative concurrent cache based on the [`HashMap`](#hashmap) implementation. [`HashCache`](#hashcache) does not keep track of the least recently used entry in the entire cache. Instead, each bucket maintains a doubly linked list of occupied entries, which is updated on entry access.

### Examples

The LRU entry in a bucket is evicted when a new entry is inserted, and the bucket is full.

```rust
use scc::HashCache;

let hashcache: HashCache<u64, u32> = HashCache::with_capacity(100, 2000);

/// The capacity cannot exceed the maximum capacity.
assert_eq!(hashcache.capacity_range(), 128..=2048);

/// If the bucket corresponding to `1` or `2` is full, the LRU entry will be evicted.
assert!(hashcache.put(1, 0).is_ok());
assert!(hashcache.put(2, 0).is_ok());

/// `1` becomes the most recently accessed entry in the bucket.
assert!(hashcache.get(&1).is_some());

/// An entry can be normally removed.
assert_eq!(hashcache.remove(&2).unwrap(), (2, 0));
```

## `TreeIndex`

[`TreeIndex`](#treeindex) is a B-plus tree variant optimized for read operations. [`sdd`](https://crates.io/crates/sdd) protects the memory used by individual entries, thus enabling lock-free read access to them.

### Locking behavior

Read access is always lock-free and non-blocking. Write access to an entry is lock-free and non-blocking as long as no structural changes are required. However, when nodes are split or merged by a write operation, other write operations on keys in the affected range are blocked.

### Entry lifetime

`TreeIndex` does not drop removed entries immediately. Instead, they are dropped when the leaf node is cleared or split, and this makes `TreeIndex` a sub-optimal choice if the workload is write-heavy.

### Examples

If the key is unique, an entry can be inserted, read, and removed afterward. Locks are acquired or awaited only when internal nodes are split or merged.

```rust
use scc::TreeIndex;

let treeindex: TreeIndex<u64, u32> = TreeIndex::new();

assert!(treeindex.insert(1, 2).is_ok());

// `peek` and `peek_with` are lock-free.
assert_eq!(treeindex.peek_with(&1, |_, v| *v).unwrap(), 2);
assert!(treeindex.remove(&1));

let future_insert = treeindex.insert_async(2, 3);
let future_remove = treeindex.remove_if_async(&1, |v| *v == 2);
```

Entries can be scanned without acquiring any locks.

```rust
use scc::TreeIndex;
use sdd::Guard;

let treeindex: TreeIndex<u64, u32> = TreeIndex::new();

assert!(treeindex.insert(1, 10).is_ok());
assert!(treeindex.insert(2, 11).is_ok());
assert!(treeindex.insert(3, 13).is_ok());

let guard = Guard::new();

// `iter` iterates over entries without acquiring a lock.
let mut iter = treeindex.iter(&guard);
assert_eq!(iter.next().unwrap(), (&1, &10));
assert_eq!(iter.next().unwrap(), (&2, &11));
assert_eq!(iter.next().unwrap(), (&3, &13));
assert!(iter.next().is_none());
```

A specific range of keys can be scanned.

```rust
use scc::ebr::Guard;
use scc::TreeIndex;

let treeindex: TreeIndex<u64, u32> = TreeIndex::new();

for i in 0..10 {
    assert!(treeindex.insert(i, 10).is_ok());
}

let guard = Guard::new();

assert_eq!(treeindex.range(1..1, &guard).count(), 0);
assert_eq!(treeindex.range(4..8, &guard).count(), 4);
assert_eq!(treeindex.range(4..=8, &guard).count(), 5);
```

## `Bag`

[`Bag`](#bag) is a concurrent lock-free unordered container. [`Bag`](#bag) is completely opaque, disallowing access to contained instances until they are popped. [`Bag`](#bag) is especially efficient if the number of contained instances can be maintained under `ARRAY_LEN (default: usize::BITS / 2)`

### Examples

```rust
use scc::Bag;

let bag: Bag<usize> = Bag::default();

bag.push(1);
assert!(!bag.is_empty());
assert_eq!(bag.pop(), Some(1));
assert!(bag.is_empty());
```

## `Queue`

[Queue](#queue) is a concurrent lock-free first-in-first-out container backed by [`sdd`](https://crates.io/crates/sdd).

### Examples

```rust
use scc::Queue;

let queue: Queue<usize> = Queue::default();

queue.push(1);
assert!(queue.push_if(2, |e| e.map_or(false, |x| **x == 1)).is_ok());
assert!(queue.push_if(3, |e| e.map_or(false, |x| **x == 1)).is_err());
assert_eq!(queue.pop().map(|e| **e), Some(1));
assert_eq!(queue.pop().map(|e| **e), Some(2));
assert!(queue.pop().is_none());
```

## `Stack`

[`Stack`](#stack) is a concurrent lock-free last-in-first-out container backed by [`sdd`](https://crates.io/crates/sdd).

### Examples

```rust
use scc::Stack;

let stack: Stack<usize> = Stack::default();

stack.push(1);
stack.push(2);
assert_eq!(stack.pop().map(|e| **e), Some(2));
assert_eq!(stack.pop().map(|e| **e), Some(1));
assert!(stack.pop().is_none());
```

## `LinkedList`

[`LinkedList`](#linkedlist) is a type trait that implements lock-free concurrent singly linked list operations backed by [`sdd`](https://crates.io/crates/sdd). It additionally provides a method for marking a linked list entry to denote a user-defined state.

### Examples

```rust
use scc::ebr::{AtomicShared, Guard, Shared};
use scc::LinkedList;
use std::sync::atomic::Ordering::Relaxed;

#[derive(Default)]
struct L(AtomicShared<L>, usize);
impl LinkedList for L {
    fn link_ref(&self) -> &AtomicShared<L> {
        &self.0
    }
}

let guard = Guard::new();

let head: L = L::default();
let tail: Shared<L> = Shared::new(L(AtomicShared::null(), 1));

// A new entry is pushed.
assert!(head.push_back(tail.clone(), false, Relaxed, &guard).is_ok());
assert!(!head.is_marked(Relaxed));

// Users can mark a flag on an entry.
head.mark(Relaxed);
assert!(head.is_marked(Relaxed));

// `next_ptr` traverses the linked list.
let next_ptr = head.next_ptr(Relaxed, &guard);
assert_eq!(next_ptr.as_ref().unwrap().1, 1);

// Once `tail` is deleted, it becomes invisible.
tail.delete_self(Relaxed);
assert!(head.next_ptr(Relaxed, &guard).is_null());
```

## Performance

### [`HashMap`](#hashmap) Tail Latency

The expected tail latency of a distribution of latencies of 1048576 insertion operations (`K = u64, V = u64`) ranges from 180 microseconds to 200 microseconds on Apple M2 Max.

### [`HashMap`](#hashmap) and [`HashIndex`](#hashindex) Throughput

- [Results on Apple M2 Max (12 cores)](https://github.com/wvwwvwwv/conc-map-bench).
- [Results on Intel Xeon (32 cores, avx2)](https://github.com/wvwwvwwv/conc-map-bench/tree/Intel).

## [Changelog](https://github.com/wvwwvwwv/scalable-concurrent-containers/blob/main/CHANGELOG.md)
