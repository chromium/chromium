use core::hash::{BuildHasher, Hasher};
use siphasher::sip::SipHasher13;

#[derive(Clone, Debug, Default, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct CloneBuildHasher<H: Hasher + Clone> {
    hasher: H,
}

impl<H: Hasher + Clone> CloneBuildHasher<H> {
    #[allow(dead_code)]
    fn new(hasher: H) -> Self {
        Self { hasher }
    }
}

impl<H: Hasher + Clone> BuildHasher for CloneBuildHasher<H> {
    type Hasher = H;
    #[inline]
    fn build_hasher(&self) -> Self::Hasher {
        self.hasher.clone()
    }
}

/// The default hasher for `BloomFilter`.
///
/// `DefaultHasher` has a faster `build_hasher` than `std::collections::hash_map::RandomState` or `SipHasher13`.
/// This is important because `build_hasher` is called once for every actual hash.
pub type DefaultHasher = CloneBuildHasher<RandomDefaultHasher>;

impl DefaultHasher {
    pub fn seeded(seed: &[u8; 16]) -> Self {
        Self {
            hasher: RandomDefaultHasher::seeded(seed),
        }
    }
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct RandomDefaultHasher(SipHasher13);

impl RandomDefaultHasher {
    #[inline]
    pub fn seeded(seed: &[u8; 16]) -> Self {
        Self(SipHasher13::new_with_key(seed))
    }
}

impl Default for RandomDefaultHasher {
    #[inline]
    fn default() -> Self {
        #[cfg(not(feature = "rand"))]
        {
            use foldhash::fast::RandomState;

            // create two random states
            let state_a = RandomState::default();
            let state_b = RandomState::default();

            // combine the two random states into a single 128-bit seed
            let low = state_a.build_hasher().finish() as u128;
            let high = state_b.build_hasher().finish() as u128;

            Self::seeded(&((high << 64) | low).to_ne_bytes())
        }
        #[cfg(feature = "rand")]
        {
            let mut seed = [0u8; 16];
            use rand::RngCore;
            rand::rng().fill_bytes(&mut seed);
            Self::seeded(&seed)
        }
    }
}

impl Hasher for RandomDefaultHasher {
    #[inline]
    fn finish(&self) -> u64 {
        self.0.finish()
    }
    #[inline]
    fn write(&mut self, bytes: &[u8]) {
        self.0.write(bytes)
    }
    #[inline]
    fn write_u8(&mut self, i: u8) {
        self.0.write_u8(i)
    }
    #[inline]
    fn write_u16(&mut self, i: u16) {
        self.0.write_u16(i)
    }
    #[inline]
    fn write_u32(&mut self, i: u32) {
        self.0.write_u32(i)
    }
    #[inline]
    fn write_u64(&mut self, i: u64) {
        self.0.write_u64(i)
    }
    #[inline]
    fn write_u128(&mut self, i: u128) {
        self.0.write_u128(i)
    }
    #[inline]
    fn write_usize(&mut self, i: usize) {
        self.0.write_usize(i)
    }
    #[inline]
    fn write_i8(&mut self, i: i8) {
        self.0.write_i8(i)
    }
    #[inline]
    fn write_i16(&mut self, i: i16) {
        self.0.write_i16(i)
    }
    #[inline]
    fn write_i32(&mut self, i: i32) {
        self.0.write_i32(i)
    }
    #[inline]
    fn write_i64(&mut self, i: i64) {
        self.0.write_i64(i)
    }
    #[inline]
    fn write_i128(&mut self, i: i128) {
        self.0.write_i128(i)
    }
    #[inline]
    fn write_isize(&mut self, i: isize) {
        self.0.write_isize(i)
    }
}

#[cfg(test)]
mod test {
    use crate::hasher::RandomDefaultHasher;
    use core::hash::Hasher;
    use siphasher::sip::SipHasher13;

    fn hash_all(mut x: impl Hasher) -> u64 {
        x.write(&[1; 16]);
        x.write_u8(1);
        x.write_u16(1);
        x.write_u32(1);
        x.write_u64(1);
        x.write_u128(1);
        x.write_usize(1);
        x.write_i8(1);
        x.write_i16(1);
        x.write_i32(1);
        x.write_i64(1);
        x.write_i128(1);
        x.write_isize(1);
        x.finish()
    }

    #[test]
    fn test_hasher() {
        let h1 = RandomDefaultHasher::seeded(&[0; 16]);
        let h2 = SipHasher13::new_with_key(&[0; 16]);
        assert_eq!(hash_all(h1), hash_all(h2),);
    }

    #[test]
    fn test_random_default_hasher() {
        // two different instances of RandomDefaultHasher should have different seeds
        let h1 = RandomDefaultHasher::default();
        let h2 = RandomDefaultHasher::default();
        assert_ne!(h1.finish(), h2.finish());

        // same seed value should result in the same hash
        let h3 = RandomDefaultHasher::seeded(&[0; 16]);
        let h4 = RandomDefaultHasher::seeded(&[0; 16]);
        assert_eq!(h3.finish(), h4.finish());

        // different seed value should result in different hash
        let h5 = RandomDefaultHasher::seeded(&[1; 16]);
        let h6 = RandomDefaultHasher::seeded(&[2; 16]);
        assert_ne!(h5.finish(), h6.finish());
    }
}

#[derive(Clone, Copy)]
pub(crate) struct DoubleHasher {
    h1: u64,
    h2: u64,
}

impl DoubleHasher {
    /// The first two hashes of the value, h1 and h2.
    ///
    /// Subsequent hashes, h, are efficiently derived from these two using `next_hash`.
    ///
    /// This strategy is a modified version of <https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf>.
    #[inline]
    pub(crate) fn new(h1: u64) -> Self {
        // 0xffff_ffff_ffff_ffff / 0x517c_c1b7_2722_0a95 = π
        let h2 = h1.wrapping_mul(0x51_7c_c1_b7_27_22_0a_95);
        Self { h1, h2 }
    }

    /// "Double hashing" produces a new hash efficiently from two orignal hashes.
    ///
    /// Modified from <https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf>.
    #[inline]
    pub(crate) fn next(&mut self) -> u64 {
        self.h1 = self.h1.rotate_left(5).wrapping_add(self.h2);
        self.h1
    }
}
