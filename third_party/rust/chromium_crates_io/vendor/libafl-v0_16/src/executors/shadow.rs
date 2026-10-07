//! A `ShadowExecutor` wraps an executor to have shadow observer that will not be considered by the feedbacks and the manager

use core::{
    fmt::{self, Debug, Formatter},
    marker::PhantomData,
    time::Duration,
};

use libafl_bolts::tuples::RefIndexable;

use super::{HasTimeout, SetTimeout};
use crate::{
    Error,
    executors::{Executor, ExitKind, HasObservers},
    observers::ObserversTuple,
};

/// A [`ShadowExecutor`] wraps an executor and a set of shadow observers
pub struct ShadowExecutor<E, I, S, SOT> {
    /// The wrapped executor
    executor: E,
    /// The shadow observers
    shadow_observers: SOT,
    phantom: PhantomData<(I, S)>,
}

impl<E, I, S, SOT> Debug for ShadowExecutor<E, I, S, SOT>
where
    E: Debug,
    SOT: Debug,
{
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("ShadowExecutor")
            .field("executor", &self.executor)
            .field("shadow_observers", &self.shadow_observers)
            .finish()
    }
}

impl<E, I, S, SOT> ShadowExecutor<E, I, S, SOT>
where
    E: HasObservers,
    SOT: ObserversTuple<I, S>,
{
    /// Create a new `ShadowExecutor`, wrapping the given `executor`.
    pub fn new(executor: E, shadow_observers: SOT) -> Self {
        Self {
            executor,
            shadow_observers,
            phantom: PhantomData,
        }
    }

    /// The shadow observers are not considered by the feedbacks and the manager, mutable
    #[inline]
    pub fn shadow_observers(&self) -> RefIndexable<&SOT, SOT> {
        RefIndexable::from(&self.shadow_observers)
    }

    /// The shadow observers are not considered by the feedbacks and the manager, mutable
    #[inline]
    pub fn shadow_observers_mut(&mut self) -> RefIndexable<&mut SOT, SOT> {
        RefIndexable::from(&mut self.shadow_observers)
    }

    /// Inner executor
    #[inline]
    pub fn executor(&self) -> &E {
        &self.executor
    }

    /// Inner executor
    #[inline]
    pub fn executor_mut(&mut self) -> &mut E {
        &mut self.executor
    }
}

impl<E, EM, I, S, SOT, Z> Executor<EM, I, S, Z> for ShadowExecutor<E, I, S, SOT>
where
    E: Executor<EM, I, S, Z> + HasObservers,
    SOT: ObserversTuple<I, S>,
{
    fn run_target(
        &mut self,
        fuzzer: &mut Z,
        state: &mut S,
        mgr: &mut EM,
        input: &I,
    ) -> Result<ExitKind, Error> {
        self.executor.run_target(fuzzer, state, mgr, input)
    }
}

impl<E, I, S, SOT> HasTimeout for ShadowExecutor<E, I, S, SOT>
where
    E: HasTimeout,
{
    #[inline]
    fn timeout(&self) -> Duration {
        self.executor.timeout()
    }
}

impl<E, I, S, SOT> SetTimeout for ShadowExecutor<E, I, S, SOT>
where
    E: SetTimeout,
{
    #[inline]
    fn set_timeout(&mut self, timeout: Duration) {
        self.executor.set_timeout(timeout);
    }
}

impl<E, I, S, SOT> HasObservers for ShadowExecutor<E, I, S, SOT>
where
    E: HasObservers,
    SOT: ObserversTuple<I, S>,
{
    type Observers = E::Observers;
    #[inline]
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        self.executor.observers()
    }

    #[inline]
    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        self.executor.observers_mut()
    }
}
