//! TrueType bytecode interpreter.

mod arith;
mod graphics_state;
mod logical;
mod stack;

use super::{
    code_state::ProgramKind, error::HintErrorKind, graphics_state::GraphicsState,
    value_stack::ValueStack,
};

pub type OpResult = Result<(), HintErrorKind>;

/// TrueType bytecode interpreter.
pub struct Engine<'a> {
    graphics_state: GraphicsState<'a>,
    value_stack: ValueStack<'a>,
    initial_program: ProgramKind,
}

#[cfg(test)]
use mock::MockEngine;

#[cfg(test)]
mod mock {
    use super::{Engine, GraphicsState, ProgramKind, ValueStack};

    /// Mock engine for testing.
    pub(super) struct MockEngine {
        value_stack: Vec<i32>,
    }

    impl MockEngine {
        pub fn new() -> Self {
            Self {
                value_stack: vec![0; 32],
            }
        }

        pub fn engine(&mut self) -> Engine {
            Engine {
                graphics_state: GraphicsState::default(),
                value_stack: ValueStack::new(&mut self.value_stack),
                initial_program: ProgramKind::Font,
            }
        }
    }

    impl Default for MockEngine {
        fn default() -> Self {
            Self::new()
        }
    }

    impl<'a> Engine<'a> {
        /// Helper to push values to the stack, invoke a callback and check
        /// the expected result.    
        pub(super) fn test_exec(
            &mut self,
            push: &[i32],
            expected_result: impl Into<i32>,
            mut f: impl FnMut(&mut Engine),
        ) {
            for &val in push {
                self.value_stack.push(val).unwrap();
            }
            f(self);
            assert_eq!(self.value_stack.pop().ok(), Some(expected_result.into()));
        }
    }
}
