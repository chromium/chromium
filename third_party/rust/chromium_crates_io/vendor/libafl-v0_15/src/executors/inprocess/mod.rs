//! The [`InProcessExecutor`] is a libfuzzer-like executor, that will simply call a function.
//! It should usually be paired with extra error-handling, such as a restarting event manager, to be effective.
//!
//! Needs the `fork` feature flag.
use alloc::boxed::Box;
use core::{
    borrow::BorrowMut,
    ffi::c_void,
    fmt::{self, Debug, Formatter},
    marker::PhantomData,
    ptr,
    time::Duration,
};

use libafl_bolts::tuples::{RefIndexable, tuple_list};

use crate::{
    Error, HasMetadata,
    corpus::{Corpus, Testcase},
    events::{Event, EventFirer, EventRestarter, EventWithStats},
    executors::{
        Executor, ExitKind, HasObservers,
        hooks::{ExecutorHooksTuple, inprocess::InProcessHooks},
        inprocess::inner::GenericInProcessExecutorInner,
    },
    feedbacks::Feedback,
    fuzzer::HasObjective,
    inputs::Input,
    observers::ObserversTuple,
    state::{HasCorpus, HasCurrentTestcase, HasExecutions, HasSolutions},
};

/// The inner structure of `InProcessExecutor`.
pub mod inner;
/// A version of `InProcessExecutor` with a state accessible from the harness.
pub mod stateful;

/// The process executor simply calls a target function, as mutable reference to a closure.
pub type InProcessExecutor<'a, EM, H, I, OT, S, Z> =
    GenericInProcessExecutor<EM, H, &'a mut H, (), I, OT, S, Z>;

/// The inprocess executor that allows hooks
pub type HookableInProcessExecutor<'a, EM, H, HT, I, OT, S, Z> =
    GenericInProcessExecutor<EM, H, &'a mut H, HT, I, OT, S, Z>;
/// The process executor simply calls a target function, as boxed `FnMut` trait object
pub type OwnedInProcessExecutor<EM, I, OT, S, Z> = GenericInProcessExecutor<
    EM,
    dyn FnMut(&I) -> ExitKind,
    Box<dyn FnMut(&I) -> ExitKind>,
    (),
    I,
    OT,
    S,
    Z,
>;

/// The inmem executor simply calls a target function, then returns afterwards.
pub struct GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z> {
    harness_fn: HB,
    inner: GenericInProcessExecutorInner<EM, HT, I, OT, S, Z>,
    phantom: PhantomData<(*const H, HB)>,
}

impl<EM, H, HB, HT, I, OT, S, Z> Debug for GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>
where
    OT: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("GenericInProcessExecutor")
            .field("inner", &self.inner)
            .field("harness_fn", &"<fn>")
            .finish_non_exhaustive()
    }
}

impl<EM, H, HB, HT, I, OT, S, Z> Executor<EM, I, S, Z>
    for GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>
where
    S: HasExecutions,
    OT: ObserversTuple<I, S>,
    HT: ExecutorHooksTuple<I, S>,
    HB: BorrowMut<H>,
    H: FnMut(&I) -> ExitKind + Sized,
{
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        *state.executions_mut() += 1;

        unsafe {
            let executor_ptr = ptr::from_ref(self) as *const c_void;
            self.inner
                .enter_target(fuzzer, state, mgr, input, executor_ptr);
        }

        self.inner.hooks.pre_exec_all(state, input);

        let ret = self.harness_fn.borrow_mut()(input);

        self.inner.hooks.post_exec_all(state, input);

        self.inner.leave_target(fuzzer, state, mgr, input);
        Ok(ret)
    }
}

impl<EM, H, HB, HT, I, OT, S, Z> HasObservers
    for GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>
{
    type Observers = OT;

    #[inline]
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        self.inner.observers()
    }

    #[inline]
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        self.inner.observers_mut()
    }
}

