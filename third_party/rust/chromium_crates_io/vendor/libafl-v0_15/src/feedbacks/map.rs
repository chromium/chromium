//! Map feedback, maximizing or minimizing maps, for example the afl-style map observer.

use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    marker::PhantomData,
    ops::{Deref, DerefMut},
};

#[cfg(all(feature = "simd", target_arch = "x86_64"))]
use libafl_bolts::simd::vector::u8x16;
#[cfg(not(feature = "simd"))]
use libafl_bolts::simd::{MinReducer, OrReducer};
#[cfg(feature = "simd")]
use libafl_bolts::simd::{SimdMaxReducer, SimdMinReducer, SimdOrReducer, vector::u8x32};
use libafl_bolts::{
    AsIter, HasRefCnt, Named,
    simd::{MaxReducer, NopReducer, Reducer},
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};
use num_traits::PrimInt;
use serde::{Deserialize, Serialize, de::DeserializeOwned};

#[cfg(feature = "simd")]
use super::simd::SimdMapFeedback;
#[cfg(feature = "track_hit_feedbacks")]
use crate::feedbacks::premature_last_result_err;
use crate::{
    Error, HasMetadata, HasNamedMetadata,
    corpus::Testcase,
    events::{Event, EventFirer, EventWithStats},
    executors::ExitKind,
    feedbacks::{Feedback, HasObserverHandle, StateInitializer},
    monitors::stats::{AggregatorOps, UserStats, UserStatsValue},
    observers::{CanTrack, MapObserver},
    state::HasExecutions,
};

#[cfg(feature = "simd")]
/// A [`SimdMapFeedback`] that implements the AFL algorithm using an [`SimdOrReducer`] combining the bits for the history map and the bit from (`HitcountsMapObserver`)[`crate::observers::HitcountsMapObserver`].
pub type AflMapFeedback<C, O> = SimdMapFeedback<C, O, SimdOrReducer, u8x32>;
#[cfg(not(feature = "simd"))]
/// A [`MapFeedback`] that implements the AFL algorithm using an [`OrReducer`] combining the bits for the history map and the bit from (`HitcountsMapObserver`)[`crate::observers::HitcountsMapObserver`].
pub type AflMapFeedback<C, O> = MapFeedback<C, DifferentIsNovel, O, OrReducer>;

#[cfg(all(feature = "simd", target_arch = "x86_64"))]
/// A [`SimdMapFeedback`] that strives to maximize the map contents.
pub type MaxMapFeedback<C, O> = SimdMapFeedback<C, O, SimdMaxReducer, u8x16>;
#[cfg(all(feature = "simd", not(target_arch = "x86_64")))]
/// A [`SimdMapFeedback`] that strives to maximize the map contents.
pub type MaxMapFeedback<C, O> = SimdMapFeedback<C, O, SimdMaxReducer, u8x32>;
#[cfg(not(feature = "simd"))]
/// A [`MapFeedback`] that strives to maximize the map contents.
pub type MaxMapFeedback<C, O> = MapFeedback<C, DifferentIsNovel, O, MaxReducer>;

#[cfg(feature = "simd")]
/// A [`SimdMapFeedback`] that strives to minimize the map contents.
pub type MinMapFeedback<C, O> = SimdMapFeedback<C, O, SimdMinReducer, u8x32>;
#[cfg(not(feature = "simd"))]
/// A [`MapFeedback`] that strives to minimize the map contents.
pub type MinMapFeedback<C, O> = MapFeedback<C, DifferentIsNovel, O, MinReducer>;

/// A [`MapFeedback`] that always returns `true` for `is_interesting`. Useful for tracing all executions.
pub type AlwaysInterestingMapFeedback<C, O> = MapFeedback<C, AllIsNovel, O, NopReducer>;

/// A [`MapFeedback`] that strives to maximize the map contents,
/// but only, if a value is larger than `pow2` of the previous.
pub type MaxMapPow2Feedback<C, O> = MapFeedback<C, NextPow2IsNovel, O, MaxReducer>;
/// A [`MapFeedback`] that strives to maximize the map contents,
/// but only, if a value is either `T::one()` or `T::max_value()`.
pub type MaxMapOneOrFilledFeedback<C, O> = MapFeedback<C, OneOrFilledIsNovel, O, MaxReducer>;

