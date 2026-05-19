//! A nop stage does nothing

use super::{Restartable, Stage};

/// A stage that does nothing
#[derive(Debug, Copy, Clone, Default)]
pub struct NopStage {}

impl NopStage {
    /// Create a [`NopStage`]
    #[must_use]
    pub fn new() -> Self {
        Self {}
    }
}

impl<E, EM, S, Z> Stage<E, EM, S, Z> for NopStage {
    fn perform(
        &mut self,
        _fuzzer: &mut Z,
        _executor: &mut E,
        _state: &mut S,
        _manager: &mut EM,
    ) -> Result<(), libafl_bolts::Error> {
        Ok(())
    }
}

impl<S> Restartable<S> for NopStage {
    fn clear_progress(&mut self, _state: &mut S) -> Result<(), libafl_bolts::Error> {
        Ok(())
    }

    fn should_restart(&mut self, _state: &mut S) -> Result<bool, libafl_bolts::Error> {
        Ok(false)
    }
}
