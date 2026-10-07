//! Feedback that captures Timeouts for re-running
use alloc::{borrow::Cow, rc::Rc};
use core::{cell::RefCell, fmt::Debug};

use libafl_bolts::{Error, Named};
use serde::{Serialize, de::DeserializeOwned};

use crate::{
    HasMetadata,
    corpus::Testcase,
    executors::ExitKind,
    feedbacks::{Feedback, StateInitializer},
    stages::verify_timeouts::TimeoutsToVerify,
    state::HasCorpus,
};

/// A Feedback that captures all timeouts and stores them in State for re-evaluation later.
/// Use in conjunction with `VerifyTimeoutsStage`
#[derive(Debug)]
pub struct CaptureTimeoutFeedback {
    enabled: Rc<RefCell<bool>>,
}

impl CaptureTimeoutFeedback {
    /// Create a new [`CaptureTimeoutFeedback`].
    pub fn new(enabled: Rc<RefCell<bool>>) -> Self {
        Self { enabled }
    }
}

impl Named for CaptureTimeoutFeedback {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("CaptureTimeoutFeedback");
        &NAME
    }
}

impl<S> StateInitializer<S> for CaptureTimeoutFeedback {}

impl<EM, I, OT, S> Feedback<EM, I, OT, S> for CaptureTimeoutFeedback
where
    S: HasCorpus<I> + HasMetadata,
    I: Debug + Serialize + DeserializeOwned + Default + 'static + Clone,
{
    #[inline]
    fn is_interesting(
        &mut self,
        state: &mut S,
        _manager: &mut EM,
        input: &I,
        _observers: &OT,
        exit_kind: &ExitKind,
    ) -> Result<bool, Error> {
        if *self.enabled.borrow() && matches!(exit_kind, ExitKind::Timeout) {
            let timeouts = state.metadata_or_insert_with(|| TimeoutsToVerify::<I>::new());
            timeouts.push(input.clone());
            return Ok(false);
        }
        Ok(matches!(exit_kind, ExitKind::Timeout))
    }

    fn append_metadata(
        &mut self,
        _state: &mut S,
        _manager: &mut EM,
        _observers: &OT,
        _testcase: &mut Testcase<I>,
    ) -> Result<(), Error> {
        Ok(())
    }

    #[cfg(feature = "track_hit_feedbacks")]
    #[inline]
    fn last_result(&self) -> Result<bool, Error> {
        Ok(false)
    }
}
