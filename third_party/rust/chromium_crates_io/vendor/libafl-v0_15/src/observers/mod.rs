//! Observers give insights about runs of a target, such as coverage, timing, stack depth, and more.
use alloc::borrow::Cow;

pub mod cmp;
pub use cmp::*;

#[cfg(feature = "std")]
pub mod stdio;
#[cfg(feature = "std")]
pub use stdio::{StdErrObserver, StdOutObserver};

#[cfg(feature = "regex")]
pub mod stacktrace;
#[cfg(feature = "regex")]
pub use stacktrace::*;

pub mod concolic;
pub mod map;
pub use map::*;

pub mod value;

/// List observer
pub mod list;
use core::{fmt::Debug, time::Duration};
#[cfg(feature = "std")]
use std::time::Instant;

#[cfg(not(feature = "std"))]
use libafl_bolts::current_time;
use libafl_bolts::{Named, tuples::MatchName};
pub use list::*;
use serde::{Deserialize, Serialize};
pub use value::*;

use crate::{Error, executors::ExitKind};

/// Observers observe different information about the target.
/// They can then be used by various sorts of feedback.
pub trait Observer<I, S>: Named {
    /// The testcase finished execution, calculate any changes.
    /// Reserved for future use.
    #[inline]
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    /// Called right before execution starts.
    #[inline]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        Ok(())
    }

    /// Called right after execution finishes.
    #[inline]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &I,
        _exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        Ok(())
    }

    /// Called right before execution starts in the child process, if any.
    #[inline]
    fn pre_exec_child(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        Ok(())
    }

    /// Called right after execution finishes in the child process, if any.
    #[inline]
    fn post_exec_child(
        &mut self,
        _state: &mut S,
        _input: &I,
        _exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        Ok(())
    }
}

/// A haskell-style tuple of observers
pub trait ObserversTuple<I, S>: MatchName {
    /// This is called right before the next execution.
    fn pre_exec_all(&mut self, state: &mut S, input: &I) -> Result<(), Error>;

