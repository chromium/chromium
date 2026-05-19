//! The tracing stage can trace the target and enrich a testcase with metadata, for example for `CmpLog`.

use alloc::{
    borrow::{Cow, ToOwned},
    string::ToString,
};
use core::{fmt::Debug, marker::PhantomData};

use libafl_bolts::Named;

#[cfg(feature = "introspection")]
use crate::monitors::stats::PerfFeature;
use crate::{
    Error, HasNamedMetadata,
    corpus::HasCurrentCorpusId,
    executors::{Executor, HasObservers},
    inputs::Input,
    mark_feature_time,
    observers::ObserversTuple,
    stages::{Restartable, RetryCountRestartHelper, Stage},
    start_timer,
    state::{HasCorpus, HasCurrentTestcase, HasExecutions, MaybeHasClientPerfMonitor},
};

/// A stage that runs a tracer executor
/// This should *NOT* be used with inprocess executor
#[derive(Debug, Clone)]
pub struct TracingStage<EM, I, TE, S, Z> {
    name: Cow<'static, str>,
    tracer_executor: TE,
    phantom: PhantomData<(EM, I, TE, S, Z)>,
}

impl<EM, I, TE, S, Z> TracingStage<EM, I, TE, S, Z>
where
    TE: Executor<EM, I, S, Z> + HasObservers,
    TE::Observers: ObserversTuple<I, S>,
    S: HasExecutions
        + HasCorpus<I>
        + HasNamedMetadata
        + HasCurrentTestcase<I>
        + MaybeHasClientPerfMonitor,
{
    /// Perform tracing on the given `CorpusId`. Useful for if wrapping [`TracingStage`] with your
    /// own stage and you need to manage [`super::NestedStageRetryCountRestartHelper`] differently
    /// see [`super::ConcolicTracingStage`]'s implementation as an example of usage.
    #[allow(rustdoc::broken_intra_doc_links)]
    pub fn trace(&mut self, fuzzer: &mut Z, state: &mut S, manager: &mut EM) -> Result<(), Error> {
        start_timer!(state);
        let input = state.current_input_cloned()?;

        mark_feature_time!(state, PerfFeature::GetInputFromCorpus);

        start_timer!(state);
        self.tracer_executor
            .observers_mut()
            .pre_exec_all(state, &input)?;
        mark_feature_time!(state, PerfFeature::PreExecObservers);

        start_timer!(state);
        let exit_kind = self
            .tracer_executor
            .run_target(fuzzer, state, manager, &input)?;
        mark_feature_time!(state, PerfFeature::TargetExecution);

        start_timer!(state);
        self.tracer_executor
            .observers_mut()
            .post_exec_all(state, &input, &exit_kind)?;
        mark_feature_time!(state, PerfFeature::PostExecObservers);

        Ok(())
    }
}

impl<E, EM, I, TE, S, Z> Stage<E, EM, S, Z> for TracingStage<EM, I, TE, S, Z>
where
    TE: Executor<EM, I, S, Z> + HasObservers,
    TE::Observers: ObserversTuple<I, S>,
    S: HasExecutions
        + HasCorpus<I>
        + HasNamedMetadata
        + HasCurrentCorpusId
        + MaybeHasClientPerfMonitor,
    I: Input,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        _executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        self.trace(fuzzer, state, manager)
    }
}

impl<EM, I, TE, S, Z> Restartable<S> for TracingStage<EM, I, TE, S, Z>
where
    S: HasNamedMetadata + HasCurrentCorpusId,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        RetryCountRestartHelper::no_retry(state, &self.name)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        RetryCountRestartHelper::clear_progress(state, &self.name)
    }
}

impl<EM, I, TE, S, Z> Named for TracingStage<EM, I, TE, S, Z> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

/// The counter for giving this stage unique id
static mut TRACING_STAGE_ID: usize = 0;
/// The name for tracing stage
pub static TRACING_STAGE_NAME: &str = "tracing";

impl<EM, I, TE, S, Z> TracingStage<EM, I, TE, S, Z> {
    /// Creates a new default stage
    pub fn new(tracer_executor: TE) -> Self {
        // unsafe but impossible that you create two threads both instantiating this instance
        let stage_id = unsafe {
            let ret = TRACING_STAGE_ID;
            TRACING_STAGE_ID += 1;
            ret
        };

        Self {
            name: Cow::Owned(TRACING_STAGE_NAME.to_owned() + ":" + stage_id.to_string().as_ref()),
            tracer_executor,
            phantom: PhantomData,
        }
    }

    /// Gets the underlying tracer executor
    pub fn executor(&self) -> &TE {
        &self.tracer_executor
    }

    /// Gets the underlying tracer executor (mut)
    pub fn executor_mut(&mut self) -> &mut TE {
        &mut self.tracer_executor
    }
}
