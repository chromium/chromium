//! Mutator definitions for [`MultipartInput`]s. See [`crate::inputs::multi`] for details.

use core::{
    cmp::{Ordering, min},
    num::NonZero,
};

use libafl_bolts::{Error, rands::Rand};

use crate::{
    corpus::{Corpus, CorpusId},
    impl_default_multipart,
    inputs::{HasMutatorBytes, Input, Keyed as _, ResizableMutator, multi::MultipartInput},
    mutators::{
        MutationResult, Mutator,
        mutations::{
            BitFlipMutator, ByteAddMutator, ByteDecMutator, ByteFlipMutator, ByteIncMutator,
            ByteInterestingMutator, ByteNegMutator, ByteRandMutator, BytesCopyMutator,
            BytesDeleteMutator, BytesExpandMutator, BytesInsertCopyMutator, BytesInsertMutator,
            BytesRandInsertMutator, BytesRandSetMutator, BytesSetMutator, BytesSwapMutator,
            CrossoverInsertMutator as BytesInputCrossoverInsertMutator,
            CrossoverReplaceMutator as BytesInputCrossoverReplaceMutator, DwordAddMutator,
            DwordInterestingMutator, QwordAddMutator, WordAddMutator, WordInterestingMutator,
            rand_range,
        },
        token_mutations::{I2SRandReplace, TokenInsert, TokenReplace},
    },
    random_corpus_id,
    state::{HasCorpus, HasMaxSize, HasRand},
};

/// Marker trait for if the default multipart input mutator implementation is appropriate.
///
/// You should implement this type for your mutator if you just want a random part of the input to
/// be selected and mutated. Use [`impl_default_multipart`] to implement this marker trait for many
/// at once.
pub trait DefaultMultipartMutator {}

impl<I, K, M, S> Mutator<MultipartInput<I, K>, S> for M
where
    M: DefaultMultipartMutator + Mutator<I, S>,
    S: HasRand,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        match NonZero::new(input.len()) {
            None => Ok(MutationResult::Skipped),
            Some(len) => {
                let idx = state.rand_mut().below(len);
                let (_key, part) = &mut input.parts_mut()[idx];
                self.mutate(state, part)
            }
        }
    }

    fn post_exec(&mut self, state: &mut S, new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        M::post_exec(self, state, new_corpus_id)
    }
}

mod macros {
    /// Implements the marker trait [`super::DefaultMultipartMutator`] for one to many types, e.g.:
    ///
    /// ```rs
    /// impl_default_multipart!(
    ///     // --- havoc ---
    ///     BitFlipMutator,
    ///     ByteAddMutator,
    ///     ByteDecMutator,
    ///     ByteFlipMutator,
    ///     ByteIncMutator,
    ///     ...
    /// );
    /// ```
    #[macro_export]
    macro_rules! impl_default_multipart {
        ($mutator: ty, $($mutators: ty),+$(,)?) => {
            impl $crate::mutators::multi::DefaultMultipartMutator for $mutator {}
            impl_default_multipart!($($mutators),+);
        };

        ($mutator: ty) => {
            impl $crate::mutators::multi::DefaultMultipartMutator for $mutator {}
        };
    }
}

impl_default_multipart!(
    // --- havoc ---
    BitFlipMutator,
    ByteAddMutator,
    ByteDecMutator,
    ByteFlipMutator,
    ByteIncMutator,
    ByteInterestingMutator,
    ByteNegMutator,
    ByteRandMutator,
    BytesCopyMutator,
    BytesDeleteMutator,
    BytesExpandMutator,
    BytesInsertCopyMutator,
    BytesInsertMutator,
    BytesRandInsertMutator,
    BytesRandSetMutator,
    BytesSetMutator,
    BytesSwapMutator,
    // crossover has a custom implementation below
    DwordAddMutator,
    DwordInterestingMutator,
    QwordAddMutator,
    WordAddMutator,
    WordInterestingMutator,
    // --- token ---
    TokenInsert,
    TokenReplace,
    // ---  i2s  ---
    I2SRandReplace,
);

