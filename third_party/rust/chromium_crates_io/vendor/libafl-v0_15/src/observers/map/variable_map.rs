//! Map observer with a shrinkable size

use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    hash::{Hash, Hasher},
    ops::{Deref, DerefMut},
};

use libafl_bolts::{
    AsSlice, AsSliceMut, HasLen, Named,
    ownedref::{OwnedMutPtr, OwnedMutSlice},
};
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    Error,
    observers::{Observer, VarLenMapObserver, map::MapObserver},
};

/// Overlooking a variable bitmap
#[derive(Serialize, Deserialize, Debug)]
#[expect(clippy::unsafe_derive_deserialize)]
pub struct VariableMapObserver<'a, T> {
    map: OwnedMutSlice<'a, T>,
    size: OwnedMutPtr<usize>,
    initial: T,
    name: Cow<'static, str>,
}

impl<I, S, T> Observer<I, S> for VariableMapObserver<'_, T>
where
    Self: MapObserver,
{
    #[inline]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.reset_map()
    }
}

impl<T> Named for VariableMapObserver<'_, T> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<T> HasLen for VariableMapObserver<'_, T> {
    #[inline]
    fn len(&self) -> usize {
        *self.size.as_ref()
    }
}

impl<T> Hash for VariableMapObserver<'_, T>
where
    T: Hash,
{
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.as_slice().hash(hasher);
    }
}

impl<T> AsRef<Self> for VariableMapObserver<'_, T> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<T> AsMut<Self> for VariableMapObserver<'_, T> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<T> MapObserver for VariableMapObserver<'_, T>
where
    T: PartialEq + Copy + Hash + Serialize + DeserializeOwned + Debug,
{
    type Entry = T;

    #[inline]
    fn initial(&self) -> T {
        self.initial
    }

    #[inline]
    fn usable_count(&self) -> usize {
        *self.size.as_ref()
    }

    fn get(&self, idx: usize) -> T {
        self.map.as_slice()[idx]
    }

    fn set(&mut self, idx: usize, val: T) {
        self.map.as_slice_mut()[idx] = val;
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

impl<T> VarLenMapObserver for VariableMapObserver<'_, T>
where
    T: PartialEq + Copy + Hash + Serialize + DeserializeOwned + Debug,
{
    fn map_slice(&self) -> &[Self::Entry] {
        self.map.as_ref()
    }

    fn map_slice_mut(&mut self) -> &mut [Self::Entry] {
        self.map.as_mut()
    }

    fn size(&self) -> &usize {
        self.size.as_ref()
    }

    fn size_mut(&mut self) -> &mut usize {
        self.size.as_mut()
    }
}

impl<T> Deref for VariableMapObserver<'_, T> {
    type Target = [T];
    fn deref(&self) -> &[T] {
        let cnt = *self.size.as_ref();
        &self.map[..cnt]
    }
}

impl<T> DerefMut for VariableMapObserver<'_, T> {
    fn deref_mut(&mut self) -> &mut [T] {
        let cnt = *self.size.as_ref();
        &mut self.map[..cnt]
    }
}

impl<'a, T> VariableMapObserver<'a, T>
where
    T: Default,
{
    /// Creates a new [`MapObserver`] from an [`OwnedMutSlice`]
    ///
    /// # Safety
    /// The observer will dereference the owned slice, as well as the `map_ptr`.
    /// Dereferences `map_ptr` with up to `max_len` elements of size.
    pub unsafe fn from_mut_slice(
        name: &'static str,
        map_slice: OwnedMutSlice<'a, T>,
        size: *mut usize,
    ) -> Self {
        VariableMapObserver {
            name: name.into(),
            map: map_slice,
            size: OwnedMutPtr::Ptr(size),
            initial: T::default(),
        }
    }

    /// Creates a new [`MapObserver`] from a raw pointer
    ///
    /// # Safety
    /// The observer will dereference the `size` ptr, as well as the `map_ptr`.
    /// Dereferences `map_ptr` with up to `max_len` elements of size.
    pub unsafe fn from_mut_ptr(
        name: &'static str,
        map_ptr: *mut T,
        max_len: usize,
        size: *mut usize,
    ) -> Self {
        unsafe {
            Self::from_mut_slice(
                name,
                OwnedMutSlice::from_raw_parts_mut(map_ptr, max_len),
                size,
            )
        }
    }
}
