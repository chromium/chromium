//! The feedbacks reduce observer state after each run to a single `is_interesting`-value.
//! If a testcase is interesting, it may be added to a Corpus.

// TODO: make S of Feedback<S> an associated type when specialisation + AT is stable

use alloc::borrow::Cow;
#[cfg(feature = "track_hit_feedbacks")]
use alloc::vec::Vec;
use core::{fmt::Debug, marker::PhantomData};

#[cfg(feature = "std")]
pub use concolic::ConcolicFeedback;
pub use differential::DiffFeedback;
use libafl_bolts::{
    Named,
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};
pub use list::*;
pub use map::*;
#[cfg(feature = "nautilus")]
pub use nautilus::*;
#[cfg(feature = "std")]
pub use new_hash_feedback::NewHashFeedback;
#[cfg(feature = "std")]
pub use new_hash_feedback::NewHashFeedbackMetadata;
use serde::{Deserialize, Serialize};

use crate::{Error, corpus::Testcase, executors::ExitKind, observers::TimeObserver};

#[cfg(feature = "std")]
pub mod capture_feedback;

pub mod bool;
pub use bool::BoolValueFeedback;

#[cfg(feature = "std")]
pub mod concolic;
#[cfg(feature = "std")]
/// The module for `CustomFilenameToTestcaseFeedback`
pub mod custom_filename;
pub mod differential;
/// The module for list feedback
pub mod list;
pub mod map;
#[cfg(feature = "nautilus")]
pub mod nautilus;
#[cfg(feature = "std")]
pub mod new_hash_feedback;
#[cfg(feature = "simd")]
pub mod simd;
#[cfg(feature = "std")]
pub mod stdio;
pub mod transferred;

#[cfg(feature = "std")]
pub use capture_feedback::CaptureTimeoutFeedback;

#[cfg(feature = "introspection")]
use crate::state::HasClientPerfMonitor;

#[cfg(feature = "value_bloom_feedback")]
pub mod value_bloom;
#[cfg(feature = "value_bloom_feedback")]
pub use value_bloom::ValueBloomFeedback;

/// Feedback which initializes a state.
///
/// This trait is separate from the general [`Feedback`] definition as it would not be sufficiently
/// specified otherwise.
pub trait StateInitializer<S> {
    /// Initializes the feedback state.
    /// This method is called after that the `State` is created.
    fn init_state(&mut self, _state: &mut S) -> Result<(), Error> {
        Ok(())
    }
}

/// Feedbacks evaluate the observers.
/// Basically, they reduce the information provided by an observer to a value,
/// indicating the "interestingness" of the last run.
pub trait Feedback<EM, I, OT, S>: StateInitializer<S> + Named {
    /// `is_interesting ` return if an input is worth the addition to the corpus
    fn is_interesting(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _input: &I,
        _observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        Ok(false)
    }

    /// Returns if the result of a run is interesting and the value input should be stored in a corpus.
    /// It also keeps track of introspection stats.
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
        // Start a timer for this feedback
        let start_time = libafl_bolts::cpu::read_time_counter();

        // Execute this feedback
        let ret = self.is_interesting(state, manager, input, observers, exit_kind);

        // Get the elapsed time for checking this feedback
        let elapsed = libafl_bolts::cpu::read_time_counter() - start_time;

        // Add this stat to the feedback metrics
        state
            .introspection_stats_mut()
            .update_feedback(self.name(), elapsed);

        ret
    }

    /// CUT MY LIFE INTO PIECES; THIS IS MY LAST [`Feedback::is_interesting`] run
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error>;

    /// Append this [`Feedback`]'s name if [`Feedback::last_result`] is true
    /// If you have any nested Feedbacks, you must call this function on them if relevant.
    /// See the implementations of [`CombinedFeedback`]
    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks(&self, list: &mut Vec<Cow<'static, str>>) -> Result<(), Error> {
        if self.last_result()? {
            list.push(self.name().clone());
        }
        Ok(())
    }

    /// Append to the testcase the generated metadata in case of a new corpus item
    ///
    /// Precondition: `testcase` must contain an input.
    #[inline]
    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _observers: &OT,
        _testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

/// Has an associated observer name (mostly used to retrieve the observer with `MatchName` from an `ObserverTuple`)
pub trait HasObserverHandle {
    /// The observer for which we hold a reference
    type Observer: ?Sized;

    /// The name associated with the observer
    fn observer_handle(&self) -> &Handle<Self::Observer>;
}

