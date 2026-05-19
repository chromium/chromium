//! Hitcount map observer is for implementing AFL's hit count bucket
use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    hash::Hash,
    mem::size_of,
    ops::{Deref, DerefMut},
    slice,
};

use libafl_bolts::{AsIter, AsIterMut, AsSlice, AsSliceMut, HasLen, Named, Truncate};
use serde::{Deserialize, Serialize};

use crate::{
    Error,
    executors::ExitKind,
    observers::{
        ConstLenMapObserver, DifferentialObserver, Observer, VarLenMapObserver, map::MapObserver,
    },
};

/// Hitcounts class lookup
static COUNT_CLASS_LOOKUP: [u8; 256] = [
    0, 1, 2, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
];

/// Hitcounts class lookup for 16-byte values
static COUNT_CLASS_LOOKUP_16: [u16; 256 * 256] = {
    let mut seq = [0u16; 256 * 256];
    let mut lo_bits = 0;
    let mut hi_bits = 0;
    while lo_bits < 256 {
        while hi_bits < 256 {
            seq[hi_bits << 8 | lo_bits] =
                (COUNT_CLASS_LOOKUP[hi_bits] as u16) << 8 | COUNT_CLASS_LOOKUP[lo_bits] as u16;
            hi_bits += 1;
        }
        hi_bits = 0;
        lo_bits += 1;
    }
    seq
};

/// AFL-style classify counts
#[inline]
#[expect(clippy::cast_ptr_alignment)]
pub(crate) fn classify_counts(map: &mut [u8]) {
    let mut len = map.len();
    let align_offset = map.as_ptr().align_offset(size_of::<u16>());

    // if len == 1, the next branch will already do this lookup
    if len > 1 && align_offset != 0 {
        debug_assert_eq!(
            align_offset, 1,
            "Aligning u8 to u16 should always be offset of 1?"
        );
        unsafe {
            *map.get_unchecked_mut(0) =
                *COUNT_CLASS_LOOKUP.get_unchecked(*map.get_unchecked(0) as usize);
        }
        len -= 1;
    }

    // Fix the last element
    if (len & 1) != 0 {
        unsafe {
            *map.get_unchecked_mut(len - 1) =
                *COUNT_CLASS_LOOKUP.get_unchecked(*map.get_unchecked(len - 1) as usize);
        }
    }

    let cnt = len / 2;

    let map16 =
        unsafe { slice::from_raw_parts_mut(map.as_mut_ptr().add(align_offset) as *mut u16, cnt) };

    for item in &mut map16[0..cnt] {
        unsafe {
            *item = *(COUNT_CLASS_LOOKUP_16).get_unchecked(*item as usize);
        }
    }
}

/// Map observer with AFL-like hitcounts postprocessing
///
/// [`MapObserver`]s that are not slice-backed, such as `MultiMapObserver`, can use
/// [`HitcountsIterableMapObserver`] instead.
#[derive(Serialize, Deserialize, Debug, Clone, Hash)]
pub struct HitcountsMapObserver<M> {
    base: M,
}

impl<M> Deref for HitcountsMapObserver<M> {
    type Target = M;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<M> DerefMut for HitcountsMapObserver<M> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

impl<I, S, M> Observer<I, S> for HitcountsMapObserver<M>
where
    M: MapObserver<Entry = u8> + Observer<I, S> + for<'a> AsSliceMut<'a, Entry = u8>,
{
    #[inline]
    fn pre_exec(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.base.pre_exec(state, input)
    }

    #[inline]
    fn post_exec(&mut self, state: &mut S, input: &I, exit_kind: &ExitKind) -> Result<(), Error> {
        classify_counts(&mut self.as_slice_mut());
        self.base.post_exec(state, input, exit_kind)
    }
}

impl<M> Named for HitcountsMapObserver<M>
where
    M: Named,
{
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.base.name()
    }
}

impl<M> HitcountsMapObserver<M> {
    /// Creates a new [`MapObserver`]
    pub fn new(base: M) -> Self {
        Self { base }
    }
}