impl<'a, EM, H, I, OT, S, Z> InProcessExecutor<'a, EM, H, I, OT, S, Z>
where
    H: FnMut(&I) -> ExitKind + Sized,
    OT: ObserversTuple<I, S>,
    S: HasCurrentTestcase<I> + HasExecutions + HasSolutions<I>,
    I: Input,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    pub fn new<OF>(
        harness_fn: &'a mut H,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
    ) -> Result<Self, Error>
    where
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        Self::with_timeout_generic::<OF>(
            tuple_list!(),
            harness_fn,
            observers,
            fuzzer,
            state,
            event_mgr,
            Duration::from_millis(5000),
        )
    }

    /// Create a new in mem executor.
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `user_hooks` - the hooks run before and after the harness's execution
    /// * `harness_fn` - the harness, executing the function
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    pub fn with_timeout<OF>(
        harness_fn: &'a mut H,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
        timeout: Duration,
    ) -> Result<Self, Error>
    where
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        let inner = GenericInProcessExecutorInner::with_timeout_generic::<Self, OF>(
            tuple_list!(),
            observers,
            fuzzer,
            state,
            event_mgr,
            timeout,
        )?;

        Ok(Self {
            harness_fn,
            inner,
            phantom: PhantomData,
        })
    }
}

impl<EM, H, HB, HT, I, OT, S, Z> GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>
where
    H: FnMut(&I) -> ExitKind + Sized,
    HB: BorrowMut<H>,
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
    S: HasCurrentTestcase<I> + HasExecutions + HasSolutions<I>,
    I: Input,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    pub fn generic<OF>(
        user_hooks: HT,
        harness_fn: HB,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
    ) -> Result<Self, Error>
    where
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        Self::with_timeout_generic::<OF>(
            user_hooks,
            harness_fn,
            observers,
            fuzzer,
            state,
            event_mgr,
            Duration::from_millis(5000),
        )
    }

    /// Create a new [`InProcessExecutor`].
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `user_hooks` - the hooks run before and after the harness's execution
    /// * `harness_fn` - the harness, executing the function
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    pub fn with_timeout_generic<OF>(
        user_hooks: HT,
        harness_fn: HB,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
        timeout: Duration,
    ) -> Result<Self, Error>
    where
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        let inner = GenericInProcessExecutorInner::with_timeout_generic::<Self, OF>(
            user_hooks, observers, fuzzer, state, event_mgr, timeout,
        )?;

        Ok(Self {
            harness_fn,
            inner,
            phantom: PhantomData,
        })
    }

    /// Retrieve the harness function.
    #[inline]
    pub fn harness(&self) -> &H {
        self.harness_fn.borrow()
    }

    /// Retrieve the harness function for a mutable reference.
    #[inline]
    pub fn harness_mut(&mut self) -> &mut H {
        self.harness_fn.borrow_mut()
    }

    /// The inprocess handlers
    #[inline]
    pub fn hooks(&self) -> &(InProcessHooks<I, S>, HT) {
        self.inner.hooks()
    }

    /// The inprocess handlers (mutable)
    #[inline]
    pub fn hooks_mut(&mut self) -> &mut (InProcessHooks<I, S>, HT) {
        self.inner.hooks_mut()
    }
}

/// The struct has [`InProcessHooks`].
pub trait HasInProcessHooks<I, S> {
    /// Get the in-process handlers.
    fn inprocess_hooks(&self) -> &InProcessHooks<I, S>;

    /// Get the mut in-process handlers.
    fn inprocess_hooks_mut(&mut self) -> &mut InProcessHooks<I, S>;
}

impl<EM, H, HB, HT, I, OT, S, Z> HasInProcessHooks<I, S>
    for GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>
{
    /// the timeout handler
    #[inline]
    fn inprocess_hooks(&self) -> &InProcessHooks<I, S> {
        self.inner.inprocess_hooks()
    }

    /// the timeout handler
    #[inline]
    fn inprocess_hooks_mut(&mut self) -> &mut InProcessHooks<I, S> {
        self.inner.inprocess_hooks_mut()
    }
}

