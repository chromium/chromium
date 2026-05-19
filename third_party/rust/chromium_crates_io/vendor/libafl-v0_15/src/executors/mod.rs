//! Executors take input, and run it in the target.

use alloc::vec::Vec;
use core::{fmt::Debug, time::Duration};
#[cfg(feature = "std")]
use std::path::PathBuf;

pub use combined::CombinedExecutor;
#[cfg(feature = "std")]
pub use command::CommandExecutor;
pub use differential::DiffExecutor;
#[cfg(all(feature = "std", feature = "fork", unix))]
pub use forkserver::{Forkserver, ForkserverExecutor};
pub use inprocess::InProcessExecutor;
#[cfg(all(feature = "std", feature = "fork", unix))]
pub use inprocess_fork::InProcessForkExecutor;
#[cfg(unix)]
use libafl_bolts::os::unix_signals::Signal;
use libafl_bolts::tuples::RefIndexable;
#[cfg(feature = "std")]
use libafl_bolts::{core_affinity::CoreId, tuples::Handle};
use serde::{Deserialize, Serialize};
pub use shadow::ShadowExecutor;
pub use with_observers::WithObservers;

use crate::Error;
#[cfg(feature = "std")]
use crate::observers::{StdErrObserver, StdOutObserver};

pub mod combined;
#[cfg(feature = "std")]
pub mod command;
pub mod differential;
#[cfg(all(feature = "std", feature = "fork", unix))]
pub mod forkserver;
pub mod inprocess;
pub mod nop;
/// SAND(<https://github.com/wtdcode/sand-aflpp>) implementation
pub mod sand;

/// The module for inproc fork executor
#[cfg(all(feature = "std", unix))]
pub mod inprocess_fork;

pub mod shadow;

pub mod with_observers;

/// The module for all the hooks
pub mod hooks;

/// How an execution finished.
#[derive(Debug, Copy, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub enum ExitKind {
    /// The run exited normally.
    Ok,
    /// The run resulted in a target crash.
    Crash,
    /// The run hit an out of memory error.
    Oom,
    /// The run timed out
    Timeout,
    /// Special case for [`DiffExecutor`] when both exitkinds don't match
    Diff {
        /// The exitkind of the primary executor
        primary: DiffExitKind,
        /// The exitkind of the secondary executor
        secondary: DiffExitKind,
    },
    // The run resulted in a custom `ExitKind`.
    // Custom(Box<dyn SerdeAny>),
}

/// How one of the diffing executions finished.
#[derive(Debug, Copy, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub enum DiffExitKind {
    /// The run exited normally.
    Ok,
    /// The run resulted in a target crash.
    Crash,
    /// The run hit an out of memory error.
    Oom,
    /// The run timed out
    Timeout,
    /// One of the executors itelf repots a differential, we can't go into further details.
    Diff,
    // The run resulted in a custom `ExitKind`.
    // Custom(Box<dyn SerdeAny>),
}

libafl_bolts::impl_serdeany!(ExitKind);

impl From<ExitKind> for DiffExitKind {
    fn from(exitkind: ExitKind) -> Self {
        match exitkind {
            ExitKind::Ok => DiffExitKind::Ok,
            ExitKind::Crash => DiffExitKind::Crash,
            ExitKind::Oom => DiffExitKind::Oom,
            ExitKind::Timeout => DiffExitKind::Timeout,
            ExitKind::Diff { .. } => DiffExitKind::Diff,
        }
    }
}

libafl_bolts::impl_serdeany!(DiffExitKind);

/// Holds a tuple of Observers
pub trait HasObservers {
    /// The observer
    type Observers;

    /// Get the linked observers
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers>;

    /// Get the linked observers (mutable)
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers>;
}

/// An executor takes the given inputs, and runs the harness/target.
pub trait Executor<EM, I, S, Z> {
    /// Instruct the target about the input and run
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error>;
}

/// A trait that allows to get an `Executor`'s timeout threshold
pub trait HasTimeout {
    /// Get a timeout
    fn timeout(&self) -> Duration;
}

/// A trait that allows to set an `Executor`'s timeout threshold
pub trait SetTimeout {
    /// Set timeout
    fn set_timeout(&mut self, timeout: Duration);
}

