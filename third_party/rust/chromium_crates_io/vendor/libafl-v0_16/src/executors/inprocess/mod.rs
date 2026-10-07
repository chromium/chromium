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
pub type InProcessExecutor<EM, H, I, OT, S, Z> =
    GenericInProcessExecutor<EM, H, H, (), I, OT, S, Z>;

/// The inprocess executor that allows hooks
pub type HookableInProcessExecutor<EM, H, HT, I, OT, S, Z> =
    GenericInProcessExecutor<EM, H, H, HT, I, OT, S, Z>;
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

/// The builder for an [`InProcessExecutor`]
#[derive(Debug, Clone)]
pub struct InProcessExecutorBuilder<E, F, H, OT, St> {
    timeout: Duration,
    crashdump: bool,
    harness_fn: H,
    observers: OT,
    fuzzer: F,
    state: St,
    event_mgr: E,
}

impl Default for InProcessExecutorBuilder<(), (), (), (), ()> {
    fn default() -> Self {
        Self::new()
    }
}

impl InProcessExecutorBuilder<(), (), (), (), ()> {
    /// Create a new builder with default timeout (5s) and crashdump enabled.
    #[must_use]
    pub fn new() -> Self {
        Self {
            timeout: Duration::from_secs(5),
            crashdump: true,
            harness_fn: (),
            observers: tuple_list!(),
            fuzzer: (),
            state: (),
            event_mgr: (),
        }
    }
}

