//! This crate provides foldhash, a fast, non-cryptographic, minimally
//! DoS-resistant hashing algorithm designed for computational uses such as
//! hashmaps, bloom filters, count sketching, etc.
//!
//! When should you **not** use foldhash:
//!
//! - You are afraid of people studying your long-running program's behavior
//!   to reverse engineer its internal random state and using this knowledge to
//!   create many colliding inputs for computational complexity attacks.
//!
//! - You expect foldhash to have a consistent output across versions or
//!   platforms, such as for persistent file formats or communication protocols.
//!   
//! - You are relying on foldhash's properties for any kind of security.
//!   Foldhash is **not appropriate for any cryptographic purpose**.
//!
//! Foldhash has two variants, one optimized for speed which is ideal for data
//! structures such as hash maps and bloom filters, and one optimized for
//! statistical quality which is ideal for algorithms such as
//! [HyperLogLog](https://en.wikipedia.org/wiki/HyperLogLog) and
//! [MinHash](https://en.wikipedia.org/wiki/MinHash).
//!
//! Foldhash can be used in a `#![no_std]` environment by disabling its default
//! `"std"` feature.
//!
//! # Usage
//!
//! The easiest way to use this crate with the standard library [`HashMap`] or
//! [`HashSet`] is to import them from `foldhash` instead, along with the
//! extension traits to make [`HashMap::new`] and [`HashMap::with_capacity`]
//! work out-of-the-box:
//!
//! ```rust
//! use foldhash::{HashMap, HashMapExt};
//!
//! let mut hm = HashMap::new();
//! hm.insert(42, "hello");
//! ```
//!
//! You can also avoid the convenience types and do it manually by initializing
//! a [`RandomState`](fast::RandomState), for example if you are using a different hash map
//! implementation like [`hashbrown`](https://docs.rs/hashbrown/):
//!
//! ```rust
//! use hashbrown::HashMap;
//! use foldhash::fast::RandomState;
//!
//! let mut hm = HashMap::with_hasher(RandomState::default());
//! hm.insert("foo", "bar");
//! ```
//!
//! The above methods are the recommended way to use foldhash, which will
//! automatically generate a randomly generated hasher instance for you. If you
//! absolutely must have determinism you can use [`FixedState`](fast::FixedState)
//! instead, but note that this makes you trivially vulnerable to HashDoS
//! attacks and might lead to quadratic runtime when moving data from one
//! hashmap/set into another:
//!
//! ```rust
//! use std::collections::HashSet;
//! use foldhash::fast::FixedState;
//!
//! let mut hm = HashSet::with_hasher(FixedState::with_seed(42));
//! hm.insert([1, 10, 100]);
//! ```
//!
//! If you rely on statistical properties of the hash for the correctness of
//! your algorithm, such as in [HyperLogLog](https://en.wikipedia.org/wiki/HyperLogLog),
//! it is suggested to use the [`RandomState`](quality::RandomState)
//! or [`FixedState`](quality::FixedState) from the [`quality`] module instead
//! of the [`fast`] module. The latter is optimized purely for speed in hash
//! tables and has known statistical imperfections.
//!
//! Finally, you can also directly use the [`RandomState`](quality::RandomState)
//! or [`FixedState`](quality::FixedState) to manually hash items using the
//! [`BuildHasher`](std::hash::BuildHasher) trait:
//! ```rust
//! use std::hash::BuildHasher;
//! use foldhash::quality::RandomState;
//!
//! let random_state = RandomState::default();
//! let hash = random_state.hash_one("hello world");
//! ```

#![cfg_attr(all(not(test), not(feature = "std")), no_std)]
#![warn(missing_docs)]

use core::hash::Hasher;

#[cfg(feature = "std")]
mod convenience;
mod seed;

#[cfg(feature = "std")]
pub use convenience::*;

// Arbitrary constants with high entropy. Hexadecimal digits of pi were used.
const ARBITRARY0: u64 = 0x243f6a8885a308d3;
const ARBITRARY1: u64 = 0x13198a2e03707344;
const ARBITRARY2: u64 = 0xa4093822299f31d0;
const ARBITRARY3: u64 = 0x082efa98ec4e6c89;
const ARBITRARY4: u64 = 0x452821e638d01377;
const ARBITRARY5: u64 = 0xbe5466cf34e90c6c;
const ARBITRARY6: u64 = 0xc0ac29b7c97c50dd;
const ARBITRARY7: u64 = 0x3f84d5b5b5470917;
const ARBITRARY8: u64 = 0x9216d5d98979fb1b;
const ARBITRARY9: u64 = 0xd1310ba698dfb5ac;