/// A `IsNovel` function is used to discriminate if a reduced value is considered novel.
pub trait IsNovel<T> {
    /// If a new value in the [`MapFeedback`] was found,
    /// this filter can decide if the result is considered novel or not.
    fn is_novel(old: T, new: T) -> bool;
}

/// [`AllIsNovel`] consider everything a novelty. Here mostly just for debugging.
#[derive(Debug, Clone)]
pub struct AllIsNovel {}

impl<T> IsNovel<T> for AllIsNovel
where
    T: Default + Copy + 'static,
{
    #[inline]
    fn is_novel(_old: T, _new: T) -> bool {
        true
    }
}

/// Calculate the next power of two
/// See <https://stackoverflow.com/a/66253960/1345238>
/// Will saturate at the max value.
/// In case of negative values, returns 1.
#[inline]
fn saturating_next_power_of_two<T: PrimInt>(n: T) -> T {
    if n <= T::one() {
        T::one()
    } else {
        (T::max_value() >> (n - T::one()).leading_zeros().try_into().unwrap())
            .saturating_add(T::one())
    }
}

/// Consider as novelty if the reduced value is different from the old value.
#[derive(Debug, Clone)]
pub struct DifferentIsNovel {}

impl<T> IsNovel<T> for DifferentIsNovel
where
    T: PartialEq + Default + Copy + 'static,
{
    #[inline]
    fn is_novel(old: T, new: T) -> bool {
        old != new
    }
}

/// Only consider as novel the values which are at least the next pow2 class of the old value
#[derive(Debug, Clone)]
pub struct NextPow2IsNovel {}

impl<T> IsNovel<T> for NextPow2IsNovel
where
    T: PrimInt + Default + Copy + 'static,
{
    #[inline]
    fn is_novel(old: T, new: T) -> bool {
        // We use a trait so we build our numbers from scratch here.
        // This way it works with Nums of any size.
        if new <= old {
            false
        } else {
            let pow2 = saturating_next_power_of_two(old.saturating_add(T::one()));
            new >= pow2
        }
    }
}

/// Only consider `T::one()` or `T::max_value()`, if they are bigger than the old value, as novel
#[derive(Debug, Clone)]
pub struct OneOrFilledIsNovel {}

impl<T> IsNovel<T> for OneOrFilledIsNovel
where
    T: PrimInt + Default + Copy + 'static,
{
    #[inline]
    fn is_novel(old: T, new: T) -> bool {
        (new == T::one() || new == T::max_value()) && new > old
    }
}

/// A testcase metadata holding a list of indexes of a map
#[derive(Debug, Serialize, Deserialize)]
#[expect(clippy::unsafe_derive_deserialize)] // for SerdeAny
pub struct MapIndexesMetadata {
    /// The list of indexes.
    pub list: Vec<usize>,
    /// A refcount used to know when we can remove this metadata
    pub tcref: isize,
}

libafl_bolts::impl_serdeany!(MapIndexesMetadata);

impl Deref for MapIndexesMetadata {
    type Target = [usize];
    /// Convert to a slice
    fn deref(&self) -> &[usize] {
        &self.list
    }
}

impl DerefMut for MapIndexesMetadata {
    /// Convert to a slice
    fn deref_mut(&mut self) -> &mut [usize] {
        &mut self.list
    }
}

impl HasRefCnt for MapIndexesMetadata {
    fn refcnt(&self) -> isize {
        self.tcref
    }

    fn refcnt_mut(&mut self) -> &mut isize {
        &mut self.tcref
    }
}

impl MapIndexesMetadata {
    /// Creates a new [`struct@MapIndexesMetadata`].
    #[must_use]
    pub fn new(list: Vec<usize>) -> Self {
        Self { list, tcref: 0 }
    }
}

/// A testcase metadata holding a list of indexes of a map
#[derive(Debug, Serialize, Deserialize)]
#[expect(clippy::unsafe_derive_deserialize)] // for SerdeAny
pub struct MapNoveltiesMetadata {
    /// A `list` of novelties.
    pub list: Vec<usize>,
}

libafl_bolts::impl_serdeany!(MapNoveltiesMetadata);

