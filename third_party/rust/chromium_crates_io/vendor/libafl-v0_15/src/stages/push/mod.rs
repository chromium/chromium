//! [`PushStage`]`s` return inputs instead of calling an executor
//!
//! While normal stages call the executor over and over again, push stages turn this concept upside down:
//! A push stage instead returns an iterator that generates a new result for each time it gets called.
//! With the new testcase, you will have to take care about testcase execution, manually.
//! The push stage relies on internal mutability of the supplied `Observers`.

/// Mutational stage is the normal fuzzing stage.
pub mod mutational;
use alloc::{
    borrow::{Cow, ToOwned},
    rc::Rc,
    string::ToString,
};
use core::{
    cell::{Cell, RefCell},
    marker::PhantomData,
};

use libafl_bolts::Named;
pub use mutational::StdMutationalPushStage;

use crate::{
    Error, EvaluatorObservers, ExecutesInput, ExecutionProcessor, HasMetadata, HasScheduler,
    common::HasNamedMetadata,
    corpus::{CorpusId, HasCurrentCorpusId},
    events::{EventFirer, EventRestarter, HasEventManagerId, ProgressReporter},
    executors::{Executor, ExitKind, HasObservers},
    observers::ObserversTuple,
    schedulers::Scheduler,
    stages::{Restartable, RetryCountRestartHelper, Stage},
    state::{HasCorpus, HasExecutions, HasLastReportTime, HasRand},
};

// The shared state for all [`PushStage`]s
/// Should be stored inside a `[Rc<RefCell<_>>`]
#[derive(Debug, Clone)]
pub struct PushStageSharedState<EM, I, OT, S, Z> {
    /// The state
    pub state: S,
    /// The [`crate::fuzzer::Fuzzer`] instance
    pub fuzzer: Z,
    /// The event manager
    pub event_mgr: EM,
    /// The [`ObserversTuple`]
    pub observers: OT,
    phantom: PhantomData<I>,
}

impl<EM, I, OT, S, Z> PushStageSharedState<EM, I, OT, S, Z> {
    /// Create a new `PushStageSharedState` that can be used by all [`PushStage`]s
    #[must_use]
    pub fn new(fuzzer: Z, state: S, observers: OT, event_mgr: EM) -> Self {
        Self {
            state,
            fuzzer,
            event_mgr,
            observers,
            phantom: PhantomData,
        }
    }
}

/// Helper class for the [`PushStage`] trait, taking care of borrowing the shared state
#[derive(Debug, Clone)]
pub struct PushStageHelper<EM, I, OT, S, Z> {
    /// If this stage has already been initalized.
    /// This gets reset to `false` after one iteration of the stage is done.
    pub initialized: bool,
    /// The shared state, keeping track of the corpus and the fuzzer
    #[expect(clippy::type_complexity)]
    pub shared_state: Rc<RefCell<Option<PushStageSharedState<EM, I, OT, S, Z>>>>,
    /// If the last iteration failed
    pub errored: bool,

    /// The corpus index we're currently working on
    pub current_corpus_id: Option<CorpusId>,

    /// The input we just ran
    pub current_input: Option<I>, // Todo: Get rid of copy

    exit_kind: Rc<Cell<Option<ExitKind>>>,
}

impl<EM, I, OT, S, Z> PushStageHelper<EM, I, OT, S, Z> {
    /// Create a new [`PushStageHelper`]
    #[must_use]
    #[expect(clippy::type_complexity)]
    pub fn new(
        shared_state: Rc<RefCell<Option<PushStageSharedState<EM, I, OT, S, Z>>>>,
        exit_kind_ref: Rc<Cell<Option<ExitKind>>>,
    ) -> Self {
        Self {
            shared_state,
            initialized: false,
            exit_kind: exit_kind_ref,
            errored: false,
            current_input: None,
            current_corpus_id: None,
        }
    }

    /// Sets the shared state for this helper (and all other helpers owning the same [`RefCell`])
    #[inline]
    pub fn set_shared_state(&mut self, shared_state: PushStageSharedState<EM, I, OT, S, Z>) {
        (*self.shared_state.borrow_mut()).replace(shared_state);
    }

    /// Takes the shared state from this helper, replacing it with `None`
    #[inline]
    pub fn take_shared_state(&mut self) -> Option<PushStageSharedState<EM, I, OT, S, Z>> {
        let shared_state_ref = &mut (*self.shared_state).borrow_mut();
        shared_state_ref.take()
    }

    /// Returns the exit kind of the last run
    #[inline]
    #[must_use]
    pub fn exit_kind(&self) -> Option<ExitKind> {
        self.exit_kind.get()
    }

    /// Resets the exit kind
    #[inline]
    pub fn reset_exit_kind(&mut self) {
        self.exit_kind.set(None);
    }

    /// Resets this state after a full stage iter.
    fn end_of_iter(&mut self, shared_state: PushStageSharedState<EM, I, OT, S, Z>, errored: bool) {
        self.set_shared_state(shared_state);
        self.errored = errored;
        self.current_corpus_id = None;
        if errored {
            self.initialized = false;
        }
    }
}

/// A push stage is a generator that returns a single testcase for each call.
/// It's an iterator so we can chain it.
/// After it has finished once, we will call it agan for the next fuzzer round.
pub trait PushStage<EM, I, OT, S, Z> {
    /// Gets the [`PushStageHelper`]
    fn push_stage_helper(&self) -> &PushStageHelper<EM, I, OT, S, Z>;
    /// Gets the [`PushStageHelper`] (mutable)
    fn push_stage_helper_mut(&mut self) -> &mut PushStageHelper<EM, I, OT, S, Z>;

