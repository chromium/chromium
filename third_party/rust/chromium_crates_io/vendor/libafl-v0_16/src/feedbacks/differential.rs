//! Diff Feedback, comparing the content of two observers of the same type.

use alloc::borrow::Cow;
use core::fmt::{self, Debug, Formatter};

use libafl_bolts::{
    Named,
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};
use serde::{Deserialize, Serialize};

#[cfg(feature = "track_hit_feedbacks")]
use crate::feedbacks::premature_last_result_err;
use crate::{
    Error,
    executors::ExitKind,
    feedbacks::{Feedback, FeedbackFactory, StateInitializer},
};

/// The result of a differential test between two observers.
#[derive(Debug, Copy, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum DiffResult {
    /// The two observers report the same outcome.
    Equal,
    /// The two observers report different outcomes.
    Diff,
}

impl DiffResult {
    /// Returns `true` if the two observers report the same outcome.
    #[must_use]
    pub fn is_equal(&self) -> bool {
        match self {
            DiffResult::Equal => true,
            DiffResult::Diff => false,
        }
    }

    /// Returns `true` if the two observers report different outcomes.
    #[must_use]
    pub fn is_diff(&self) -> bool {
        !self.is_equal()
    }
}

/// Compares two [`crate::observers::Observer`]s to see if the result should be denoted as equal
pub trait DiffComparator<O1, O2> {
    /// Performs the comparison between two [`crate::observers::Observer`]s
    fn compare(&mut self, first: &O1, second: &O2) -> DiffResult;
}

impl<F, O1, O2> DiffComparator<O1, O2> for F
where
    F: Fn(&O1, &O2) -> DiffResult,
{
    fn compare(&mut self, first: &O1, second: &O2) -> DiffResult {
        self(first, second)
    }
}

/// A [`DiffFeedback`] compares the content of two observers using the given compare function.
#[derive(Serialize, Deserialize)]
pub struct DiffFeedback<C, O1, O2> {
    /// This feedback's name
    name: Cow<'static, str>,
    /// The first observer to compare against
    o1_ref: Handle<O1>,
    /// The second observer to compare against
    o2_ref: Handle<O2>,
    // The previous run's result of `Self::is_interesting`
    #[cfg(feature = "track_hit_feedbacks")]
    last_result: Option<bool>,
    /// The comparator used to compare the two observers
    comparator: C,
}

impl<C, O1, O2> DiffFeedback<C, O1, O2>
where
    O1: Named,
    O2: Named,
{
    /// Create a new [`DiffFeedback`] using two observers and a test function.
    pub fn new(name: &'static str, o1: &O1, o2: &O2, comparator: C) -> Result<Self, Error> {
        let o1_ref = o1.handle();
        let o2_ref = o2.handle();
        if o1_ref.name() == o2_ref.name() {
            Err(Error::illegal_argument(format!(
                "DiffFeedback: observer names must be different (both were {})",
                o1_ref.name()
            )))
        } else {
            Ok(Self {
                o1_ref,
                o2_ref,
                name: Cow::from(name),
                #[cfg(feature = "track_hit_feedbacks")]
                last_result: None,
                comparator,
            })
        }
    }
}

impl<C, O1, O2, T> FeedbackFactory<DiffFeedback<C, O1, O2>, T> for DiffFeedback<C, O1, O2>
where
    C: Clone,
{
    fn create_feedback(&self, _ctx: &T) -> DiffFeedback<C, O1, O2> {
        Self {
            name: self.name.clone(),
            o1_ref: self.o1_ref.clone(),
            o2_ref: self.o2_ref.clone(),
            comparator: self.comparator.clone(),
            #[cfg(feature = "track_hit_feedbacks")]
            last_result: None,
        }
    }
}

impl<C, O1, O2> Named for DiffFeedback<C, O1, O2> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<C, O1, O2> Debug for DiffFeedback<C, O1, O2>
where
    O1: Debug,
    O2: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("DiffFeedback")
            .field("name", self.name())
            .field("o1", &self.o1_ref)
            .field("o2", &self.o2_ref)
            .finish_non_exhaustive()
    }
}

impl<C, O1, O2, S> StateInitializer<S> for DiffFeedback<C, O1, O2> {}

impl<C, EM, I, O1, O2, OT, S> Feedback<EM, I, OT, S> for DiffFeedback<C, O1, O2>
where
    OT: MatchName,
    C: DiffComparator<O1, O2>,
{
    fn is_interesting(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _input: &I,
        observers: &OT,
        _exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        fn err(name: &str) -> Error {
            Error::illegal_argument(format!("DiffFeedback: observer {name} not found"))
        }
        let o1: &O1 = observers
            .get(&self.o1_ref)
            .ok_or_else(|| err(self.o1_ref.name()))?;
        let o2: &O2 = observers
            .get(&self.o2_ref)
            .ok_or_else(|| err(self.o2_ref.name()))?;
        let res = self.comparator.compare(o1, o2) == DiffResult::Diff;
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

#[cfg(test)]
mod tests {
    use alloc::borrow::Cow;

    use libafl_bolts::{Named, tuples::tuple_list};

    use crate::{
        events::NopEventManager,
        executors::ExitKind,
        feedbacks::{DiffFeedback, Feedback, differential::DiffResult},
        inputs::BytesInput,
        observers::Observer,
        state::NopState,
    };

    #[derive(Debug)]
    struct DummyObserver {
        name: Cow<'static, str>,
        value: bool,
    }
    impl DummyObserver {
        fn new(name: &'static str, value: bool) -> Self {
            Self {
                name: Cow::from(name),
                value,
            }
        }
    }
    impl<I, S> Observer<I, S> for DummyObserver {}
    impl PartialEq for DummyObserver {
        fn eq(&self, other: &Self) -> bool {
            self.value == other.value
        }
    }
    impl Named for DummyObserver {
        fn name(&self) -> &Cow<'static, str> {
            &self.name
        }
    }

    fn comparator(o1: &DummyObserver, o2: &DummyObserver) -> DiffResult {
        if o1 == o2 {
            DiffResult::Equal
        } else {
            DiffResult::Diff
        }
    }

    fn test_diff(should_equal: bool) {
        let mut nop_state: NopState<BytesInput> = NopState::new();

        let o1 = DummyObserver::new("o1", true);
        let o2 = DummyObserver::new("o2", should_equal);

        let mut diff_feedback = DiffFeedback::new("diff_feedback", &o1, &o2, comparator).unwrap();
        let observers = tuple_list![o1, o2];
        assert_eq!(
            !should_equal,
            DiffFeedback::<_, _, _>::is_interesting(
                &mut diff_feedback,
                &mut nop_state,
                &mut NopEventManager::default(),
                &BytesInput::new(vec![0]),
                &observers,
                &ExitKind::Ok
            )
            .unwrap()
        );
    }

    #[test]
    fn test_diff_eq() {
        test_diff(true);
    }

    #[test]
    fn test_diff_neq() {
        test_diff(false);
    }
}
