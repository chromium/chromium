//! Definitions for inputs which have multiple distinct subcomponents.
//!
//! Unfortunately, since both [`serde::de::Deserialize`] and [`Clone`] require [`Sized`], it is not
//! possible to dynamically define a single input with dynamic typing. As such, [`ListInput`]
//! requires that each subcomponent be the same subtype.

use alloc::{
    borrow::Cow,
    string::{String, ToString as _},
    vec::Vec,
};
use core::num::NonZero;

use arrayvec::ArrayVec;
use libafl_bolts::{
    Error, Named,
    rands::Rand as _,
    tuples::{Map, MappingFunctor},
};
use serde::{Deserialize, Serialize};

use crate::{
    corpus::CorpusId,
    inputs::Input,
    mutators::{MutationResult, Mutator},
    state::HasRand,
};

/// An input composed of multiple parts. Use in situations where subcomponents are not necessarily
/// related, or represent distinct parts of the input.
#[derive(Debug, Clone, Serialize, Deserialize, Hash)]
pub struct ListInput<I> {
    parts: Vec<I>,
}

impl<I> Default for ListInput<I> {
    fn default() -> Self {
        Self::empty()
    }
}

impl<I> ListInput<I> {
    /// Create a new empty [`ListInput`].
    #[must_use]
    #[inline]
    pub fn empty() -> Self {
        Self { parts: Vec::new() }
    }

    /// Create a new [`ListInput`] with the given parts.
    #[must_use]
    #[inline]
    pub fn new(parts: Vec<I>) -> Self {
        Self { parts }
    }

    fn idxs_to_skips(idxs: &mut [usize]) {
        for following in (1..idxs.len()).rev() {
            let first = idxs[following - 1];
            let second = idxs[following];

            idxs[following] = second
                .checked_sub(first)
                .expect("idxs was not sorted")
                .checked_sub(1)
                .expect("idxs had duplicate elements");
        }
    }

    /// Get the individual parts of this input.
    #[must_use]
    #[inline]
    pub fn parts(&self) -> &[I] {
        &self.parts
    }

    /// Get the individual parts of this input.
    #[must_use]
    #[inline]
    pub fn parts_mut(&mut self) -> &mut [I] {
        &mut self.parts
    }

    /// Get a specific part of this input by index.
    #[must_use]
    #[inline]
    pub fn part_at_index(&self, idx: usize) -> Option<&I> {
        self.parts.get(idx)
    }

    /// Get a specific part of this input by index.
    #[must_use]
    #[inline]
    pub fn part_at_index_mut(&mut self, idx: usize) -> Option<&mut I> {
        self.parts.get_mut(idx)
    }

    /// Access multiple parts mutably.
    ///
    /// ## Panics
    ///
    /// Panics if idxs is not sorted, has duplicate elements, or any entry is out of bounds.
    #[must_use]
    pub fn parts_at_indices_mut<const C: usize>(&mut self, mut idxs: [usize; C]) -> [&mut I; C] {
        Self::idxs_to_skips(&mut idxs);

        let mut parts = self.parts.iter_mut();
        if let Ok(arr) = idxs
            .into_iter()
            .map(|i| parts.nth(i).expect("idx had an out of bounds entry"))
            .collect::<ArrayVec<_, C>>()
            .into_inner()
        {
            arr
        } else {
            // avoid Debug trait requirement for expect/unwrap
            panic!("arrayvec collection failed somehow")
        }
    }

    /// Adds a part to this input
    #[inline]
    pub fn append_part(&mut self, part: I) {
        self.parts.push(part);
    }

    /// Inserts a part to this input at the given index
    #[inline]
    pub fn insert_part(&mut self, idx: usize, part: I) {
        self.parts.insert(idx, part);
    }

    /// Removes a part from this input at the given index.
    ///
    /// # Safety
    ///
    /// Panics if the index is out of bounds.
    #[inline]
    pub fn remove_part_at_index(&mut self, idx: usize) {
        self.parts.remove(idx);
    }

    /// Removes the last part from this input.
    ///
    /// Returns [`None`] if the input is empty.
    #[inline]
    pub fn pop_part(&mut self) -> Option<I> {
        self.parts.pop()
    }

