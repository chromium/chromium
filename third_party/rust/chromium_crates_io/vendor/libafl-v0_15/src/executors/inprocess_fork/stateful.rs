//! A version of `InProcessForkExecutor` with a state accessible from the harness.
//!
//! The `StatefulGenericInProcessForkExecutor` to do forking before executing the harness in-process.
//! The harness can access internal state.
use core::{
    fmt::{self, Debug, Formatter},
    time::Duration,
};

use libafl_bolts::{
    shmem::ShMemProvider,
    tuples::{RefIndexable, tuple_list},
};
use nix::unistd::{ForkResult, fork};

use crate::{
    Error,
    executors::{
        Executor, ExitKind, HasObservers, hooks::ExecutorHooksTuple,
        inprocess_fork::GenericInProcessForkExecutorInner,
    },
    observers::ObserversTuple,
    state::HasExecutions,
};

/// The `StatefulInProcessForkExecutor` with no user hooks
pub type StatefulInProcessForkExecutor<'a, EM, ES, H, I, OT, S, SP, Z> =
    StatefulGenericInProcessForkExecutor<'a, EM, ES, H, (), I, OT, S, SP, Z>;

impl<'a, H, I, OT, S, SP, EM, ES, Z> StatefulInProcessForkExecutor<'a, EM, ES, H, I, OT, S, SP, Z>
where
    OT: ObserversTuple<I, S>,
    SP: ShMemProvider,
{
    #[expect(clippy::too_many_arguments)]
    /// The constructor for `InProcessForkExecutor`
    pub fn new(
        harness_fn: &'a mut H,
        exposed_executor_state: ES,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
        timeout: Duration,
        shmem_provider: SP,
    ) -> Result<Self, Error> {
        Self::with_hooks(
            tuple_list!(),
            harness_fn,
            exposed_executor_state,
            observers,
            fuzzer,
            state,
            event_mgr,
            timeout,
            shmem_provider,
        )
    }
}

/// [`StatefulGenericInProcessForkExecutor`] is an executor that forks the current process before each execution. Harness can access some internal state.
pub struct StatefulGenericInProcessForkExecutor<'a, EM, ES, H, HT, I, OT, S, SP, Z> {
    /// The harness function, being executed for each fuzzing loop execution
    harness_fn: &'a mut H,
    /// The state used as argument of the harness
    pub exposed_executor_state: ES,
    /// Inner state of the executor
    pub inner: GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z>,
}

impl<H, HT, I, OT, S, SP, EM, ES, Z> Debug
    for StatefulGenericInProcessForkExecutor<'_, EM, ES, H, HT, I, OT, S, SP, Z>
where
    HT: Debug,
    OT: Debug,
    SP: Debug,
{
    #[cfg(target_os = "linux")]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("GenericInProcessForkExecutor")
            .field("GenericInProcessForkExecutionInner", &self.inner)
            .finish()
    }

    #[cfg(not(target_os = "linux"))]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        #[cfg(not(target_os = "linux"))]
        return f
            .debug_struct("GenericInProcessForkExecutor")
            .field("GenericInProcessForkExecutionInner", &self.inner)
            .finish();
    }
}

impl<EM, H, HT, I, OT, S, SP, Z, ES> Executor<EM, I, S, Z>
    for StatefulGenericInProcessForkExecutor<'_, EM, ES, H, HT, I, OT, S, SP, Z>
where
    H: FnMut(&mut ES, &I) -> ExitKind + Sized,
    HT: ExecutorHooksTuple<I, S>,
    S: HasExecutions,
    SP: ShMemProvider,
    OT: ObserversTuple<I, S>,
{
    #[inline]
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        *state.executions_mut() += 1;

        unsafe {
            self.inner.shmem_provider.pre_fork()?;
            match fork() {
                Ok(ForkResult::Child) => {
                    // Child
                    self.inner.pre_run_target_child(fuzzer, state, mgr, input)?;
                    (self.harness_fn)(&mut self.exposed_executor_state, input);
                    self.inner.post_run_target_child(fuzzer, state, mgr, input);

                    unreachable!(
                        "post_run_target_child should make the process quit. This is a bug."
                    );
                }
                Ok(ForkResult::Parent { child }) => {
                    // Parent
                    self.inner.parent(child)
                }
                Err(e) => Err(Error::from(e)),
            }
        }
    }
}

impl<'a, H, HT, I, OT, S, SP, EM, ES, Z>
    StatefulGenericInProcessForkExecutor<'a, EM, ES, H, HT, I, OT, S, SP, Z>
where
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
{
    /// Creates a new [`StatefulGenericInProcessForkExecutor`] with custom hooks
    #[expect(clippy::too_many_arguments)]
    pub fn with_hooks(
        userhooks: HT,
        harness_fn: &'a mut H,
        exposed_executor_state: ES,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
        timeout: Duration,
        shmem_provider: SP,
    ) -> Result<Self, Error> {
        Ok(Self {
            harness_fn,
            exposed_executor_state,
            inner: GenericInProcessForkExecutorInner::with_hooks(
                userhooks,
                observers,
                fuzzer,
                state,
                event_mgr,
                timeout,
                shmem_provider,
            )?,
        })
    }

    /// Retrieve the harness function.
    #[inline]
    pub fn harness(&self) -> &H {
        self.harness_fn
    }

    /// Retrieve the harness function for a mutable reference.
    #[inline]
    pub fn harness_mut(&mut self) -> &mut H {
        self.harness_fn
    }
}

impl<H, HT, I, OT, S, SP, EM, ES, Z> HasObservers
    for StatefulGenericInProcessForkExecutor<'_, EM, ES, H, HT, I, OT, S, SP, Z>
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
