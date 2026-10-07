//! The [`GenStage`] generates a single input and evaluates it.
//!
//! A [`Stage`] that generates a single input via a
//! [`crate::generators::Generator`] and evaluates it using the fuzzer, possibly
//! adding it to the corpus.

use core::marker::PhantomData;

use crate::{
    Error, Evaluator,
    generators::Generator,
    stages::{Restartable, Stage},
    state::HasRand,
};

/// A [`Stage`] that generates a single input via a [`Generator`] and evaluates
/// it using the fuzzer, possibly adding it to the corpus.
///
/// This stage can be used to construct black-box (e.g., grammar-based) fuzzers.
#[derive(Debug)]
pub struct GenStage<G, I, S, Z>(G, PhantomData<(I, S, Z)>);

impl<G, I, S, Z> GenStage<G, I, S, Z> {
    /// Create a new [`GenStage`].
    pub fn new(g: G) -> Self {
        Self(g, PhantomData)
    }
}

impl<E, EM, G, I, S, Z> Stage<E, EM, S, Z> for GenStage<G, I, S, Z>
where
    G: Generator<I, S>,
    S: HasRand,
    Z: Evaluator<E, EM, I, S>,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let input = self.0.generate(state)?;
        fuzzer.evaluate_filtered(state, executor, manager, &input)?;
        Ok(())
    }
}

impl<G, I, S, Z> Restartable<S> for GenStage<G, I, S, Z> {
    fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
        // It's a random generation stage
        // so you can restart for whatever times you want
        Ok(true)
    }

    fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        Ok(())
    }
}
