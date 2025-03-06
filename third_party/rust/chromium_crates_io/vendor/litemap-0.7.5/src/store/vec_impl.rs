// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::*;
use alloc::vec::Vec;

type MapF<K, V> = fn(&(K, V)) -> (&K, &V);

#[inline]
fn map_f<K, V>(input: &(K, V)) -> (&K, &V) {
    (&input.0, &input.1)
}

type MapFMut<K, V> = fn(&mut (K, V)) -> (&K, &mut V);

#[inline]
fn map_f_mut<K, V>(input: &mut (K, V)) -> (&K, &mut V) {
    (&input.0, &mut input.1)
}

impl<K, V> StoreConstEmpty<K, V> for Vec<(K, V)> {
    const EMPTY: Vec<(K, V)> = Vec::new();
}

impl<K, V> Store<K, V> for Vec<(K, V)> {
    #[inline]
    fn lm_len(&self) -> usize {
        self.as_slice().len()
    }

    #[inline]
    fn lm_is_empty(&self) -> bool {
        self.as_slice().is_empty()
    }

    #[inline]
    fn lm_get(&self, index: usize) -> Option<(&K, &V)> {
        self.as_slice().get(index).map(map_f)
    }

    #[inline]
    fn lm_last(&self) -> Option<(&K, &V)> {
        self.as_slice().last().map(map_f)
    }

    #[inline]
    fn lm_binary_search_by<F>(&self, mut cmp: F) -> Result<usize, usize>
    where
        F: FnMut(&K) -> Ordering,
    {
        self.as_slice().binary_search_by(|(k, _)| cmp(k))
    }
}

impl<K, V> StoreSlice<K, V> for Vec<(K, V)> {
    type Slice = [(K, V)];

    fn lm_get_range(&self, range: Range<usize>) -> Option<&Self::Slice> {
        self.get(range)
    }
}

impl<K, V> StoreMut<K, V> for Vec<(K, V)> {
    #[inline]
    fn lm_with_capacity(capacity: usize) -> Self {
        Self::with_capacity(capacity)
    }

    #[inline]
    fn lm_reserve(&mut self, additional: usize) {
        self.reserve(additional)
    }

    #[inline]
    fn lm_get_mut(&mut self, index: usize) -> Option<(&K, &mut V)> {
        self.as_mut_slice().get_mut(index).map(map_f_mut)
    }

    #[inline]
    fn lm_push(&mut self, key: K, value: V) {
        self.push((key, value))
    }

    #[inline]
    fn lm_insert(&mut self, index: usize, key: K, value: V) {
        self.insert(index, (key, value))
    }

    #[inline]
    fn lm_remove(&mut self, index: usize) -> (K, V) {
        self.remove(index)
    }

    #[inline]
    fn lm_clear(&mut self) {
        self.clear()
    }

    #[inline]
    fn lm_retain<F>(&mut self, mut predicate: F)
    where
        F: FnMut(&K, &V) -> bool,
    {
        self.retain(|(k, v)| predicate(k, v))
    }
}

impl<K: Ord, V> StoreFromIterable<K, V> for Vec<(K, V)> {
    fn lm_sort_from_iter<I: IntoIterator<Item = (K, V)>>(iter: I) -> Self {
        let iter = iter.into_iter();
        let mut container = match iter.size_hint() {
            (_, Some(upper)) => Self::with_capacity(upper),
            (lower, None) => Self::with_capacity(lower),
        };

        for (key, value) in iter {
            if let Some(last) = container.lm_last() {
                if last.0 >= &key {
                    match container.lm_binary_search_by(|k| k.cmp(&key)) {
                        #[allow(clippy::unwrap_used)] // Index came from binary_search
                        Ok(found) => {
                            let _ =
                                core::mem::replace(container.lm_get_mut(found).unwrap().1, value);
                        }
                        Err(ins) => {
                            container.insert(ins, (key, value));
                        }
                    }
                } else {
                    container.push((key, value))
                }
            } else {
                container.push((key, value))
            }
        }

        container
    }
}

impl<'a, K: 'a, V: 'a> StoreIterable<'a, K, V> for Vec<(K, V)> {
    type KeyValueIter = core::iter::Map<core::slice::Iter<'a, (K, V)>, MapF<K, V>>;

    #[inline]
    fn lm_iter(&'a self) -> Self::KeyValueIter {
        self.as_slice().iter().map(map_f)
    }
}

impl<'a, K: 'a, V: 'a> StoreIterableMut<'a, K, V> for Vec<(K, V)> {
    type KeyValueIterMut = core::iter::Map<core::slice::IterMut<'a, (K, V)>, MapFMut<K, V>>;

    #[inline]
    fn lm_iter_mut(&'a mut self) -> Self::KeyValueIterMut {
        self.as_mut_slice().iter_mut().map(map_f_mut)
    }
}

impl<K, V> StoreIntoIterator<K, V> for Vec<(K, V)> {
    type KeyValueIntoIter = alloc::vec::IntoIter<(K, V)>;

    #[inline]
    fn lm_into_iter(self) -> Self::KeyValueIntoIter {
        IntoIterator::into_iter(self)
    }

    #[inline]
    fn lm_extend_end(&mut self, other: Self) {
        self.extend(other)
    }

    #[inline]
    fn lm_extend_start(&mut self, other: Self) {
        self.splice(0..0, other);
    }
}

impl<K, V> StoreFromIterator<K, V> for Vec<(K, V)> {}

#[test]
fn test_vec_impl() {
    crate::testing::check_store_full::<Vec<(u32, u64)>>();
}
