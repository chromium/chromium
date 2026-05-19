//! All the map observer variants

use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    hash::{Hash, Hasher},
    ops::{Deref, DerefMut},
};

use libafl_bolts::{AsSlice, AsSliceMut, HasLen, Named, Truncate, ownedref::OwnedMutSlice};
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    Error,
    executors::ExitKind,
    observers::{DifferentialObserver, Observer},
};

pub mod const_map;
pub use const_map::*;

pub mod variable_map;
pub use variable_map::*;

pub mod hitcount_map;
pub use hitcount_map::*;

pub mod multi_map;
pub use multi_map::*;

pub mod owned_map;
pub use owned_map::*;

/// A trait indicating tracking of observed map values after testcase execution
///
/// Trait marker which indicates that this [`MapObserver`] is tracked for indices or novelties.
/// Implementors of feedbacks similar to [`crate::feedbacks::MapFeedback`] may wish to use this to
/// ensure that edge metadata is recorded as is appropriate for the provided observer.
///
/// If you get a type constraint failure for your map due to this type being unfulfilled, you must
/// call [`CanTrack::track_indices`] or [`CanTrack::track_novelties`] **at
/// the initialization site of your map**.
///
/// This trait allows various components which interact with map metadata to ensure that the
/// information they need is actually recorded by the map feedback.
/// For example, if you are using [`crate::schedulers::MinimizerScheduler`]:
/// ```
/// # use libafl::corpus::InMemoryCorpus;
/// # use libafl::feedbacks::{Feedback, MapFeedbackMetadata};
/// use libafl::feedbacks::{MaxMapFeedback, StateInitializer};
/// # use libafl::inputs::BytesInput;
/// use libafl::observers::{StdMapObserver, CanTrack};
/// use libafl::schedulers::{IndexesLenTimeMinimizerScheduler, QueueScheduler};
/// # use libafl::state::StdState;
/// # use libafl_bolts::serdeany::RegistryBuilder;
/// #
/// # #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
/// # unsafe { MapFeedbackMetadata::<u8>::register() }
/// # #[cfg(not(feature = "std"))]
/// # #[unsafe(no_mangle)]
/// # pub extern "C" fn external_current_millis() -> u64 { 0 }
///
/// use libafl_bolts::ownedref::OwnedMutSlice;
/// # use libafl_bolts::rands::StdRand;
///
/// // initialise your map as necessary
/// let edges_observer = StdMapObserver::from_ownedref("edges", OwnedMutSlice::from(vec![0u8; 16]));
/// // inform the feedback to track indices (required by IndexesLenTimeMinimizerScheduler), but not novelties
/// // this *MUST* be done before it is passed to MaxMapFeedback!
/// let edges_observer = edges_observer.track_indices();
/// // init the feedback
/// let mut feedback = MaxMapFeedback::new(&edges_observer);
/// #
/// # // init the state
/// # let mut state = StdState::new(
/// #     StdRand::with_seed(0),
/// #     InMemoryCorpus::<BytesInput>::new(),
/// #     InMemoryCorpus::new(),
/// #     &mut feedback,
/// #     &mut (),
/// # ).unwrap();
/// let scheduler = IndexesLenTimeMinimizerScheduler::new(&edges_observer, QueueScheduler::new());
/// # scheduler.cull(&state).unwrap();
/// ```
///
/// [`MapObserver`] implementors: see [`StdMapObserver`] for an example implementation.
pub trait CanTrack {
    /// The resulting type of enabling index tracking.
    type WithIndexTracking: CanTrack;
    /// The resulting type of enabling novelty tracking.
    type WithNoveltiesTracking: CanTrack;

    /// Whether indices should be tracked for this [`MapObserver`].
    const INDICES: bool;
    /// Whether novelties should be tracked for this [`MapObserver`].
    const NOVELTIES: bool;

    /// Convert this map observer into one that tracks indices.
    fn track_indices(self) -> Self::WithIndexTracking;
    /// Convert this map observer into one that tracks novelties.
    fn track_novelties(self) -> Self::WithNoveltiesTracking;
}