impl Deref for MapNoveltiesMetadata {
    type Target = [usize];
    /// Convert to a slice
    fn deref(&self) -> &[usize] {
        &self.list
    }
}

impl DerefMut for MapNoveltiesMetadata {
    /// Convert to a slice
    fn deref_mut(&mut self) -> &mut [usize] {
        &mut self.list
    }
}

impl MapNoveltiesMetadata {
    /// Creates a new [`struct@MapNoveltiesMetadata`]
    #[must_use]
    pub fn new(list: Vec<usize>) -> Self {
        Self { list }
    }
}

/// The state of [`MapFeedback`]
#[derive(Default, Serialize, Deserialize, Debug, Clone)]
#[expect(clippy::unsafe_derive_deserialize)] // for SerdeAny
pub struct MapFeedbackMetadata<T> {
    /// Contains information about untouched entries
    pub history_map: Vec<T>,
    /// Tells us how many non-initial entries there are in `history_map`
    pub num_covered_map_indexes: usize,
}

libafl_bolts::impl_serdeany!(
    MapFeedbackMetadata<T: 'static + Debug + Serialize + DeserializeOwned>,
    <u8>,<u16>,<u32>,<u64>,<i8>,<i16>,<i32>,<i64>,<f32>,<f64>,<bool>,<char>,<usize>
);

impl<T> MapFeedbackMetadata<T>
where
    T: Default + Copy + 'static + Serialize + DeserializeOwned + PartialEq,
{
    /// Create new `MapFeedbackMetadata`
    #[must_use]
    pub fn new(map_size: usize) -> Self {
        Self {
            history_map: vec![T::default(); map_size],
            num_covered_map_indexes: 0,
        }
    }

    /// Create new `MapFeedbackMetadata` using a name and a map.
    /// The map can be shared.
    /// `initial_elem_value` is used to calculate `Self.num_covered_map_indexes`
    #[must_use]
    pub fn with_history_map(history_map: Vec<T>, initial_elem_value: T) -> Self {
        let num_covered_map_indexes = history_map
            .iter()
            .fold(0, |acc, x| acc + usize::from(*x != initial_elem_value));
        Self {
            history_map,
            num_covered_map_indexes,
        }
    }

    /// Reset the map
    pub fn reset(&mut self) -> Result<(), Error> {
        let cnt = self.history_map.len();
        for i in 0..cnt {
            self.history_map[i] = T::default();
        }
        self.num_covered_map_indexes = 0;
        Ok(())
    }

    /// Reset the map with any value
    pub fn reset_with_value(&mut self, value: T) -> Result<(), Error> {
        let cnt = self.history_map.len();
        for i in 0..cnt {
            self.history_map[i] = value;
        }
        // assume that resetting the map should indicate no coverage,
        // regardless of value
        self.num_covered_map_indexes = 0;
        Ok(())
    }
}

/// The most common AFL-like feedback type
#[derive(Debug, Clone)]
pub struct MapFeedback<C, N, O, R> {
    /// New indexes observed in the last observation
    pub(crate) novelties: Option<Vec<usize>>,
    /// Name identifier of this instance
    name: Cow<'static, str>,
    /// Name identifier of the observer
    map_ref: Handle<C>,
    /// Name of the feedback as shown in the `UserStats`
    stats_name: Cow<'static, str>,
    // The previous run's result of [`Self::is_interesting`]
    #[cfg(feature = "track_hit_feedbacks")]
    pub(crate) last_result: Option<bool>,
    /// Phantom Data of Reducer
    #[expect(clippy::type_complexity)]
    phantom: PhantomData<fn() -> (N, O, R)>,
}

impl<C, N, O, R, S> StateInitializer<S> for MapFeedback<C, N, O, R>
where
    O: MapObserver,
    O::Entry: 'static + Default + Debug + DeserializeOwned + Serialize,
    S: HasNamedMetadata,
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        // Initialize `MapFeedbackMetadata` with an empty vector and add it to the state.
        // The `MapFeedbackMetadata` would be resized on-demand in `is_interesting`
        state.add_named_metadata_checked(&self.name, MapFeedbackMetadata::<O::Entry>::default())?;
        Ok(())
    }
}

