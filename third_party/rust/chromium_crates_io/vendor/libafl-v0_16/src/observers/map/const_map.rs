//! Map observer with a const size

use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    hash::{Hash, Hasher},
    ops::{Deref, DerefMut},
    ptr::NonNull,
};

use libafl_bolts::{HasLen, Named, ownedref::OwnedMutSizedSlice};
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    Error,
    observers::{ConstLenMapObserver, Observer, map::MapObserver},
};

/// Use a const size to speedup `Feedback::is_interesting` when the user can
/// know the size of the map at compile time.
#[derive(Serialize, Deserialize, Debug)]
#[expect(clippy::unsafe_derive_deserialize)]
pub struct ConstMapObserver<'a, T, const N: usize> {
    map: OwnedMutSizedSlice<'a, T, N>,
    initial: T,
    name: Cow<'static, str>,
}

impl<I, S, T, const N: usize> Observer<I, S> for ConstMapObserver<'_, T, N>
where
    Self: MapObserver,
{
    #[inline]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.reset_map()
    }
}

impl<T, const N: usize> Named for ConstMapObserver<'_, T, N> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<T, const N: usize> HasLen for ConstMapObserver<'_, T, N> {
    #[inline]
    fn len(&self) -> usize {
        N
    }
}

impl<T, const N: usize> Hash for ConstMapObserver<'_, T, N>
where
    T: Hash,
{
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.map.as_slice().hash(hasher);
    }
}
impl<T, const N: usize> AsRef<Self> for ConstMapObserver<'_, T, N> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<T, const N: usize> AsMut<Self> for ConstMapObserver<'_, T, N> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<T, const N: usize> MapObserver for ConstMapObserver<'_, T, N>
where
    T: PartialEq + Copy + Hash + Serialize + DeserializeOwned + Debug + 'static,
{
    type Entry = T;

    #[inline]
    fn initial(&self) -> T {
        self.initial
    }

    #[inline]
    fn get(&self, idx: usize) -> T {
        self[idx]
    }

    #[inline]
    fn set(&mut self, idx: usize, val: T) {
        (*self)[idx] = val;
    }

    /// Count the set bytes in the map
    fn count_bytes(&self) -> u64 {
        let initial = self.initial();
        let cnt = self.usable_count();
        let map = self.map.as_slice();
        let mut res = 0;
        for x in &map[0..cnt] {
            if *x != initial {
                res += 1;
            }
        }
        res
    }

    fn usable_count(&self) -> usize {
        self.len()
    }

    /// Reset the map
    #[inline]
    fn reset_map(&mut self) -> Result<(), Error> {
        // Normal memset, see https://rust.godbolt.org/z/Trs5hv
        let initial = self.initial();
        let cnt = self.usable_count();
        let map = &mut (*self);
        for x in &mut map[0..cnt] {
            *x = initial;
        }
        Ok(())
    }

    fn to_vec(&self) -> Vec<T> {
        self.map.to_vec()
    }

    /// Get the number of set entries with the specified indexes
    fn how_many_set(&self, indexes: &[usize]) -> usize {
        let initial = self.initial();
        let cnt = self.usable_count();
        let map = self.map.as_slice();
        let mut res = 0;
        for i in indexes {
            if *i < cnt && map[*i] != initial {
                res += 1;
            }
        }
        res
    }
}

impl<T, const N: usize> ConstLenMapObserver<N> for ConstMapObserver<'_, T, N>
where
    T: PartialEq + Copy + Hash + Serialize + DeserializeOwned + Debug + 'static,
{
    fn map_slice(&self) -> &[Self::Entry; N] {
        &self.map
    }

    fn map_slice_mut(&mut self) -> &mut [Self::Entry; N] {
        &mut self.map
    }
}

impl<T, const N: usize> Deref for ConstMapObserver<'_, T, N> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        self.map.as_slice()
    }
}

impl<T, const N: usize> DerefMut for ConstMapObserver<'_, T, N> {
    fn deref_mut(&mut self) -> &mut [T] {
        self.map.as_mut_slice()
    }
}

impl<'a, T, const N: usize> ConstMapObserver<'a, T, N>
where
    T: Default,
{
    /// Creates a new [`MapObserver`]
    ///
    /// # Note
    /// Will get a pointer to the map and dereference it at any point in time.
    /// The map must not move in memory!
    #[must_use]
    pub fn new(name: &'static str, map: &'a mut [T; N]) -> Self {
        assert!(map.len() >= N);
        Self {
            map: OwnedMutSizedSlice::from(map),
            name: Cow::from(name),
            initial: T::default(),
        }
    }

    /// Creates a new [`MapObserver`] from a raw pointer
    ///
    /// # Safety
    /// Will dereference the `map_ptr` with up to len elements.
    #[must_use]
    pub unsafe fn from_mut_ptr(name: &'static str, map_ptr: NonNull<[T; N]>) -> Self {
        unsafe {
            ConstMapObserver {
                map: OwnedMutSizedSlice::from_raw_mut(map_ptr),
                name: Cow::from(name),
                initial: T::default(),
            }
        }
    }

    /// Gets the initial value for this map, mutably
    pub fn initial_mut(&mut self) -> &mut T {
        &mut self.initial
    }
}