/// A combined feedback consisting of multiple [`Feedback`]s
#[derive(Debug)]
pub struct CombinedFeedback<A, B, FL> {
    /// First [`Feedback`]
    pub first: A,
    /// Second [`Feedback`]
    pub second: B,
    name: Cow<'static, str>,
    phantom: PhantomData<FL>,
}

impl<A, B, FL> Named for CombinedFeedback<A, B, FL> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<A, B, FL> CombinedFeedback<A, B, FL>
where
    A: Named,
    B: Named,
    FL: FeedbackLogic,
{
    /// Create a new combined feedback
    pub fn new(first: A, second: B) -> Self {
        let name = Cow::from(format!(
            "{} ({},{})",
            FL::name(),
            first.name(),
            second.name()
        ));
        Self {
            first,
            second,
            name,
            phantom: PhantomData,
        }
    }
}

impl<A, B, FL, S> StateInitializer<S> for CombinedFeedback<A, B, FL>
where
    A: StateInitializer<S>,
    B: StateInitializer<S>,
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        self.first.init_state(state)?;
        self.second.init_state(state)?;
        Ok(())
    }
}

impl<A, B, FL, EM, I, OT, S> Feedback<EM, I, OT, S> for CombinedFeedback<A, B, FL>
where
    A: Feedback<EM, I, OT, S>,
    B: Feedback<EM, I, OT, S>,
    FL: FeedbackLogic,
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        FL::is_pair_interesting(
            |state, manager, input, observers, exit_kind| {
                self.first
                    .is_interesting(state, manager, input, observers, exit_kind)
            },
            |state, manager, input, observers, exit_kind| {
                self.second
                    .is_interesting(state, manager, input, observers, exit_kind)
            },
            state,
            manager,
            input,
            observers,
            exit_kind,
        )
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
        FL::is_pair_interesting(
            |state, manager, input, observers, exit_kind| {
                self.first
                    .is_interesting_introspection(state, manager, input, observers, exit_kind)
            },
            |state, manager, input, observers, exit_kind| {
                self.second
                    .is_interesting_introspection(state, manager, input, observers, exit_kind)
            },
            state,
            manager,
            input,
            observers,
            exit_kind,
        )
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        FL::last_result(self.first.last_result(), self.second.last_result())
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks(&self, list: &mut Vec<Cow<'static, str>>) -> Result<(), Error> {
        FL::append_hit_feedbacks(
            self.first.last_result(),
            |list| self.first.append_hit_feedbacks(list),
            self.second.last_result(),
            |list| self.second.append_hit_feedbacks(list),
            list,
        )
    }

    #[inline]
    fn append_metadata(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        self.first
            .append_metadata(state, manager, observers, testcase)?;
        self.second
            .append_metadata(state, manager, observers, testcase)
    }
}

impl<A, B, FL, T> FeedbackFactory<CombinedFeedback<A, B, FL>, T> for CombinedFeedback<A, B, FL>
where
    A: FeedbackFactory<A, T> + Named,
    B: FeedbackFactory<B, T> + Named,
    FL: FeedbackLogic,
{
    fn create_feedback(&self, ctx: &T) -> CombinedFeedback<A, B, FL> {
        CombinedFeedback::new(
            self.first.create_feedback(ctx),
            self.second.create_feedback(ctx),
        )
    }
}

/// Logical combination of two feedbacks
pub trait FeedbackLogic {
    /// The name of this combination
    fn name() -> &'static str;

