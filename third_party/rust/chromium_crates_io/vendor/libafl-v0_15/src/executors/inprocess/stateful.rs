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
pub type StatefulInProcessExecutor<'a, EM, ES, H, I, OT, S, Z> =
    StatefulGenericInProcessExecutor<EM, ES, H, &'a mut H, (), I, OT, S, Z>;

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
    pub exposed_executor_state: ES,
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

        let ret = self.harness_fn.borrow_mut()(&mut self.exposed_executor_state, state, input);

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

impl<'a, EM, ES, H, I, OT, S, Z> StatefulInProcessExecutor<'a, EM, ES, H, I, OT, S, Z>
where
    H: FnMut(&mut ES, &mut S, &I) -> ExitKind + Sized,
    OT: ObserversTuple<I, S>,
    S: HasExecutions + HasSolutions<I> + HasCurrentTestcase<I>,
    I: Clone + Input,
{
    /// Create a new in mem executor with the default timeout (5 sec)
    pub fn new<OF>(
        harness_fn: &'a mut H,
        exposed_executor_state: ES,
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
        Self::with_timeout_generic(
            tuple_list!(),
            harness_fn,
            exposed_executor_state,
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
        exposed_executor_state: ES,
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
            exposed_executor_state,
            inner,
            phantom: PhantomData,
        })
    }
}

impl<EM, ES, H, HB, HT, I, OT, S, Z>
    StatefulGenericInProcessExecutor<EM, ES, H, HB, HT, I, OT, S, Z>
{
    /// The executor state given to the harness
    pub fn exposed_executor_state(&self) -> &ES {
        &self.exposed_executor_state
    }

    /// The mutable executor state given to the harness
    pub fn exposed_executor_state_mut(&mut self) -> &mut ES {
        &mut self.exposed_executor_state
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
    pub fn generic<OF>(
        user_hooks: HT,
        harness_fn: HB,
        exposed_executor_state: ES,
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
        Self::with_timeout_generic(
            user_hooks,
            harness_fn,
            exposed_executor_state,
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
    #[expect(clippy::too_many_arguments)]
    pub fn with_timeout_generic<OF>(
        user_hooks: HT,
        harness_fn: HB,
        exposed_executor_state: ES,
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
            exposed_executor_state,
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
