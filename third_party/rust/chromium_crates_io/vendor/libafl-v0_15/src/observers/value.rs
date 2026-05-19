//! A simple observer with a single value.

use alloc::{borrow::Cow, boxed::Box, vec::Vec};
use core::{
    cell::{Ref, RefCell, RefMut},
    fmt::Debug,
    hash::{Hash, Hasher},
    ops::{Deref, DerefMut},
};

use ahash::RandomState;
use libafl_bolts::{AsIter, AsIterMut, AsSlice, AsSliceMut, HasLen, Named, ownedref::OwnedRef};
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    Error,
    observers::{DifferentialObserver, MapObserver, Observer, ObserverWithHashField},
};

/// A simple observer with a single value.
///
/// The intent is that the value is something with interior mutability which the target could write to even though this
/// observer has a reference to it. Use [`RefCellValueObserver`] if using a [`RefCell`] around the value.
#[derive(Serialize, Deserialize, Debug)]
#[expect(clippy::unsafe_derive_deserialize)]
pub struct ValueObserver<'a, T> {
    /// The name of this observer.
    name: Cow<'static, str>,
    /// The value.
    pub value: OwnedRef<'a, T>,
}

impl<'a, T> ValueObserver<'a, T> {
    /// Creates a new [`ValueObserver`] with the given name.
    #[must_use]
    pub fn new(name: &'static str, value: OwnedRef<'a, T>) -> Self {
        Self {
            name: Cow::from(name),
            value,
        }
    }

    /// Get a reference to the underlying value.
    #[must_use]
    pub fn get_ref(&self) -> &T {
        self.value.as_ref()
    }

    /// Set the value.
    pub fn set(&mut self, new_value: T) {
        self.value = OwnedRef::Owned(Box::new(new_value));
    }

    /// Clone or move the current value out of this object.
    #[must_use]
    pub fn take(self) -> T
    where
        T: Clone,
    {
        match self.value {
            OwnedRef::RefRaw(r, _) => unsafe { (*r).clone() },
            OwnedRef::Ref(r) => r.clone(),
            OwnedRef::Owned(v) => *v,
        }
    }
}

/// This *does not* reset the value inside the observer.
impl<I, S, T> Observer<I, S> for ValueObserver<'_, T> {}

impl<T> Named for ValueObserver<'_, T> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<T: Hash> ObserverWithHashField for ValueObserver<'_, T> {
    fn hash(&self) -> Option<u64> {
        Some(RandomState::with_seeds(1, 2, 3, 4).hash_one(self.value.as_ref()))
    }
}

impl<OTA, OTB, I, S, T> DifferentialObserver<OTA, OTB, I, S> for ValueObserver<'_, T> {}

/// A simple observer with a single [`RefCell`]'d value.
#[derive(Serialize, Deserialize, Debug)]
#[expect(clippy::unsafe_derive_deserialize)]
pub struct RefCellValueObserver<'a, T> {
    /// The name of this observer.
    name: Cow<'static, str>,
    /// The value.
    pub value: OwnedRef<'a, RefCell<T>>,
}

impl<'a, T> RefCellValueObserver<'a, T> {
    /// Creates a new [`RefCellValueObserver`] with the given name.
    #[must_use]
    pub fn new(name: &'static str, value: OwnedRef<'a, RefCell<T>>) -> Self {
        Self {
            name: Cow::from(name),
            value,
        }
    }

    /// Get a reference to the underlying value.
    ///
    /// Panics if it can't borrow.
    #[must_use]
    pub fn get_ref<'b>(&'b self) -> Ref<'a, T>
    where
        'b: 'a,
    {
        self.value.as_ref().borrow()
    }

    /// Get a mutable reference to the underlying value.
    ///
    /// Panics if it can't borrow.
    #[must_use]
    pub fn get_ref_mut<'b>(&'b self) -> RefMut<'a, T>
    where
        'b: 'a,
    {
        self.value.as_ref().borrow_mut()
    }

    /// Set the value.
    pub fn set(&mut self, new_value: T) {
        self.value.as_ref().replace(new_value);
    }

    /// Clone or move the current value out of this object.
    #[must_use]
    pub fn take(self) -> T
    where
        T: Clone,
    {
        match self.value {
            OwnedRef::RefRaw(r, _) => unsafe { (*r).borrow().deref().clone() },
            OwnedRef::Ref(r) => r.borrow().deref().clone(),
            OwnedRef::Owned(v) => v.borrow().clone(),
        }
    }
}

/// This *does not* reset the value inside the observer.
impl<I, S, T> Observer<I, S> for RefCellValueObserver<'_, T> {}

impl<T> Named for RefCellValueObserver<'_, T> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<T> ObserverWithHashField for RefCellValueObserver<'_, T>
where
    T: Hash,
{
    fn hash(&self) -> Option<u64> {
        Some(RandomState::with_seeds(1, 2, 3, 4).hash_one(&*self.value.as_ref().borrow()))
    }
}

impl<OTA, OTB, I, S, T> DifferentialObserver<OTA, OTB, I, S> for RefCellValueObserver<'_, T> {}

/// [`Iterator`] over [`RefCellValueObserver`] of a [`Deref`] to `[T]`.
#[derive(Debug)]
pub struct RefCellValueObserverIter<'it, T> {
    v: Ref<'it, [T]>,
}