    /// Get the number of parts in this input.
    #[must_use]
    #[inline]
    pub fn len(&self) -> usize {
        self.parts.len()
    }

    /// Check if this input has no parts.
    #[must_use]
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.parts.is_empty()
    }

    /// Map a tuple of mutators targeting [`ListInput`]'s inner type to a tuple of mutators able to work on the entire [`ListInput`],
    /// by mutating on the last part. If the input is empty, [`MutationResult::Skipped`] is returned.
    #[must_use]
    #[inline]
    pub fn map_to_mutate_on_last_part<M: Map<ToLastEntryMutator>>(
        inner: M,
    ) -> <M as Map<ToLastEntryMutator>>::MapResult {
        inner.map(ToLastEntryMutator)
    }

    /// Map a tuple of mutators targeting [`ListInput`]'s inner type to a tuple of mutators able to work on the entire [`ListInput`],
    /// by mutating on a random part. If the input is empty, [`MutationResult::Skipped`] is returned.
    #[must_use]
    #[inline]
    pub fn map_to_mutate_on_random_part<M: Map<ToRandomEntryMutator>>(
        inner: M,
    ) -> <M as Map<ToRandomEntryMutator>>::MapResult {
        inner.map(ToRandomEntryMutator)
    }
}

impl<I, It> From<It> for ListInput<I>
where
    It: IntoIterator<Item = I>,
{
    fn from(parts: It) -> Self {
        let vec = parts.into_iter().collect();
        Self { parts: vec }
    }
}

impl<I> Input for ListInput<I>
where
    I: Input,
{
    fn generate_name(&self, id: Option<CorpusId>) -> String {
        if self.parts.is_empty() {
            "empty_list_input".to_string() // empty strings cause issues with OnDiskCorpus
        } else {
            self.parts
                .iter()
                .map(|input| input.generate_name(id))
                .collect::<Vec<_>>()
                .join(",")
        }
    }
}

/// Mutator that applies mutations to the last element of a [`ListInput`].
///
///  If the input is empty, [`MutationResult::Skipped`] is returned.
#[derive(Debug)]
pub struct LastEntryMutator<M> {
    inner: M,
    name: Cow<'static, str>,
}

impl<M: Named> LastEntryMutator<M> {
    /// Create a new [`LastEntryMutator`].
    #[must_use]
    pub fn new(inner: M) -> Self {
        let name = Cow::Owned(format!("LastEntryMutator<{}>", inner.name()));
        Self { inner, name }
    }
}

impl<I, M, S> Mutator<ListInput<I>, S> for LastEntryMutator<M>
where
    M: Mutator<I, S>,
{
    fn mutate(&mut self, state: &mut S, input: &mut ListInput<I>) -> Result<MutationResult, Error> {
        match input.parts.len() {
            0 => Ok(MutationResult::Skipped),
            len => self.inner.mutate(state, &mut input.parts[len - 1]),
        }
    }

    fn post_exec(&mut self, state: &mut S, new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        self.inner.post_exec(state, new_corpus_id)
    }
}

/// Mapping functor to convert mutators to [`LastEntryMutator`].
#[derive(Debug)]
pub struct ToLastEntryMutator;

impl<M: Named> MappingFunctor<M> for ToLastEntryMutator {
    type Output = LastEntryMutator<M>;

    fn apply(&mut self, from: M) -> Self::Output {
        LastEntryMutator::new(from)
    }
}

impl<M> Named for LastEntryMutator<M> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

/// Mutator that applies mutations to a random element of a [`ListInput`].
///
///  If the input is empty, [`MutationResult::Skipped`] is returned.
#[derive(Debug)]
pub struct RandomEntryMutator<M> {
    inner: M,
    name: Cow<'static, str>,
}

impl<M: Named> RandomEntryMutator<M> {
    /// Create a new [`RandomEntryMutator`].
    #[must_use]
    pub fn new(inner: M) -> Self {
        let name = Cow::Owned(format!("RandomEntryMutator<{}>", inner.name()));
        Self { inner, name }
    }
}

