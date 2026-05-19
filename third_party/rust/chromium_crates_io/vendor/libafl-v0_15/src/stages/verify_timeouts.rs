#![expect(clippy::too_long_first_doc_paragraph)]
//! Stage that re-runs captured Timeouts with double the timeout to verify
//! Note: To capture the timeouts, use in conjunction with `CaptureTimeoutFeedback`
//! Note: Will NOT work with in process executors due to the potential for restarts/crashes when
//! running inputs.
use alloc::{collections::VecDeque, rc::Rc};
use core::{cell::RefCell, fmt::Debug, marker::PhantomData, time::Duration};

use libafl_bolts::Error;
use serde::{Deserialize, Serialize, de::DeserializeOwned};

use crate::{
    Evaluator, HasMetadata,
    executors::{Executor, HasObservers, SetTimeout},
    inputs::BytesInput,
    observers::ObserversTuple,
    stages::{Restartable, Stage},
};

/// Stage that re-runs inputs deemed as timeouts with double the timeout to assert that they are
/// not false positives. AFL++ style.
/// Note: Will NOT work with in process executors due to the potential for restarts/crashes when
/// running inputs.
#[derive(Debug)]
pub struct VerifyTimeoutsStage<E, I, S> {
    doubled_timeout: Duration,
    original_timeout: Duration,
    capture_timeouts: Rc<RefCell<bool>>,
    phantom: PhantomData<(E, I, S)>,
}

impl<E, I, S> VerifyTimeoutsStage<E, I, S> {
    /// Create a `VerifyTimeoutsStage`
    pub fn new(capture_timeouts: Rc<RefCell<bool>>, configured_timeout: Duration) -> Self {
        Self {
            capture_timeouts,
            doubled_timeout: configured_timeout * 2,
            original_timeout: configured_timeout,
            phantom: PhantomData,
        }
    }
}

/// Timeouts that `VerifyTimeoutsStage` will read from
#[derive(Default, Serialize, Deserialize, Debug, Clone)]
#[serde(bound = "I: for<'a> Deserialize<'a> + Serialize")]
pub struct TimeoutsToVerify<I> {
    inputs: VecDeque<I>,
}

libafl_bolts::impl_serdeany!(
    TimeoutsToVerify<I: Debug + 'static + Serialize + DeserializeOwned + Clone>,
    <BytesInput>
);

impl<I> TimeoutsToVerify<I> {
    /// Create a new `TimeoutsToVerify`
    #[must_use]
    pub fn new() -> Self {
        Self {
            inputs: VecDeque::new(),
        }
    }

    /// Add a `TimeoutsToVerify` to queue
    pub fn push(&mut self, input: I) {
        self.inputs.push_back(input);
    }

    /// Pop a `TimeoutsToVerify` to queue
    pub fn pop(&mut self) -> Option<I> {
        self.inputs.pop_front()
    }

    /// Count `TimeoutsToVerify` in queue
    #[must_use]
    pub fn count(&self) -> usize {
        self.inputs.len()
    }
}

impl<E, EM, I, S, Z> Stage<E, EM, S, Z> for VerifyTimeoutsStage<E, I, S>
where
    E::Observers: ObserversTuple<I, S>,
    E: Executor<EM, I, S, Z> + HasObservers + SetTimeout,
    Z: Evaluator<E, EM, I, S>,
    S: HasMetadata,
    I: Debug + Serialize + DeserializeOwned + Default + 'static + Clone,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let mut timeouts = state
            .metadata_or_insert_with(TimeoutsToVerify::<I>::new)
            .clone();
        if timeouts.count() == 0 {
            return Ok(());
        }
        executor.set_timeout(self.doubled_timeout);
        *self.capture_timeouts.borrow_mut() = false;
        while let Some(input) = timeouts.pop() {
            fuzzer.evaluate_input(state, executor, manager, &input)?;
        }
        executor.set_timeout(self.original_timeout);
        *self.capture_timeouts.borrow_mut() = true;
        let res = state.metadata_mut::<TimeoutsToVerify<I>>().unwrap();
        *res = TimeoutsToVerify::<I>::new();
        Ok(())
    }
}

impl<E, I, S> Restartable<S> for VerifyTimeoutsStage<E, I, S> {
    fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
        Ok(true)
    }

    fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        Ok(())
    }
}