impl<C, EM, I, N, O, OT, R, S> Feedback<EM, I, OT, S> for MapFeedback<C, N, O, R>
where
    C: CanTrack + AsRef<O>,
    EM: EventFirer<I, S>,
    N: IsNovel<O::Entry>,
    O: MapObserver + for<'it> AsIter<'it, Item = O::Entry>,
    O::Entry: 'static + Default + Debug + DeserializeOwned + Serialize,
    OT: MatchName,
    R: Reducer<O::Entry>,
    S: HasNamedMetadata + HasExecutions,
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        _input: &I,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        let res = self.is_interesting_default(state, observers);

        #[cfg(feature = "track_hit_feedbacks")]
        {
            self.last_result = Some(res);
        }
        Ok(res)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        self.last_result.ok_or(premature_last_result_err())
    }

    fn append_metadata(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        if let Some(novelties) = self.novelties.as_mut().map(core::mem::take) {
            let meta = MapNoveltiesMetadata::new(novelties);
            testcase.add_metadata(meta);
        }
        let observer = observers.get(&self.map_ref).expect("MapObserver not found. This is likely because you entered the crash handler with the wrong executor/observer").as_ref();
        let initial = observer.initial();
        let map_state = state
            .named_metadata_map_mut()
            .get_mut::<MapFeedbackMetadata<O::Entry>>(&self.name)
            .unwrap();
        let len = observer.len();
        if map_state.history_map.len() < len {
            map_state.history_map.resize(len, observer.initial());
        }

        let history_map = &mut map_state.history_map;
        if C::INDICES {
            let mut indices = Vec::new();

            for (i, value) in observer
                .as_iter()
                .map(|x| *x)
                .enumerate()
                .filter(|(_, value)| *value != initial)
            {
                let val = R::reduce(history_map[i], value);
                if history_map[i] == initial && val != initial {
                    map_state.num_covered_map_indexes += 1;
                }
                history_map[i] = val;
                indices.push(i);
            }
            let meta = MapIndexesMetadata::new(indices);
            if testcase.try_add_metadata(meta).is_err() {
                return Err(Error::key_exists(
                    "MapIndexesMetadata is already attached to this testcase. You should not have more than one observer with tracking.",
                ));
            }
        } else {
            for (i, value) in observer
                .as_iter()
                .map(|x| *x)
                .enumerate()
                .filter(|(_, value)| *value != initial)
            {
                let val = R::reduce(history_map[i], value);
                if history_map[i] == initial && val != initial {
                    map_state.num_covered_map_indexes += 1;
                }
                history_map[i] = val;
            }
        }

        debug_assert!(
            history_map
                .iter()
                .fold(0, |acc, x| acc + usize::from(*x != initial))
                == map_state.num_covered_map_indexes,
            "history_map had {} filled, but map_state.num_covered_map_indexes was {}",
            history_map
                .iter()
                .fold(0, |acc, x| acc + usize::from(*x != initial)),
            map_state.num_covered_map_indexes,
        );

        // at this point you are executing this code, the testcase is always interesting
        let covered = map_state.num_covered_map_indexes;
        let len = history_map.len();
        // opt: if not tracking optimisations, we technically don't show the *current* history
        // map but the *last* history map; this is better than walking over and allocating
        // unnecessarily
        manager.fire(
            state,
            EventWithStats::with_current_time(
                Event::UpdateUserStats {
                    name: self.stats_name.clone(),
                    value: UserStats::new(
                        UserStatsValue::Ratio(covered as u64, len as u64),
                        AggregatorOps::Avg,
                    ),
                    phantom: PhantomData,
                },
                *state.executions(),
            ),
        )?;

        Ok(())
    }
}

impl<C, N, O, R> Named for MapFeedback<C, N, O, R> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

