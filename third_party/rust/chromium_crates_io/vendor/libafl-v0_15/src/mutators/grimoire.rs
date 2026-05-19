//! Grimoire is the rewritten grimoire mutator in rust.
//! See the original repo [`Grimoire`](https://github.com/RUB-SysSec/grimoire) for more details.

use alloc::{borrow::Cow, vec::Vec};
use core::{
    cmp::{max, min},
    marker::PhantomData,
    num::NonZero,
};

use libafl_bolts::{
    Named,
    rands::{Rand, choose, fast_bound},
};

use crate::{
    Error, HasMetadata,
    corpus::Corpus,
    inputs::{GeneralizedInputMetadata, GeneralizedItem},
    mutators::{MutationResult, Mutator, token_mutations::Tokens},
    random_corpus_id,
    state::{HasCorpus, HasRand},
};

const RECURSIVE_REPLACEMENT_DEPTH: [usize; 6] = [2, 4, 8, 16, 32, 64];
const MAX_RECURSIVE_REPLACEMENT_LEN: usize = 64 << 10;
const CHOOSE_SUBINPUT_PROB: f64 = 0.5;

fn extend_with_random_generalized<I, S>(
    state: &mut S,
    items: &mut Vec<GeneralizedItem>,
    gap_indices: &mut Vec<usize>,
) -> Result<MutationResult, Error>
where
    S: HasMetadata + HasRand + HasCorpus<I>,
{
    let id = random_corpus_id!(state.corpus(), state.rand_mut());

    if state.rand_mut().coinflip(CHOOSE_SUBINPUT_PROB) {
        if state.rand_mut().coinflip(0.5) {
            let rand1 = state.rand_mut().next();
            let rand2 = state.rand_mut().next();

            let other_testcase = state.corpus().get(id)?.borrow();
            if let Some(other) = other_testcase
                .metadata_map()
                .get::<GeneralizedInputMetadata>()
            {
                let generalized = other.generalized();

                for (i, _) in generalized
                    .iter()
                    .enumerate()
                    .filter(|&(_, x)| *x == GeneralizedItem::Gap)
                {
                    gap_indices.push(i);
                }
                let min_idx = *choose(&*gap_indices, rand1).unwrap();
                let max_idx = *choose(&*gap_indices, rand2).unwrap();
                let (mut min_idx, max_idx) = (min(min_idx, max_idx), max(min_idx, max_idx));

                gap_indices.clear();

                if items.last() == Some(&GeneralizedItem::Gap) {
                    min_idx += 1;
                }
                items.extend_from_slice(&generalized[min_idx..=max_idx]);

                debug_assert!(items.first() == Some(&GeneralizedItem::Gap));
                debug_assert!(items.last() == Some(&GeneralizedItem::Gap));

                return Ok(MutationResult::Mutated);
            }
        }

        let rand1 = state.rand_mut().next();

        if let Some(meta) = state.metadata_map().get::<Tokens>() {
            if !meta.tokens().is_empty() {
                let tok = choose(meta.tokens(), rand1).unwrap();
                if items.last() != Some(&GeneralizedItem::Gap) {
                    items.push(GeneralizedItem::Gap);
                }
                items.push(GeneralizedItem::Bytes(tok.clone()));
                items.push(GeneralizedItem::Gap);

                debug_assert!(items.first() == Some(&GeneralizedItem::Gap));
                debug_assert!(items.last() == Some(&GeneralizedItem::Gap));

                return Ok(MutationResult::Mutated);
            }
        }
    }

    let other_testcase = state.corpus().get(id)?.borrow();
    match other_testcase
        .metadata_map()
        .get::<GeneralizedInputMetadata>()
    {
        Some(other) => {
            let generalized = other.generalized();

            if items.last() == Some(&GeneralizedItem::Gap)
                && generalized.first() == Some(&GeneralizedItem::Gap)
            {
                items.extend_from_slice(&generalized[1..]);
            } else {
                items.extend_from_slice(generalized);
            }

            debug_assert!(items.first() == Some(&GeneralizedItem::Gap));
            debug_assert!(items.last() == Some(&GeneralizedItem::Gap));

            Ok(MutationResult::Mutated)
        }
        _ => Ok(MutationResult::Skipped),
    }
}

/// Extend the generalized input with another random one from the corpus
#[derive(Debug, Default)]
pub struct GrimoireExtensionMutator<I> {
    gap_indices: Vec<usize>,
    phantom: PhantomData<I>,
}

