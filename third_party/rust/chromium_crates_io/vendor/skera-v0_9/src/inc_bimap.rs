//! inc_bimap: incremental bijective map, only lhs is given, rhs is incrementally assigned
//! ported from Harfbuzz hb_inc_bimap_t: <https://github.com/harfbuzz/harfbuzz/blob/b5a65e0f20c30a7f13b2f6619479a6d666e603e0/src/hb-bimap.hh#L97>

use crate::FastHashMap;
use core::slice::Iter;

#[derive(Default)]
pub(crate) struct IncBiMap {
    forw_map: FastHashMap<u32, u32>,
    back_map: Vec<u32>,
}

impl IncBiMap {
    pub(crate) fn with_capacity(capacity: usize) -> Self {
        Self {
            forw_map: FastHashMap::with_capacity_and_hasher(capacity, Default::default()),
            back_map: Vec::with_capacity(capacity),
        }
    }

    pub(crate) fn len(&self) -> usize {
        self.forw_map.len()
    }

    /// Add a mapping from lhs to rhs with a unique value if lhs is unknown.
    /// Return the rhs value as the result.
    pub(crate) fn add(&mut self, lhs: u32) -> u32 {
        match self.forw_map.get(&lhs) {
            Some(&rhs) => rhs,
            None => {
                let rhs = self.back_map.len() as u32;
                self.forw_map.insert(lhs, rhs);
                self.back_map.push(lhs);
                rhs
            }
        }
    }

    pub(crate) fn get(&self, lhs: u32) -> Option<&u32> {
        self.forw_map.get(&lhs)
    }

    pub(crate) fn get_backward(&self, rhs: u32) -> Option<&u32> {
        self.back_map.get(rhs as usize)
    }

    pub(crate) fn keys(&self) -> Iter<'_, u32> {
        self.back_map.iter()
    }

    // after finished adding all mappings in a random order,
    // reassign rhs to lhs so that they are in the same order.
    pub(crate) fn sort(&mut self) {
        // Every lhs is in back_map exactly once and already a key in forw_map, so
        // sort back_map in place and update the existing forw_map entries, instead of
        // cloning back_map and rebuilding both maps.
        self.back_map.sort_unstable();
        for (rhs, lhs) in self.back_map.iter().enumerate() {
            self.forw_map.insert(*lhs, rhs as u32);
        }
    }
}

impl FromIterator<u32> for IncBiMap {
    fn from_iter<I: IntoIterator<Item = u32>>(iter: I) -> Self {
        let mut m = IncBiMap::default();

        for lhs in iter {
            m.add(lhs);
        }
        m
    }
}

#[cfg(test)]
mod test {
    use super::IncBiMap;

    #[test]
    fn test_bimap() {
        let mut bimap = IncBiMap::default();

        assert_eq!(bimap.add(13), 0);
        assert_eq!(bimap.add(8), 1);
        assert_eq!(bimap.add(10), 2);
        assert_eq!(bimap.add(8), 1);
        assert_eq!(bimap.add(7), 3);
        assert_eq!(bimap.len(), 4);
        assert_eq!(bimap.get(7), Some(&3));
        assert_eq!(bimap.get_backward(0), Some(&13));
    }

    #[test]
    fn test_bimap_sort() {
        let mut bimap: IncBiMap = [13, 8, 10, 8, 7].into_iter().collect();
        bimap.sort();

        // rhs values are reassigned in lhs order
        assert_eq!(bimap.len(), 4);
        assert_eq!(bimap.keys().copied().collect::<Vec<_>>(), [7, 8, 10, 13]);
        for (rhs, lhs) in [7, 8, 10, 13].into_iter().enumerate() {
            assert_eq!(bimap.get(lhs), Some(&(rhs as u32)));
            assert_eq!(bimap.get_backward(rhs as u32), Some(&lhs));
        }
        assert_eq!(bimap.get(9), None);

        // adding after sorting continues from the end
        assert_eq!(bimap.add(9), 4);
    }
}