    /// If the feedback pair is interesting.
    ///
    /// `first` and `second` are closures which invoke the corresponding
    /// [`Feedback::is_interesting`] methods of the associated feedbacks. Implementors may choose to
    /// use the closure or not, depending on eagerness logic
    fn is_pair_interesting<EM, I, OT, S, F1, F2>(
        first: F1,
        second: F2,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        F1: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
        F2: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>;

    /// Get the result of the last `Self::is_interesting` run
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(first: Result<bool, Error>, second: Result<bool, Error>) -> Result<bool, Error>;

    /// Append each [`Feedback`]'s name according to the logic implemented by this
    /// [`FeedbackLogic`]. `if_first` and `if_second` are closures which invoke the corresponding
    /// [`Feedback::append_hit_feedbacks`] logics of the relevant closures.
    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks<F1, F2>(
        first_result: Result<bool, Error>,
        if_first: F1,
        second_result: Result<bool, Error>,
        if_second: F2,
        list: &mut Vec<Cow<'static, str>>,
    ) -> Result<(), Error>
    where
        F1: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
        F2: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>;
}

/// Factory for feedbacks which should be sensitive to an existing context, e.g. observer(s) from a
/// specific execution
pub trait FeedbackFactory<F, T> {
    /// Create the feedback from the provided context
    fn create_feedback(&self, ctx: &T) -> F;
}

impl<FE, FU, T> FeedbackFactory<FE, T> for FU
where
    FU: Fn(&T) -> FE,
{
    fn create_feedback(&self, ctx: &T) -> FE {
        self(ctx)
    }
}
/// Eager `OR` combination of two feedbacks
///
/// When the `track_hit_feedbacks` feature is used, [`LogicEagerOr`]'s hit feedback preferences will
/// behave like [`LogicFastOr`]'s because the second feedback will not have contributed to the
/// result. When using [`crate::feedback_or`], ensure that you set the first parameter to the
/// prioritized feedback.
#[derive(Debug, Clone)]
pub struct LogicEagerOr;
/// Fast `OR` combination of two feedbacks
#[derive(Debug, Clone)]
pub struct LogicFastOr;

/// Eager `AND` combination of two feedbacks
#[derive(Debug, Clone)]
pub struct LogicEagerAnd;

/// Fast `AND` combination of two feedbacks
#[derive(Debug, Clone)]
pub struct LogicFastAnd;

impl FeedbackLogic for LogicEagerOr {
    fn name() -> &'static str {
        "Eager OR"
    }

    fn is_pair_interesting<EM, I, OT, S, F1, F2>(
        first: F1,
        second: F2,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        F1: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
        F2: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
    {
        Ok(first(state, manager, input, observers, exit_kind)?
            | second(state, manager, input, observers, exit_kind)?)
    }
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(first: Result<bool, Error>, second: Result<bool, Error>) -> Result<bool, Error> {
        first.and_then(|first| second.map(|second| first | second))
    }
    /// Note: Eager OR's hit feedbacks will behave like Fast OR
    /// because the second feedback will not have contributed to the result.
    /// Set the second feedback as the first (A, B) vs (B, A)
    /// to "prioritize" the result in case of Eager OR.
    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks<F1, F2>(
        first_result: Result<bool, Error>,
        if_first: F1,
        second_result: Result<bool, Error>,
        if_second: F2,
        list: &mut Vec<Cow<'static, str>>,
    ) -> Result<(), Error>
    where
        F1: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
        F2: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
    {
        LogicFastOr::append_hit_feedbacks(first_result, if_first, second_result, if_second, list)
    }
}

impl FeedbackLogic for LogicFastOr {
    fn name() -> &'static str {
        "Fast OR"
    }

    fn is_pair_interesting<EM, I, OT, S, F1, F2>(
        first: F1,
        second: F2,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        F1: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
        F2: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
    {
        let a = first(state, manager, input, observers, exit_kind)?;
        if a {
            return Ok(true);
        }

        second(state, manager, input, observers, exit_kind)
    }
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(first: Result<bool, Error>, second: Result<bool, Error>) -> Result<bool, Error> {
        first.and_then(|first| Ok(first || second?))
    }
    /// Note: Eager OR's hit feedbacks will behave like Fast OR
    /// because the second feedback will not have contributed to the result.
    /// Set the second feedback as the first (A, B) vs (B, A)
    /// to "prioritize" the result in case of Eager OR.
    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks<F1, F2>(
        first_result: Result<bool, Error>,
        if_first: F1,
        second_result: Result<bool, Error>,
        if_second: F2,
        list: &mut Vec<Cow<'static, str>>,
    ) -> Result<(), Error>
    where
        F1: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
        F2: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
    {
        if first_result? {
            if_first(list)
        } else if second_result? {
            if_second(list)
        } else {
            Ok(())
        }
    }
}

impl FeedbackLogic for LogicEagerAnd {
    fn name() -> &'static str {
        "Eager AND"
    }