impl<I, S> Mutator<GeneralizedInputMetadata, S> for GrimoireExtensionMutator<I>
where
    S: HasMetadata + HasRand + HasCorpus<I>,
{
    fn mutate(
        &mut self,
        state: &mut S,
        generalised_meta: &mut GeneralizedInputMetadata,
    ) -> Result<MutationResult, Error> {
        extend_with_random_generalized(
            state,
            generalised_meta.generalized_mut(),
            &mut self.gap_indices,
        )
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

impl<I> Named for GrimoireExtensionMutator<I> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GrimoireExtensionMutator");
        &NAME
    }
}

impl<I> GrimoireExtensionMutator<I> {
    /// Creates a new [`GrimoireExtensionMutator`].
    #[must_use]
    pub fn new() -> Self {
        Self {
            gap_indices: vec![],
            phantom: PhantomData,
        }
    }
}

/// Extend the generalized input with another random one from the corpus
#[derive(Debug, Default)]
pub struct GrimoireRecursiveReplacementMutator<I> {
    scratch: Vec<GeneralizedItem>,
    gap_indices: Vec<usize>,
    phantom: PhantomData<I>,
}

impl<I, S> Mutator<GeneralizedInputMetadata, S> for GrimoireRecursiveReplacementMutator<I>
where
    S: HasMetadata + HasRand + HasCorpus<I>,
{
    fn mutate(
        &mut self,
        state: &mut S,
        generalised_meta: &mut GeneralizedInputMetadata,
    ) -> Result<MutationResult, Error> {
        let mut mutated = MutationResult::Skipped;

        let depth = *state
            .rand_mut()
            .choose(&RECURSIVE_REPLACEMENT_DEPTH)
            .unwrap();
        for _ in 0..depth {
            if generalised_meta.generalized_len() >= MAX_RECURSIVE_REPLACEMENT_LEN {
                break;
            }

            let generalized = generalised_meta.generalized_mut();

            for (i, _) in generalized
                .iter()
                .enumerate()
                .filter(|&(_, x)| *x == GeneralizedItem::Gap)
            {
                self.gap_indices.push(i);
            }
            if self.gap_indices.is_empty() {
                break;
            }
            let selected = *state.rand_mut().choose(&self.gap_indices).unwrap();
            self.gap_indices.clear();

            self.scratch.extend_from_slice(&generalized[selected + 1..]);
            generalized.truncate(selected);

            if extend_with_random_generalized(state, generalized, &mut self.gap_indices)?
                == MutationResult::Skipped
            {
                generalized.push(GeneralizedItem::Gap);
            }

            generalized.extend_from_slice(&self.scratch);
            self.scratch.clear();

            mutated = MutationResult::Mutated;
        }

        Ok(mutated)
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

impl<I> Named for GrimoireRecursiveReplacementMutator<I> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GrimoireRecursiveReplacementMutator");
        &NAME
    }
}

impl<I> GrimoireRecursiveReplacementMutator<I> {
    /// Creates a new [`GrimoireRecursiveReplacementMutator`].
    #[must_use]
    pub fn new() -> Self {
        Self {
            scratch: vec![],
            gap_indices: vec![],
            phantom: PhantomData,
        }
    }
}

/// Replace matching tokens with others from the tokens metadata
#[derive(Debug, Default)]
pub struct GrimoireStringReplacementMutator<I> {
    phantom: PhantomData<I>,
}

