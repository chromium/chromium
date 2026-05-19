# fastbloom
[![Crates.io](https://img.shields.io/crates/v/fastbloom.svg)](https://crates.io/crates/fastbloom)
[![docs.rs](https://docs.rs/fastbloom/badge.svg)](https://docs.rs/fastbloom)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/tomtomwombat/fastbloom/blob/main/LICENSE-MIT)
[![License: APACHE](https://img.shields.io/badge/License-Apache-blue.svg)](https://github.com/tomtomwombat/fastbloom/blob/main/LICENSE-APACHE)

The fastest Bloom filter in Rust. No accuracy compromises. Full concurrency support and compatible with any hasher.

## Overview

fastbloom is a fast, flexible, and accurate Bloom filter implemented in Rust. fastbloom's default hasher is SipHash-1-3 using randomized keys but can be seeded or configured to use any hasher. fastbloom is 2-400 times faster and magnitudes more accurate than existing Bloom filter implementations. fastbloom's `AtomicBloomFilter` is a concurrent Bloom filter that avoids lock contention.

## Usage

Due to a different (improved!) algorithm in 0.14.x, Bloomfilters have incompatible serialization/deserialization with prior versions.

```toml
# Cargo.toml
[dependencies]
fastbloom = "0.14.1"
```
Basic usage:
```rust
use fastbloom::BloomFilter;

let mut filter = BloomFilter::with_num_bits(1024).expected_items(2);
filter.insert("42");
filter.insert("🦀");
```
Instantiate with a target false positive rate:
```rust
use fastbloom::BloomFilter;

let filter = BloomFilter::with_false_pos(0.001).items(["42", "🦀"]);
assert!(filter.contains("42"));
assert!(filter.contains("🦀"));
```
Use any hasher:
```rust
use fastbloom::BloomFilter;
use ahash::RandomState;

let filter = BloomFilter::with_num_bits(1024)
    .hasher(RandomState::default())
    .items(["42", "🦀"]);
```
Full concurrency support. `AtomicBloomFilter` is a drop-in replacement for `RwLock<OtherBloomFilter>` because all methods take `&self`:
```rust
use fastbloom::AtomicBloomFilter;

let filter = AtomicBloomFilter::with_num_bits(1024).expected_items(2);
filter.insert("42");
filter.insert("🦀");
```

## Background
Bloom filters are space-efficient approximate membership set data structures supported by an underlying bit array to track item membership. To insert/check membership, a number of bits are set/checked at positions based on the item's hash. False positives from a membership check are possible, but false negatives are not. Once constructed, neither the Bloom filter's underlying memory usage nor number of bits per item change. [See more.](https://en.wikipedia.org/wiki/Bloom_filter)

```text
hash(4) ──────┬─────┬───────────────┐
              ↓     ↓               ↓
0 0 0 0 0 0 0 1 0 0 1 0 0 0 0 0 0 0 1 0
  ↑           ↑           ↑
  └───────────┴───────────┴──── hash(3) (not in the set)
```

## Implementation

fastbloom is blazingly fast because it efficiently derives many index bits from **only one real hash per item** and leverages other research findings on Bloom filters. fastbloom employs "hash composition" on two 32-bit halves of an original 64-bit hash. Each subsequent hash is derived by combining the original hash value with a different constant using modular arithmetic and bitwise operations. This results in a set of hash functions that are effectively independent and uniformly distributed, even though they are derived from the same original hash function. Computing the composition of two original hashes is faster than re-computing the hash with a different seed. This technique is [explained in depth in this paper.](https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf)

## Speed

![perf-non-member](https://github.com/user-attachments/assets/b785160e-ed94-4035-9c2a-cb8d55be39d3)
![perf-member](https://github.com/user-attachments/assets/c06e12ff-8193-4784-8e26-b6dabf27de19)
> Hashers used:
> - xxhash: sbbf
> - Sip1-3: bloom, bloomfilter, probabilistic-collections
> - ahash: fastbloom
> 
> [Benchmark source](https://github.com/tomtomwombat/bench-bloom-filters)

## Accuracy

fastbloom does not compromise accuracy. Below is a comparison of false positive rates with other Bloom filter crates:

![fp](https://github.com/user-attachments/assets/473dc8f3-6501-4f3c-94e8-1f693d4efce1)

[Benchmark source](https://github.com/tomtomwombat/bench-bloom-filters)

## Available Features

- **`rand`** - Enabled by default, this has the `DefaultHasher` source its random state using `thread_rng()` instead of hardware sources. Getting entropy from a user-space source is considerably faster, but requires additional dependencies to achieve this. Disabling this feature by using `default-features = false` makes `DefaultHasher` source its entropy using `getrandom`, which will have a much simpler code footprint at the expense of speed.
- **`serde`** - `BloomFilter`s implement `Serialize` and `Deserialize` when possible.
- **`loom`** - `AtomicBloomFilter`s use [loom](https://github.com/tokio-rs/loom) atomics, making it compatible with loom testing.

## References
- [Bloom filter - Wikipedia](https://en.wikipedia.org/wiki/Bloom_filter)
- [Bloom filters debunked: Dispelling 30 Years of bad math with Coq!](https://gopiandcode.uk/logs/log-bloomfilters-debunked.html)
- [Bloom Filter Interactive Demonstration](https://www.jasondavies.com/bloomfilter/)
- [Cache-, Hash- and Space-Efficient Bloom Filters](https://web.archive.org/web/20070623102632/http://algo2.iti.uni-karlsruhe.de/singler/publications/cacheefficientbloomfilters-wea2007.pdf)
- [Less hashing, same performance: Building a better Bloom filter](https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf)
- [A fast alternative to the modulo reduction](https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/)

## License

Licensed under either of

 * Apache License, Version 2.0
   ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license
   ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