    /// This is called right after the last execution
    fn post_exec_all(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error>;

    /// This is called right before the next execution in the child process, if any.
    fn pre_exec_child_all(&mut self, state: &mut S, input: &I) -> Result<(), Error>;

    /// This is called right after the last execution in the child process, if any.
    fn post_exec_child_all(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error>;
}

impl<I, S> ObserversTuple<I, S> for () {
    fn pre_exec_all(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        Ok(())
    }

    fn post_exec_all(
        &mut self,
        _state: &mut S,
        _input: &I,
        _exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        Ok(())
    }

    fn pre_exec_child_all(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        Ok(())
    }

    fn post_exec_child_all(
        &mut self,
        _state: &mut S,
        _input: &I,
        _exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl<Head, Tail, I, S> ObserversTuple<I, S> for (Head, Tail)
where
    Head: Observer<I, S>,
    Tail: ObserversTuple<I, S>,
{
    fn pre_exec_all(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.0.pre_exec(state, input)?;
        self.1.pre_exec_all(state, input)
    }

    fn post_exec_all(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.0.post_exec(state, input, exit_kind)?;
        self.1.post_exec_all(state, input, exit_kind)
    }

    fn pre_exec_child_all(&mut self, state: &mut S, input: &I) -> Result<(), Error> {
        self.0.pre_exec_child(state, input)?;
        self.1.pre_exec_child_all(state, input)
    }

    fn post_exec_child_all(
        &mut self,
        state: &mut S,
        input: &I,
        exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.0.post_exec_child(state, input, exit_kind)?;
        self.1.post_exec_child_all(state, input, exit_kind)
    }
}

/// A trait for [`Observer`]`s` with a hash field
pub trait ObserverWithHashField {
    /// get the value of the hash field
    fn hash(&self) -> Option<u64>;
}

/// A trait for [`Observer`]`s` which observe over differential execution.
///
/// Differential observers have the following flow during a single execution:
///  - `Observer::pre_exec` for the differential observer is invoked.
///  - `DifferentialObserver::pre_observe_first` for the differential observer is invoked.
///  - `Observer::pre_exec` for each of the observers for the first executor is invoked.
///  - The first executor is invoked.
///  - `Observer::post_exec` for each of the observers for the first executor is invoked.
///  - `DifferentialObserver::post_observe_first` for the differential observer is invoked.
///  - `DifferentialObserver::pre_observe_second` for the differential observer is invoked.
///  - `Observer::pre_exec` for each of the observers for the second executor is invoked.
///  - The second executor is invoked.
///  - `Observer::post_exec` for each of the observers for the second executor is invoked.
///  - `DifferentialObserver::post_observe_second` for the differential observer is invoked.
///  - `Observer::post_exec` for the differential observer is invoked.
///
/// You should perform any preparation for the diff execution in `Observer::pre_exec` and respective
/// cleanup in `Observer::post_exec`. For individual executions, use
/// `DifferentialObserver::{pre,post}_observe_{first,second}` as necessary for first and second,
/// respectively.
#[expect(unused_variables)]
pub trait DifferentialObserver<OTA, OTB, I, S>: Observer<I, S> {
    /// Perform an operation with the first set of observers before they are `pre_exec`'d.
    fn pre_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        Ok(())
    }

    /// Perform an operation with the first set of observers after they are `post_exec`'d.
    fn post_observe_first(&mut self, observers: &mut OTA) -> Result<(), Error> {
        Ok(())
    }

    /// Perform an operation with the second set of observers before they are `pre_exec`'d.
    fn pre_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        Ok(())
    }

    /// Perform an operation with the second set of observers after they are `post_exec`'d.
    fn post_observe_second(&mut self, observers: &mut OTB) -> Result<(), Error> {
        Ok(())
    }
}

/// Differential observers tuple, for when you're using multiple differential observers.
pub trait DifferentialObserversTuple<OTA, OTB, I, S>: ObserversTuple<I, S> {
    /// Perform an operation with the first set of observers before they are `pre_exec`'d on all the
    /// differential observers in this tuple.
    fn pre_observe_first_all(&mut self, observers: &mut OTA) -> Result<(), Error>;

    /// Perform an operation with the first set of observers after they are `post_exec`'d on all the
    /// differential observers in this tuple.
    fn post_observe_first_all(&mut self, observers: &mut OTA) -> Result<(), Error>;

    /// Perform an operation with the second set of observers before they are `pre_exec`'d on all
    /// the differential observers in this tuple.
    fn pre_observe_second_all(&mut self, observers: &mut OTB) -> Result<(), Error>;

    /// Perform an operation with the second set of observers after they are `post_exec`'d on all
    /// the differential observers in this tuple.
    fn post_observe_second_all(&mut self, observers: &mut OTB) -> Result<(), Error>;
}

impl<OTA, OTB, I, S> DifferentialObserversTuple<OTA, OTB, I, S> for () {
    fn pre_observe_first_all(&mut self, _: &mut OTA) -> Result<(), Error> {
        Ok(())
    }

    fn post_observe_first_all(&mut self, _: &mut OTA) -> Result<(), Error> {
        Ok(())
    }

    fn pre_observe_second_all(&mut self, _: &mut OTB) -> Result<(), Error> {
        Ok(())
    }

    fn post_observe_second_all(&mut self, _: &mut OTB) -> Result<(), Error> {
        Ok(())
    }
}

impl<Head, Tail, OTA, OTB, I, S> DifferentialObserversTuple<OTA, OTB, I, S> for (Head, Tail)
where
    Head: DifferentialObserver<OTA, OTB, I, S>,
    Tail: DifferentialObserversTuple<OTA, OTB, I, S>,
{
    fn pre_observe_first_all(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.0.pre_observe_first(observers)?;
        self.1.pre_observe_first_all(observers)
    }

    fn post_observe_first_all(&mut self, observers: &mut OTA) -> Result<(), Error> {
        self.0.post_observe_first(observers)?;
        self.1.post_observe_first_all(observers)
    }

    fn pre_observe_second_all(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.0.pre_observe_second(observers)?;
        self.1.pre_observe_second_all(observers)
    }

    fn post_observe_second_all(&mut self, observers: &mut OTB) -> Result<(), Error> {
        self.0.post_observe_second(observers)?;
        self.1.post_observe_second_all(observers)
    }
}

/// A simple observer, just overlooking the runtime of the target.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct TimeObserver {
    name: Cow<'static, str>,

    #[cfg(feature = "std")]
    #[serde(with = "instant_serializer")]
    start_time: Instant,

    #[cfg(not(feature = "std"))]
    start_time: Duration,

    last_runtime: Option<Duration>,
}

#[cfg(feature = "std")]
mod instant_serializer {
    use core::time::Duration;
    use std::time::Instant;

