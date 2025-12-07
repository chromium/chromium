use derivre::HashMap;
use std::{hash::Hash, num::NonZeroU32};

#[derive(Debug)]
pub struct HashId<T> {
    id: NonZeroU32,
    _marker: std::marker::PhantomData<T>,
}

impl<T> Clone for HashId<T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Copy for HashId<T> {}

impl<T> PartialEq for HashId<T> {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Eq for HashId<T> {}

impl<T> PartialOrd for HashId<T> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for HashId<T> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.id.cmp(&other.id)
    }
}

impl<T> Hash for HashId<T> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}

#[derive(Debug, Clone)]
pub struct HashCons<T> {
    by_t: HashMap<T, NonZeroU32>,
    all_t: Vec<T>,
}

impl<T: Eq + Hash + Clone> Default for HashCons<T> {
    fn default() -> Self {
        HashCons {
            by_t: HashMap::default(),
            all_t: Vec::new(),
        }
    }
}

impl<T: Eq + Hash + Clone> HashCons<T> {
    pub fn insert(&mut self, t: T) -> HashId<T> {
        if let Some(&id) = self.by_t.get(&t) {
            return HashId {
                id,
                _marker: std::marker::PhantomData,
            };
        }
        let idx = self.all_t.len();
        let id = NonZeroU32::new((idx + 1) as u32).unwrap();
        self.by_t.insert(t.clone(), id);
        self.all_t.push(t);
        HashId {
            id,
            _marker: std::marker::PhantomData,
        }
    }

    pub fn get(&self, id: HashId<T>) -> &T {
        &self.all_t[id.id.get() as usize - 1]
    }

    // pub fn len(&self) -> usize {
    //     self.all_t.len()
    // }
}