    /// Set the current corpus index this stage works on
    fn set_current_corpus_id(&mut self, corpus_id: CorpusId) {
        self.push_stage_helper_mut().current_corpus_id = Some(corpus_id);
    }

    /// Called by `next_std` when this stage is being initialized.
    /// This is called before the first iteration of the stage.
    /// After the stage has finished once (after `deinit`), this will be called again.
    #[inline]
    fn init(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _event_mgr: &mut EM,
        _observers: &mut OT,
    ) -> Result<(), Error> {
        Ok(())
    }

    /// Called before the a test case is executed.
    /// Should return the test case to be executed.
    /// After this stage has finished, or if the stage does not process any inputs, this should return `None`.
    fn pre_exec(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _event_mgr: &mut EM,
        _observers: &mut OT,
    ) -> Option<Result<I, Error>>;

    /// Called after the execution of a testcase finished.
    #[inline]
    fn post_exec(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _event_mgr: &mut EM,
        _observers: &mut OT,
        _input: I,
        _exit_kind: ExitKind,
    ) -> Result<(), Error> {
        Ok(())
    }

    /// Called after the stage finished (`pre_exec` returned `None`)
    #[inline]
    fn deinit(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _event_mgr: &mut EM,
        _observers: &mut OT,
    ) -> Result<(), Error> {
        Ok(())
    }
}

/// Allows us to use a [`PushStage`] as a normal [`Stage`]
#[derive(Debug)]
pub struct PushStageAdapter<CS, EM, I, OT, PS, Z> {
    name: Cow<'static, str>,
    push_stage: PS,
    phantom: PhantomData<(CS, EM, I, OT, Z)>,
}

impl<CS, EM, I, OT, PS, Z> PushStageAdapter<CS, EM, I, OT, PS, Z> {
    /// Create a new [`PushStageAdapter`], wrapping the given [`PushStage`]
    /// to be used as a normal [`Stage`]
    #[must_use]
    pub fn new(push_stage: PS) -> Self {
        // unsafe but impossible that you create two threads both instantiating this instance
        let stage_id = unsafe {
            let ret = PUSH_STAGE_ADAPTER_ID;
            PUSH_STAGE_ADAPTER_ID += 1;
            ret
        };
        Self {
            name: Cow::Owned(
                PUSH_STAGE_ADAPTER_NAME.to_owned() + ":" + stage_id.to_string().as_str(),
            ),
            push_stage,
            phantom: PhantomData,
        }
    }
}
/// The unique counter for this stage
static mut PUSH_STAGE_ADAPTER_ID: usize = 0;
/// The name for push stage adapter
pub static PUSH_STAGE_ADAPTER_NAME: &str = "pushstageadapter";

impl<CS, EM, I, OT, PS, Z> Named for PushStageAdapter<CS, EM, I, OT, PS, Z> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<CS, EM, I, OT, PS, S, Z> Restartable<S> for PushStageAdapter<CS, EM, I, OT, PS, Z>
where
    S: HasMetadata + HasNamedMetadata + HasCurrentCorpusId,
{
    #[inline]
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        // TODO: Proper restart handling - call post_exec at the right time, etc...
        RetryCountRestartHelper::no_retry(state, &self.name)
    }

    #[inline]
    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        RetryCountRestartHelper::clear_progress(state, &self.name)
    }
}

impl<CS, E, EM, I, OT, PS, S, Z> Stage<E, EM, S, Z> for PushStageAdapter<CS, EM, I, OT, PS, Z>
where
    CS: Scheduler<I, S>,
    S: HasExecutions
        + HasRand
        + HasCorpus<I>
        + HasLastReportTime
        + HasCurrentCorpusId
        + HasNamedMetadata
        + HasMetadata,
    E: Executor<EM, I, S, Z> + HasObservers<Observers = OT>,
    EM: EventFirer<I, S> + EventRestarter<S> + HasEventManagerId + ProgressReporter<S>,
    OT: ObserversTuple<I, S>,
    PS: PushStage<EM, I, OT, S, Z>,
    Z: ExecutesInput<E, EM, I, S>
        + ExecutionProcessor<EM, I, OT, S>
        + EvaluatorObservers<E, EM, I, OT>
        + HasScheduler<I, S>,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        event_mgr: &mut EM,
    ) -> Result<(), Error> {
        let push_stage = &mut self.push_stage;

        let Some(corpus_id) = state.current_corpus_id()? else {
            return Err(Error::illegal_state(
                "state is not currently processing a corpus index",
            ));
        };

        push_stage.set_current_corpus_id(corpus_id);

        push_stage.init(fuzzer, state, event_mgr, &mut *executor.observers_mut())?;

        loop {
            let input =
                match push_stage.pre_exec(fuzzer, state, event_mgr, &mut *executor.observers_mut())
                {
                    Some(Ok(next_input)) => next_input,
                    Some(Err(err)) => return Err(err),
                    None => break,
                };

            let exit_kind = fuzzer.execute_input(state, executor, event_mgr, &input)?;

            push_stage.post_exec(
                fuzzer,
                state,
                event_mgr,
                &mut *executor.observers_mut(),
                input,
                exit_kind,
            )?;
        }

        self.push_stage
            .deinit(fuzzer, state, event_mgr, &mut *executor.observers_mut())
    }
}