impl<M> HasLen for HitcountsMapObserver<M>
where
    M: HasLen,
{
    #[inline]
    fn len(&self) -> usize {
        self.base.len()
    }
}

impl<M> AsRef<Self> for HitcountsMapObserver<M> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<M> AsMut<Self> for HitcountsMapObserver<M> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<M> MapObserver for HitcountsMapObserver<M>
where
    M: MapObserver<Entry = u8>,
{
    type Entry = u8;

    #[inline]
    fn initial(&self) -> u8 {
        self.base.initial()
    }

    #[inline]
    fn usable_count(&self) -> usize {
        self.base.usable_count()
    }

    #[inline]
    fn get(&self, idx: usize) -> u8 {
        self.base.get(idx)
    }

    #[inline]
    fn set(&mut self, idx: usize, val: u8) {
        self.base.set(idx, val);
    }

    /// Count the set bytes in the map
    fn count_bytes(&self) -> u64 {
        self.base.count_bytes()
    }

    /// Reset the map
    #[inline]
    fn reset_map(&mut self) -> Result<(), Error> {
        self.base.reset_map()
    }

    fn to_vec(&self) -> Vec<u8> {
        self.base.to_vec()
    }

    fn how_many_set(&self, indexes: &[usize]) -> usize {
        self.base.how_many_set(indexes)
    }
}

impl<M, const N: usize> ConstLenMapObserver<N> for HitcountsMapObserver<M>
where
    M: ConstLenMapObserver<N> + MapObserver<Entry = u8>,
{
    fn map_slice(&self) -> &[Self::Entry; N] {
        self.base.map_slice()
    }

    fn map_slice_mut(&mut self) -> &mut [Self::Entry; N] {
        self.base.map_slice_mut()
    }
}

impl<M> VarLenMapObserver for HitcountsMapObserver<M>
where
    M: VarLenMapObserver + MapObserver<Entry = u8>,
{
    fn map_slice(&self) -> &[Self::Entry] {
        self.base.map_slice()
    }

    fn map_slice_mut(&mut self) -> &mut [Self::Entry] {
        self.base.map_slice_mut()
    }

    fn size(&self) -> &usize {
        self.base.size()
    }

    fn size_mut(&mut self) -> &mut usize {
        self.base.size_mut()
    }
}

impl<M> Truncate for HitcountsMapObserver<M>
where
    M: Named + Serialize + serde::de::DeserializeOwned + Truncate,
{
    fn truncate(&mut self, new_len: usize) {
        self.base.truncate(new_len);
    }
}

impl<'a, M> AsSlice<'a> for HitcountsMapObserver<M>
where
    M: AsSlice<'a>,
{
    type Entry = <M as AsSlice<'a>>::Entry;
    type SliceRef = <M as AsSlice<'a>>::SliceRef;

    #[inline]
    fn as_slice(&'a self) -> Self::SliceRef {
        self.base.as_slice()
    }
}

impl<'a, M> AsSliceMut<'a> for HitcountsMapObserver<M>
where
    M: AsSliceMut<'a>,
{
    type SliceRefMut = <M as AsSliceMut<'a>>::SliceRefMut;
    #[inline]
    fn as_slice_mut(&'a mut self) -> Self::SliceRefMut {
        self.base.as_slice_mut()
    }
}

impl<M, OTA, OTB, I, S> DifferentialObserver<OTA, OTB, I, S> for HitcountsMapObserver<M>
where
    M: DifferentialObserver<OTA, OTB, I, S>
        + MapObserver<Entry = u8>
        + for<'a> AsSliceMut<'a, Entry = u8>,
{
    fn pre_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.base.pre_observe_first(observers)
    }

    fn post_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.base.post_observe_first(observers)
    }

    fn pre_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.base.pre_observe_second(observers)
    }

    fn post_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.base.post_observe_second(observers)
    }
}

/// Map observer with hitcounts postprocessing
/// Less optimized version for non-slice iterators.
/// Slice-backed observers should use a [`HitcountsMapObserver`].
#[derive(Serialize, Deserialize, Debug, Clone, Hash)]
pub struct HitcountsIterableMapObserver<M> {
    base: M,
}

