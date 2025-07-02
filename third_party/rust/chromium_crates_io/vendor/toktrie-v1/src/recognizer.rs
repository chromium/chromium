use crate::toktree::Recognizer;
use std::fmt::Debug;

pub trait FunctionalRecognizer<S: Copy> {
    /// Initial state
    fn initial(&self) -> S;
    /// Extend the recognizer with given byte if allowed.
    fn try_append(&self, state: S, byte: u8) -> Option<S>;
    /// Get error message if recognizer is in error state.
    fn get_error(&self, _state: S) -> Option<String> {
        None
    }
}

#[derive(Clone)]
pub struct StackRecognizer<S: Copy, R: FunctionalRecognizer<S>> {
    rec: R,
    stack: Vec<S>,
    stack_ptr: usize,
}

impl<S: Copy, R: FunctionalRecognizer<S>> StackRecognizer<S, R> {
    pub fn from(rec: R) -> Self {
        let stack = vec![rec.initial(); 300];
        StackRecognizer {
            rec,
            stack,
            stack_ptr: 0,
        }
    }

    pub fn reset(&mut self) {
        self.stack_ptr = 0;
        self.stack[0] = self.rec.initial();
    }

    pub fn recognizer(&self) -> &R {
        &self.rec
    }

    pub fn recognizer_mut(&mut self) -> &mut R {
        &mut self.rec
    }
}

impl<S: Copy + Debug, R: FunctionalRecognizer<S>> Debug for StackRecognizer<S, R> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("StackRecognizer")
            .field("top", &self.stack[self.stack_ptr])
            .finish()
    }
}

impl<S: Copy + Debug, R: FunctionalRecognizer<S>> Recognizer for StackRecognizer<S, R> {
    #[inline(always)]
    fn pop_bytes(&mut self, num: usize) {
        self.stack_ptr -= num;
    }

    fn trie_finished(&mut self) {
        // println!("{:?}", &self.stack[0..=self.stack_ptr]);
        // assert!(self.stack_ptr == 0);
        self.stack_ptr = 0;
    }

    fn collapse(&mut self) {
        self.stack[0] = self.stack[self.stack_ptr];
        self.stack_ptr = 0;
    }

    fn get_error(&mut self) -> Option<String> {
        self.rec.get_error(self.stack[self.stack_ptr])
    }

    #[inline(always)]
    fn try_push_byte(&mut self, byte: u8) -> bool {
        match self.rec.try_append(self.stack[self.stack_ptr], byte) {
            Some(state) => {
                self.stack_ptr += 1;
                self.stack[self.stack_ptr] = state;
                true
            }
            None => false,
        }
    }
}

#[derive(Clone)]
pub struct AnythingGoes {}

impl FunctionalRecognizer<()> for AnythingGoes {
    fn initial(&self) {}

    fn try_append(&self, state: (), _byte: u8) -> Option<()> {
        Some(state)
    }
}
