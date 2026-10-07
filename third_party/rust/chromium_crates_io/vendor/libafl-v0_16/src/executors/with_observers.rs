//! A wrapper for any [`Executor`] to make it implement [`HasObservers`] using a given [`ObserversTuple`].

use core::{fmt::Debug, marker::PhantomData};

use libafl_bolts::tuples::RefIndexable;

use crate::{
    Error,
    executors::{Executor, ExitKind, HasObservers},
    observers::ObserversTuple,
};

/// A wrapper for any [`Executor`] to make it implement [`HasObservers`] using a given [`ObserversTuple`].
#[derive(Debug)]
pub struct WithObservers<E, I, OT, S> {
    executor: E,
    observers: OT,
    phantom: PhantomData<(I, S)>,
}

impl<E, EM, I, OT, S, Z> Executor<EM, I, S, Z> for WithObservers<E, I, OT, S>
where
    E: Executor<EM, I, S, Z>,
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

impl<E, I, OT, S> HasObservers for WithObservers<E, I, OT, S>
where
    OT: ObserversTuple<I, S>,
{
    type Observers = OT;
    fn observers(&self) -> RefIndexable<&Self::Observers, Self::Observers> {
        RefIndexable::from(&self.observers)
    }

    fn observers_mut(&mut self) -> RefIndexable<&mut Self::Observers, Self::Observers> {
        RefIndexable::from(&mut self.observers)
    }
}

impl<E, I, OT, S> WithObservers<E, I, OT, S> {
    /// Wraps the given [`Executor`] with the given [`ObserversTuple`] to implement [`HasObservers`].
    ///
    /// If the executor already implements [`HasObservers`], then the original implementation will be overshadowed by
    /// the implementation of this wrapper.
    pub fn new(executor: E, observers: OT) -> Self {
        Self {
            executor,
            observers,
            phantom: PhantomData,
        }
    }
}