impl<M> Deref for HitcountsIterableMapObserver<M> {
    type Target = M;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

impl<M> DerefMut for HitcountsIterableMapObserver<M> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.base
    }
}

impl<I, S, M> Observer<I, S> for HitcountsIterableMapObserver<M>
where
    M: MapObserver<Entry = u8> + Observer<I, S> + for<'it> AsIterMut<'it, Item = u8>,
{
    #[inline]
    fn pre_exec(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.base.pre_exec(state, input)
    }

    #[inline]
    fn post_exec(&mut self, state: &mut S, input: &I, exit_kind: &ExitKind) -> Result<(), Error> {
        for mut item in self.as_iter_mut() {
            *item = unsafe { *COUNT_CLASS_LOOKUP.get_unchecked((*item) as usize) };
        }

        self.base.post_exec(state, input, exit_kind)
    }
}

impl<M> Named for HitcountsIterableMapObserver<M>
where
    M: Named,
{
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.base.name()
    }
}

impl<M> HitcountsIterableMapObserver<M> {
    /// Creates a new [`MapObserver`]
    pub fn new(base: M) -> Self {
        Self { base }
    }
}

impl<M> HasLen for HitcountsIterableMapObserver<M>
where
    M: HasLen,
{
    #[inline]
    fn len(&self) -> usize {
        self.base.len()
    }
}

impl<M> AsRef<Self> for HitcountsIterableMapObserver<M> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<M> AsMut<Self> for HitcountsIterableMapObserver<M> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<M> MapObserver for HitcountsIterableMapObserver<M>
where
    M: MapObserver<Entry = u8>,
{
    type Entry = u8;

    #[inline]
    fn initial(&self) -> u8 {
        self.base.initial()
    }

    #[inline]
    fn usable_count(&self) -> usize {
        self.base.usable_count()
    }

    #[inline]
    fn get(&self, idx: usize) -> u8 {
        self.base.get(idx)
    }

    #[inline]
    fn set(&mut self, idx: usize, val: u8) {
        self.base.set(idx, val);
    }

    /// Count the set bytes in the map
    fn count_bytes(&self) -> u64 {
        self.base.count_bytes()
    }

    /// Reset the map
    #[inline]
    fn reset_map(&mut self) -> Result<(), Error> {
        self.base.reset_map()
    }

    fn to_vec(&self) -> Vec<u8> {
        self.base.to_vec()
    }

    fn how_many_set(&self, indexes: &[usize]) -> usize {
        self.base.how_many_set(indexes)
    }
}

impl<M> Truncate for HitcountsIterableMapObserver<M>
where
    M: Named + Serialize + serde::de::DeserializeOwned + Truncate,
{
    fn truncate(&mut self, new_len: usize) {
        self.base.truncate(new_len);
    }
}

impl<M, OTA, OTB, I, S> DifferentialObserver<OTA, OTB, I, S> for HitcountsIterableMapObserver<M>
where
    M: DifferentialObserver<OTA, OTB, I, S>
        + MapObserver<Entry = u8>
        + for<'it> AsIterMut<'it, Item = u8>,
{
    fn pre_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.base.pre_observe_first(observers)
    }

    fn post_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.base.post_observe_first(observers)
    }

    fn pre_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.base.pre_observe_second(observers)
    }

    fn post_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.base.post_observe_second(observers)
    }
}

impl<'it, M> AsIter<'it> for HitcountsIterableMapObserver<M>
where
    M: AsIter<'it>,
{
    type Item = M::Item;
    type Ref = M::Ref;
    type IntoIter = M::IntoIter;

    fn as_iter(&'it self) -> Self::IntoIter {
        self.base.as_iter()
    }
}

impl<'it, M> AsIterMut<'it> for HitcountsIterableMapObserver<M>
where
    M: AsIterMut<'it>,
{
    type RefMut = M::RefMut;
    type IntoIterMut = M::IntoIterMut;

    fn as_iter_mut(&'it mut self) -> Self::IntoIterMut {
        self.base.as_iter_mut()
    }
}
