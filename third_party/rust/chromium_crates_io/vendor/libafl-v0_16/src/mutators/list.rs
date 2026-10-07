//! Mutator definitions for [`ListInput`]s. See [`crate::inputs::list`] for details.

use alloc::borrow::Cow;
use core::num::NonZero;

use libafl_bolts::{Error, Named, rands::Rand};
use tuple_list::{tuple_list, tuple_list_type};

use crate::{
    corpus::Corpus,
    generators::Generator,
    inputs::{Input, ListInput, multi::MultipartInput},
    mutators::{MutationResult, Mutator},
    random_corpus_id,
    state::{HasCorpus, HasMaxSize, HasRand},
};

/// A list of mutators that can be used on a [`ListInput`].
pub type GenericListInputMutators = tuple_list_type!(
    RemoveLastEntryMutator,
    RemoveRandomEntryMutator,
    CrossoverInsertMutator,
    CrossoverReplaceMutator
);

/// Create a list of mutators that can be used on a [`ListInput`].
///
/// You may also want to use [`GenerateToAppendMutator`]
#[must_use]
pub fn generic_list_input_mutators() -> GenericListInputMutators {
    tuple_list!(
        RemoveLastEntryMutator,
        RemoveRandomEntryMutator,
        CrossoverInsertMutator,
        CrossoverReplaceMutator
    )
}

/// Mutator that generates a new input and appends it to the list.
#[derive(Debug)]
pub struct GenerateToAppendMutator<G> {
    generator: G,
}

impl<G> GenerateToAppendMutator<G> {
    /// Create a new `GenerateToAppendMutator`.
    #[must_use]
    pub fn new(generator: G) -> Self {
        Self { generator }
    }
}

impl<G, I, S> Mutator<ListInput<I>, S> for GenerateToAppendMutator<G>
where
    G: Generator<I, S>,
    I: Input,
{
    fn mutate(&mut self, state: &mut S, input: &mut ListInput<I>) -> Result<MutationResult, Error> {
        let generated = self.generator.generate(state)?;
        input.append_part(generated);
        Ok(MutationResult::Mutated)
    }
    #[inline]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<crate::corpus::CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl<G> Named for GenerateToAppendMutator<G> {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("GenerateToAppendMutator")
    }
}

/// Mutator that removes the last entry from a [`MultipartInput`].
///
/// Returns [`MutationResult::Skipped`] if the input is empty.
#[derive(Debug)]
pub struct RemoveLastEntryMutator;

impl<I, K, S> Mutator<MultipartInput<I, K>, S> for RemoveLastEntryMutator
where
    K: Default,
{
    fn mutate(
        &mut self,
        _state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        match input.pop_part() {
            Some(_) => Ok(MutationResult::Mutated),
            None => Ok(MutationResult::Skipped),
        }
    }
    #[inline]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<crate::corpus::CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for RemoveLastEntryMutator {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("RemoveLastEntryMutator")
    }
}

/// Mutator that removes a random entry from a [`MultipartInput`].
///
/// Returns [`MutationResult::Skipped`] if the input is empty.
#[derive(Debug)]
pub struct RemoveRandomEntryMutator;

impl<I, K, S> Mutator<MultipartInput<I, K>, S> for RemoveRandomEntryMutator
where
    S: HasRand,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        match MultipartInput::len(input) {
            0 => Ok(MutationResult::Skipped),
            len => {
                // Safety: null checks are done above
                let index = state
                    .rand_mut()
                    .below(unsafe { NonZero::new_unchecked(len) });
                input.remove_part_at_index(index);
                Ok(MutationResult::Mutated)
            }
        }
    }
    #[inline]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<crate::corpus::CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for RemoveRandomEntryMutator {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("RemoveRandomEntryMutator")
    }
}

/// Mutator that inserts a random part from another [`MultipartInput`] into the current input.
#[derive(Debug)]
pub struct CrossoverInsertMutator;

impl<I, K, S> Mutator<MultipartInput<I, K>, S> for CrossoverInsertMutator
where
    S: HasCorpus<MultipartInput<I, K>> + HasMaxSize + HasRand,
    I: Clone,
    K: Clone,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        let current_idx = match input.len() {
            0 => return Ok(MutationResult::Skipped),
            len => state
                .rand_mut()
                .below(unsafe { NonZero::new_unchecked(len) }),
        };
        let other_idx_raw = state.rand_mut().next() as usize;

        let id = random_corpus_id!(state.corpus(), state.rand_mut());
        let mut testcase = state.corpus().get(id)?.borrow_mut();
        let other = testcase.load_input(state.corpus())?;

        let other_len = other.len();

        let (key, part) = match other_len {
            0 => return Ok(MutationResult::Skipped),
            len => other.parts()[other_idx_raw % len].clone(),
        };

        input.insert_part(current_idx, (key, part));
        Ok(MutationResult::Mutated)
    }
    #[inline]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<crate::corpus::CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for CrossoverInsertMutator {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("CrossoverInsertMutator")
    }
}

/// Mutator that replaces a random part from the current [`MultipartInput`] with a random part from another input.
#[derive(Debug)]
pub struct CrossoverReplaceMutator;

impl<I, K, S> Mutator<MultipartInput<I, K>, S> for CrossoverReplaceMutator
where
    S: HasCorpus<MultipartInput<I, K>> + HasMaxSize + HasRand,
    I: Clone,
    K: Clone,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        let current_idx = match input.len() {
            0 => return Ok(MutationResult::Skipped),
            len => state
                .rand_mut()
                .below(unsafe { NonZero::new_unchecked(len) }),
        };
        let other_idx_raw = state.rand_mut().next() as usize;

        let id = random_corpus_id!(state.corpus(), state.rand_mut());
        let mut testcase = state.corpus().get(id)?.borrow_mut();
        let other = testcase.load_input(state.corpus())?;

        let other_len = other.len();

        let (key, part) = match other_len {
            0 => return Ok(MutationResult::Skipped),
            len => other.parts()[other_idx_raw % len].clone(),
        };

        input.remove_part_at_index(current_idx);
        input.insert_part(current_idx, (key, part));
        Ok(MutationResult::Mutated)
    }
    #[inline]
    fn post_exec(
        &mut self,
        _state: &mut S,
        _new_corpus_id: Option<crate::corpus::CorpusId>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for CrossoverReplaceMutator {
    fn name(&self) -> &Cow<'static, str> {
        &Cow::Borrowed("CrossoverReplaceMutator")
    }
}