impl<I, S> Mutator<GeneralizedInputMetadata, S> for GrimoireStringReplacementMutator<I>
where
    S: HasMetadata + HasRand + HasCorpus<I>,
{
    fn mutate(
        &mut self,
        state: &mut S,
        generalised_meta: &mut GeneralizedInputMetadata,
    ) -> Result<MutationResult, Error> {
        let tokens_len = {
            let meta = state.metadata_map().get::<Tokens>();
            if let Some(tokens) = meta {
                if let Some(tokens_len) = NonZero::new(tokens.tokens().len()) {
                    tokens_len
                } else {
                    return Ok(MutationResult::Skipped);
                }
            } else {
                return Ok(MutationResult::Skipped);
            }
        };

        let generalized = generalised_meta.generalized_mut();
        let Some(_) = NonZero::new(generalized.len()) else {
            return Err(Error::illegal_state("No generalized metadata found."));
        };

        let token_find = state.rand_mut().below(tokens_len);
        let mut token_replace = state.rand_mut().below(tokens_len);
        if token_find == token_replace {
            token_replace = state.rand_mut().below(tokens_len);
        }

        let stop_at_first = state.rand_mut().coinflip(0.5);
        let rand_idx = state.rand_mut().next();

        let meta = state.metadata_map().get::<Tokens>().unwrap();
        let token_1 = &meta.tokens()[token_find];
        let token_2 = &meta.tokens()[token_replace];

        let mut mutated = MutationResult::Skipped;

        // # Safety
        // gen.len() is positive.
        let rand_idx = fast_bound(rand_idx, unsafe {
            NonZero::new(generalized.len()).unwrap_unchecked()
        });

        'first: for item in &mut generalized[..rand_idx] {
            if let GeneralizedItem::Bytes(bytes) = item {
                let mut i = 0;
                while bytes
                    .len()
                    .checked_sub(token_1.len())
                    .is_some_and(|len| i < len)
                {
                    if bytes[i..].starts_with(token_1) {
                        bytes.splice(i..(i + token_1.len()), token_2.iter().copied());

                        mutated = MutationResult::Mutated;
                        if stop_at_first {
                            break 'first;
                        }
                        i += token_2.len();
                    } else {
                        i += 1;
                    }
                }
            }
        }
        if mutated == MutationResult::Skipped || !stop_at_first {
            'second: for item in &mut generalized[rand_idx..] {
                if let GeneralizedItem::Bytes(bytes) = item {
                    let mut i = 0;
                    while bytes
                        .len()
                        .checked_sub(token_1.len())
                        .is_some_and(|len| i < len)
                    {
                        if bytes[i..].starts_with(token_1) {
                            bytes.splice(i..(i + token_1.len()), token_2.iter().copied());

                            mutated = MutationResult::Mutated;
                            if stop_at_first {
                                break 'second;
                            }
                            i += token_2.len();
                        } else {
                            i += 1;
                        }
                    }
                }
            }
        }

        Ok(mutated)
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

impl<I> Named for GrimoireStringReplacementMutator<I> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GrimoireStringReplacementMutator");
        &NAME
    }
}

impl<I> GrimoireStringReplacementMutator<I> {
    /// Creates a new [`GrimoireExtensionMutator`].
    #[must_use]
    pub fn new() -> Self {
        Self {
            phantom: PhantomData,
        }
    }
}

/// Randomly delete a part of the generalized input
#[derive(Debug, Default)]
pub struct GrimoireRandomDeleteMutator<I> {
    gap_indices: Vec<usize>,
    phantom: PhantomData<I>,
}

impl<I, S> Mutator<GeneralizedInputMetadata, S> for GrimoireRandomDeleteMutator<I>
where
    S: HasMetadata + HasRand + HasCorpus<I>,
{
    fn mutate(
        &mut self,
        state: &mut S,
        generalised_meta: &mut GeneralizedInputMetadata,
    ) -> Result<MutationResult, Error> {
        let generalized = generalised_meta.generalized_mut();

        for i in generalized
            .iter()
            .enumerate()
            .filter_map(|(i, x)| (*x == GeneralizedItem::Gap).then_some(i))
        {
            self.gap_indices.push(i);
        }

        let Some(gap_indeces_len) = NonZero::new(self.gap_indices.len()) else {
            return Err(Error::illegal_state(
                "Gap indices may not be empty in grimoire mutator!",
            ));
        };

        let min_idx = self.gap_indices[state.rand_mut().below(gap_indeces_len)];
        let max_idx = self.gap_indices[state.rand_mut().below(gap_indeces_len)];

        let (min_idx, max_idx) = (min(min_idx, max_idx), max(min_idx, max_idx));

        self.gap_indices.clear();

        let result = if min_idx == max_idx {
            MutationResult::Skipped
        } else {
            generalized.drain(min_idx..max_idx);
            MutationResult::Mutated
        };

        Ok(result)
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

impl<I> Named for GrimoireRandomDeleteMutator<I> {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GrimoireRandomDeleteMutator");
        &NAME
    }
}

impl<I> GrimoireRandomDeleteMutator<I> {
    /// Creates a new [`GrimoireExtensionMutator`].
    #[must_use]
    pub fn new() -> Self {
        Self {
            gap_indices: vec![],
            phantom: PhantomData,
        }
    }
}
