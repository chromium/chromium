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
    Error,
    events::{EventFirer, EventRestarter},
    executors::{
        Executor, ExitKind, HasObservers,
        hooks::{ExecutorHooksTuple, inprocess::InProcessHooks},
        inprocess::{GenericInProcessExecutorInner, HasInProcessHooks},
    },
    feedbacks::Feedback,
    fuzzer::HasObjective,
    inputs::Input,
    observers::ObserversTuple,
    state::{HasCurrentTestcase, HasExecutions, HasSolutions},
};

/// The process executor simply calls a target function, as mutable reference to a closure
/// The internal state of the executor is made available to the harness.
pub type StatefulInProcessExecutor<EM, ES, H, I, OT, S, Z> =
    StatefulGenericInProcessExecutor<EM, ES, H, H, (), I, OT, S, Z>;

/// The process executor simply calls a target function, as boxed `FnMut` trait object
/// The internal state of the executor is made available to the harness.
pub type OwnedInProcessExecutor<EM, ES, I, OT, S, Z> = StatefulGenericInProcessExecutor<
    EM,
    ES,
    dyn FnMut(&mut ES, &I) -> ExitKind,
    Box<dyn FnMut(&mut ES, &I) -> ExitKind>,
    (),
    I,
    OT,
    S,
    Z,
>;

/// The inmem executor simply calls a target function, then returns afterwards.
/// The harness can access the internal state of the executor.
pub struct StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z> {
    /// The harness function, being executed for each fuzzing loop execution
    harness_fn: HB,
    /// The state used as argument of the harness
    pub executor_state: ES,
    /// Inner state of the executor
    pub inner: GenericInProcessExecutorInner<EM, HT, I, OT, S, Z>,
    phantom: PhantomData<(ES, *const H)>,
}

impl<EM, ES, H, HB, HT, I, OT, S, Z> Debug
    for StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
where
    OT: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("StatefulGenericInProcessExecutor")
            .field("harness_fn", &"<fn>")
            .field("inner", &self.inner)
            .finish_non_exhaustive()
    }
}

impl<EM, H, HB, HT, I, OT, S, Z, ES> Executor<EM, I, S, Z>
    for StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
where
    H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
    HB: BorrowMut<H>,
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
    S: HasExecutions,
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

        let ret = self.harness_fn.borrow_mut()(&mut self.executor_state, state, input);

        self.inner.hooks.post_exec_all(state, input);

        self.inner.leave_target(fuzzer, state, mgr, input);
        Ok(ret)
    }
}

impl<EM, ES, H, HB, HT, I, OT, S, Z> HasObservers
    for StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
where
    H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
    HB: BorrowMut<H>,
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
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

/// The builder for a [`StatefulInProcessExecutor`]
#[derive(Debug, Clone)]
pub struct StatefulInProcessExecutorBuilder<E, ES, F, H, OT, St> {
    timeout: Duration,
    crashdump: bool,
    harness_fn: H,
    observers: OT,
    executor_state: ES,
    fuzzer: F,
    state: St,
    event_mgr: E,
}

impl Default for StatefulInProcessExecutorBuilder<(), (), (), (), (), ()> {
    fn default() -> Self {
        Self::new()
    }
}

impl StatefulInProcessExecutorBuilder<(), (), (), (), (), ()> {
    /// Create a new builder with default timeout (5s) and crashdump enabled.
    #[must_use]
    pub fn new() -> Self {
        Self {
            timeout: Duration::from_secs(5),
            crashdump: true,
            harness_fn: (),
            observers: tuple_list!(),
            executor_state: (),
            fuzzer: (),
            state: (),
            event_mgr: (),
        }
    }
}

impl<E, ES, F, H, OT, S> StatefulInProcessExecutorBuilder<E, ES, F, H, OT, S> {
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
    pub fn harness<H2>(
        self,
        harness_fn: H2,
    ) -> StatefulInProcessExecutorBuilder<E, ES, F, H2, OT, S> {
        StatefulInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
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
    ) -> StatefulInProcessExecutorBuilder<E, ES, F, H, OT2, S> {
        StatefulInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers,
            executor_state: self.executor_state,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the executor state for the executor.
    #[must_use]
    pub fn executor_state<ES2>(
        self,
        executor_state: ES2,
    ) -> StatefulInProcessExecutorBuilder<E, ES2, F, H, OT, S> {
        StatefulInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state,
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
    ) -> StatefulInProcessExecutorBuilder<E, ES, &mut Z, H, OT, S> {
        StatefulInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
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
    ) -> StatefulInProcessExecutorBuilder<E, ES, F, H, OT, &mut S2> {
        StatefulInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
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
    ) -> StatefulInProcessExecutorBuilder<&mut EM, ES, F, H, OT, S> {
        StatefulInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr,
        }
    }
}

