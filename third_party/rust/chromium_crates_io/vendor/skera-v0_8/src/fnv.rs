//! A fork of the `fnv` crate from <https://github.com/servo/rust-fnv>. The code is forked since it
//! is small and to reduce the maintenance overhead of importing into other monorepos.
//!
//! FNV is an implementation of the Fowler–Noll–Vo hash function.
//!
//! ## About
//!
//! The FNV hash function is a custom `Hasher` implementation that is more
//! efficient for smaller hash keys.
//!
//! The Rust Standard Library documentation states that while the default `Hasher` implementation,
//! SipHash, is good in many cases, it is notably slower than other algorithms with short keys, such
//! as when you have a map of integers to other values.  In cases like these, FNV is demonstrably
//! faster.
//!
//! Its disadvantages are that it performs badly on larger inputs, and provides no protection
//! against collision attacks, where a malicious user can craft specific keys designed to slow a
//! hasher down. Thus, it is important to profile your program to ensure that you are using small
//! hash keys, and be certain that your program could not be exposed to malicious inputs (including
//! being a networked server).
//!
//! The Rust compiler itself uses FNV, as it is not worried about denial-of-service attacks, and can
//! assume that its inputs are going to be small—a perfect use case for FNV.
use std::hash::{BuildHasherDefault, Hasher};

const INITIAL_STATE: u64 = 0xcbf2_9ce4_8422_2325;
const PRIME: u64 = 0x0100_0000_01b3;

#[derive(Clone)]
pub struct FnvHasher(u64);

impl Default for FnvHasher {
    #[inline]
    fn default() -> FnvHasher {
        FnvHasher(INITIAL_STATE)
    }
}

impl Hasher for FnvHasher {
    #[inline]
    fn finish(&self) -> u64 {
        self.0
    }

    #[inline]
    fn write(&mut self, bytes: &[u8]) {
        let FnvHasher(mut hash) = *self;
        for byte in bytes {
            hash ^= u64::from(*byte);
            hash = hash.wrapping_mul(PRIME);
        }
        *self = FnvHasher(hash);
    }
}

pub type FnvBuildHasher = BuildHasherDefault<FnvHasher>;
pub type FnvHashMap<K, V> = std::collections::HashMap<K, V, FnvBuildHasher>;

#[cfg(test)]
mod test {
    use super::*;
    use std::hash::Hasher;

    fn fnv1a(bytes: &[u8]) -> u64 {
        let mut hasher = FnvHasher::default();
        hasher.write(bytes);
        hasher.finish()
    }

    #[test]
    fn basic_tests() {
        assert_eq!(fnv1a(b""), 0xcbf29ce484222325);
        assert_eq!(fnv1a(b"a"), 0xaf63dc4c8601ec8c);
        assert_eq!(fnv1a(b"b"), 0xaf63df4c8601f1a5);
        assert_eq!(fnv1a(b"c"), 0xaf63de4c8601eff2);
        assert_eq!(fnv1a(b"d"), 0xaf63d94c8601e773);
        assert_eq!(fnv1a(b"e"), 0xaf63d84c8601e5c0);
        assert_eq!(fnv1a(b"f"), 0xaf63db4c8601ead9);
        assert_eq!(fnv1a(b"fo"), 0x08985907b541d342);
        assert_eq!(fnv1a(b"foo"), 0xdcb27518fed9d577);
        assert_eq!(fnv1a(b"foob"), 0xdd120e790c2512af);
        assert_eq!(fnv1a(b"fooba"), 0xcac165afa2fef40a);
        assert_eq!(fnv1a(b"foobar"), 0x85944171f73967e8);
        assert_eq!(fnv1a(b"\0"), 0xaf63bd4c8601b7df);
        assert_eq!(fnv1a(b"a\0"), 0x089be207b544f1e4);
        assert_eq!(fnv1a(b"b\0"), 0x08a61407b54d9b5f);
        assert_eq!(fnv1a(b"c\0"), 0x08a2ae07b54ab836);
        assert_eq!(fnv1a(b"d\0"), 0x0891b007b53c4869);
        assert_eq!(fnv1a(b"e\0"), 0x088e4a07b5396540);
        assert_eq!(fnv1a(b"f\0"), 0x08987c07b5420ebb);
        assert_eq!(fnv1a(b"fo\0"), 0xdcb28a18fed9f926);
        assert_eq!(fnv1a(b"foo\0"), 0xdd1270790c25b935);
    }
}