#[inline]
/// Save state if it is an objective
/// Note that unlike the logic in fuzzer/mod.rs
/// This will *NOT* put any testcase into the corpus.
/// As it totally does not make any sense to put when we use inprocess executor or its descendants.
pub fn run_observers_and_save_state<E, EM, I, OF, S, Z>(
    executor: &mut E,
    state: &mut S,
    input: &I,
    fuzzer: &mut Z,
    event_mgr: &mut EM,
    exitkind: ExitKind,
) where
    E: HasObservers,
    E::Observers: ObserversTuple<I, S>,
    EM: EventFirer<I, S> + EventRestarter<S>,
    OF: Feedback<EM, I, E::Observers, S>,
    S: HasExecutions + HasSolutions<I> + HasCorpus<I> + HasCurrentTestcase<I>,
    Z: HasObjective<Objective = OF>,
    I: Input + Clone,
{
    log::info!("in crash handler!");
    let mut observers = executor.observers_mut();

    observers
        .post_exec_all(state, input, &exitkind)
        .expect("Observers post_exec_all failed");

    let is_solution = fuzzer
        .objective_mut()
        .is_interesting(state, event_mgr, input, &*observers, &exitkind)
        .expect("In run_observers_and_save_state objective failure.");

    if is_solution {
        let mut new_testcase = Testcase::from(input.clone());
        new_testcase.set_executions(*state.executions());
        new_testcase.add_metadata(exitkind);
        new_testcase.set_parent_id_optional(*state.corpus().current());

        if let Ok(mut tc) = state.current_testcase_mut() {
            tc.found_objective();
        }

        fuzzer
            .objective_mut()
            .append_metadata(state, event_mgr, &*observers, &mut new_testcase)
            .expect("Failed adding metadata");
        state
            .solutions_mut()
            .add(new_testcase)
            .expect("In run_observers_and_save_state solutions failure.");

        let event = Event::Objective {
            input: fuzzer.share_objectives().then_some(input.clone()),
            objective_size: state.solutions().count(),
        };

        event_mgr
            .fire(
                state,
                EventWithStats::with_current_time(event, *state.executions()),
            )
            .expect("Could not send off events in run_observers_and_save_state");
    }

    // Serialize the state and wait safely for the broker to read pending messages
    event_mgr.on_restart(state).unwrap();

    log::info!("Bye!");
}

#[cfg(test)]
mod tests {
    use libafl_bolts::{rands::XkcdRand, tuples::tuple_list};

    use crate::{
        StdFuzzer,
        corpus::InMemoryCorpus,
        events::NopEventManager,
        executors::{Executor, ExitKind, InProcessExecutor},
        feedbacks::CrashFeedback,
        inputs::NopInput,
        schedulers::RandScheduler,
        state::{NopState, StdState},
    };

    #[test]
    fn test_inmem_exec() {
        let mut harness = |_buf: &NopInput| ExitKind::Ok;
        let rand = XkcdRand::new();
        let corpus = InMemoryCorpus::<NopInput>::new();
        let solutions = InMemoryCorpus::new();
        let mut objective = CrashFeedback::new();
        let mut feedback = tuple_list!();
        let sche: RandScheduler<NopState<NopInput>> = RandScheduler::new();
        let mut mgr = NopEventManager::new();
        let mut state =
            StdState::new(rand, corpus, solutions, &mut feedback, &mut objective).unwrap();
        let mut fuzzer = StdFuzzer::new(sche, feedback, objective);

        let mut in_process_executor = InProcessExecutor::new(
            &mut harness,
            tuple_list!(),
            &mut fuzzer,
            &mut state,
            &mut mgr,
        )
        .unwrap();
        let input = NopInput {};
        in_process_executor
            .run_target(&mut fuzzer, &mut state, &mut mgr, &input)
            .unwrap();
    }
}
