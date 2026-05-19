//! Gramatron generator
use alloc::{string::String, vec::Vec};
use core::{marker::PhantomData, num::NonZero};

use libafl_bolts::rands::Rand;
use serde::{Deserialize, Serialize};

use crate::{
    Error,
    generators::Generator,
    inputs::{GramatronInput, Terminal},
    state::HasRand,
};

/// A trigger
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct Trigger {
    /// the destination
    pub dest: usize,
    /// the term
    pub term: String,
}

/// The [`Automaton`]
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct Automaton {
    /// final state
    pub final_state: usize,
    /// init state
    pub init_state: usize,
    /// pda of [`Trigger`]s
    pub pda: Vec<Vec<Trigger>>,
}

#[derive(Debug, Clone)]
/// Generates random inputs from a grammar automaton
pub struct GramatronGenerator<'a, S> {
    automaton: &'a Automaton,
    phantom: PhantomData<S>,
}

impl<S> Generator<GramatronInput, S> for GramatronGenerator<'_, S>
where
    S: HasRand,
{
    fn generate(&mut self, state: &mut S) -> Result<GramatronInput, Error> {
        let mut input = GramatronInput::new(vec![]);
        self.append_generated_terminals(&mut input, state);
        Ok(input)
    }
}

impl<'a, S> GramatronGenerator<'a, S>
where
    S: HasRand,
{
    /// Returns a new [`GramatronGenerator`]
    #[must_use]
    pub fn new(automaton: &'a Automaton) -> Self {
        Self {
            automaton,
            phantom: PhantomData,
        }
    }

    /// Append the generated terminals
    pub fn append_generated_terminals(&self, input: &mut GramatronInput, state: &mut S) -> usize {
        let mut counter = 0;
        let final_state = self.automaton.final_state;
        let mut current_state =
            input
                .terminals()
                .last()
                .map_or(self.automaton.init_state, |last| {
                    let triggers = &self.automaton.pda[last.state];
                    let idx = state.rand_mut().below(
                        NonZero::new(triggers.len())
                            .expect("Triggers are empty in append_generated_terminals!"),
                    );
                    triggers[idx].dest
                });

        while current_state != final_state {
            let triggers = &self.automaton.pda[current_state];
            let idx =
                state
                    .rand_mut()
                    .below(NonZero::new(triggers.len()).expect(
                        "Automation.pda triggers are empty in append_generated_terminals!",
                    ));
            let trigger = &triggers[idx];
            input
                .terminals_mut()
                .push(Terminal::new(current_state, idx, trigger.term.clone()));
            current_state = trigger.dest;
            counter += 1;
        }

        counter
    }
}