    fn is_pair_interesting<EM, I, OT, S, F1, F2>(
        first: F1,
        second: F2,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        F1: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
        F2: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
    {
        Ok(first(state, manager, input, observers, exit_kind)?
            & second(state, manager, input, observers, exit_kind)?)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(first: Result<bool, Error>, second: Result<bool, Error>) -> Result<bool, Error> {
        Ok(first? & second?)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks<F1, F2>(
        first_result: Result<bool, Error>,
        if_first: F1,
        second_result: Result<bool, Error>,
        if_second: F2,
        list: &mut Vec<Cow<'static, str>>,
    ) -> Result<(), Error>
    where
        F1: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
        F2: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
    {
        if first_result? & second_result? {
            if_first(list)?;
            if_second(list)?;
        }
        Ok(())
    }
}

impl FeedbackLogic for LogicFastAnd {
    fn name() -> &'static str {
        "Fast AND"
    }

    fn is_pair_interesting<EM, I, OT, S, F1, F2>(
        first: F1,
        second: F2,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error>
    where
        F1: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
        F2: FnOnce(&mut S, &mut EM, &I, &OT, &ExitKind) -> Result<bool, Error>,
    {
        Ok(first(state, manager, input, observers, exit_kind)?
            && second(state, manager, input, observers, exit_kind)?)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(first: Result<bool, Error>, second: Result<bool, Error>) -> Result<bool, Error> {
        Ok(first? && second?)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn append_hit_feedbacks<F1, F2>(
        first_result: Result<bool, Error>,
        if_first: F1,
        second_result: Result<bool, Error>,
        if_second: F2,
        list: &mut Vec<Cow<'static, str>>,
    ) -> Result<(), Error>
    where
        F1: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
        F2: FnOnce(&mut Vec<Cow<'static, str>>) -> Result<(), Error>,
    {
        if first_result? && second_result? {
            if_first(list)?;
            if_second(list)?;
        }
        Ok(())
    }
}

/// Combine two feedbacks with an eager AND operation,
/// will call all feedbacks functions even if not necessary to conclude the result
pub type EagerAndFeedback<A, B> = CombinedFeedback<A, B, LogicEagerAnd>;

/// Combine two feedbacks with an fast AND operation,
/// might skip calling feedbacks functions if not necessary to conclude the result
pub type FastAndFeedback<A, B> = CombinedFeedback<A, B, LogicFastAnd>;

/// Combine two feedbacks with an eager OR operation,
/// will call all feedbacks functions even if not necessary to conclude the result
pub type EagerOrFeedback<A, B> = CombinedFeedback<A, B, LogicEagerOr>;

/// Combine two feedbacks with an fast OR operation - fast.
///
/// This might skip calling feedbacks functions if not necessary to conclude the result.
/// This means any feedback that is not first might be skipped, use caution when using with
/// `TimeFeedback`
pub type FastOrFeedback<A, B> = CombinedFeedback<A, B, LogicFastOr>;

/// Compose feedbacks with an `NOT` operation
#[derive(Debug, Clone)]
pub struct NotFeedback<A> {
    /// The feedback to invert
    pub inner: A,
    /// The name
    name: Cow<'static, str>,
}

impl<A, S> StateInitializer<S> for NotFeedback<A>
where
    A: StateInitializer<S>,
{
    fn init_state(&mut self, state: &mut S) -> Result<(), Error> {
        self.inner.init_state(state)
    }
}

impl<A, EM, I, OT, S> Feedback<EM, I, OT, S> for NotFeedback<A>
where
    A: Feedback<EM, I, OT, S>,
{
    fn is_interesting(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        input: &I,
        observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        Ok(!self
            .inner
            .is_interesting(state, manager, input, observers, exit_kind)?)
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(!self.inner.last_result()?)
    }

    #[inline]
    fn append_metadata(
        &mut self,
        state: &mut S,
        manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        self.inner
            .append_metadata(state, manager, observers, testcase)
    }
}

impl<A> Named for NotFeedback<A> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<A, T> FeedbackFactory<NotFeedback<A>, T> for NotFeedback<A>
where
    A: Named + FeedbackFactory<A, T>,
{
    fn create_feedback(&self, ctx: &T) -> NotFeedback<A> {
        NotFeedback::new(self.inner.create_feedback(ctx))
    }
}

impl<A> NotFeedback<A>
where
    A: Named,
{
    /// Creates a new [`NotFeedback`].
    pub fn new(inner: A) -> Self {
        let name = Cow::from(format!("Not({})", inner.name()));
        Self { inner, name }
    }
}

/// Variadic macro to create a chain of [`AndFeedback`](EagerAndFeedback)
#[macro_export]
macro_rules! feedback_and {
    ( $last:expr ) => { $last };

    ( $last:expr, ) => { $last };

    ( $head:expr, $($tail:expr),+ $(,)?) => {
        // recursive call
        $crate::feedbacks::EagerAndFeedback::new($head , feedback_and!($($tail),+))
    };
}
///
/// Variadic macro to create a chain of (fast) [`AndFeedback`](FastAndFeedback)
#[macro_export]
macro_rules! feedback_and_fast {
    ( $last:expr ) => { $last };

    ( $last:expr, ) => { $last };

    ( $head:expr, $($tail:expr),+ $(,)?) => {
        // recursive call
        $crate::feedbacks::FastAndFeedback::new($head , feedback_and_fast!($($tail),+))
    };
}

/// Variadic macro to create a chain of [`OrFeedback`](EagerOrFeedback)
#[macro_export]
macro_rules! feedback_or {
    ( $last:expr ) => { $last };

    ( $last:expr, ) => { $last };

    ( $head:expr, $($tail:expr),+ $(,)?) => {
        // recursive call
        $crate::feedbacks::EagerOrFeedback::new($head , feedback_or!($($tail),+))
    };
}

/// Combines multiple feedbacks with an `OR` operation, not executing feedbacks after the first positive result
#[macro_export]
macro_rules! feedback_or_fast {
    ( $last:expr ) => { $last };

    ( $last:expr, ) => { $last };

    ( $head:expr, $($tail:expr),+ $(,)?) => {
        // recursive call
        $crate::feedbacks::FastOrFeedback::new($head , feedback_or_fast!($($tail),+))
    };
}

/// Variadic macro to create a [`NotFeedback`]
#[macro_export]
macro_rules! feedback_not {
    ($last:expr) => {
        $crate::feedbacks::NotFeedback::new($last)
    };
}

impl<S> StateInitializer<S> for () {}

/// Hack to use () as empty Feedback
impl<EM, I, OT, S> Feedback<EM, I, OT, S> for () {
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }
}

/// Logic for measuring whether a given [`ExitKind`] is interesting as a [`Feedback`]. Use with
/// [`ExitKindFeedback`].
pub trait ExitKindLogic {
    /// The name of this kind of logic
    const NAME: Cow<'static, str>;

    /// Check whether the provided [`ExitKind`] is actually interesting
    fn check_exit_kind(kind: &ExitKind) -> Result<bool, Error>;
}
/// Name used by `CrashFeedback`
pub const CRASH_FEEDBACK_NAME: &str = "CrashFeedback";
/// Logic which finds all [`ExitKind::Crash`] exits interesting
#[derive(Debug, Copy, Clone)]
pub struct CrashLogic;

impl ExitKindLogic for CrashLogic {
    const NAME: Cow<'static, str> = Cow::Borrowed(CRASH_FEEDBACK_NAME);

    fn check_exit_kind(kind: &ExitKind) -> Result<bool, Error> {
        Ok(matches!(kind, ExitKind::Crash))
    }
}
/// Name used by `TimeoutFeedback`
pub const TIMEOUT_FEEDBACK_NAME: &str = "TimeoutFeedback";

/// Logic which finds all [`ExitKind::Timeout`] exits interesting
#[derive(Debug, Copy, Clone)]
pub struct TimeoutLogic;

impl ExitKindLogic for TimeoutLogic {
    const NAME: Cow<'static, str> = Cow::Borrowed(TIMEOUT_FEEDBACK_NAME);

    fn check_exit_kind(kind: &ExitKind) -> Result<bool, Error> {
        Ok(matches!(kind, ExitKind::Timeout))
    }
}

/// Logic which finds all [`ExitKind::Diff`] exits interesting
#[derive(Debug, Copy, Clone)]
pub struct GenericDiffLogic;

impl ExitKindLogic for GenericDiffLogic {
    const NAME: Cow<'static, str> = Cow::Borrowed("DiffExitKindFeedback");

    fn check_exit_kind(kind: &ExitKind) -> Result<bool, Error> {
        Ok(matches!(kind, ExitKind::Diff { .. }))
    }
}

/// A generic exit type checking feedback. Use [`CrashFeedback`], [`TimeoutFeedback`], or
/// [`DiffExitKindFeedback`] directly instead.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ExitKindFeedback<L> {
    #[cfg(feature = "track_hit_feedbacks")]
    /// The previous run's result of [`Self::is_interesting`]
    last_result: Option<bool>,
    name: Cow<'static, str>,
    phantom: PhantomData<fn() -> L>,
}

impl<L, S> StateInitializer<S> for ExitKindFeedback<L> where L: ExitKindLogic {}

impl<EM, I, L, OT, S> Feedback<EM, I, OT, S> for ExitKindFeedback<L>
where
    L: ExitKindLogic,
{
    fn is_interesting(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _input: &I,
        _observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        let res = L::check_exit_kind(exit_kind)?;
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
}

impl<L> Named for ExitKindFeedback<L> {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<L> ExitKindFeedback<L>
where
    L: ExitKindLogic,
{
    /// Creates a new [`ExitKindFeedback`]
    #[must_use]
    pub fn new() -> Self {
        Self {
            #[cfg(feature = "track_hit_feedbacks")]
            last_result: None,
            name: L::NAME,
            phantom: PhantomData,
        }
    }
}

impl<L> Default for ExitKindFeedback<L>
where
    L: ExitKindLogic,
{
    fn default() -> Self {
        Self::new()
    }
}

impl<L, T> FeedbackFactory<ExitKindFeedback<L>, T> for ExitKindFeedback<L>
where
    L: ExitKindLogic,
{
    fn create_feedback(&self, _ctx: &T) -> ExitKindFeedback<L> {
        Self::new()
    }
}

/// A [`CrashFeedback`] reports as interesting if the target crashed.
pub type CrashFeedback = ExitKindFeedback<CrashLogic>;
/// A [`TimeoutFeedback`] reduces the timeout value of a run.
pub type TimeoutFeedback = ExitKindFeedback<TimeoutLogic>;
/// A [`DiffExitKindFeedback`] checks if there is a difference in the [`ExitKind`]s in a [`crate::executors::DiffExecutor`].
pub type DiffExitKindFeedback = ExitKindFeedback<GenericDiffLogic>;

/// A [`Feedback`] to track execution time.
///
/// Nop feedback that annotates execution time in the new testcase, if any
/// for this Feedback, the testcase is never interesting (use with an OR).
/// It decides, if the given [`TimeObserver`] value of a run is interesting.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct TimeFeedback {
    observer_handle: Handle<TimeObserver>,
}
impl<S> StateInitializer<S> for TimeFeedback {}

impl<EM, I, OT, S> Feedback<EM, I, OT, S> for TimeFeedback
where
    OT: MatchName,
{
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }

    /// Append to the testcase the generated metadata in case of a new corpus item
    #[inline]
    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        let Some(observer) = observers.get(&self.observer_handle) else {
            return Err(Error::illegal_state(
                "Observer referenced by TimeFeedback is not found in observers given to the fuzzer",
            ));
        };

        *testcase.exec_time_mut() = *observer.last_runtime();
        Ok(())
    }
}

impl Named for TimeFeedback {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        self.observer_handle.name()
    }
}

impl TimeFeedback {
    /// Creates a new [`TimeFeedback`], deciding if the given [`TimeObserver`] value of a run is interesting.
    #[must_use]
    pub fn new(observer: &TimeObserver) -> Self {
        Self {
            observer_handle: observer.handle(),
        }
    }
}

/// The [`ConstFeedback`] reports the same value, always.
/// It can be used to enable or disable feedback results through composition.
#[derive(Serialize, Deserialize, Debug, Copy, Clone, PartialEq, Eq)]
pub enum ConstFeedback {
    /// Always returns `true`
    True,
    /// Always returns `false`
    False,
}

impl<S> StateInitializer<S> for ConstFeedback {}

impl<EM, I, OT, S> Feedback<EM, I, OT, S> for ConstFeedback {
    #[inline]
    fn is_interesting(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _input: &I,
        _observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        Ok((*self).into())
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok((*self).into())
    }
}

impl Named for ConstFeedback {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("ConstFeedback");
        &NAME
    }
}

impl ConstFeedback {
    /// Creates a new [`ConstFeedback`] from the given boolean
    #[must_use]
    pub fn new(val: bool) -> Self {
        Self::from(val)
    }
}

impl From<bool> for ConstFeedback {
    fn from(val: bool) -> Self {
        if val { Self::True } else { Self::False }
    }
}

impl From<ConstFeedback> for bool {
    fn from(value: ConstFeedback) -> Self {
        match value {
            ConstFeedback::True => true,
            ConstFeedback::False => false,
        }
    }
}

impl<T> FeedbackFactory<ConstFeedback, T> for ConstFeedback {
    fn create_feedback(&self, _ctx: &T) -> ConstFeedback {
        *self
    }
}

#[cfg(feature = "track_hit_feedbacks")]
/// Error if [`Feedback::last_result`] is called before the `Feedback` is actually run.
pub(crate) fn premature_last_result_err() -> Error {
    Error::illegal_state("last_result called before Feedback was run")
}