/// Struct which wraps [`MapObserver`] instances to explicitly give them tracking data.
///
/// # Safety
///
/// This is a bit of a magic structure. We pass it to the observer tuple as itself, but when its
/// referred to with `match_name`, there is a cast from this type to its inner type. This is
/// *guaranteed to be safe* by `#[repr(transparent)]`.
#[derive(Debug, Copy, Clone, Deserialize, Serialize)]
pub struct ExplicitTracking<T, const ITH: bool, const NTH: bool>(T);

impl<T, const ITH: bool, const NTH: bool> CanTrack for ExplicitTracking<T, ITH, NTH> {
    type WithIndexTracking = ExplicitTracking<T, true, NTH>;
    type WithNoveltiesTracking = ExplicitTracking<T, ITH, true>;
    const INDICES: bool = ITH;
    const NOVELTIES: bool = NTH;

    fn track_indices(self) -> Self::WithIndexTracking {
        ExplicitTracking::<T, true, NTH>(self.0)
    }

    fn track_novelties(self) -> Self::WithNoveltiesTracking {
        ExplicitTracking::<T, ITH, true>(self.0)
    }
}

impl<T, const ITH: bool, const NTH: bool> Named for ExplicitTracking<T, ITH, NTH>
where
    T: Named,
{
    fn name(&self) -> &Cow<'static, str> {
        self.0.name()
    }
}

impl<S, I, T, const ITH: bool, const NTH: bool> Observer<I, S> for ExplicitTracking<T, ITH, NTH>
where
    T: Observer<I, S>,
{
    fn flush(&mut self) -> Result<(), Error> {
        self.0.flush()
    }

    fn pre_exec(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.0.pre_exec(state, input)
    }

    fn post_exec(&mut self, state: &mut S, input: &I, exit_kind: &ExitKind) -> Result<(), Error> {
        self.0.post_exec(state, input, exit_kind)
    }

    fn pre_exec_child(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.0.pre_exec_child(state, input)
    }

    fn post_exec_child(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.0.post_exec_child(state, input, exit_kind)
    }
}

impl<T, OTA, OTB, I, S, const ITH: bool, const NTH: bool> DifferentialObserver<OTA, OTB, I, S>
    for ExplicitTracking<T, ITH, NTH>
where
    T: DifferentialObserver<OTA, OTB, I, S>,
{
    fn pre_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.as_mut().pre_observe_first(observers)
    }

    fn post_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.as_mut().post_observe_first(observers)
    }

    fn pre_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.as_mut().pre_observe_second(observers)
    }

    fn post_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.as_mut().post_observe_second(observers)
    }
}

impl<T, const ITH: bool, const NTH: bool> AsRef<T> for ExplicitTracking<T, ITH, NTH> {
    fn as_ref(&self) -> &T {
        &self.0
    }
}

impl<T, const ITH: bool, const NTH: bool> AsMut<T> for ExplicitTracking<T, ITH, NTH> {
    fn as_mut(&mut self) -> &mut T {
        &mut self.0
    }
}

/// Module which holds the necessary functions and types for map-relevant macros, namely
/// [`crate::require_index_tracking`] and [`crate::require_novelties_tracking`].
pub mod macros {
    pub use const_format::{concatcp, str_repeat};
    pub use const_panic::{FmtArg, concat_panic};