    use serde::{Deserialize, Deserializer, Serialize, Serializer};

    pub fn serialize<S>(instant: &Instant, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let duration = instant.elapsed();
        duration.serialize(serializer)
    }

    pub fn deserialize<'de, D>(deserializer: D) -> Result<Instant, D::Error>
    where
        D: Deserializer<'de>,
    {
        let duration = Duration::deserialize(deserializer)?;
        let instant = Instant::now().checked_sub(duration).unwrap();
        Ok(instant)
    }
}

impl TimeObserver {
    /// Creates a new [`TimeObserver`] with the given name.
    #[must_use]
    pub fn new<S>(name: S) -> Self
    where
        S: Into<Cow<'static, str>>,
    {
        Self {
            name: name.into(),

            #[cfg(feature = "std")]
            start_time: Instant::now(),

            #[cfg(not(feature = "std"))]
            start_time: Duration::from_secs(0),

            last_runtime: None,
        }
    }

    /// Gets the runtime for the last execution of this target.
    #[must_use]
    pub fn last_runtime(&self) -> &Option<Duration> {
        &self.last_runtime
    }
}

impl<I, S> Observer<I, S> for TimeObserver {
    #[cfg(feature = "std")]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.last_runtime = None;
        self.start_time = Instant::now();
        Ok(())
    }

    #[cfg(not(feature = "std"))]
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.last_runtime = None;
        self.start_time = current_time();
        Ok(())
    }

    #[cfg(feature = "std")]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &I,
        _exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.last_runtime = Some(self.start_time.elapsed());
        Ok(())
    }

    #[cfg(not(feature = "std"))]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &I,
        _exit_kind: &ExitKind,
    ) -> Result<(), Error> {
        self.last_runtime = current_time().checked_sub(self.start_time);
        Ok(())
    }
}

impl Named for TimeObserver {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<OTA, OTB, I, S> DifferentialObserver<OTA, OTB, I, S> for TimeObserver {}

#[cfg(feature = "std")]
#[cfg(test)]
mod tests {

    use libafl_bolts::{
        Named,
        ownedref::OwnedMutSlice,
        tuples::{tuple_list, tuple_list_type},
    };

    use crate::observers::{StdMapObserver, TimeObserver};

    static mut MAP: [u32; 4] = [0; 4];

    #[test]
    fn test_observer_serde() {
        let map_ptr = &raw const MAP;
        let obv = tuple_list!(TimeObserver::new("time"), unsafe {
            let len = (*map_ptr).len();
            StdMapObserver::from_ownedref(
                "map",
                OwnedMutSlice::from_raw_parts_mut(&raw mut MAP as *mut u32, len),
            )
        });
        let vec = postcard::to_allocvec(&obv).unwrap();
        log::info!("{vec:?}");
        let obv2: tuple_list_type!(TimeObserver, StdMapObserver<u32, false>) =
            postcard::from_bytes(&vec).unwrap();
        assert_eq!(obv.0.name(), obv2.0.name());
    }
}