impl<'it, T: 'it, A: Deref<Target = [T]>> AsSlice<'it> for RefCellValueObserver<'_, A> {
    type Entry = T;
    type SliceRef = Ref<'it, [T]>;

    fn as_slice(&'it self) -> Self::SliceRef {
        Ref::map(self.value.as_ref().borrow(), |s| &**s)
    }
}

impl<'it, T: 'it, A: DerefMut<Target = [T]>> AsSliceMut<'it> for RefCellValueObserver<'_, A> {
    type SliceRefMut = RefMut<'it, [T]>;

    fn as_slice_mut(&'it mut self) -> Self::SliceRefMut {
        RefMut::map(self.value.as_ref().borrow_mut(), |s| &mut **s)
    }
}

impl<'it, T: 'it, A: Deref<Target = [T]>> AsIter<'it> for RefCellValueObserver<'_, A> {
    type Item = T;
    type Ref = Ref<'it, Self::Item>;
    type IntoIter = RefCellValueObserverIter<'it, T>;

    fn as_iter(&'it self) -> Self::IntoIter {
        Self::IntoIter {
            v: Ref::map(self.value.as_ref().borrow(), Deref::deref),
        }
    }
}

impl<'it, T: 'it> Iterator for RefCellValueObserverIter<'it, T> {
    type Item = Ref<'it, T>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.v.is_empty() {
            return None;
        }
        let cloned = Ref::clone(&self.v);
        let (next, remainder) = Ref::map_split(cloned, |v| (&v[0], &v[1..]));
        self.v = remainder;
        Some(next)
    }
}

/// [`Iterator`] over [`RefCellValueObserver`] of a [`DerefMut`] to `[T]`.
#[derive(Debug)]
pub struct RefCellValueObserverIterMut<'it, T> {
    v: Option<RefMut<'it, [T]>>,
}

impl<'it, T: 'it, A: DerefMut<Target = [T]> + Serialize> AsIterMut<'it>
    for RefCellValueObserver<'_, A>
{
    type RefMut = RefMut<'it, T>;
    type IntoIterMut = RefCellValueObserverIterMut<'it, T>;

    fn as_iter_mut(&'it mut self) -> Self::IntoIterMut {
        Self::IntoIterMut {
            v: Some(RefMut::map(
                self.value.as_ref().borrow_mut(),
                DerefMut::deref_mut,
            )),
        }
    }
}

impl<'it, T: 'it> Iterator for RefCellValueObserverIterMut<'it, T> {
    type Item = RefMut<'it, T>;

    fn next(&mut self) -> Option<Self::Item> {
        match self.v.take() {
            Some(v) => {
                let next_slice = if v.len() > 1 {
                    let (next, remainder) = RefMut::map_split(v, |v| v.split_at_mut(1));
                    self.v = Some(remainder);
                    next
                } else {
                    v
                };
                Some(RefMut::map(next_slice, |v| &mut v[0]))
            }
            _ => None,
        }
    }
}

impl<A: Hash> Hash for RefCellValueObserver<'_, A> {
    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.get_ref().hash(hasher);
    }
}

/// Panics if the contained value is already mutably borrowed (calls
/// [`RefCell::borrow`]).
impl<A> HasLen for RefCellValueObserver<'_, A>
where
    A: HasLen,
{
    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn len(&self) -> usize {
        self.get_ref().len()
    }

    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn is_empty(&self) -> bool {
        self.get_ref().is_empty()
    }
}

impl<T> AsRef<Self> for RefCellValueObserver<'_, T> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<T> AsMut<Self> for RefCellValueObserver<'_, T> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<T, A> MapObserver for RefCellValueObserver<'_, A>
where
    T: PartialEq + Copy + Hash + Default + DeserializeOwned + Serialize + Debug,
    A: DerefMut<Target = [T]> + Hash + Serialize + DeserializeOwned + HasLen + Default,
{
    type Entry = T;

    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn get(&self, idx: usize) -> Self::Entry {
        self.get_ref()[idx]
    }

    /// Panics if the contained value is already borrowed (calls
    /// [`RefCell::borrow_mut`]).
    fn set(&mut self, idx: usize, val: Self::Entry) {
        self.get_ref_mut()[idx] = val;
    }

    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn usable_count(&self) -> usize {
        self.get_ref().len()
    }

    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn count_bytes(&self) -> u64 {
        let default = Self::Entry::default();
        let mut count = 0;
        for entry in self.get_ref().iter() {
            if entry != &default {
                count += 1;
            }
        }
        count
    }

    /// Panics if the contained value is already borrowed (calls
    /// [`RefCell::borrow_mut`]).
    fn reset_map(&mut self) -> Result<(), Error> {
        // This is less than optimal for `Vec`, for which we could use
        // `.clear()`. However, it makes the `impl` more general. Worth it?
        *self.get_ref_mut() = A::default();
        Ok(())
    }

    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn to_vec(&self) -> Vec<Self::Entry> {
        self.get_ref().to_vec()
    }

    /// Panics if the contained value is already mutably borrowed (calls
    /// [`RefCell::borrow`]).
    fn how_many_set(&self, indexes: &[usize]) -> usize {
        let default = Self::Entry::default();
        let mut count = 0;
        let arr = self.get_ref();
        for idx in indexes {
            if arr[*idx] != default {
                count += 1;
            }
        }
        count
    }

    fn initial(&self) -> Self::Entry {
        Self::Entry::default()
    }
}
