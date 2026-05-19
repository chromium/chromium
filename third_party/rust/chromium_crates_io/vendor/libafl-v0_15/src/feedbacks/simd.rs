//! SIMD accelerated map feedback with stable Rust.

use alloc::borrow::Cow;
#[cfg(feature = "track_hit_feedbacks")]
use alloc::vec::Vec;
use core::{
    fmt::Debug,
    marker::PhantomData,
    ops::{Deref, DerefMut},
};

use libafl_bolts::{
    AsIter, AsSlice, Error, Named,
    simd::{Reducer, SimdReducer, VectorType, covmap_is_interesting_simd},
    tuples::{Handle, MatchName, MatchNameRef},
};
use serde::{Serialize, de::DeserializeOwned};

use super::{DifferentIsNovel, Feedback, HasObserverHandle, MapFeedback, StateInitializer};
#[cfg(feature = "introspection")]
use crate::state::HasClientPerfMonitor;
use crate::{
    HasNamedMetadata,
    corpus::Testcase,
    events::EventFirer,
    executors::ExitKind,
    feedbacks::MapFeedbackMetadata,
    observers::{CanTrack, MapObserver},
    state::HasExecutions,
};

/// Stable Rust wrapper for SIMD accelerated map feedback. Unfortunately, we have to
/// keep this until specialization is stablized (not yet since 2016).
#[derive(Debug, Clone)]
pub struct SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
{
    map: MapFeedback<C, DifferentIsNovel, O, R::PrimitiveReducer>,
    _ph: PhantomData<V>,
}

impl<C, O, R, V> SimdMapFeedback<C, O, R, V>
where
    O: MapObserver<Entry = u8> + for<'a> AsSlice<'a, Entry = u8> + for<'a> AsIter<'a, Item = u8>,
    C: CanTrack + AsRef<O>,
    R: SimdReducer<V>,
    V: VectorType + Copy + Eq,
{
    fn is_interesting_u8_simd_optimized<S, OT>(&mut self, state: &mut S, observers: &OT) -> bool
    where
        S: HasNamedMetadata,
        OT: MatchName,
    {
        // TODO Replace with match_name_type when stable
        let observer = observers.get(self.map.observer_handle()).expect("MapObserver not found. This is likely because you entered the crash handler with the wrong executor/observer").as_ref();

        let map_state = state
            .named_metadata_map_mut()
            .get_mut::<MapFeedbackMetadata<u8>>(self.map.name())
            .unwrap();
        let size = observer.usable_count();
        let len = observer.len();
        if map_state.history_map.len() < len {
            map_state.history_map.resize(len, u8::default());
        }

        let map = observer.as_slice();
        debug_assert!(map.len() >= size);

        let history_map = map_state.history_map.as_slice();

        let (interesting, novelties) = unsafe {
            covmap_is_interesting_simd::<R, V>(history_map, &map, self.map.novelties.is_some())
        };
        if let Some(nov) = self.map.novelties.as_mut() {
            *nov = novelties;
        }
        #[cfg(feature = "track_hit_feedbacks")]
        {
            self.last_result = Some(interesting);
        }
        interesting
    }
}

impl<C, O, R, V> SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
{
    /// Wraps an existing map and enable SIMD acceleration. This will use standard SIMD
    /// implementation, which might vary based on target architecture according to our
    /// benchmark.
    #[must_use]
    pub fn wrap(map: MapFeedback<C, DifferentIsNovel, O, R::PrimitiveReducer>) -> Self {
        Self {
            map,
            _ph: PhantomData,
        }
    }
}

/// Implementation that mocks [`MapFeedback`], note the bound of O is intentionally stricter
/// than we we need to hint users when their entry is not `u8`. Without this bound, there
/// would be bound related errors in [`crate::fuzzer::StdFuzzer`], which is super confusing
/// and misleading.
impl<C, O, R, V> SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
    C: CanTrack + AsRef<O> + Named,
    O: MapObserver<Entry = u8> + for<'a> AsSlice<'a, Entry = u8> + for<'a> AsIter<'a, Item = u8>,
{
    /// Mock [`MapFeedback::new`]. If you are getting bound errors, your entry is probably not
    /// `u8` and you should use [`MapFeedback`] instead.
    #[must_use]
    pub fn new(map_observer: &C) -> Self {
        let map = MapFeedback::new(map_observer);
        Self {
            map,
            _ph: PhantomData,
        }
    }

    /// Mock [`MapFeedback::with_name`] If you are getting bound errors, your entry is probably not
    /// `u8` and you should use [`MapFeedback`] instead.
    #[must_use]
    pub fn with_name(name: &'static str, map_observer: &C) -> Self {
        let map = MapFeedback::with_name(name, map_observer);
        Self {
            map,
            _ph: PhantomData,
        }
    }
}

impl<C, O, R, V> Deref for SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
{
    type Target = MapFeedback<C, DifferentIsNovel, O, R::PrimitiveReducer>;
    fn deref(&self) -> &Self::Target {
        &self.map
    }
}

impl<C, O, R, V> DerefMut for SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.map
    }
}

impl<C, O, S, R, V> StateInitializer<S> for SimdMapFeedback<C, O, R, V>
where
    O: MapObserver,
    O::Entry: 'static + Default + Debug + DeserializeOwned + Serialize,
    S: HasNamedMetadata,
    R: SimdReducer<V>,
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        self.map.init_state(state)
    }
}

impl<C, O, R, V> HasObserverHandle for SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
{
    type Observer = C;

    #[inline]
    fn observer_handle(&self) -> &Handle<C> {
        self.map.observer_handle()
    }
}

impl<C, O, R, V> Named for SimdMapFeedback<C, O, R, V>
where
    R: SimdReducer<V>,
{
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.map.name()
    }
}

// Delegate implementations to inner mapping except is_interesting
impl<C, O, EM, I, OT, S, R, V> Feedback<EM, I, OT, S> for SimdMapFeedback<C, O, R, V>
where
    C: CanTrack + AsRef<O>,
    EM: EventFirer<I, S>,
    O: MapObserver<Entry = u8> + for<'a> AsSlice<'a, Entry = u8> + for<'a> AsIter<'a, Item = u8>,
    OT: MatchName,
    S: HasNamedMetadata + HasExecutions,
    R: SimdReducer<V>,
    V: VectorType + Copy + Eq,
    R::PrimitiveReducer: Reducer<u8>,
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _input: &I,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        let res = self.is_interesting_u8_simd_optimized(state, observers);
        Ok(res)
    }

    #[cfg(feature = "introspection")]
    fn is_interesting_introspection(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        S: HasClientPerfMonitor,
    {
        self.map
            .is_interesting_introspection(state, manager, input, observers, exit_kind)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        // cargo +nightly doc asks so
        <MapFeedback<C, DifferentIsNovel, O, <R as SimdReducer<V>>::PrimitiveReducer> as Feedback<
            EM,
            I,
            OT,
            S,
        >>::last_result(&self.map)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks(&self, list: &mut Vec<Cow<'static, str>>) -> Result<(), Error> {
        // cargo +nightly doc asks so
        <MapFeedback<C, DifferentIsNovel, O, <R as SimdReducer<V>>::PrimitiveReducer> as Feedback<
            EM,
            I,
            OT,
            S,
        >>::append_hit_feedbacks(&self.map, list)
    }

    #[inline]
    fn append_metadata(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        self.map
            .append_metadata(state, manager, observers, testcase)
    }
}
