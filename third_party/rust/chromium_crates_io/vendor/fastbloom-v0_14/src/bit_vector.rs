use crate::AtomicU64;
use alloc::{boxed::Box, vec::Vec};
use core::sync::atomic::Ordering::Relaxed;

/// A bit vector partitioned in to `u64` blocks.
#[derive(Debug, Clone)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct BitVec {
    bits: Box<[u64]>,
}

/// A bit vector partitioned in to `u64` blocks.
#[derive(Debug)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct AtomicBitVec {
    bits: Box<[AtomicU64]>,
}

macro_rules! impl_bitvec {
    ($name:ident, $bits:ty) => {
        impl $name {
            #[inline]
            pub(crate) const fn len(&self) -> usize {
                self.bits.len()
            }

            #[inline]
            pub(crate) const fn num_bits(&self) -> usize {
                self.len() * u64::BITS as usize
            }

            #[inline(always)]
            pub(crate) fn as_slice(&self) -> &[$bits] {
                &self.bits
            }

            #[inline]
            pub(crate) fn iter(&self) -> impl Iterator<Item = u64> + '_ {
                self.bits.iter().map(Self::fetch)
            }

            #[inline(always)]
            pub(crate) fn check(&self, index: usize) -> bool {
                let (index, bit) = coord(index);
                Self::fetch(&self.bits[index]) & bit > 0
            }
        }

        impl FromIterator<u64> for $name {
            fn from_iter<I: IntoIterator<Item = u64>>(iter: I) -> Self {
                let mut bits = iter.into_iter().map(Self::new).collect::<Vec<_>>();
                bits.shrink_to_fit();
                Self { bits: bits.into() }
            }
        }

        impl PartialEq for $name {
            fn eq(&self, other: &Self) -> bool {
                if self.len() != other.len() {
                    return false;
                }
                core::iter::zip(self.iter(), other.iter()).all(|(l, r)| l == r)
            }
        }
        impl Eq for $name {}
    };
}

impl_bitvec!(BitVec, u64);
impl_bitvec!(AtomicBitVec, AtomicU64);

impl BitVec {
    #[inline(always)]
    fn new(x: u64) -> u64 {
        x
    }

    #[inline(always)]
    fn fetch(x: &u64) -> u64 {
        *x
    }

    #[inline(always)]
    pub(crate) fn set(&mut self, index: usize) -> bool {
        let (index, bit) = coord(index);
        let previously_contained = self.bits[index] & bit > 0;
        self.bits[index] |= bit;
        previously_contained
    }

    #[inline]
    pub(crate) fn clear(&mut self) {
        for i in 0..self.len() {
            self.bits[i] = 0;
        }
    }

    #[inline]
    pub(crate) fn union(&mut self, other: &BitVec) {
        assert_eq!(self.len(), other.len(), "expected same length");
        for i in 0..self.len() {
            self.bits[i] |= other.bits[i];
        }
    }

    #[inline]
    pub(crate) fn intersect(&mut self, other: &BitVec) {
        assert_eq!(self.len(), other.len(), "expected same length");
        for i in 0..self.len() {
            self.bits[i] &= other.bits[i];
        }
    }
}

impl AtomicBitVec {
    #[inline(always)]
    fn new(x: u64) -> AtomicU64 {
        AtomicU64::new(x)
    }

    #[inline(always)]
    fn fetch(x: &AtomicU64) -> u64 {
        x.load(Relaxed)
    }

    #[inline(always)]
    pub(crate) fn set(&self, index: usize) -> bool {
        let (index, bit) = coord(index);
        self.bits[index].fetch_or(bit, Relaxed) & bit > 0
    }

    #[inline]
    pub(crate) fn clear(&self) {
        for i in 0..self.len() {
            self.bits[i].store(0, Relaxed);
        }
    }

    #[inline]
    pub(crate) fn union(&self, other: &AtomicBitVec) {
        assert_eq!(self.len(), other.len(), "expected same length");
        for i in 0..self.len() {
            let x = other.bits[i].load(Relaxed);
            self.bits[i].fetch_or(x, Relaxed);
        }
    }

    #[inline]
    pub(crate) fn intersect(&self, other: &AtomicBitVec) {
        assert_eq!(self.len(), other.len(), "expected same length");
        for i in 0..self.len() {
            let x = other.bits[i].load(Relaxed);
            self.bits[i].fetch_and(x, Relaxed);
        }
    }
}

impl Clone for AtomicBitVec {
    fn clone(&self) -> Self {
        self.iter().collect()
    }
}

#[inline]
fn coord(index: usize) -> (usize, u64) {
    (index >> 6, 1u64 << (index & 0b111111))
}

macro_rules! impl_tests {
    ($modname:ident, $name:ident) => {
        #[allow(unused_mut)]
        #[cfg(not(feature = "loom"))]
        #[cfg(test)]
        mod $modname {
            use super::*;
            use core::iter::repeat;
            use rand::Rng;

            #[test]
            fn test_to_from_vec() {
                let size = 42;
                let b: BitVec = repeat(0).take(size).collect();
                assert_eq!(b.num_bits(), b.len() * 64);
                assert!(size <= b.len());
                assert!((size + 64) > b.len());
            }

            #[test]
            fn test_only_random_inserts_are_contained() {
                let mut vec: BitVec = repeat(0).take(80).collect();
                let mut control = Vec::with_capacity(1000);
                let mut rng = rand::rng();

                for _ in 0..1000 {
                    let index = rng.random_range(0..vec.num_bits());

                    if !control.contains(&index) {
                        assert!(!vec.check(index));
                    }
                    control.push(index);
                    vec.set(index);
                    assert!(vec.check(index));
                }
            }
        }
    };
}

impl_tests!(non_atomic, BitVec);
impl_tests!(atomic, AtomicBitVec);