impl<I, K, S> Mutator<MultipartInput<I, K>, S> for BytesInputCrossoverInsertMutator
where
    S: HasCorpus<MultipartInput<I, K>> + HasMaxSize + HasRand,
    I: Input + ResizableMutator<u8> + HasMutatorBytes,
    K: Clone + PartialEq,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        // we can eat the slight bias; number of parts will be small
        let key_choice = state.rand_mut().next() as usize;
        let part_choice = state.rand_mut().next() as usize;

        // We special-case crossover with self
        let id = random_corpus_id!(state.corpus(), state.rand_mut());
        if let Some(cur) = state.corpus().current() {
            if id == *cur {
                let len = input.len();
                if len == 0 {
                    return Ok(MutationResult::Skipped);
                }
                let choice = key_choice % len;
                // Safety: len is checked above
                let (key, part) = &input.parts()[choice];

                let other_size = part.mutator_bytes().len();

                if other_size < 2 {
                    return Ok(MutationResult::Skipped);
                }

                let parts = input.with_key(key).count() - 1;

                if parts == 0 {
                    return Ok(MutationResult::Skipped);
                }

                let maybe_size = input
                    .with_key(key)
                    .filter(|&(p, _)| p != choice)
                    .nth(part_choice % parts)
                    .map(|(id, part)| (id, part.mutator_bytes().len()));

                if let Some((part_idx, size)) = maybe_size {
                    let Some(nz) = NonZero::new(size) else {
                        return Ok(MutationResult::Skipped);
                    };
                    let target = state.rand_mut().below(nz);
                    // # Safety
                    // size is nonzero here (checked above), target is smaller than size
                    // -> the subtraction result is greater than 0.
                    // other_size is checked above to be larger than zero.
                    let range = rand_range(state, other_size, unsafe {
                        NonZero::new(min(other_size, size - target)).unwrap_unchecked()
                    });

                    let [part, chosen] = match part_idx.cmp(&choice) {
                        Ordering::Less => input.parts_at_indices_mut([part_idx, choice]),
                        Ordering::Equal => {
                            unreachable!("choice should never equal the part idx!")
                        }
                        Ordering::Greater => {
                            let [chosen, part] = input.parts_at_indices_mut([choice, part_idx]);
                            [part, chosen]
                        }
                    };

                    return Ok(Self::crossover_insert(
                        &mut part.1,
                        size,
                        target,
                        range,
                        chosen.1.mutator_bytes(),
                    ));
                }

                return Ok(MutationResult::Skipped);
            }
        }

        let mut other_testcase = state.corpus().get(id)?.borrow_mut();
        let other = other_testcase.load_input(state.corpus())?;
        let other_len = other.len();
        if other_len == 0 {
            return Ok(MutationResult::Skipped);
        }

        let choice = key_choice % other_len;
        // Safety: choice is checked above
        let (key, part) = &other.parts()[choice];

        let other_size = part.mutator_bytes().len();
        if other_size < 2 {
            return Ok(MutationResult::Skipped);
        }

        let parts = input.with_key(key).count();

        if parts > 0 {
            let (_, part) = input.with_key_mut(key).nth(part_choice % parts).unwrap();
            drop(other_testcase);
            let size = part.mutator_bytes().len();
            let Some(nz) = NonZero::new(size) else {
                return Ok(MutationResult::Skipped);
            };

            let target = state.rand_mut().below(nz);
            // # Safety
            // other_size is larger than 0, checked above.
            // size is larger than 0.
            // target is smaller than size -> the subtraction is larger than 0.
            let range = rand_range(state, other_size, unsafe {
                NonZero::new_unchecked(min(other_size, size - target))
            });

            let other_testcase = state.corpus().get(id)?.borrow_mut();
            // No need to load the input again, it'll still be cached.
            let other = other_testcase.input().as_ref().unwrap();

            Ok(Self::crossover_insert(
                part,
                size,
                target,
                range,
                other.part_at_index(choice).unwrap().1.mutator_bytes(),
            ))
        } else {
            // just add it!
            input.append_part(other.part_at_index(choice).unwrap().clone());

            Ok(MutationResult::Mutated)
        }
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl<I, K, S> Mutator<MultipartInput<I, K>, S> for BytesInputCrossoverReplaceMutator
where
    S: HasCorpus<MultipartInput<I, K>> + HasMaxSize + HasRand,
    I: Input + ResizableMutator<u8> + HasMutatorBytes,
    K: Clone + PartialEq,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut MultipartInput<I, K>,
    ) -> Result<MutationResult, Error> {
        // we can eat the slight bias; number of parts will be small
        let key_choice = state.rand_mut().next() as usize;
        let part_choice = state.rand_mut().next() as usize;

        // We special-case crossover with self
        let id = random_corpus_id!(state.corpus(), state.rand_mut());
        if let Some(cur) = state.corpus().current() {
            if id == *cur {
                let len = input.len();
                if len == 0 {
                    return Ok(MutationResult::Skipped);
                }
                let choice = key_choice % len;
                // Safety: len is checked above
                let (key, part) = &input.parts()[choice];

                let other_size = part.mutator_bytes().len();
                if other_size < 2 {
                    return Ok(MutationResult::Skipped);
                }

                let parts = input.with_key(key).count() - 1;

                if parts == 0 {
                    return Ok(MutationResult::Skipped);
                }

                let maybe_size = input
                    .with_key(key)
                    .filter(|&(p, _)| p != choice)
                    .nth(part_choice % parts)
                    .map(|(id, part)| (id, part.mutator_bytes().len()));

                if let Some((part_idx, size)) = maybe_size {
                    let Some(nz) = NonZero::new(size) else {
                        return Ok(MutationResult::Skipped);
                    };

                    let target = state.rand_mut().below(nz);
                    // # Safety
                    // other_size is checked above.
                    // size is larger than than target and larger than 1. The subtraction result will always be positive.
                    let range = rand_range(state, other_size, unsafe {
                        NonZero::new_unchecked(min(other_size, size - target))
                    });

                    let [part, chosen] = match part_idx.cmp(&choice) {
                        Ordering::Less => input.parts_at_indices_mut([part_idx, choice]),
                        Ordering::Equal => {
                            unreachable!("choice should never equal the part idx!")
                        }
                        Ordering::Greater => {
                            let [chosen, part] = input.parts_at_indices_mut([choice, part_idx]);
                            [part, chosen]
                        }
                    };

                    return Ok(Self::crossover_replace(
                        &mut part.1,
                        target,
                        range,
                        chosen.1.mutator_bytes(),
                    ));
                }

                return Ok(MutationResult::Skipped);
            }
        }

        let mut other_testcase = state.corpus().get(id)?.borrow_mut();
        let other = other_testcase.load_input(state.corpus())?;

        let other_len = other.len();
        if other_len == 0 {
            return Ok(MutationResult::Skipped);
        }

        let choice = key_choice % other_len;
        // Safety: choice is checked above
        let (key, part) = &other.parts()[choice];

        let other_size = part.mutator_bytes().len();
        if other_size < 2 {
            return Ok(MutationResult::Skipped);
        }

        let parts = input.with_key(key).count();

        if parts > 0 {
            let (_, part) = input.with_key_mut(key).nth(part_choice % parts).unwrap();
            drop(other_testcase);
            let size = part.mutator_bytes().len();
            let Some(nz) = NonZero::new(size) else {
                return Ok(MutationResult::Skipped);
            };

            let target = state.rand_mut().below(nz);
            // # Safety
            // other_size is checked above.
            // size is larger than than target and larger than 1. The subtraction result will always be positive.
            let range = rand_range(state, other_size, unsafe {
                NonZero::new_unchecked(min(other_size, size - target))
            });

            let other_testcase = state.corpus().get(id)?.borrow_mut();
            // No need to load the input again, it'll still be cached.
            let other = other_testcase.input().as_ref().unwrap();

            Ok(Self::crossover_replace(
                part,
                target,
                range,
                other.part_at_index(choice).unwrap().1.mutator_bytes(),
            ))
        } else {
            // just add it!
            input.append_part(other.part_at_index(choice).unwrap().clone());

            Ok(MutationResult::Mutated)
        }
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}