impl<I, M, S> Mutator<ListInput<I>, S> for RandomEntryMutator<M>
where
    M: Mutator<I, S>,
    S: HasRand,
{
    fn mutate(&mut self, state: &mut S, input: &mut ListInput<I>) -> Result<MutationResult, Error> {
        let rand = state.rand_mut();
        match input.parts.len() {
            0 => Ok(MutationResult::Skipped),
            len => {
                let index = rand.below(unsafe { NonZero::new_unchecked(len) });
                self.inner.mutate(state, &mut input.parts[index])
            }
        }
    }

    fn post_exec(&mut self, state: &mut S, new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        self.inner.post_exec(state, new_corpus_id)
    }
}

/// Mapping functor to convert mutators to [`RandomEntryMutator`].
#[derive(Debug)]
pub struct ToRandomEntryMutator;

impl<M: Named> MappingFunctor<M> for ToRandomEntryMutator {
    type Output = RandomEntryMutator<M>;

    fn apply(&mut self, from: M) -> Self::Output {
        RandomEntryMutator::new(from)
    }
}

impl<M> Named for RandomEntryMutator<M> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

#[cfg(test)]
mod tests {
    use tuple_list::tuple_list;

    use super::ListInput;
    use crate::{
        inputs::ValueInput,
        mutators::{MutationResult, MutatorsTuple as _, numeric::IncMutator},
        state::NopState,
    };

    #[test]
    fn map_to_mutate_on_last_part() {
        let mutator = tuple_list!(IncMutator);
        let mut mapped_mutator = ListInput::<ValueInput<u8>>::map_to_mutate_on_last_part(mutator);
        let mut input: ListInput<ValueInput<u8>> =
            ListInput::from(vec![ValueInput::new(1_u8), ValueInput::new(2)]);
        let mut state = NopState::<ListInput<ValueInput<u8>>>::new();
        let res = mapped_mutator.mutate_all(&mut state, &mut input);
        assert_eq!(res.unwrap(), MutationResult::Mutated);
        assert_eq!(input.parts, vec![ValueInput::new(1), ValueInput::new(3)]);
    }

    #[test]
    fn map_to_mutate_on_last_part_empty() {
        let mutator = tuple_list!(IncMutator);
        let mut mapped_mutator = ListInput::<ValueInput<u8>>::map_to_mutate_on_last_part(mutator);
        let mut input = ListInput::<ValueInput<u8>>::default();
        let mut state = NopState::<ListInput<ValueInput<u8>>>::new();
        let res = mapped_mutator.mutate_all(&mut state, &mut input);
        assert_eq!(res.unwrap(), MutationResult::Skipped);
        assert_eq!(input.parts, vec![]);
    }

    #[test]
    fn map_to_mutate_on_random_part() {
        let mutator = tuple_list!(IncMutator);
        let mut mapped_mutator = ListInput::<ValueInput<u8>>::map_to_mutate_on_random_part(mutator);
        let initial_input = vec![ValueInput::new(1_u8), ValueInput::new(2)];
        let mut input = ListInput::<ValueInput<u8>>::from(initial_input.clone());
        let mut state = NopState::<ListInput<ValueInput<u8>>>::new();
        let res = mapped_mutator.mutate_all(&mut state, &mut input);
        assert_eq!(res.unwrap(), MutationResult::Mutated);
        assert_eq!(
            1,
            input
                .parts
                .iter()
                .zip(initial_input.iter())
                .filter(|&(a, b)| a != b)
                .count()
        );
    }

    #[test]
    fn map_to_mutate_on_random_part_empty() {
        let mutator = tuple_list!(IncMutator);
        let mut mapped_mutator = ListInput::<ValueInput<u8>>::map_to_mutate_on_random_part(mutator);
        let mut input = ListInput::<ValueInput<u8>>::default();
        let mut state = NopState::<ListInput<ValueInput<u8>>>::new();
        let res = mapped_mutator.mutate_all(&mut state, &mut input);
        assert_eq!(res.unwrap(), MutationResult::Skipped);
        assert_eq!(input.parts, vec![]);
    }
}
