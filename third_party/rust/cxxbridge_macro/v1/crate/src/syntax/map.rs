use std::borrow::Borrow;
use std::hash::Hash;
use std::ops::Index;
use std::slice;

pub(crate) use self::ordered::OrderedMap;
pub(crate) use self::unordered::UnorderedMap;
pub(crate) use std::collections::hash_map::Entry;

mod ordered {
    use super::{Entry, Iter, UnorderedMap};
    use std::borrow::Borrow;
    use std::hash::Hash;
    use std::mem;

    pub(crate) struct OrderedMap<K, V> {
        map: UnorderedMap<K, usize>,
        vec: Vec<(K, V)>,
    }

    impl<K, V> OrderedMap<K, V> {
        pub(crate) fn new() -> Self {
            OrderedMap {
                map: UnorderedMap::new(),
                vec: Vec::new(),
            }
        }

        pub(crate) fn iter(&self) -> Iter<K, V> {
            Iter(self.vec.iter())
        }

        #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
        pub(crate) fn keys(&self) -> impl Iterator<Item = &K> {
            self.vec.iter().map(|(k, _v)| k)
        }
    }

    impl<K, V> OrderedMap<K, V>
    where
        K: Copy + Hash + Eq,
    {
        pub(crate) fn insert(&mut self, key: K, value: V) -> Option<V> {
            match self.map.entry(key) {
                Entry::Occupied(entry) => {
                    let i = &mut self.vec[*entry.get()];
                    Some(mem::replace(&mut i.1, value))
                }
                Entry::Vacant(entry) => {
                    entry.insert(self.vec.len());
                    self.vec.push((key, value));
                    None
                }
            }
        }

        pub(crate) fn contains_key<Q>(&self, key: &Q) -> bool
        where
            K: Borrow<Q>,
            Q: ?Sized + Hash + Eq,
        {
            self.map.contains_key(key)
        }
    }

    impl<'a, K, V> IntoIterator for &'a OrderedMap<K, V> {
        type Item = (&'a K, &'a V);
        type IntoIter = Iter<'a, K, V>;
        fn into_iter(self) -> Self::IntoIter {
            self.iter()
        }
    }
}

mod unordered {
    use crate::syntax::set::UnorderedSet;
    use std::borrow::Borrow;
    use std::collections::hash_map::{Entry, HashMap};
    use std::hash::Hash;

    // Wrapper prohibits accidentally introducing iteration over the map, which
    // could lead to nondeterministic generated code.
    pub(crate) struct UnorderedMap<K, V>(HashMap<K, V>);

    impl<K, V> UnorderedMap<K, V> {
        pub(crate) fn new() -> Self {
            UnorderedMap(HashMap::new())
        }
    }

    impl<K, V> UnorderedMap<K, V>
    where
        K: Hash + Eq,
    {
        pub(crate) fn insert(&mut self, key: K, value: V) -> Option<V> {
            self.0.insert(key, value)
        }

        pub(crate) fn contains_key<Q>(&self, key: &Q) -> bool
        where
            K: Borrow<Q>,
            Q: ?Sized + Hash + Eq,
        {
            self.0.contains_key(key)
        }

        pub(crate) fn get<Q>(&self, key: &Q) -> Option<&V>
        where
            K: Borrow<Q>,
            Q: ?Sized + Hash + Eq,
        {
            self.0.get(key)
        }

        pub(crate) fn entry(&mut self, key: K) -> Entry<K, V> {
            self.0.entry(key)
        }

        #[allow(dead_code)] // only used by cxx-build, not cxxbridge-macro
        pub(crate) fn remove<Q>(&mut self, key: &Q) -> Option<V>
        where
            K: Borrow<Q>,
            Q: ?Sized + Hash + Eq,
        {
            self.0.remove(key)
        }

        pub(crate) fn keys(&self) -> UnorderedSet<K>
        where
            K: Copy,
        {
            let mut set = UnorderedSet::new();
            for key in self.0.keys() {
                set.insert(*key);
            }
            set
        }
    }
}

pub(crate) struct Iter<'a, K, V>(slice::Iter<'a, (K, V)>);

impl<'a, K, V> Iterator for Iter<'a, K, V> {
    type Item = (&'a K, &'a V);

    fn next(&mut self) -> Option<Self::Item> {
        let (k, v) = self.0.next()?;
        Some((k, v))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.0.size_hint()
    }
}

impl<K, V> Default for UnorderedMap<K, V> {
    fn default() -> Self {
        UnorderedMap::new()
    }
}

impl<Q, K, V> Index<&Q> for UnorderedMap<K, V>
where
    K: Borrow<Q> + Hash + Eq,
    Q: ?Sized + Hash + Eq,
{
    type Output = V;

    fn index(&self, key: &Q) -> &V {
        self.get(key).unwrap()
    }
}