impl<E, F, H, OT, S> InProcessExecutorBuilder<E, F, H, OT, S> {
    /// Set the timeout for the executor.
    #[must_use]
    pub fn timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }

    /// Enable or disable minibsod crashdumps on crash.
    #[must_use]
    pub fn crashdump(mut self, crashdump: bool) -> Self {
        self.crashdump = crashdump;
        self
    }

    /// Set the harness function for the executor.
    #[must_use]
    pub fn harness<H2>(self, harness_fn: H2) -> InProcessExecutorBuilder<E, F, H2, OT, S> {
        InProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the observers for the executor.
    #[must_use]
    pub fn observers<OT2>(self, observers: OT2) -> InProcessExecutorBuilder<E, F, H, OT2, S> {
        InProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the fuzzer for the executor.
    #[must_use]
    pub fn fuzzer<Z>(self, fuzzer: &mut Z) -> InProcessExecutorBuilder<E, &mut Z, H, OT, S> {
        InProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the state for the executor.
    #[must_use]
    pub fn state<S2>(self, state: &mut S2) -> InProcessExecutorBuilder<E, F, H, OT, &mut S2> {
        InProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the event manager for the executor.
    #[must_use]
    pub fn event_mgr<EM>(
        self,
        event_mgr: &mut EM,
    ) -> InProcessExecutorBuilder<&mut EM, F, H, OT, S> {
        InProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr,
        }
    }
}

impl<'a, EM, H, OT, S, Z> InProcessExecutorBuilder<&'a mut EM, &'a mut Z, H, OT, &'a mut S> {
    /// Build the [`InProcessExecutor`].
    #[allow(clippy::type_complexity)]
    pub fn build<I, OF>(self) -> Result<InProcessExecutor<EM, H, I, OT, S, Z>, Error>
    where
        H: FnMut(&I) -> ExitKind + Sized,
        OT: ObserversTuple<I, S>,
        S: HasCurrentTestcase<I> + HasExecutions + HasSolutions<I>,
        I: Input,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: tuple_list!(),
            harness_fn: self.harness_fn,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
        .build::<I, OF>()
    }
}

/// The builder for a [`GenericInProcessExecutor`]
#[derive(Debug, Clone)]
pub struct GenericInProcessExecutorBuilder<E, F, HB, HT, OT, St> {
    timeout: Duration,
    crashdump: bool,
    harness_fn: HB,
    user_hooks: HT,
    observers: OT,
    fuzzer: F,
    state: St,
    event_mgr: E,
}

impl Default for GenericInProcessExecutorBuilder<(), (), (), (), (), ()> {
    fn default() -> Self {
        Self::new()
    }
}

impl GenericInProcessExecutorBuilder<(), (), (), (), (), ()> {
    /// Create a new builder with default timeout (5s) and crashdump enabled.
    #[must_use]
    pub fn new() -> Self {
        Self {
            timeout: Duration::from_secs(5),
            crashdump: true,
            harness_fn: (),
            user_hooks: tuple_list!(),
            observers: tuple_list!(),
            fuzzer: (),
            state: (),
            event_mgr: (),
        }
    }
}

impl<E, F, HB, HT, OT, S> GenericInProcessExecutorBuilder<E, F, HB, HT, OT, S> {
    /// Set the timeout for the executor.
    #[must_use]
    pub fn timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }

    /// Enable or disable minibsod crashdumps on crash.
    #[must_use]
    pub fn crashdump(mut self, crashdump: bool) -> Self {
        self.crashdump = crashdump;
        self
    }

    /// Set the harness function for the executor.
    #[must_use]
    pub fn harness<HB2>(
        self,
        harness_fn: HB2,
    ) -> GenericInProcessExecutorBuilder<E, F, HB2, HT, OT, S> {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn,
            user_hooks: self.user_hooks,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the user hooks for the executor.
    #[must_use]
    pub fn user_hooks<HT2>(
        self,
        user_hooks: HT2,
    ) -> GenericInProcessExecutorBuilder<E, F, HB, HT2, OT, S> {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            user_hooks,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the observers for the executor.
    #[must_use]
    pub fn observers<OT2>(
        self,
        observers: OT2,
    ) -> GenericInProcessExecutorBuilder<E, F, HB, HT, OT2, S> {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            user_hooks: self.user_hooks,
            observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the fuzzer for the executor.
    #[must_use]
    pub fn fuzzer<Z>(
        self,
        fuzzer: &mut Z,
    ) -> GenericInProcessExecutorBuilder<E, &mut Z, HB, HT, OT, S> {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            user_hooks: self.user_hooks,
            observers: self.observers,
            fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the state for the executor.
    #[must_use]
    pub fn state<S2>(
        self,
        state: &mut S2,
    ) -> GenericInProcessExecutorBuilder<E, F, HB, HT, OT, &mut S2> {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            user_hooks: self.user_hooks,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the event manager for the executor.
    #[must_use]
    pub fn event_mgr<EM>(
        self,
        event_mgr: &mut EM,
    ) -> GenericInProcessExecutorBuilder<&mut EM, F, HB, HT, OT, S> {
        GenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            user_hooks: self.user_hooks,
            observers: self.observers,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr,
        }
    }
}

impl<'a, EM, HB, HT, OT, S, Z>
    GenericInProcessExecutorBuilder<&'a mut EM, &'a mut Z, HB, HT, OT, &'a mut S>
{
    /// Build the [`GenericInProcessExecutor`].
    #[allow(clippy::type_complexity)]
    pub fn build<I, OF>(
        self,
    ) -> Result<GenericInProcessExecutor<EM, HB, HB, HT, I, OT, S, Z>, Error>
    where
        HB: FnMut(&I) -> ExitKind + Sized,
        HT: ExecutorHooksTuple<I, S>,
        OT: ObserversTuple<I, S>,
        S: HasCurrentTestcase<I> + HasExecutions + HasSolutions<I>,
        I: Input,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        self.build_custom::<HB, I, OF>()
    }

    /// Build the [`GenericInProcessExecutor`] with a custom harness type `H`.
    #[allow(clippy::type_complexity)]
    pub fn build_custom<H, I, OF>(
        self,
    ) -> Result<GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>, Error>
    where
        H: FnMut(&I) -> ExitKind + Sized,
        HB: BorrowMut<H>,
        HT: ExecutorHooksTuple<I, S>,
        OT: ObserversTuple<I, S>,
        S: HasCurrentTestcase<I> + HasExecutions + HasSolutions<I>,
        I: Input,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        let inner = GenericInProcessExecutorInner::with_timeout_generic::<
            GenericInProcessExecutor<EM, H, HB, HT, I, OT, S, Z>,
            OF,
        >(
            self.user_hooks,
            self.observers,
            self.fuzzer,
            self.state,
            self.event_mgr,
            self.timeout,
            self.crashdump,
        )?;

        Ok(GenericInProcessExecutor {
            harness_fn: self.harness_fn,
            inner,
            phantom: PhantomData,
        })
    }
}

impl InProcessExecutor<(), (), (), (), (), ()> {
    /// Create a builder for an [`InProcessExecutor`].
    #[must_use]
    pub fn builder() -> InProcessExecutorBuilder<(), (), (), (), ()> {
        InProcessExecutorBuilder::new()
    }
}

impl GenericInProcessExecutor<(), (), (), (), (), (), (), ()> {
    /// Create a builder for a [`GenericInProcessExecutor`].
    #[must_use]
    pub fn builder_generic() -> GenericInProcessExecutorBuilder<(), (), (), (), (), ()> {
        GenericInProcessExecutorBuilder::new()
    }
}

impl<EM, H, I, OT, S, Z> InProcessExecutor<EM, H, I, OT, S, Z>
where
    H: FnMut(&I) -> ExitKind + Sized,
    OT: ObserversTuple<I, S>,
    S: HasCurrentTestcase<I> + HasExecutions + HasSolutions<I>,
    I: Input,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    #[deprecated(since = "0.16.0", note = "Use InProcessExecutor::builder() instead")]
    pub fn new<OF>(
        harness_fn: H,
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
        InProcessExecutor::builder()
            .harness(harness_fn)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build()
    }

    /// Create a new in mem executor.
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `user_hooks` - the hooks run before and after the harness's execution
    /// * `harness_fn` - the harness, executing the function
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    #[deprecated(since = "0.16.0", note = "Use InProcessExecutor::builder() instead")]
    pub fn with_timeout<OF>(
        harness_fn: H,
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
        InProcessExecutor::builder()
            .timeout(timeout)
            .harness(harness_fn)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build()
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
    #[deprecated(
        since = "0.16.0",
        note = "Use GenericInProcessExecutor::builder_generic() instead"
    )]
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
        GenericInProcessExecutor::builder_generic()
            .user_hooks(user_hooks)
            .harness(harness_fn)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build_custom::<H, I, OF>()
    }

    /// Create a new [`InProcessExecutor`].
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `user_hooks` - the hooks run before and after the harness's execution
    /// * `harness_fn` - the harness, executing the function
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    #[deprecated(
        since = "0.16.0",
        note = "Use GenericInProcessExecutor::builder_generic() instead"
    )]
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
        GenericInProcessExecutor::builder_generic()
            .timeout(timeout)
            .user_hooks(user_hooks)
            .harness(harness_fn)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build_custom::<H, I, OF>()
    }

    /// Retrieve the harness function.
    #[inline]
    #[must_use]
    pub fn harness(&self) -> &H {
        self.harness_fn.borrow()
    }

    /// Retrieve the harness function for a mutable reference.
    #[inline]
    #[must_use]
    pub fn harness_mut(&mut self) -> &mut H {
        self.harness_fn.borrow_mut()
    }

    /// The inprocess handlers
    #[inline]
    #[must_use]
    pub fn hooks(&self) -> &(InProcessHooks<I, S>, HT) {
        self.inner.hooks()
    }

    /// The inprocess handlers (mutable)
    #[inline]
    #[must_use]
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
}

#[cfg(test)]
mod tests {
    use libafl_bolts::{rands::XkcdRand, tuples::tuple_list};
    #[cfg(feature = "serial_test")]
    use serial_test::serial;

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
    #[cfg_attr(feature = "std", serial)]
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

        let mut in_process_executor = InProcessExecutor::builder()
            .harness(&mut harness)
            .observers(tuple_list!())
            .fuzzer(&mut fuzzer)
            .state(&mut state)
            .event_mgr(&mut mgr)
            .build()
            .unwrap();
        let input = NopInput {};
        in_process_executor
            .run_target(&mut fuzzer, &mut state, &mut mgr, &input)
            .unwrap();
    }
}