    /// Use in the constructor of your component which requires index tracking of a
    /// [`super::MapObserver`]. See [`super::CanTrack`] for details.
    ///
    /// As an example, if you are developing the type `MyCustomScheduler<O>` which requires novelty
    /// tracking, use this in your constructor:
    /// ```
    /// # use libafl::observers::{MapObserver, CanTrack};
    /// # use libafl::require_index_tracking;
    /// # use core::marker::PhantomData;
    /// #
    /// # struct MyCustomScheduler<C, O> {
    /// #     phantom: PhantomData<(C, O)>,
    /// # }
    /// #
    /// impl<C, O> MyCustomScheduler<C, O>
    /// where
    ///     O: MapObserver,
    ///     C: CanTrack + AsRef<O>,
    /// {
    ///     pub fn new(obs: &C) -> Self {
    ///         require_index_tracking!("MyCustomScheduler", C);
    ///         todo!("Construct your type")
    ///     }
    /// }
    /// ```
    #[macro_export]
    macro_rules! require_index_tracking {
        ($name: literal, $obs: ident) => {
            struct TrackingEnabledCheck<O: $crate::observers::CanTrack> {
                phantom: ::core::marker::PhantomData<O>,
            }

            impl<O: $crate::observers::CanTrack> TrackingEnabledCheck<O> {
                #[rustfmt::skip]
                const MESSAGE: &'static str = {
                    const LINE_OFFSET: usize = line!().ilog10() as usize + 2;
                    const SPACING: &str = $crate::observers::map::macros::str_repeat!(" ", LINE_OFFSET);
                    $crate::observers::map::macros::concatcp!(
                        "\n",
                        SPACING, "|\n",
                        SPACING, "= note: index tracking is required by ", $name, "\n",
                        SPACING, "= note: see the documentation of CanTrack for details\n",
                        SPACING, "|\n",
                        SPACING, "= hint: call `.track_indices()` on the map observer passed to ", $name, " at the point where it is defined\n",
                        SPACING, "|\n",
                        SPACING, "| ",
                    )
                };
                const TRACKING_ENABLED: bool = {
                    if !O::INDICES {
                        panic!("{}", Self::MESSAGE)
                    } else {
                        true
                    }
                };

                #[inline(always)]
                fn check_enabled() {
                    if !Self::TRACKING_ENABLED {
                        unreachable!("{}", Self::MESSAGE);
                    }
                }
            }
            TrackingEnabledCheck::<$obs>::check_enabled(); // check that tracking is enabled for this map
        };
    }

    /// Use in the constructor of your component which requires novelties tracking of a
    /// [`super::MapObserver`]. See [`super::CanTrack`] for details on the concept.
    ///
    /// As an example, if you are developing the type `MyCustomScheduler<O>` which requires novelty
    /// tracking, use this in your constructor:
    /// ```
    /// # use libafl::observers::{MapObserver, CanTrack};
    /// # use libafl::require_novelties_tracking;
    /// # use core::marker::PhantomData;
    /// #
    /// # struct MyCustomScheduler<C, O> {
    /// #     phantom: PhantomData<(C, O)>,
    /// # }
    /// #
    /// impl<C, O> MyCustomScheduler<C, O>
    /// where
    ///     O: MapObserver,
    ///     C: CanTrack + AsRef<O>,
    /// {
    ///     pub fn new(obs: &C) -> Self {
    ///         require_novelties_tracking!("MyCustomScheduler", C);
    ///         todo!("Construct your type")
    ///     }
    /// }
    /// ```
    #[macro_export]
    macro_rules! require_novelties_tracking {
        ($name: literal, $obs: ident) => {
            struct TrackingEnabledCheck<O: $crate::observers::CanTrack> {
                phantom: ::core::marker::PhantomData<O>,
            }

            impl<O: $crate::observers::CanTrack> TrackingEnabledCheck<O> {
                #[rustfmt::skip]
                const MESSAGE: &'static str = {
                    const LINE_OFFSET: usize = line!().ilog10() as usize + 2;
                    const SPACING: &str =
                        $crate::observers::map::macros::str_repeat!(" ", LINE_OFFSET);
                    $crate::observers::map::macros::concatcp!(
                        "\n",
                        SPACING, "|\n",
                        SPACING, "= note: novelty tracking is required by ", $name, "\n",
                        SPACING, "= note: see the documentation of CanTrack for details\n",
                        SPACING, "|\n",
                        SPACING, "= hint: call `.track_novelties()` on the map observer passed to ", $name, " at the point where it is defined\n",
                        SPACING, "|\n",
                        SPACING, "| ",
                    )
                };
                const TRACKING_ENABLED: bool = {
                    if !O::NOVELTIES {
                        panic!("{}", Self::MESSAGE)
                    } else {
                        true
                    }
                };

                #[inline(always)]
                fn check_enabled() {
                    if !Self::TRACKING_ENABLED {
                        unreachable!("{}", Self::MESSAGE);
                    }
                }
            }
            TrackingEnabledCheck::<$obs>::check_enabled(); // check that tracking is enabled for this map
        };
    }
}