#[inline(always)]
const fn folded_multiply(x: u64, y: u64) -> u64 {
    #[cfg(target_pointer_width = "64")]
    {
        // We compute the full u64 x u64 -> u128 product, this is a single mul
        // instruction on x86-64, one mul plus one mulhi on ARM64.
        let full = (x as u128) * (y as u128);
        let lo = full as u64;
        let hi = (full >> 64) as u64;

        // The middle bits of the full product fluctuate the most with small
        // changes in the input. This is the top bits of lo and the bottom bits
        // of hi. We can thus make the entire output fluctuate with small
        // changes to the input by XOR'ing these two halves.
        lo ^ hi
    }

    #[cfg(target_pointer_width = "32")]
    {
        // u64 x u64 -> u128 product is prohibitively expensive on 32-bit.
        // Decompose into 32-bit parts.
        let lx = x as u32;
        let ly = y as u32;
        let hx = (x >> 32) as u32;
        let hy = (y >> 32) as u32;

        // u32 x u32 -> u64 the low bits of one with the high bits of the other.
        let afull = (lx as u64) * (hy as u64);
        let bfull = (hx as u64) * (ly as u64);

        // Combine, swapping low/high of one of them so the upper bits of the
        // product of one combine with the lower bits of the other.
        afull ^ bfull.rotate_right(32)
    }
}

/// The foldhash implementation optimized for speed.
pub mod fast {
    use super::*;

    pub use seed::fast::{FixedState, RandomState};

    /// A [`Hasher`] instance implementing foldhash, optimized for speed.
    ///
    /// It can't be created directly, see [`RandomState`] or [`FixedState`].
    #[derive(Clone)]
    pub struct FoldHasher {
        accumulator: u64,
        sponge: u128,
        sponge_len: u8,
        fold_seed: u64,
        expand_seed: u64,
        expand_seed2: u64,
        expand_seed3: u64,
    }

    impl FoldHasher {
        pub(crate) fn with_seed(per_hasher_seed: u64, global_seed: &[u64; 4]) -> FoldHasher {
            FoldHasher {
                accumulator: per_hasher_seed,
                sponge: 0,
                sponge_len: 0,
                fold_seed: global_seed[0],
                expand_seed: global_seed[1],
                expand_seed2: global_seed[2],
                expand_seed3: global_seed[3],
            }
        }

        #[inline(always)]
        fn write_num<T: Into<u128>>(&mut self, x: T) {
            let bits: usize = 8 * core::mem::size_of::<T>();
            if self.sponge_len as usize + bits > 128 {
                let lo = self.sponge as u64;
                let hi = (self.sponge >> 64) as u64;
                self.accumulator = folded_multiply(lo ^ self.accumulator, hi ^ self.fold_seed);
                self.sponge = x.into();
                self.sponge_len = 0;
            } else {
                self.sponge |= x.into() << self.sponge_len;
                self.sponge_len += bits as u8;
            }
        }
    }

    impl Hasher for FoldHasher {
        #[inline(always)]
        fn write(&mut self, bytes: &[u8]) {
            let mut s0 = self.accumulator;
            let mut s1 = self.expand_seed;
            let len = bytes.len();
            if len <= 16 {
                // XOR the input into s0, s1, then multiply and fold.
                if len >= 8 {
                    s0 ^= u64::from_ne_bytes(bytes[0..8].try_into().unwrap());
                    s1 ^= u64::from_ne_bytes(bytes[len - 8..].try_into().unwrap());
                } else if len >= 4 {
                    s0 ^= u32::from_ne_bytes(bytes[0..4].try_into().unwrap()) as u64;
                    s1 ^= u32::from_ne_bytes(bytes[len - 4..].try_into().unwrap()) as u64;
                } else if len > 0 {
                    let lo = bytes[0];
                    let mid = bytes[len / 2];
                    let hi = bytes[len - 1];
                    s0 ^= lo as u64;
                    s1 ^= ((hi as u64) << 8) | mid as u64;
                }
                self.accumulator = folded_multiply(s0, s1);
            } else if len < 256 {
                self.accumulator = hash_bytes_medium(bytes, s0, s1, self.fold_seed);
            } else {
                self.accumulator = hash_bytes_long(
                    bytes,
                    s0,
                    s1,
                    self.expand_seed2,
                    self.expand_seed3,
                    self.fold_seed,
                );
            }
        }

        #[inline(always)]
        fn write_u8(&mut self, i: u8) {
            self.write_num(i);
        }

        #[inline(always)]
        fn write_u16(&mut self, i: u16) {
            self.write_num(i);
        }

        #[inline(always)]
        fn write_u32(&mut self, i: u32) {
            self.write_num(i);
        }

        #[inline(always)]
        fn write_u64(&mut self, i: u64) {
            self.write_num(i);
        }

        #[inline(always)]
        fn write_u128(&mut self, i: u128) {
            let lo = i as u64;
            let hi = (i >> 64) as u64;
            self.accumulator = folded_multiply(lo ^ self.accumulator, hi ^ self.fold_seed);
        }

        #[inline(always)]
        fn write_usize(&mut self, i: usize) {
            // u128 doesn't implement From<usize>.
            #[cfg(target_pointer_width = "32")]
            self.write_num(i as u32);
            #[cfg(target_pointer_width = "64")]
            self.write_num(i as u64);
        }