impl<'a, EM, ES, H, OT, S, Z>
    StatefulInProcessExecutorBuilder<&'a mut EM, ES, &'a mut Z, H, OT, &'a mut S>
{
    /// Build the [`StatefulInProcessExecutor`].
    #[allow(clippy::type_complexity)]
    pub fn build<I, OF>(self) -> Result<StatefulInProcessExecutor<EM, ES, H, I, OT, S, Z>, Error>
    where
        H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
        OT: ObserversTuple<I, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        I: Clone + Input,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: tuple_list!(),
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
        .build::<I, OF>()
    }
}

/// The builder for a [`StatefulGenericInProcessExecutor`]
#[derive(Debug, Clone)]
pub struct StatefulGenericInProcessExecutorBuilder<E, ES, F, HB, HT, OT, S> {
    timeout: Duration,
    crashdump: bool,
    user_hooks: HT,
    harness_fn: HB,
    observers: OT,
    executor_state: ES,
    fuzzer: F,
    state: S,
    event_mgr: E,
}

impl Default for StatefulGenericInProcessExecutorBuilder<(), (), (), (), (), (), ()> {
    fn default() -> Self {
        Self::new()
    }
}

impl StatefulGenericInProcessExecutorBuilder<(), (), (), (), (), (), ()> {
    /// Create a new builder with default timeout (5s) and crashdump enabled.
    #[must_use]
    pub fn new() -> Self {
        Self {
            timeout: Duration::from_secs(5),
            crashdump: true,
            user_hooks: tuple_list!(),
            harness_fn: (),
            observers: tuple_list!(),
            executor_state: (),
            fuzzer: (),
            state: (),
            event_mgr: (),
        }
    }
}

impl<E, ES, F, HB, HT, OT, S> StatefulGenericInProcessExecutorBuilder<E, ES, F, HB, HT, OT, S> {
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

    /// Set the user hooks for the executor.
    #[must_use]
    pub fn user_hooks<HT2>(
        self,
        user_hooks: HT2,
    ) -> StatefulGenericInProcessExecutorBuilder<E, ES, F, HB, HT2, OT, S> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the harness function for the executor.
    #[must_use]
    pub fn harness<HB2>(
        self,
        harness_fn: HB2,
    ) -> StatefulGenericInProcessExecutorBuilder<E, ES, F, HB2, HT, OT, S> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: self.user_hooks,
            harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
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
    ) -> StatefulGenericInProcessExecutorBuilder<E, ES, F, HB, HT, OT2, S> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: self.user_hooks,
            harness_fn: self.harness_fn,
            observers,
            executor_state: self.executor_state,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr: self.event_mgr,
        }
    }

    /// Set the executor state for the executor.
    #[must_use]
    pub fn executor_state<ES2>(
        self,
        executor_state: ES2,
    ) -> StatefulGenericInProcessExecutorBuilder<E, ES2, F, HB, HT, OT, S> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: self.user_hooks,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state,
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
    ) -> StatefulGenericInProcessExecutorBuilder<E, ES, &mut Z, HB, HT, OT, S> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: self.user_hooks,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
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
    ) -> StatefulGenericInProcessExecutorBuilder<E, ES, F, HB, HT, OT, &mut S2> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: self.user_hooks,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
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
    ) -> StatefulGenericInProcessExecutorBuilder<&mut EM, ES, F, HB, HT, OT, S> {
        StatefulGenericInProcessExecutorBuilder {
            timeout: self.timeout,
            crashdump: self.crashdump,
            user_hooks: self.user_hooks,
            harness_fn: self.harness_fn,
            observers: self.observers,
            executor_state: self.executor_state,
            fuzzer: self.fuzzer,
            state: self.state,
            event_mgr,
        }
    }
}

impl<'a, EM, ES, HB, HT, OT, S, Z>
    StatefulGenericInProcessExecutorBuilder<&'a mut EM, ES, &'a mut Z, HB, HT, OT, &'a mut S>
{
    /// Build the [`StatefulGenericInProcessExecutor`].
    #[allow(clippy::type_complexity)]
    pub fn build<I, OF>(
        self,
    ) -> Result<StatefulGenericInProcessExecutor<EM, ES, HB, HB, HT, I, OT, S, Z>, Error>
    where
        HB: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
        HT: ExecutorHooksTuple<I, S>,
        OT: ObserversTuple<I, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        I: Clone + Input,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        self.build_custom::<HB, I, OF>()
    }

    /// Build the [`StatefulGenericInProcessExecutor`] with a custom harness type `H`.
    #[allow(clippy::type_complexity)]
    pub fn build_custom<H, I, OF>(
        self,
    ) -> Result<StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>, Error>
    where
        H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
        HB: BorrowMut<H>,
        HT: ExecutorHooksTuple<I, S>,
        OT: ObserversTuple<I, S>,
        S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
        I: Clone + Input,
        EM: EventFirer<I, S> + EventRestarter<S>,
        OF: Feedback<EM, I, OT, S>,
        Z: HasObjective<Objective = OF>,
    {
        let inner = GenericInProcessExecutorInner::with_timeout_generic::<
            StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>,
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

        Ok(StatefulGenericInProcessExecutor {
            harness_fn: self.harness_fn,
            executor_state: self.executor_state,
            inner,
            phantom: PhantomData,
        })
    }
}