/// A [`MapObserver`] observes the static map, as oftentimes used for AFL-like coverage information
///
/// When referring to this type in a constraint (e.g. `O: MapObserver`), ensure that you only refer
/// to instances of a second type, e.g. `C: AsRef<O>` or `A: AsMut<O>`. Map observer instances are
/// passed around in a way that may be potentially wrapped by e.g. [`ExplicitTracking`] as a way to
/// encode metadata into the type. This is an unfortunate additional requirement that we can't get
/// around without specialization.
///
/// See [`crate::require_index_tracking`] for an example of how to do so.
///
/// TODO: enforce `iter() -> AssociatedTypeIter` when generic associated types stabilize
pub trait MapObserver:
    HasLen + Named + Serialize + DeserializeOwned + AsRef<Self> + AsMut<Self>
// where
//     for<'it> &'it Self: IntoIterator<Item = &'it Self::Entry>
{
    /// Type of each entry in this map
    type Entry: PartialEq + Copy;

    /// Get the value at `idx`
    fn get(&self, idx: usize) -> Self::Entry;

    /// Set the value at `idx`
    fn set(&mut self, idx: usize, val: Self::Entry);

    /// Get the number of usable entries in the map (all by default)
    fn usable_count(&self) -> usize;

    /// Count the set bytes in the map
    fn count_bytes(&self) -> u64;

    /// Get the initial value for `reset()`
    fn initial(&self) -> Self::Entry;

    /// Reset the map
    fn reset_map(&mut self) -> Result<(), Error>;

    /// Get these observer's contents as [`Vec`]
    fn to_vec(&self) -> Vec<Self::Entry>;

    /// Get the number of set entries with the specified indexes
    fn how_many_set(&self, indexes: &[usize]) -> usize;
}

/// The "real" length of the underlying map could change at any point in time.
/// Thus, the size of the map should be fetched each time it is used.
pub trait VarLenMapObserver: MapObserver {
    /// A mutable slice reference to the map.
    /// The length of the map gives the maximum allocatable size.
    fn map_slice(&self) -> &[Self::Entry];

    /// A slice reference to the map.
    /// The length of the map gives the maximum allocatable size.
    fn map_slice_mut(&mut self) -> &mut [Self::Entry];

    /// A reference to the size of the map.
    fn size(&self) -> &usize;

    /// A mutable reference to the size of the map.
    fn size_mut(&mut self) -> &mut usize;
}

/// Implementors guarantee the size of the map is constant at any point in time and equals N.
pub trait ConstLenMapObserver<const N: usize>: MapObserver {
    /// The size of the map
    const LENGTH: usize = N;

    /// A mutable slice reference to the map
    fn map_slice(&self) -> &[Self::Entry; N];

    /// A mutable slice reference to the map
    fn map_slice_mut(&mut self) -> &mut [Self::Entry; N];
}

impl<M> CanTrack for M
where
    M: MapObserver,
{
    type WithIndexTracking = ExplicitTracking<Self, true, false>;
    type WithNoveltiesTracking = ExplicitTracking<Self, false, true>;
    const INDICES: bool = false;
    const NOVELTIES: bool = false;

    fn track_indices(self) -> Self::WithIndexTracking {
        ExplicitTracking::<Self, true, false>(self)
    }

    fn track_novelties(self) -> Self::WithNoveltiesTracking {
        ExplicitTracking::<Self, false, true>(self)
    }
}

/// The Map Observer retrieves the state of a map,
/// that will get updated by the target.
/// A well-known example is the AFL-Style coverage map.
#[derive(Clone, Serialize, Deserialize, Debug)]
#[expect(clippy::unsafe_derive_deserialize)]
pub struct StdMapObserver<'a, T, const DIFFERENTIAL: bool> {
    map: OwnedMutSlice<'a, T>,
    initial: T,
    name: Cow<'static, str>,
}