        #[inline(always)]
        fn finish(&self) -> u64 {
            if self.sponge_len > 0 {
                let lo = self.sponge as u64;
                let hi = (self.sponge >> 64) as u64;
                folded_multiply(lo ^ self.accumulator, hi ^ self.fold_seed)
            } else {
                self.accumulator
            }
        }
    }
}

/// The foldhash implementation optimized for quality.
pub mod quality {
    use super::*;

    pub use seed::quality::{FixedState, RandomState};

    /// A [`Hasher`] instance implementing foldhash, optimized for quality.
    ///
    /// It can't be created directly, see [`RandomState`] or [`FixedState`].
    #[derive(Clone)]
    pub struct FoldHasher {
        pub(crate) inner: fast::FoldHasher,
    }

    impl Hasher for FoldHasher {
        #[inline(always)]
        fn write(&mut self, bytes: &[u8]) {
            self.inner.write(bytes);
        }

        #[inline(always)]
        fn write_u8(&mut self, i: u8) {
            self.inner.write_u8(i);
        }

        #[inline(always)]
        fn write_u16(&mut self, i: u16) {
            self.inner.write_u16(i);
        }

        #[inline(always)]
        fn write_u32(&mut self, i: u32) {
            self.inner.write_u32(i);
        }

        #[inline(always)]
        fn write_u64(&mut self, i: u64) {
            self.inner.write_u64(i);
        }

        #[inline(always)]
        fn write_u128(&mut self, i: u128) {
            self.inner.write_u128(i);
        }

        #[inline(always)]
        fn write_usize(&mut self, i: usize) {
            self.inner.write_usize(i);
        }

        #[inline(always)]
        fn finish(&self) -> u64 {
            folded_multiply(self.inner.finish(), ARBITRARY0)
        }
    }
}

/// Hashes strings >= 16 bytes, has unspecified behavior when bytes.len() < 16.
fn hash_bytes_medium(bytes: &[u8], mut s0: u64, mut s1: u64, fold_seed: u64) -> u64 {
    // Process 32 bytes per iteration, 16 bytes from the start, 16 bytes from
    // the end. On the last iteration these two chunks can overlap, but that is
    // perfectly fine.
    let left_to_right = bytes.chunks_exact(16);
    let mut right_to_left = bytes.rchunks_exact(16);
    for lo in left_to_right {
        let hi = right_to_left.next().unwrap();
        let unconsumed_start = lo.as_ptr();
        let unconsumed_end = hi.as_ptr_range().end;
        if unconsumed_start >= unconsumed_end {
            break;
        }

        let a = u64::from_ne_bytes(lo[0..8].try_into().unwrap());
        let b = u64::from_ne_bytes(lo[8..16].try_into().unwrap());
        let c = u64::from_ne_bytes(hi[0..8].try_into().unwrap());
        let d = u64::from_ne_bytes(hi[8..16].try_into().unwrap());
        s0 = folded_multiply(a ^ s0, c ^ fold_seed);
        s1 = folded_multiply(b ^ s1, d ^ fold_seed);
    }

    s0 ^ s1
}

/// Hashes strings >= 16 bytes, has unspecified behavior when bytes.len() < 16.
#[cold]
#[inline(never)]
fn hash_bytes_long(
    bytes: &[u8],
    mut s0: u64,
    mut s1: u64,
    mut s2: u64,
    mut s3: u64,
    fold_seed: u64,
) -> u64 {
    let chunks = bytes.chunks_exact(64);
    let remainder = chunks.remainder().len();
    for chunk in chunks {
        let a = u64::from_ne_bytes(chunk[0..8].try_into().unwrap());
        let b = u64::from_ne_bytes(chunk[8..16].try_into().unwrap());
        let c = u64::from_ne_bytes(chunk[16..24].try_into().unwrap());
        let d = u64::from_ne_bytes(chunk[24..32].try_into().unwrap());
        let e = u64::from_ne_bytes(chunk[32..40].try_into().unwrap());
        let f = u64::from_ne_bytes(chunk[40..48].try_into().unwrap());
        let g = u64::from_ne_bytes(chunk[48..56].try_into().unwrap());
        let h = u64::from_ne_bytes(chunk[56..64].try_into().unwrap());
        s0 = folded_multiply(a ^ s0, e ^ fold_seed);
        s1 = folded_multiply(b ^ s1, f ^ fold_seed);
        s2 = folded_multiply(c ^ s2, g ^ fold_seed);
        s3 = folded_multiply(d ^ s3, h ^ fold_seed);
    }
    s0 ^= s2;
    s1 ^= s3;

    if remainder > 0 {
        hash_bytes_medium(&bytes[bytes.len() - remainder.max(16)..], s0, s1, fold_seed)
    } else {
        s0 ^ s1
    }
}
