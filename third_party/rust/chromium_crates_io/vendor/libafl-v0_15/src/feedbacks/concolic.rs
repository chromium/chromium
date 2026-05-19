//! Concolic feedback for concolic fuzzing.
//!
//! It is used to attach concolic tracing metadata to the testcase.
//! This feedback should be used in combination with another feedback as this feedback always considers testcases
//! to be not interesting.
//! Requires a [`ConcolicObserver`] to observe the concolic trace.
use alloc::borrow::Cow;
use core::fmt::Debug;

use libafl_bolts::{
    Named,
    tuples::{Handle, Handled, MatchName, MatchNameRef},
};

use crate::{
    Error, HasMetadata,
    corpus::Testcase,
    feedbacks::{Feedback, StateInitializer},
    observers::concolic::ConcolicObserver,
};

/// The concolic feedback. It is used to attach concolic tracing metadata to the testcase.
///
/// This feedback should be used in combination with another feedback as this feedback always considers testcases
/// to be not interesting.
/// Requires a [`ConcolicObserver`] to observe the concolic trace.
#[derive(Debug)]
pub struct ConcolicFeedback<'map> {
    observer_handle: Handle<ConcolicObserver<'map>>,
}

impl<'map> ConcolicFeedback<'map> {
    /// Creates a concolic feedback from an observer
    #[must_use]
    pub fn from_observer(observer: &ConcolicObserver<'map>) -> Self {
        Self {
            observer_handle: observer.handle(),
        }
    }

    fn add_concolic_feedback_to_metadata<I, OT>(
        &mut self,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) where
        OT: MatchName,
    {
        if let Some(metadata) = observers
            .get(&self.observer_handle)
            .map(ConcolicObserver::create_metadata_from_current_map)
        {
            testcase.metadata_map_mut().insert(metadata);
        }
    }
}

impl Named for ConcolicFeedback<'_> {
    fn name(&self) -> &Cow<'static, str> {
        self.observer_handle.name()
    }
}

impl<S> StateInitializer<S> for ConcolicFeedback<'_> {}

impl<EM, I, OT, S> Feedback<EM, I, OT, S> for ConcolicFeedback<'_>
where
    OT: MatchName,
{
    #[cfg(feature = "track_hit_feedbacks")]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }

    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        observers: &OT,
        testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        self.add_concolic_feedback_to_metadata(observers, testcase);
        Ok(())
    }
}