impl<I, S, T> Observer<I, S> for StdMapObserver<'_, T, false>
where
    Self: MapObserver,
{
    #[inline]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.reset_map()
    }
}

impl<I, S, T> Observer<I, S> for StdMapObserver<'_, T, true> {}

impl<T, const DIFFERENTIAL: bool> Named for StdMapObserver<'_, T, DIFFERENTIAL> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<T, const DIFFERENTIAL: bool> HasLen for StdMapObserver<'_, T, DIFFERENTIAL> {
    #[inline]
    fn len(&self) -> usize {
        self.map.as_slice().len()
    }
}

impl<T, const DIFFERENTIAL: bool> Hash for StdMapObserver<'_, T, DIFFERENTIAL>
where
    T: Hash,
{
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.as_slice().hash(hasher);
    }
}

impl<T, const DIFFERENTIAL: bool> AsRef<Self> for StdMapObserver<'_, T, DIFFERENTIAL> {
    fn as_ref(&self) -> &Self {
        self
    }
}

impl<T, const DIFFERENTIAL: bool> AsMut<Self> for StdMapObserver<'_, T, DIFFERENTIAL> {
    fn as_mut(&mut self) -> &mut Self {
        self
    }
}

impl<T, const DIFFERENTIAL: bool> MapObserver for StdMapObserver<'_, T, DIFFERENTIAL>
where
    T: PartialEq + Copy + Hash + Serialize + DeserializeOwned + Debug,
{
    type Entry = T;

    #[inline]
    fn get(&self, pos: usize) -> T {
        self.as_slice()[pos]
    }

    fn set(&mut self, pos: usize, val: T) {
        self.map.as_slice_mut()[pos] = val;
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

    fn to_vec(&self) -> Vec<T> {
        self.as_slice().to_vec()
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

impl<T, const DIFFERENTIAL: bool> Truncate for StdMapObserver<'_, T, DIFFERENTIAL> {
    fn truncate(&mut self, new_len: usize) {
        self.map.truncate(new_len);
    }
}

impl<T, const DIFFERENTIAL: bool> Deref for StdMapObserver<'_, T, DIFFERENTIAL> {
    type Target = [T];
    fn deref(&self) -> &[T] {
        &self.map
    }
}

impl<T, const DIFFERENTIAL: bool> DerefMut for StdMapObserver<'_, T, DIFFERENTIAL> {
    fn deref_mut(&mut self) -> &mut [T] {
        &mut self.map
    }
}

impl<'a, T, const DIFFERENTIAL: bool> StdMapObserver<'a, T, DIFFERENTIAL>
where
    T: Default,
{
    /// Creates a new [`MapObserver`]
    ///
    /// # Safety
    /// Will get a pointer to the map and dereference it at any point in time.
    /// The map must not move in memory!
    #[must_use]
    unsafe fn maybe_differential<S>(name: S, map: &'a mut [T]) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        unsafe {
            let len = map.len();
            let ptr = map.as_mut_ptr();
            Self::maybe_differential_from_mut_ptr(name, ptr, len)
        }
    }

    /// Creates a new [`MapObserver`] from an [`OwnedMutSlice`]
    #[must_use]
    fn maybe_differential_from_mut_slice<S>(name: S, map: OwnedMutSlice<'a, T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        StdMapObserver {
            name: name.into(),
            map,
            initial: T::default(),
        }
    }

    /// Creates a new [`MapObserver`] with an owned map
    #[must_use]
    fn maybe_differential_owned<S>(name: S, map: Vec<T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self {
            map: OwnedMutSlice::from(map),
            name: name.into(),
            initial: T::default(),
        }
    }

    /// Creates a new [`MapObserver`] from an [`OwnedMutSlice`] map.
    ///
    /// # Safety
    /// Will dereference the owned slice with up to len elements.
    #[must_use]
    fn maybe_differential_from_ownedref<S>(name: S, map: OwnedMutSlice<'a, T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self {
            map,
            name: name.into(),
            initial: T::default(),
        }
    }

    /// Creates a new [`MapObserver`] from a raw pointer
    ///
    /// # Safety
    /// Will dereference the `map_ptr` with up to len elements.
    unsafe fn maybe_differential_from_mut_ptr<S>(name: S, map_ptr: *mut T, len: usize) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        unsafe {
            Self::maybe_differential_from_mut_slice(
                name,
                OwnedMutSlice::from_raw_parts_mut(map_ptr, len),
            )
        }
    }

    /// Gets the initial value for this map, mutably
    pub fn initial_mut(&mut self) -> &mut T {
        &mut self.initial
    }

    /// Gets the backing for this map
    pub fn map(&self) -> &OwnedMutSlice<'a, T> {
        &self.map
    }

    /// Gets the backing for this map mutably
    pub fn map_mut(&mut self) -> &mut OwnedMutSlice<'a, T> {
        &mut self.map
    }
}

