//! A `CombinedExecutor` wraps a primary executor and a secondary one
//! In comparison to the [`crate::executors::DiffExecutor`] it does not run the secondary executor in `run_target`.

use core::{fmt::Debug, time::Duration};

use libafl_bolts::tuples::RefIndexable;

use super::HasTimeout;
use crate::{
    Error,
    executors::{Executor, ExitKind, HasObservers, SetTimeout},
};

/// A [`CombinedExecutor`] wraps a primary executor, forwarding its methods, and a secondary one
#[derive(Debug)]
pub struct CombinedExecutor<A, B> {
    primary: A,
    secondary: B,
}

impl<A, B> CombinedExecutor<A, B> {
    /// Create a new `CombinedExecutor`, wrapping the given `executor`s.
    pub fn new(primary: A, secondary: B) -> Self {
        Self { primary, secondary }
    }

    /// Retrieve the primary `Executor` that is wrapped by this `CombinedExecutor`.
    pub fn primary(&mut self) -> &mut A {
        &mut self.primary
    }

    /// Retrieve the secondary `Executor` that is wrapped by this `CombinedExecutor`.
    pub fn secondary(&mut self) -> &mut B {
        &mut self.secondary
    }
}

impl<A, B, EM, I, S, Z> Executor<EM, I, S, Z> for CombinedExecutor<A, B>
where
    A: Executor<EM, I, S, Z>,
    B: Executor<EM, I, S, Z>,
{
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        self.primary.run_target(fuzzer, state, mgr, input)
    }
}

impl<A, B> HasTimeout for CombinedExecutor<A, B>
where
    A: HasTimeout,
    B: HasTimeout,
{
    #[inline]
    fn timeout(&self) -> Duration {
        assert!(
            self.primary.timeout() == self.secondary.timeout(),
            "Primary and Secondary Executors have different timeouts!"
        );
        self.primary.timeout()
    }
}

impl<A, B> SetTimeout for CombinedExecutor<A, B>
where
    A: SetTimeout,
    B: SetTimeout,
{
    #[inline]
    fn set_timeout(&mut self, timeout: Duration) {
        self.primary.set_timeout(timeout);
        self.secondary.set_timeout(timeout);
    }
}

impl<A, B> HasObservers for CombinedExecutor<A, B>
where
    A: HasObservers,
{
    type Observers = A::Observers;

    #[inline]
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        self.primary.observers()
    }

    #[inline]
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        self.primary.observers_mut()
    }
}
