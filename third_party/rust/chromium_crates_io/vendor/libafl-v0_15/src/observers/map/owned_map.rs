//! An observer that owns its map

use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    hash::{Hash, Hasher},
    ops::{Deref, DerefMut},
};

use libafl_bolts::{AsSlice, AsSliceMut, HasLen, Named};
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    Error,
    observers::{Observer, map::MapObserver},
};

/// Exact copy of `StdMapObserver` that owns its map
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct OwnedMapObserver<T> {
    map: Vec<T>,
    initial: T,
    name: Cow<'static, str>,
}

impl<I, S, T> Observer<I, S> for OwnedMapObserver<T>
where
    Self: MapObserver,
{
    #[inline]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.reset_map()
    }
}

impl<T> Named for OwnedMapObserver<T> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<T> HasLen for OwnedMapObserver<T> {
    #[inline]
    fn len(&self) -> usize {
        self.map.as_slice().len()
    }
}

impl<T> Hash for OwnedMapObserver<T>
where
    T: Hash,
{
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.as_slice().hash(hasher);
    }
}

impl<T> AsRef<Self> for OwnedMapObserver<T> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<T> AsMut<Self> for OwnedMapObserver<T> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<T> MapObserver for OwnedMapObserver<T>
where
    T: PartialEq + Copy + Hash + Serialize + DeserializeOwned + Debug,
{
    type Entry = T;

    #[inline]
    fn get(&self, pos: usize) -> T {
        self.as_slice()[pos]
    }

    #[inline]
    fn set(&mut self, pos: usize, val: Self::Entry) {
        self.as_slice_mut()[pos] = val;
    }

    /// Count the set bytes in the map
    fn count_bytes(&self) -> u64 {
        let initial = self.initial();
        let cnt = self.usable_count();
        let map = self.as_slice();
        let mut res = 0;
        for x in &map[0..cnt] {
            if *x != initial {
                res += 1;
            }
        }
        res
    }

    #[inline]
    fn usable_count(&self) -> usize {
        self.as_slice().len()
    }

    #[inline]
    fn initial(&self) -> T {
        self.initial
    }

    /// Reset the map
    #[inline]
    fn reset_map(&mut self) -> Result<(), Error> {
        // Normal memset, see https://rust.godbolt.org/z/Trs5hv
        let initial = self.initial();
        let cnt = self.usable_count();
        let map = self.as_slice_mut();
        for x in &mut map[0..cnt] {
            *x = initial;
        }
        Ok(())
    }
    fn to_vec(&self) -> Vec<T> {
        self.as_slice().to_vec()
    }

    fn how_many_set(&self, indexes: &[usize]) -> usize {
        let initial = self.initial();
        let cnt = self.usable_count();
        let map = self.as_slice();
        let mut res = 0;
        for i in indexes {
            if *i < cnt && map[*i] != initial {
                res += 1;
            }
        }
        res
    }
}

impl<T> Deref for OwnedMapObserver<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        &self.map
    }
}

impl<T> DerefMut for OwnedMapObserver<T> {
    fn deref_mut(&mut self) -> &mut [T] {
        &mut self.map
    }
}

impl<T> OwnedMapObserver<T>
where
    T: Copy + Default,
{
    /// Creates a new [`MapObserver`] with an owned map
    #[must_use]
    pub fn new(name: &'static str, map: Vec<T>) -> Self {
        let initial = if map.is_empty() { T::default() } else { map[0] };
        Self {
            map,
            name: Cow::from(name),
            initial,
        }
    }
}