impl StatefulInProcessExecutor<(), (), (), (), (), (), ()> {
    /// Create a builder for a [`StatefulInProcessExecutor`].
    #[must_use]
    pub fn builder() -> StatefulInProcessExecutorBuilder<(), (), (), (), (), ()> {
        StatefulInProcessExecutorBuilder::new()
    }
}

impl StatefulGenericInProcessExecutor<(), (), (), (), (), (), (), (), ()> {
    /// Create a builder for a [`StatefulGenericInProcessExecutor`].
    #[must_use]
    pub fn builder_generic() -> StatefulGenericInProcessExecutorBuilder<(), (), (), (), (), (), ()>
    {
        StatefulGenericInProcessExecutorBuilder::new()
    }
}

impl<EM, ES, H, I, OT, S, Z> StatefulInProcessExecutor<EM, ES, H, I, OT, S, Z>
where
    H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
    OT: ObserversTuple<I, S>,
    S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
    I: Clone + Input,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    #[deprecated(
        since = "0.16.0",
        note = "Use StatefulInProcessExecutor::builder() instead"
    )]
    pub fn new<OF>(
        harness_fn: H,
        executor_state: ES,
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
        StatefulInProcessExecutor::builder()
            .harness(harness_fn)
            .executor_state(executor_state)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build()
    }

    /// Create a new in mem executor.
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `harness_fn` - the harness, executing the function
    /// * `executor_state` - state exposed to the harness
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    #[deprecated(
        since = "0.16.0",
        note = "Use StatefulInProcessExecutor::builder() instead"
    )]
    pub fn with_timeout<OF>(
        harness_fn: H,
        executor_state: ES,
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
        StatefulInProcessExecutor::builder()
            .timeout(timeout)
            .harness(harness_fn)
            .executor_state(executor_state)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build()
    }
}

impl<EM, ES, H, HB, HT, I, OT, S, Z>
    StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
{
    /// The executor state given to the harness
    pub fn executor_state(&self) -> &ES {
        &self.executor_state
    }

    /// The mutable executor state given to the harness
    pub fn executor_state_mut(&mut self) -> &mut ES {
        &mut self.executor_state
    }
}

impl<EM, ES, H, HB, HT, I, OT, S, Z>
    StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
where
    H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
    HB: BorrowMut<H>,
    HT: ExecutorHooksTuple<I, S>,
    I: Input + Clone,
    OT: ObserversTuple<I, S>,
    S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    #[deprecated(
        since = "0.16.0",
        note = "Use StatefulGenericInProcessExecutor::builder_generic() instead"
    )]
    pub fn generic<OF>(
        user_hooks: HT,
        harness_fn: HB,
        executor_state: ES,
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
        StatefulGenericInProcessExecutor::builder_generic()
            .user_hooks(user_hooks)
            .harness(harness_fn)
            .executor_state(executor_state)
            .observers(observers)
            .fuzzer(fuzzer)
            .state(state)
            .event_mgr(event_mgr)
            .build_custom::<H, I, OF>()
    }

    /// Create a new in mem executor.
    /// Caution: crash and restart in one of them will lead to odd behavior if multiple are used,
    /// depending on different corpus or state.
    /// * `user_hooks` - the hooks run before and after the harness's execution
    /// * `harness_fn` - the harness, executing the function
    /// * `observers` - the observers observing the target during execution
    ///
    /// This may return an error on unix, if signal handler setup fails
    #[expect(clippy::too_many_arguments)]
    #[deprecated(
        since = "0.16.0",
        note = "Use StatefulGenericInProcessExecutor::builder_generic() instead"
    )]
    pub fn with_timeout_generic<OF>(
        user_hooks: HT,
        harness_fn: HB,
        executor_state: ES,
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
        StatefulGenericInProcessExecutor::builder_generic()
            .timeout(timeout)
            .user_hooks(user_hooks)
            .harness(harness_fn)
            .executor_state(executor_state)
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

    /// Retrieve the state, consuming the executor.
    #[inline]
    #[must_use]
    pub fn into_state(self) -> ES {
        self.executor_state
    }
}

impl<EM, ES, H, HB, HT, I, OT, S, Z> HasInProcessHooks<I, S>
    for StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
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
