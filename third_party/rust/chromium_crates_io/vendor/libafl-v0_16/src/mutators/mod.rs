//! [`Mutator`]`s` mutate input during fuzzing.
//!
//! These can be used standalone or in combination with other mutators to explore the input space more effectively.
//! You can read more about mutators in the [LibAFL book](https://aflplus.plus/libafl-book/core_concepts/mutator.html)
pub mod scheduled;
use core::fmt;

pub use scheduled::*;
pub mod mutations;
pub use mutations::*;
pub mod token_mutations;
use serde::{Deserialize, Serialize};
pub use token_mutations::*;
pub mod havoc_mutations;
pub use havoc_mutations::*;
pub mod numeric;
pub use numeric::{int_mutators, mapped_int_mutators};
pub mod encoded_mutations;
pub use encoded_mutations::*;
pub mod mopt_mutator;
pub use mopt_mutator::*;
pub mod gramatron;
pub use gramatron::*;
pub mod grimoire;
pub use grimoire::*;
pub mod mapping;
pub use mapping::*;
pub mod tuneable;
pub use tuneable::*;

#[cfg(feature = "lua_mutator")]
pub mod lua;

#[cfg(feature = "std")]
pub mod hash;
#[cfg(feature = "std")]
pub use hash::*;

#[cfg(feature = "unicode")]
pub mod unicode;
#[cfg(feature = "unicode")]
pub use unicode::*;

#[cfg(feature = "multipart_inputs")]
pub mod list;
#[cfg(feature = "multipart_inputs")]
pub mod multi;

#[cfg(feature = "nautilus")]
pub mod nautilus;

use alloc::{borrow::Cow, boxed::Box, vec::Vec};

use libafl_bolts::{HasLen, Named, tuples::IntoVec};
#[cfg(feature = "nautilus")]
pub use nautilus::*;
use tuple_list::NonEmptyTuple;

use crate::{Error, corpus::CorpusId};

// TODO mutator stats method that produces something that can be sent with the NewTestcase event
// We can use it to report which mutations generated the testcase in the broker logs

/// The index of a mutation in the mutations tuple
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
#[repr(transparent)]
pub struct MutationId(pub(crate) usize);

impl fmt::Display for MutationId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MutationId({})", self.0)
    }
}

impl From<usize> for MutationId {
    fn from(value: usize) -> Self {
        MutationId(value)
    }
}

impl From<u64> for MutationId {
    fn from(value: u64) -> Self {
        MutationId(value as usize)
    }
}

impl From<i32> for MutationId {
    #[expect(clippy::cast_sign_loss)]
    fn from(value: i32) -> Self {
        debug_assert!(value >= 0);
        MutationId(value as usize)
    }
}

/// Result of the mutation.
///
/// [`MutationResult::Mutated`] does not necessarily mean that the input changed,
/// just that the mutator did something. For slow targets, consider using
/// a fuzzer with a input filter
/// or wrapping your mutator in a [`hash::MutationChecker`].
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum MutationResult {
    /// The [`Mutator`] executed on this `Input`. It may not guarantee that the input has actually been changed.
    Mutated,
    /// The [`Mutator`] did not mutate this `Input`. It was `Skipped`.
    Skipped,
}

/// A [`Mutator`] takes an input, and mutates it.
/// Simple as that.
pub trait Mutator<I, S>: Named {
    /// Mutate a given input
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error>;

    /// Post-process given the outcome of the execution
    /// `new_corpus_id` will be `Some` if a new [`crate::corpus::Testcase`] was created this execution.
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error>;
}

/// A mutator that takes input, and returns a vector of mutated inputs.
/// Simple as that.
pub trait MultiMutator<I, S>: Named {
    /// Mutate a given input up to `max_count` times,
    /// or as many times as appropriate, if no `max_count` is given
    fn multi_mutate(
        &mut self,
        state: &mut S,
        input: &I,
        max_count: Option<usize>,
    ) -> Result<Vec<I>, Error>;