#[expect(clippy::ptr_arg)]
fn create_stats_name(name: &Cow<'static, str>) -> Cow<'static, str> {
    if name.chars().all(char::is_lowercase) {
        name.clone()
    } else {
        name.to_lowercase().into()
    }
}

impl<C, N, O, R> MapFeedback<C, N, O, R>
where
    C: CanTrack + AsRef<O> + Named,
{
    /// Create new `MapFeedback`
    #[must_use]
    pub fn new(map_observer: &C) -> Self {
        Self {
            novelties: if C::NOVELTIES { Some(vec![]) } else { None },
            name: map_observer.name().clone(),
            map_ref: map_observer.handle(),
            stats_name: create_stats_name(map_observer.name()),
            #[cfg(feature = "track_hit_feedbacks")]
            last_result: None,
            phantom: PhantomData,
        }
    }

    /// Creating a new `MapFeedback` with a specific name. This is usefully whenever the same
    /// feedback is needed twice, but with a different history. Using `new()` always results in the
    /// same name and therefore also the same history.
    #[must_use]
    pub fn with_name(name: &'static str, map_observer: &C) -> Self {
        let name = Cow::from(name);
        Self {
            novelties: if C::NOVELTIES { Some(vec![]) } else { None },
            map_ref: map_observer.handle(),
            stats_name: create_stats_name(&name),
            name,
            #[cfg(feature = "track_hit_feedbacks")]
            last_result: None,
            phantom: PhantomData,
        }
    }
}

impl<C, N, O, R> HasObserverHandle for MapFeedback<C, N, O, R> {
    type Observer = C;

    #[inline]
    fn observer_handle(&self) -> &Handle<C> {
        &self.map_ref
    }
}

impl<C, N, O, R> MapFeedback<C, N, O, R>
where
    R: Reducer<O::Entry>,
    O: MapObserver + for<'it> AsIter<'it, Item = O::Entry>,
    O::Entry: 'static + Debug + Serialize + DeserializeOwned,
    N: IsNovel<O::Entry>,
    C: AsRef<O>,
{
    fn is_interesting_default<OT, S>(&mut self, state: &mut S, observers: &OT) -> bool
    where
        S: HasNamedMetadata,
        OT: MatchName,
    {
        let mut interesting = false;
        // TODO Replace with match_name_type when stable
        let observer = observers.get(&self.map_ref).unwrap().as_ref();

        let map_state = state
            .named_metadata_map_mut()
            .get_mut::<MapFeedbackMetadata<O::Entry>>(&self.name)
            .unwrap();
        let len = observer.len();
        if map_state.history_map.len() < len {
            map_state.history_map.resize(len, observer.initial());
        }

        let history_map = map_state.history_map.as_slice();

        let initial = observer.initial();

        if let Some(novelties) = self.novelties.as_mut() {
            novelties.clear();
            for (i, item) in observer
                .as_iter()
                .map(|x| *x)
                .enumerate()
                .filter(|(_, item)| *item != initial)
            {
                let existing = unsafe { *history_map.get_unchecked(i) };
                let reduced = R::reduce(existing, item);
                if N::is_novel(existing, reduced) {
                    interesting = true;
                    novelties.push(i);
                }
            }
        } else {
            for (i, item) in observer
                .as_iter()
                .map(|x| *x)
                .enumerate()
                .filter(|(_, item)| *item != initial)
            {
                let existing = unsafe { *history_map.get_unchecked(i) };
                let reduced = R::reduce(existing, item);
                if N::is_novel(existing, reduced) {
                    interesting = true;
                    break;
                }
            }
        }

        interesting
    }
}

#[cfg(test)]
mod tests {
    use crate::feedbacks::{AllIsNovel, IsNovel, NextPow2IsNovel};

    #[test]
    fn test_map_is_novel() {
        // This should always hold
        assert!(AllIsNovel::is_novel(0_u8, 0));

        assert!(!NextPow2IsNovel::is_novel(0_u8, 0));
        assert!(NextPow2IsNovel::is_novel(0_u8, 1));
        assert!(!NextPow2IsNovel::is_novel(1_u8, 1));
        assert!(NextPow2IsNovel::is_novel(1_u8, 2));
        assert!(!NextPow2IsNovel::is_novel(2_u8, 2));
        assert!(!NextPow2IsNovel::is_novel(2_u8, 3));
        assert!(NextPow2IsNovel::is_novel(2_u8, 4));
        assert!(!NextPow2IsNovel::is_novel(128_u8, 128));
        assert!(!NextPow2IsNovel::is_novel(129_u8, 128));
        assert!(NextPow2IsNovel::is_novel(128_u8, 255));
        assert!(!NextPow2IsNovel::is_novel(255_u8, 128));
        assert!(NextPow2IsNovel::is_novel(254_u8, 255));
        assert!(!NextPow2IsNovel::is_novel(255_u8, 255));
    }
}
