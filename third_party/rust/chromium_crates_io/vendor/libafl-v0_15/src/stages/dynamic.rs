//! A stage implementation that can have dynamic stage runtime

use super::{Restartable, Stage};

/// A dynamic stage implementation. This explicity uses enum so that rustc can better
/// reason about the bounds.
#[derive(Debug)]
pub enum DynamicStage<T1, T2> {
    /// One stage
    Stage1(T1),
    /// The alernative stage
    Stage2(T2),
}

impl<T1, T2, E, EM, S, Z> Stage<E, EM, S, Z> for DynamicStage<T1, T2>
where
    T1: Stage<E, EM, S, Z>,
    T2: Stage<E, EM, S, Z>,
{
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), libafl_bolts::Error> {
        match self {
            Self::Stage1(st1) => st1.perform(fuzzer, executor, state, manager),
            Self::Stage2(st2) => st2.perform(fuzzer, executor, state, manager),
        }
    }
}

impl<T1, T2, S> Restartable<S> for DynamicStage<T1, T2>
where
    T1: Restartable<S>,
    T2: Restartable<S>,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, libafl_bolts::Error> {
        match self {
            Self::Stage1(st1) => st1.should_restart(state),
            Self::Stage2(st2) => st2.should_restart(state),
        }
    }
    fn clear_progress(&mut self, state: &mut S) -> Result<(), libafl_bolts::Error> {
        match self {
            Self::Stage1(st1) => st1.clear_progress(state),
            Self::Stage2(st2) => st2.clear_progress(state),
        }
    }
}
