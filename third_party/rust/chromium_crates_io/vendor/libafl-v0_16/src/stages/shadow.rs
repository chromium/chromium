//! A stage that runs the shadow executor using also the shadow observers. Unlike tracing stage, this
//! stage *CAN* be used with inprocess executor.

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
    executors::{Executor, HasObservers, ShadowExecutor},
    mark_feature_time,
    observers::ObserversTuple,
    stages::{Restartable, RetryCountRestartHelper, Stage},
    start_timer,
    state::{HasCorpus, HasCurrentTestcase, HasExecutions, MaybeHasClientPerfMonitor},
};

/// A stage that runs the shadow executor using also the shadow observers
#[derive(Debug, Clone)]
pub struct ShadowTracingStage<E, EM, I, SOT, S, Z> {
    name: Cow<'static, str>,
    phantom: PhantomData<(E, EM, I, SOT, S, Z)>,
}

impl<E, EM, I, SOT, S, Z> Default for ShadowTracingStage<E, EM, I, SOT, S, Z>
where
    E: Executor<EM, I, S, Z> + HasObservers,
    S: HasExecutions + HasCorpus<I>,
    SOT: ObserversTuple<I, S>,
{
    fn default() -> Self {
        Self::new()
    }
}

/// The counter for giving this stage unique id
static mut SHADOW_TRACING_STAGE_ID: usize = 0;
/// Name for shadow tracing stage
pub static SHADOW_TRACING_STAGE_NAME: &str = "shadow";

impl<E, EM, I, SOT, S, Z> Named for ShadowTracingStage<E, EM, I, SOT, S, Z> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<E, EM, I, SOT, S, Z> Stage<ShadowExecutor<E, I, S, SOT>, EM, S, Z>
    for ShadowTracingStage<E, EM, I, SOT, S, Z>
where
    E: Executor<EM, I, S, Z> + HasObservers,
    E::Observers: ObserversTuple<I, S>,
    SOT: ObserversTuple<I, S>,
    S: HasExecutions
        + HasCorpus<I>
        + HasNamedMetadata
        + Debug
        + HasCurrentTestcase<I>
        + HasCurrentCorpusId
        + MaybeHasClientPerfMonitor,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut ShadowExecutor<E, I, S, SOT>,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        start_timer!(state);
        let input = state.current_input_cloned()?;

        mark_feature_time!(state, PerfFeature::GetInputFromCorpus);

        start_timer!(state);
        executor
            .shadow_observers_mut()
            .pre_exec_all(state, &input)?;
        executor.observers_mut().pre_exec_all(state, &input)?;
        mark_feature_time!(state, PerfFeature::PreExecObservers);

        start_timer!(state);
        let exit_kind = executor.run_target(fuzzer, state, manager, &input)?;
        mark_feature_time!(state, PerfFeature::TargetExecution);

        start_timer!(state);
        executor
            .shadow_observers_mut()
            .post_exec_all(state, &input, &exit_kind)?;
        executor
            .observers_mut()
            .post_exec_all(state, &input, &exit_kind)?;
        mark_feature_time!(state, PerfFeature::PostExecObservers);

        Ok(())
    }
}

impl<E, EM, I, SOT, S, Z> Restartable<S> for ShadowTracingStage<E, EM, I, SOT, S, Z>
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

impl<E, EM, I, SOT, S, Z> ShadowTracingStage<E, EM, I, SOT, S, Z>
where
    E: Executor<EM, I, S, Z> + HasObservers,
    S: HasExecutions + HasCorpus<I>,
    SOT: ObserversTuple<I, S>,
{
    /// Creates a new default stage
    pub fn new() -> Self {
        // unsafe but impossible that you create two threads both instantiating this instance
        let stage_id = unsafe {
            let ret = SHADOW_TRACING_STAGE_ID;
            SHADOW_TRACING_STAGE_ID += 1;
            ret
        };
        Self {
            name: Cow::Owned(
                SHADOW_TRACING_STAGE_NAME.to_owned() + ":" + stage_id.to_string().as_str(),
            ),
            phantom: PhantomData,
        }
    }
}