    /// Post-process given the outcome of the execution
    /// `new_corpus_id` will be `Some` if a new `Testcase` was created this execution.
    #[inline]
    fn multi_post_exec(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

/// A `Tuple` of [`Mutator`]`s` that can execute multiple `Mutators` in a row.
pub trait MutatorsTuple<I, S>: HasLen {
    /// Runs the [`Mutator::mutate`] function on all [`Mutator`]`s` in this `Tuple`.
    fn mutate_all(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error>;

    /// Runs the [`Mutator::post_exec`] function on all [`Mutator`]`s` in this `Tuple`.
    /// `new_corpus_id` will be `Some` if a new `Testcase` was created this execution.
    fn post_exec_all(
        &mut self,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error>;

    /// Gets the [`Mutator`] at the given index and runs the `mutate` function on it.
    fn get_and_mutate(
        &mut self,
        index: MutationId,
        state: &mut S,
        input: &mut I,
    ) -> Result<MutationResult, Error>;

    /// Gets the [`Mutator`] at the given index and runs the `post_exec` function on it.
    /// `new_corpus_id` will be `Some` if a new `Testcase` was created this execution.
    fn get_and_post_exec(
        &mut self,
        index: usize,
        state: &mut S,

        corpus_id: Option<CorpusId>,
    ) -> Result<(), Error>;
}

impl<I, S> MutatorsTuple<I, S> for () {
    #[inline]
    fn mutate_all(&mut self, _state: &mut S, _input: &mut I) -> Result<MutationResult, Error> {
        Ok(MutationResult::Skipped)
    }

    #[inline]
    fn post_exec_all(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }

    #[inline]
    fn get_and_mutate(
        &mut self,
        _index: MutationId,
        _state: &mut S,
        _input: &mut I,
    ) -> Result<MutationResult, Error> {
        Ok(MutationResult::Skipped)
    }

    #[inline]
    fn get_and_post_exec(
        &mut self,
        _index: usize,
        _state: &mut S,
        _new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl<Head, Tail, I, S> MutatorsTuple<I, S> for (Head, Tail)
where
    Head: Mutator<I, S>,
    Tail: MutatorsTuple<I, S>,
{
    fn mutate_all(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        let r = self.0.mutate(state, input)?;
        if self.1.mutate_all(state, input)? == MutationResult::Mutated {
            Ok(MutationResult::Mutated)
        } else {
            Ok(r)
        }
    }

    fn post_exec_all(
        &mut self,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        self.0.post_exec(state, new_corpus_id)?;
        self.1.post_exec_all(state, new_corpus_id)
    }

    fn get_and_mutate(
        &mut self,
        index: MutationId,
        state: &mut S,
        input: &mut I,
    ) -> Result<MutationResult, Error> {
        if index.0 == 0 {
            self.0.mutate(state, input)
        } else {
            self.1.get_and_mutate((index.0 - 1).into(), state, input)
        }
    }

    fn get_and_post_exec(
        &mut self,
        index: usize,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        if index == 0 {
            self.0.post_exec(state, new_corpus_id)
        } else {
            self.1.get_and_post_exec(index - 1, state, new_corpus_id)
        }
    }
}

impl<Head, Tail, I, S> IntoVec<Box<dyn Mutator<I, S>>> for (Head, Tail)
where
    Head: Mutator<I, S> + 'static,
    Tail: IntoVec<Box<dyn Mutator<I, S>>>,
{
    fn into_vec_reversed(self) -> Vec<Box<dyn Mutator<I, S>>> {
        let (head, tail) = self.uncons();
        let mut ret = tail.into_vec_reversed();
        ret.push(Box::new(head));
        ret
    }

    fn into_vec(self) -> Vec<Box<dyn Mutator<I, S>>> {
        let mut ret = self.into_vec_reversed();
        ret.reverse();
        ret
    }
}

impl<Tail, I, S> MutatorsTuple<I, S> for (Tail,)
where
    Tail: MutatorsTuple<I, S>,
{
    fn mutate_all(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        self.0.mutate_all(state, input)
    }

    fn post_exec_all(
        &mut self,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        self.0.post_exec_all(state, new_corpus_id)
    }

    fn get_and_mutate(
        &mut self,
        index: MutationId,
        state: &mut S,
        input: &mut I,
    ) -> Result<MutationResult, Error> {
        self.0.get_and_mutate(index, state, input)
    }

    fn get_and_post_exec(
        &mut self,
        index: usize,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        self.0.get_and_post_exec(index, state, new_corpus_id)
    }
}

impl<Tail, I, S> IntoVec<Box<dyn Mutator<I, S>>> for (Tail,)
where
    Tail: IntoVec<Box<dyn Mutator<I, S>>>,
{
    fn into_vec(self) -> Vec<Box<dyn Mutator<I, S>>> {
        self.0.into_vec()
    }
}

impl<I, S> MutatorsTuple<I, S> for Vec<Box<dyn Mutator<I, S>>> {
    fn mutate_all(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        self.iter_mut()
            .try_fold(MutationResult::Skipped, |ret, mutator| {
                if mutator.mutate(state, input)? == MutationResult::Mutated {
                    Ok(MutationResult::Mutated)
                } else {
                    Ok(ret)
                }
            })
    }

    fn post_exec_all(
        &mut self,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        for mutator in self.iter_mut() {
            mutator.post_exec(state, new_corpus_id)?;
        }
        Ok(())
    }

    fn get_and_mutate(
        &mut self,
        index: MutationId,
        state: &mut S,
        input: &mut I,
    ) -> Result<MutationResult, Error> {
        let mutator = self
            .get_mut(index.0)
            .ok_or_else(|| Error::key_not_found(format!("Mutator with id {index:?} not found.")))?;
        mutator.mutate(state, input)
    }

    fn get_and_post_exec(
        &mut self,
        index: usize,
        state: &mut S,
        new_corpus_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        let mutator = self
            .get_mut(index)
            .ok_or_else(|| Error::key_not_found(format!("Mutator with id {index:?} not found.")))?;
        mutator.post_exec(state, new_corpus_id)
    }
}

impl<I, S> IntoVec<Box<dyn Mutator<I, S>>> for Vec<Box<dyn Mutator<I, S>>> {
    fn into_vec(self) -> Vec<Box<dyn Mutator<I, S>>> {
        self
    }
}

/// [`Mutator`] that does nothing, used for testing.
///
/// Example:
///
/// ```rust,ignore
/// let mut stages = tuple_list!(StdMutationalStage::new(NopMutator(MutationResult::Mutated)));
/// ```
#[derive(Debug, Copy, Clone)]
pub struct NopMutator {
    result: MutationResult,
}

impl NopMutator {
    /// The passed argument is returned every time the mutator is called.
    #[must_use]
    pub fn new(result: MutationResult) -> Self {
        Self { result }
    }
}

impl<I, S> Mutator<I, S> for NopMutator {
    fn mutate(&mut self, _state: &mut S, _input: &mut I) -> Result<MutationResult, Error> {
        Ok(self.result)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for NopMutator {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("NopMutator")
    }
}

/// [`Mutator`] that inverts a boolean value.
///
/// Mostly useful in combination with [`mapping::MappingMutator`]s to mutate parts of a complex input.
#[derive(Debug)]
pub struct BoolInvertMutator;

impl<S> Mutator<bool, S> for BoolInvertMutator {
    fn mutate(&mut self, _state: &mut S, input: &mut bool) -> Result<MutationResult, Error> {
        *input = !*input;
        Ok(MutationResult::Mutated)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for BoolInvertMutator {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("BoolInvertMutator")
    }
}
