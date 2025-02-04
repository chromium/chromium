// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::ShortBoxSlice;
use super::ShortBoxSliceInner;
use super::ShortBoxSliceIntoIter;
use alloc::vec::Vec;
use litemap::store::*;

impl<K, V> StoreConstEmpty<K, V> for ShortBoxSlice<(K, V)> {
    const EMPTY: ShortBoxSlice<(K, V)> = ShortBoxSlice::new();
}

impl<K, V> Store<K, V> for ShortBoxSlice<(K, V)> {
    #[inline]
    fn lm_len(&self) -> usize {
        self.len()
    }

    #[inline]
    fn lm_is_empty(&self) -> bool {
        use ShortBoxSliceInner::*;
        matches!(self.0, ZeroOne(None))
    }

    #[inline]
    fn lm_get(&self, index: usize) -> Option<(&K, &V)> {
        self.get(index).map(|elt| (&elt.0, &elt.1))
    }

    #[inline]
    fn lm_last(&self) -> Option<(&K, &V)> {
        use ShortBoxSliceInner::*;
        match self.0 {
            ZeroOne(ref v) => v.as_ref(),
            Multi(ref v) => v.last(),
        }
        .map(|elt| (&elt.0, &elt.1))
    }

    #[inline]
    fn lm_binary_search_by<F>(&self, mut cmp: F) -> Result<usize, usize>
    where
        F: FnMut(&K) -> core::cmp::Ordering,
    {
        self.binary_search_by(|(k, _)| cmp(k))
    }
}

impl<K: Ord, V> StoreFromIterable<K, V> for ShortBoxSlice<(K, V)> {
    fn lm_sort_from_iter<I: IntoIterator<Item = (K, V)>>(iter: I) -> Self {
        let v: Vec<(K, V)> = Vec::lm_sort_from_iter(iter);
        v.into()
    }
}

impl<K, V> StoreMut<K, V> for ShortBoxSlice<(K, V)> {
    fn lm_with_capacity(_capacity: usize) -> Self {
        ShortBoxSlice::new()
    }

    fn lm_reserve(&mut self, _additional: usize) {}

    fn lm_get_mut(&mut self, index: usize) -> Option<(&K, &mut V)> {
        self.get_mut(index).map(|elt| (&elt.0, &mut elt.1))
    }

    fn lm_push(&mut self, key: K, value: V) {
        self.push((key, value))
    }

    fn lm_insert(&mut self, index: usize, key: K, value: V) {
        self.insert(index, (key, value))
    }

    fn lm_remove(&mut self, index: usize) -> (K, V) {
        self.remove(index)
    }

    fn lm_clear(&mut self) {
        self.clear();
    }

    fn lm_retain<F>(&mut self, mut predicate: F)
    where
        F: FnMut(&K, &V) -> bool,
    {
        self.retain(|(k, v)| predicate(k, v))
    }
}

impl<'a, K: 'a, V: 'a> StoreIterable<'a, K, V> for ShortBoxSlice<(K, V)> {
    type KeyValueIter =
        core::iter::Map<core::slice::Iter<'a, (K, V)>, for<'r> fn(&'r (K, V)) -> (&'r K, &'r V)>;

    fn lm_iter(&'a self) -> Self::KeyValueIter {
        self.iter().map(|elt| (&elt.0, &elt.1))
    }
}

impl<K, V> StoreFromIterator<K, V> for ShortBoxSlice<(K, V)> {}

impl<'a, K: 'a, V: 'a> StoreIterableMut<'a, K, V> for ShortBoxSlice<(K, V)> {
    type KeyValueIterMut = core::iter::Map<
        core::slice::IterMut<'a, (K, V)>,
        for<'r> fn(&'r mut (K, V)) -> (&'r K, &'r mut V),
    >;

    fn lm_iter_mut(
        &'a mut self,
    ) -> <Self as litemap::store::StoreIterableMut<'a, K, V>>::KeyValueIterMut {
        self.iter_mut().map(|elt| (&elt.0, &mut elt.1))
    }
}

impl<K, V> StoreIntoIterator<K, V> for ShortBoxSlice<(K, V)> {
    type KeyValueIntoIter = ShortBoxSliceIntoIter<(K, V)>;

    fn lm_into_iter(self) -> Self::KeyValueIntoIter {
        self.into_iter()
    }

    // leave lm_extend_end as default

    // leave lm_extend_start as default
}

#[test]
fn test_short_slice_impl() {
    litemap::testing::check_store::<ShortBoxSlice<(u32, u64)>>();
}

#[test]
fn test_short_slice_impl_full() {
    litemap::testing::check_store_full::<ShortBoxSlice<(u32, u64)>>();
}
