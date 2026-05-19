//! The `GenericInProcessForkExecutor` to do forking before executing the harness in-processly
use core::{
    fmt::{self, Debug, Formatter},
    time::Duration,
};

use libafl_bolts::{
    os::unix_signals::{Signal, ucontext_t},
    shmem::ShMemProvider,
    tuples::{RefIndexable, tuple_list},
};
use libc::siginfo_t;
use nix::unistd::{ForkResult, fork};

use super::hooks::ExecutorHooksTuple;
use crate::{
    Error,
    executors::{
        Executor, ExitKind, HasObservers, hooks::inprocess_fork::InProcessForkExecutorGlobalData,
        inprocess_fork::inner::GenericInProcessForkExecutorInner,
    },
    observers::ObserversTuple,
    state::HasExecutions,
};

/// The signature of the crash handler function
pub(crate) type ForkHandlerFuncPtr = unsafe fn(
    Signal,
    &mut siginfo_t,
    Option<&mut ucontext_t>,
    data: *mut InProcessForkExecutorGlobalData,
);

/// The inner structure of `InProcessForkExecutor`.
pub mod inner;
pub mod stateful;

/// The `InProcessForkExecutor` with no user hooks.
///
/// On Linux, when fuzzing a Rust target, set `panic = "abort"` in your `Cargo.toml` (see [Cargo documentation](https://doc.rust-lang.org/cargo/reference/profiles.html#panic)).
/// Else panics can not be caught by `LibAFL`.
pub type InProcessForkExecutor<'a, EM, H, I, OT, S, SP, Z> =
    GenericInProcessForkExecutor<'a, EM, H, (), I, OT, S, SP, Z>;

impl<'a, H, I, OT, S, SP, EM, Z> InProcessForkExecutor<'a, EM, H, I, OT, S, SP, Z>
where
    OT: ObserversTuple<I, S>,
{
    /// The constructor for `InProcessForkExecutor`
    pub fn new(
        harness_fn: &'a mut H,
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
            observers,
            fuzzer,
            state,
            event_mgr,
            timeout,
            shmem_provider,
        )
    }
}

/// [`GenericInProcessForkExecutor`] is an executor that forks the current process before each execution.
///
/// On Linux, when fuzzing a Rust target, set `panic = "abort"` in your `Cargo.toml` (see [Cargo documentation](https://doc.rust-lang.org/cargo/reference/profiles.html#panic)).
/// Else panics can not be caught by `LibAFL`.
pub struct GenericInProcessForkExecutor<'a, EM, H, HT, I, OT, S, SP, Z> {
    harness_fn: &'a mut H,
    inner: GenericInProcessForkExecutorInner<EM, HT, I, OT, S, SP, Z>,
}

impl<H, HT, I, OT, S, SP, EM, Z> Debug
    for GenericInProcessForkExecutor<'_, EM, H, HT, I, OT, S, SP, Z>
where
    HT: Debug,
    OT: Debug,
    SP: Debug,
{
    #[cfg(target_os = "linux")]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("GenericInProcessForkExecutor")
            .field("GenericInProcessForkExecutorInner", &self.inner)
            .finish()
    }

    #[cfg(not(target_os = "linux"))]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        #[cfg(not(target_os = "linux"))]
        return f
            .debug_struct("GenericInProcessForkExecutor")
            .field("GenericInProcessForkExecutorInner", &self.inner)
            .finish();
    }
}

impl<EM, H, HT, I, OT, S, SP, Z> Executor<EM, I, S, Z>
    for GenericInProcessForkExecutor<'_, EM, H, HT, I, OT, S, SP, Z>
where
    H: FnMut(&I) -> ExitKind + Sized,
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
    S: HasExecutions,
    SP: ShMemProvider,
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
                    (self.harness_fn)(input);
                    self.inner.post_run_target_child(fuzzer, state, mgr, input);
                    Ok(ExitKind::Ok)
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

