# fastbloom
[![Github](https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github)](https://github.com/tomtomwombat/fastbloom)
[![Crates.io](https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust)](https://crates.io/crates/fastbloom)
[![docs.rs](https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs)](https://docs.rs/fastbloom)
![Downloads](https://img.shields.io/crates/d/fastbloom?style=for-the-badge)

The fastest Bloom filter in Rust. No accuracy compromises. Full concurrency support and compatible with any hasher.

## Overview

fastbloom is a fast, flexible, and accurate Bloom filter implemented in Rust. fastbloom's default hasher is SipHash-1-3 using randomized keys but can be seeded or configured to use any hasher. fastbloom is 2-20 times faster and magnitudes more accurate than existing Bloom filter implementations. fastbloom's `AtomicBloomFilter` is a concurrent Bloom filter that avoids lock contention.

## Usage

Due to a different (improved!) algorithm in 0.17.x, Bloomfilters have incompatible serialization/deserialization with prior versions.

```toml
# Cargo.toml
[dependencies]
fastbloom = "0.17.0"
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

let filter = BloomFilter::with_false_pos(0.001).items(["42", "🦀"].iter());
assert!(filter.contains("42"));
assert!(filter.contains("🦀"));
```
Use any hasher:
```rust
use fastbloom::BloomFilter;
use foldhash::fast::RandomState;

let filter = BloomFilter::with_num_bits(1024)
    .hasher(RandomState::default())
    .items(["42", "🦀"].iter());
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
- AMD Ryzen 9 5900X 12-Core Processor             (3.70 GHz)
- 64-bit operating system, x64-based processor

![perf-non-member](https://github.com/user-attachments/assets/a825d2f7-4cd7-4b6b-a447-fd7f3dab356b)
![perf-member](https://github.com/user-attachments/assets/998f9470-b91f-460c-ad2d-1f197160c210)
> Hashers used:
> - xxhash: sbbf
> - Sip1-3: bloom, bloomfilter, probabilistic-collections
> - foldhash: fastbloom
> 
> [Benchmark source](https://github.com/tomtomwombat/bench-bloom-filters)

## Accuracy

fastbloom does not compromise accuracy. Below is a comparison of false positive rates with other Bloom filter crates:

![fp](https://github.com/user-attachments/assets/17ddaab7-c63f-456a-9e2e-b22c7f994386)

[Benchmark source](https://github.com/tomtomwombat/bench-bloom-filters)

## Available Features

- **`rand`** - Enabled by default, this has the `DefaultHasher` source its random state using `thread_rng()` instead of hardware sources. Getting entropy from a user-space source is considerably faster, but requires additional dependencies to achieve this. Disabling this feature by using `default-features = false` makes `DefaultHasher` source its entropy using `foldhash`, which will have a much simpler code footprint at the expense of speed.
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