/// Like [`crate::observers::ObserversTuple`], a list of executors
pub trait ExecutorsTuple<EM, I, S, Z> {
    /// Execute the executors and stop if any of them returns a crash
    fn run_target_all(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error>;
}

/// Since in most cases, the executors types can not be determined during compilation
/// time (for instance, the number of executors might change), this implementation would
/// act as a small helper.
impl<E, EM, I, S, Z> ExecutorsTuple<EM, I, S, Z> for Vec<E>
where
    E: Executor<EM, I, S, Z>,
{
    fn run_target_all(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        let mut kind = ExitKind::Ok;
        for e in self.iter_mut() {
            kind = e.run_target(fuzzer, state, mgr, input)?;
            if kind == ExitKind::Crash {
                return Ok(kind);
            }
        }
        Ok(kind)
    }
}

impl<EM, I, S, Z> ExecutorsTuple<EM, I, S, Z> for () {
    fn run_target_all(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _mgr: &mut EM,
        _input: &I,
    ) -> Result<ExitKind, Error> {
        Ok(ExitKind::Ok)
    }
}

impl<Head, Tail, EM, I, S, Z> ExecutorsTuple<EM, I, S, Z> for (Head, Tail)
where
    Head: Executor<EM, I, S, Z>,
    Tail: ExecutorsTuple<EM, I, S, Z>,
{
    fn run_target_all(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        let kind = self.0.run_target(fuzzer, state, mgr, input)?;
        if kind == ExitKind::Crash {
            return Ok(kind);
        }
        self.1.run_target_all(fuzzer, state, mgr, input)
    }
}

/// The common signals we want to handle
#[cfg(unix)]
#[inline]
#[must_use]
pub fn common_signals() -> Vec<Signal> {
    vec![
        Signal::SigAlarm,
        Signal::SigUser2,
        Signal::SigAbort,
        Signal::SigBus,
        #[cfg(feature = "handle_sigpipe")]
        Signal::SigPipe,
        Signal::SigFloatingPointException,
        Signal::SigIllegalInstruction,
        Signal::SigSegmentationFault,
        Signal::SigTrap,
    ]
}

#[cfg(feature = "std")]
/// The inner shared members of [`StdChildArgs`]
#[derive(Debug, Clone)]
pub struct StdChildArgsInner {
    /// The timeout of the children
    pub timeout: Duration,
    /// The stderr handle of the children
    pub stderr_observer: Option<Handle<StdErrObserver>>,
    /// The stdout handle of the children
    pub stdout_observer: Option<Handle<StdOutObserver>>,
    /// The current directory of the spawned children
    pub current_directory: Option<PathBuf>,
    /// Whether debug child by inheriting stdout/stderr
    pub debug_child: bool,
    /// Core to bind for the children
    pub core: Option<CoreId>,
}

#[cfg(feature = "std")]
impl Default for StdChildArgsInner {
    fn default() -> Self {
        Self {
            timeout: Duration::from_millis(5000),
            stderr_observer: None,
            stdout_observer: None,
            current_directory: None,
            debug_child: false,
            core: None,
        }
    }
}

#[cfg(feature = "std")]
/// The shared implementation for children with stdout/stderr/timeouts.
pub trait StdChildArgs: Sized {
    /// The inner struct of child environment.
    fn inner(&self) -> &StdChildArgsInner;

    /// The mutable inner struct of child environment.
    fn inner_mut(&mut self) -> &mut StdChildArgsInner;

    #[must_use]
    /// Sets the execution timeout duration.
    fn timeout(mut self, timeout: Duration) -> Self {
        self.inner_mut().timeout = timeout;
        self
    }

    #[must_use]
    /// Sets the stdout observer
    fn stdout_observer(mut self, stdout: Handle<StdOutObserver>) -> Self {
        self.inner_mut().stdout_observer = Some(stdout);
        self
    }

    #[must_use]
    /// Sets the stderr observer
    fn stderr_observer(mut self, stderr: Handle<StdErrObserver>) -> Self {
        self.inner_mut().stderr_observer = Some(stderr);
        self
    }

    #[must_use]
    /// Sets the working directory for the child process.
    fn current_dir(mut self, current_dir: PathBuf) -> Self {
        self.inner_mut().current_directory = Some(current_dir);
        self
    }

    #[must_use]
    /// If set to true, the child's output won't be redirecited to `/dev/null` and will go to parent's stdout/stderr
    /// Defaults to `false`.
    fn debug_child(mut self, debug_child: bool) -> Self {
        if debug_child {
            assert!(
                self.inner().stderr_observer.is_none() && self.inner().stdout_observer.is_none(),
                "you can not set debug_child when you have stderr_observer or stdout_observer"
            );
        }
        self.inner_mut().debug_child = debug_child;
        self
    }

    #[must_use]
    /// Set the core to bind for the children
    fn core(mut self, core: CoreId) -> Self {
        self.inner_mut().core = Some(core);
        self
    }
}

#[cfg(test)]
/// Tester for executor
pub mod test {
    use super::nop::NopExecutor;
    use crate::{
        events::NopEventManager,
        executors::{Executor, ExitKind},
        fuzzer::NopFuzzer,
        inputs::BytesInput,
        state::NopState,
    };

    #[test]
    fn nop_executor() {
        let empty_input = BytesInput::new(vec![]);
        let mut executor = NopExecutor::ok();
        let mut fuzzer = NopFuzzer::new();
        let mut mgr: NopEventManager = NopEventManager::new();
        let mut state: NopState<BytesInput> = NopState::new();

        assert_eq!(
            executor
                .run_target(&mut fuzzer, &mut state, &mut mgr, &empty_input)
                .unwrap(),
            ExitKind::Ok
        );
    }
}
