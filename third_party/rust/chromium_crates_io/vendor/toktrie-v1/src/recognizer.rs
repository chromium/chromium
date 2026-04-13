//! Functional recognizer trait and stack-based adapter.
//!
//! This module provides the [`FunctionalRecognizer`] trait and the [`StackRecognizer`] adapter.
//! Users implement `FunctionalRecognizer<S>` as a pure state-transition function: given the
//! current state and a byte, return the next state (or `None` to reject). Then, wrapping that
//! implementation in a [`StackRecognizer`] yields a full [`Recognizer`] implementation that
//! [`TokTrie`](crate::TokTrie) can drive during trie walks.

use crate::toktree::Recognizer;
use std::fmt::Debug;

/// A pure, stateless-object interface for recognizing byte sequences.
///
/// Implementors define a state type `S: Copy` and a transition function: given the current
/// state and a byte, return `Some(next_state)` to accept the byte or `None` to reject it.
/// The recognizer object itself is not mutated—all state lives in `S`.
///
/// This contrasts with the [`Recognizer`] trait, which manages its own mutable stack
/// internally. To bridge the two, wrap a `FunctionalRecognizer` in a [`StackRecognizer`],
/// which maintains the state stack and implements `Recognizer`.
pub trait FunctionalRecognizer<S: Copy> {
    /// Returns the initial state of the recognizer, before any bytes have been processed.
    fn initial(&self) -> S;
    /// Extend the recognizer with the given byte if allowed.
    ///
    /// Returns `Some(next_state)` when `byte` is accepted from `state`,
    /// or `None` to reject the byte.
    fn try_append(&self, state: S, byte: u8) -> Option<S>;
    /// Get a diagnostic message for the current state
    ///
    /// Returns a descriptive message about the given state (or `None` in this default
    /// implementation). Users will most likely call this in response to a rejection
    /// (i.e., when `try_append` returns `None`), and if used via [`StackRecognizer`],
    /// the state passed to `get_error` will be the state which rejected the byte. As
    /// such, the most useful messages will describe what bytes the state would accept.
    fn get_error(&self, _state: S) -> Option<String> {
        None
    }
}

/// A stack-based adapter that wraps a [`FunctionalRecognizer<S>`] and implements the
/// [`Recognizer`] trait.
///
/// Each entry on the stack is a state value of type `S`, corresponding to one byte
/// pushed by the trie walker. The maximum stack depth equals the length (in bytes) of
/// the longest token in the vocabulary. The stack is pre-allocated to
/// [`STACK_CAPACITY`] entries.
///
/// **Note:** The stack does not currently grow. If the vocabulary contains tokens
/// longer than [`STACK_CAPACITY`] bytes (e.g., GLM4 has a 1024-space token),
/// `try_push_byte` will panic with an out-of-bounds index.
///
/// As the trie walker descends it pushes states via
/// [`try_push_byte`](Recognizer::try_push_byte), and pops them when backtracking via
/// [`pop_bytes`](Recognizer::pop_bytes).
#[derive(Clone)]
pub struct StackRecognizer<S: Copy, R: FunctionalRecognizer<S>> {
    rec: R,
    stack: Vec<S>,
    stack_ptr: usize,
}

/// Number of state entries pre-allocated for [`StackRecognizer`]'s internal stack.
///
/// This is sufficient for most tokenizers, but the stack does **not** grow at runtime.
/// Tokenizers with tokens longer than this many bytes will cause a panic.
pub const STACK_CAPACITY: usize = 300;

impl<S: Copy, R: FunctionalRecognizer<S>> StackRecognizer<S, R> {
    /// Creates a new `StackRecognizer` from a [`FunctionalRecognizer`].
    ///
    /// The internal stack is pre-allocated to [`STACK_CAPACITY`] entries,
    /// all initialized to the recognizer's initial state, with the stack pointer
    /// at position 0. The stack does not grow at runtime.
    pub fn from(rec: R) -> Self {
        let stack = vec![rec.initial(); STACK_CAPACITY];
        StackRecognizer {
            rec,
            stack,
            stack_ptr: 0,
        }
    }

    /// Resets the stack to contain only the initial state, as if no bytes had been pushed.
    pub fn reset(&mut self) {
        self.stack_ptr = 0;
        self.stack[0] = self.rec.initial();
    }

    /// Returns a shared reference to the underlying [`FunctionalRecognizer`].
    pub fn recognizer(&self) -> &R {
        &self.rec
    }

    /// Returns a mutable reference to the underlying [`FunctionalRecognizer`].
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
                // Stack growth logic would go here if needed
                self.stack[self.stack_ptr] = state;
                true
            }
            None => false,
        }
    }
}

/// A no-op recognizer that accepts every byte sequence. The state type is `()`.
///
/// Useful for testing or when you want to perform trie operations without applying any
/// constraint on the recognized bytes.
///
/// This type implements [`FunctionalRecognizer<()>`] and must be wrapped in a
/// [`StackRecognizer`] to be used with [`TokTrie::add_bias`](crate::TokTrie::add_bias).
/// For a recognizer that implements [`Recognizer`] directly (no wrapper needed), see
/// [`toktrie::AnythingGoes`](crate::toktree::AnythingGoes).
#[derive(Clone)]
pub struct AnythingGoes {}

impl FunctionalRecognizer<()> for AnythingGoes {
    fn initial(&self) {}

    fn try_append(&self, state: (), _byte: u8) -> Option<()> {
        Some(state)
    }
}
