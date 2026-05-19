//! Stage that wraps another stage and tracks it's execution time in `State`
use core::{marker::PhantomData, time::Duration};

use libafl_bolts::{Error, current_time};

use crate::{
    HasMetadata,
    stages::{Restartable, Stage},
};
/// Track an inner Stage's execution time
#[derive(Debug)]
pub struct TimeTrackingStageWrapper<T, S, ST> {
    inner: ST,
    count: Duration,
    phantom: PhantomData<(T, S)>,
}

impl<T, S, ST> TimeTrackingStageWrapper<T, S, ST> {
    /// Create a `TimeTrackingStageWrapper`
    pub fn new(inner: ST) -> Self {
        Self {
            inner,
            count: Duration::from_secs(0),
            phantom: PhantomData,
        }
    }
}

impl<T, E, M, Z, S, ST> Stage<E, M, S, Z> for TimeTrackingStageWrapper<T, S, ST>
where
    S: HasMetadata,
    ST: Stage<E, M, S, Z>,
    T: libafl_bolts::serdeany::SerdeAny + From<Duration>,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut M,
    ) -> Result<(), Error> {
        let before_run = current_time();
        self.inner.perform(fuzzer, executor, state, manager)?;
        let after_run = current_time();
        self.count += after_run
            .checked_sub(before_run)
            .ok_or(Error::illegal_state(format!(
                "The time seems to have jumped in TimetrackingStageWrapper! {before_run:?}"
            )))?;

        if let Ok(meta) = state.metadata_mut::<T>() {
            *meta = T::from(self.count);
        } else {
            state.add_metadata::<T>(T::from(self.count));
        }
        Ok(())
    }
}

impl<T, S, ST> Restartable<S> for TimeTrackingStageWrapper<T, S, ST>
where
    ST: Restartable<S>,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        self.inner.should_restart(state)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        self.inner.clear_progress(state)
    }
}
