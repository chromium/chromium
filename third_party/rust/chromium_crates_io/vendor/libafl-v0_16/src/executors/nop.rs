//! Trivial Constant Executor

use core::time::Duration;

use libafl_bolts::tuples::RefIndexable;

use super::{Executor, ExitKind, HasObservers, HasTimeout};
use crate::executors::SetTimeout;

/// [`NopExecutor`] is an executor that does nothing
pub type NopExecutor = ConstantExecutor<()>;

/// Constant Executor that returns a fixed value. Mostly helpful
/// when you need it to satisfy some bounds like [`crate::fuzzer::NopFuzzer`]
#[derive(Debug)]
pub struct ConstantExecutor<OT = ()> {
    exit: ExitKind,
    tm: Duration,
    ot: OT,
}

impl<OT> ConstantExecutor<OT> {
    /// Construct a [`ConstantExecutor`]
    #[must_use]
    pub fn new(exit: ExitKind, tm: Duration, ot: OT) -> Self {
        Self { exit, tm, ot }
    }
}

impl ConstantExecutor<()> {
    /// Create a new `nop` executor that does nothing.
    #[must_use]
    pub fn nop() -> Self {
        Self::new(ExitKind::Ok, Duration::default(), ())
    }
}

impl ConstantExecutor<()> {
    /// Construct a [`ConstantExecutor`] that always returns Ok
    #[must_use]
    pub fn ok() -> Self {
        Self::new(ExitKind::Ok, Duration::default(), ())
    }

    /// Construct a [`ConstantExecutor`] that always returns Crash
    #[must_use]
    pub fn crash() -> Self {
        Self::new(ExitKind::Crash, Duration::default(), ())
    }
}

/// These are important to allow [`ConstantExecutor`] to be used with other components
impl<OT> HasObservers for ConstantExecutor<OT> {
    type Observers = OT;
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        RefIndexable::from(&self.ot)
    }

    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        RefIndexable::from(&mut self.ot)
    }
}

impl<OT> HasTimeout for ConstantExecutor<OT> {
    fn timeout(&self) -> Duration {
        self.tm
    }
}

impl<OT> SetTimeout for ConstantExecutor<OT> {
    fn set_timeout(&mut self, timeout: Duration) {
        self.tm = timeout;
    }
}

impl<OT, EM, I, S, Z> Executor<EM, I, S, Z> for ConstantExecutor<OT> {
    fn run_target(
        &mut self,
        _fuzzer: &mut Z,
        _state: &mut S,
        _mgr: &mut EM,
        _input: &I,
    ) -> Result<ExitKind, libafl_bolts::Error> {
        Ok(self.exit)
    }
}