impl<'a, T> StdMapObserver<'a, T, false>
where
    T: Default,
{
    /// Creates a new [`MapObserver`]
    ///
    /// # Safety
    /// The observer will keep a pointer to the map.
    /// Hence, the map may never move in memory.
    #[must_use]
    pub unsafe fn new<S>(name: S, map: &'a mut [T]) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        unsafe { Self::maybe_differential(name, map) }
    }

    /// Creates a new [`MapObserver`] from an [`OwnedMutSlice`]
    pub fn from_mut_slice<S>(name: S, map: OwnedMutSlice<'a, T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::maybe_differential_from_mut_slice(name, map)
    }

    /// Creates a new [`MapObserver`] with an owned map
    #[must_use]
    pub fn owned<S>(name: S, map: Vec<T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::maybe_differential_owned(name, map)
    }

    /// Creates a new [`MapObserver`] from an [`OwnedMutSlice`] map.
    ///
    /// # Note
    /// Will dereference the owned slice with up to len elements.
    #[must_use]
    pub fn from_ownedref<S>(name: S, map: OwnedMutSlice<'a, T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::maybe_differential_from_ownedref(name, map)
    }

    /// Creates a new [`MapObserver`] from a raw pointer
    ///
    /// # Safety
    /// Will dereference the `map_ptr` with up to len elements.
    pub unsafe fn from_mut_ptr<S>(name: S, map_ptr: *mut T, len: usize) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        unsafe { Self::maybe_differential_from_mut_ptr(name, map_ptr, len) }
    }
}

impl<'a, T> StdMapObserver<'a, T, true>
where
    T: Default,
{
    /// Creates a new [`MapObserver`] in differential mode
    ///
    /// # Safety
    /// Will get a pointer to the map and dereference it at any point in time.
    /// The map must not move in memory!
    #[must_use]
    pub unsafe fn differential<S>(name: S, map: &'a mut [T]) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        unsafe { Self::maybe_differential(name, map) }
    }

    /// Creates a new [`MapObserver`] with an owned map in differential mode
    #[must_use]
    pub fn differential_owned<S>(name: S, map: Vec<T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::maybe_differential_owned(name, map)
    }

    /// Creates a new [`MapObserver`] from an [`OwnedMutSlice`] map in differential mode.
    ///
    /// # Note
    /// Will dereference the owned slice with up to len elements.
    #[must_use]
    pub fn differential_from_ownedref<S>(name: S, map: OwnedMutSlice<'a, T>) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self::maybe_differential_from_ownedref(name, map)
    }

    /// Creates a new [`MapObserver`] from a raw pointer in differential mode
    ///
    /// # Safety
    /// Will dereference the `map_ptr` with up to len elements.
    pub unsafe fn differential_from_mut_ptr<S>(name: S, map_ptr: *mut T, len: usize) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        unsafe { Self::maybe_differential_from_mut_ptr(name, map_ptr, len) }
    }
}

impl<OTA, OTB, I, S, T> DifferentialObserver<OTA, OTB, I, S> for StdMapObserver<'_, T, true> {}