impl<'a, H, HT, I, OT, S, SP, EM, Z> GenericInProcessForkExecutor<'a, EM, H, HT, I, OT, S, SP, Z>
where
    HT: ExecutorHooksTuple<I, S>,
    OT: ObserversTuple<I, S>,
{
    /// Creates a new [`GenericInProcessForkExecutor`] with custom hooks
    #[expect(clippy::too_many_arguments)]
    pub fn with_hooks(
        userhooks: HT,
        harness_fn: &'a mut H,
        observers: OT,
        fuzzer: &mut Z,
        state: &mut S,
        event_mgr: &mut EM,
        timeout: Duration,
        shmem_provider: SP,
    ) -> Result<Self, Error>
where {
        Ok(Self {
            harness_fn,
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

impl<H, HT, I, OT, S, SP, EM, Z> HasObservers
    for GenericInProcessForkExecutor<'_, EM, H, HT, I, OT, S, SP, Z>
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

/// signal hooks and `panic_hooks` for the child process
pub mod child_signal_handlers {
    use alloc::boxed::Box;
    use std::panic;

    use libafl_bolts::os::unix_signals::{Signal, ucontext_t};
    use libc::siginfo_t;

    use crate::{
        executors::{
            ExitKind, HasObservers,
            hooks::inprocess_fork::{FORK_EXECUTOR_GLOBAL_DATA, InProcessForkExecutorGlobalData},
        },
        observers::ObserversTuple,
    };

    /// invokes the `post_exec_child` hook on all observer in case the child process panics
    pub fn setup_child_panic_hook<E, I, S>()
    where
        E: HasObservers,
        E::Observers: ObserversTuple<I, S>,
    {
        let old_hook = panic::take_hook();
        panic::set_hook(Box::new(move |panic_info| unsafe {
            old_hook(panic_info);
            let data = &raw mut FORK_EXECUTOR_GLOBAL_DATA;
            if !data.is_null() && (*data).is_valid() {
                let executor = (*data).executor_mut::<E>();
                let mut observers = executor.observers_mut();
                let state = (*data).state_mut::<S>();
                // Invalidate data to not execute again the observer hooks in the crash handler
                let input = (*data).take_current_input::<I>();
                observers
                    .post_exec_child_all(state, input, &ExitKind::Crash)
                    .expect("Failed to run post_exec on observers");

                // std::process::abort();
                libc::_exit(128 + 6); // ABORT exit code
            }
        }));
    }

    /// invokes the `post_exec` hook on all observer in case the child process crashes
    ///
    /// # Safety
    /// The function should only be called from a child crash handler.
    /// It will dereference the `data` pointer and assume it's valid.
    #[cfg(unix)]
    #[allow(clippy::needless_pass_by_value)] // nightly no longer requires this
    pub(crate) unsafe fn child_crash_handler<E, I, S>(
        _signal: Signal,
        _info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
        data: &mut InProcessForkExecutorGlobalData,
    ) where
        E: HasObservers,
        E::Observers: ObserversTuple<I, S>,
    {
        unsafe {
            if data.is_valid() {
                let executor = data.executor_mut::<E>();
                let mut observers = executor.observers_mut();
                let state = data.state_mut::<S>();
                let input = data.take_current_input::<I>();
                observers
                    .post_exec_child_all(state, input, &ExitKind::Crash)
                    .expect("Failed to run post_exec on observers");
            }

            libc::_exit(128 + (_signal as i32));
        }
    }

    #[cfg(unix)]
    #[allow(clippy::needless_pass_by_value)] // nightly no longer requires this
    pub(crate) unsafe fn child_timeout_handler<E, I, S>(
        #[cfg(unix)] _signal: Signal,
        _info: &mut siginfo_t,
        _context: Option<&mut ucontext_t>,
        data: &mut InProcessForkExecutorGlobalData,
    ) where
        E: HasObservers,
        E::Observers: ObserversTuple<I, S>,
    {
        unsafe {
            if data.is_valid() {
                let executor = data.executor_mut::<E>();
                let mut observers = executor.observers_mut();
                let state = data.state_mut::<S>();
                let input = data.take_current_input::<I>();
                observers
                    .post_exec_child_all(state, input, &ExitKind::Timeout)
                    .expect("Failed to run post_exec on observers");
            }
            libc::_exit(128 + (_signal as i32));
        }
    }
}

#[cfg(test)]
#[cfg(all(feature = "fork", unix))]
mod tests {
    use libafl_bolts::tuples::tuple_list;
    use serial_test::serial;

    use crate::{
        executors::{Executor, ExitKind, inprocess_fork::GenericInProcessForkExecutorInner},
        inputs::NopInput,
    };

    #[test]
    #[serial]
    #[cfg_attr(miri, ignore)]
    fn test_inprocessfork_exec() {
        use core::marker::PhantomData;

        use libafl_bolts::shmem::{ShMemProvider, StdShMemProvider};
        #[cfg(target_os = "linux")]
        use libc::{itimerspec, timespec};

        #[cfg(not(target_os = "linux"))]
        use crate::executors::hooks::timer::{Itimerval, Timeval};
        use crate::{
            events::SimpleEventManager,
            executors::{
                hooks::inprocess_fork::InChildProcessHooks,
                inprocess_fork::GenericInProcessForkExecutor,
            },
            fuzzer::NopFuzzer,
            state::NopState,
        };

        let provider = StdShMemProvider::new().unwrap();

        #[cfg(target_os = "linux")]
        let timespec = timespec {
            tv_sec: 5,
            tv_nsec: 0,
        };
        #[cfg(target_os = "linux")]
        let itimerspec = itimerspec {
            it_interval: timespec,
            it_value: timespec,
        };

        #[cfg(not(target_os = "linux"))]
        let timespec = Timeval {
            tv_sec: 5,
            tv_usec: 0,
        };
        #[cfg(not(target_os = "linux"))]
        let itimerspec = Itimerval {
            it_interval: timespec,
            it_value: timespec,
        };

        let mut harness = |_buf: &NopInput| ExitKind::Ok;
        let default = InChildProcessHooks::<NopInput, NopState<NopInput>>::nop();
        #[cfg(target_os = "linux")]
        let mut in_process_fork_executor = GenericInProcessForkExecutor {
            harness_fn: &mut harness,
            inner: GenericInProcessForkExecutorInner {
                hooks: tuple_list!(default),
                shmem_provider: provider,
                observers: tuple_list!(),
                itimerspec,
                phantom: PhantomData,
            },
        };
        #[cfg(not(target_os = "linux"))]
        let mut in_process_fork_executor = GenericInProcessForkExecutor {
            harness_fn: &mut harness,
            inner: GenericInProcessForkExecutorInner {
                hooks: tuple_list!(default),
                shmem_provider: provider,
                observers: tuple_list!(),
                itimerval: itimerspec,
                phantom: PhantomData,
            },
        };
        let input = NopInput {};
        let mut fuzzer = NopFuzzer::new();
        let mut state = NopState::new();
        let mut mgr: SimpleEventManager<NopInput, _, NopState<NopInput>> =
            SimpleEventManager::printing();
        in_process_fork_executor
            .run_target(&mut fuzzer, &mut state, &mut mgr, &input)
            .unwrap();
    }
}
