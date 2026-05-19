//! [`GramatronRandomMutator`] is a random mutator using grammar automatons to perform grammar-aware fuzzing.
//!
//! See the original gramatron repo [`Gramatron`](https://github.com/HexHive/Gramatron) for more details.
use alloc::{borrow::Cow, vec::Vec};
use core::{cmp::max, num::NonZero};

use hashbrown::HashMap;
use libafl_bolts::{
    Named,
    rands::{Rand, choose},
};
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata,
    corpus::Corpus,
    generators::GramatronGenerator,
    inputs::{GramatronInput, Terminal},
    mutators::{MutationResult, Mutator},
    nonzero, random_corpus_id,
    state::{HasCorpus, HasRand},
};

const RECUR_THRESHOLD: usize = 5;

/// A random mutator for grammar fuzzing
#[derive(Debug)]
pub struct GramatronRandomMutator<'a, S>
where
    S: HasRand + HasMetadata,
{
    generator: &'a GramatronGenerator<'a, S>,
}

impl<S> Mutator<GramatronInput, S> for GramatronRandomMutator<'_, S>
where
    S: HasRand + HasMetadata,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut GramatronInput,
    ) -> Result<MutationResult, Error> {
        if !input.terminals().is_empty() {
            // # Safety
            // We can assume that the count of terminals + 1 will never wrap around (otherwise it will break somewhere else).
            // So len + 1 is always non-zero.
            let size = state
                .rand_mut()
                .below(unsafe { NonZero::new(input.terminals().len() + 1).unwrap_unchecked() });
            input.terminals_mut().truncate(size);
        }
        if self.generator.append_generated_terminals(input, state) > 0 {
            Ok(MutationResult::Mutated)
        } else {
            Ok(MutationResult::Skipped)
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

impl<S> Named for GramatronRandomMutator<'_, S>
where
    S: HasRand + HasMetadata,
{
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GramatronRandomMutator");
        &NAME
    }
}

impl<'a, S> GramatronRandomMutator<'a, S>
where
    S: HasRand + HasMetadata,
{
    /// Creates a new [`GramatronRandomMutator`].
    #[must_use]
    pub fn new(generator: &'a GramatronGenerator<'a, S>) -> Self {
        Self { generator }
    }
}

/// The metadata used for `gramatron`
#[derive(Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct GramatronIdxMapMetadata {
    /// The map containing a vec for each terminal
    pub map: HashMap<usize, Vec<usize>>,
}

libafl_bolts::impl_serdeany!(GramatronIdxMapMetadata);

impl GramatronIdxMapMetadata {
    /// Creates a new [`struct@GramatronIdxMapMetadata`].
    #[must_use]
    pub fn new(input: &GramatronInput) -> Self {
        let mut map = HashMap::default();
        for i in 0..input.terminals().len() {
            let entry = map.entry(input.terminals()[i].state).or_insert(vec![]);
            (*entry).push(i);
        }
        Self { map }
    }
}

/// A [`Mutator`] that mutates a [`GramatronInput`] by splicing inputs together.
#[derive(Default, Debug)]
pub struct GramatronSpliceMutator;

impl<S> Mutator<GramatronInput, S> for GramatronSpliceMutator
where
    S: HasRand + HasCorpus<GramatronInput> + HasMetadata,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut GramatronInput,
    ) -> Result<MutationResult, Error> {
        let Some(terminals_len) = NonZero::new(input.terminals().len()) else {
            return Ok(MutationResult::Skipped);
        };

        let id = random_corpus_id!(state.corpus(), state.rand_mut());

        let insert_at = state.rand_mut().below(terminals_len);

        let rand_num = state.rand_mut().next();

        let mut other_testcase = state.corpus().get(id)?.borrow_mut();

        if !other_testcase.has_metadata::<GramatronIdxMapMetadata>() {
            let meta = GramatronIdxMapMetadata::new(other_testcase.load_input(state.corpus())?);
            other_testcase.add_metadata(meta);
        }
        let meta = other_testcase
            .metadata_map()
            .get::<GramatronIdxMapMetadata>()
            .unwrap();
        let other = other_testcase.input().as_ref().unwrap();

        meta.map.get(&input.terminals()[insert_at].state).map_or(
            Ok(MutationResult::Skipped),
            |splice_points| {
                let from = if let Some(from) = choose(splice_points, rand_num) {
                    *from
                } else {
                    return Ok(MutationResult::Skipped);
                };

                input.terminals_mut().truncate(insert_at);
                input
                    .terminals_mut()
                    .extend_from_slice(&other.terminals()[from..]);

                Ok(MutationResult::Mutated)
            },
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

impl Named for GramatronSpliceMutator {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GramatronSpliceMutator");
        &NAME
    }
}

impl GramatronSpliceMutator {
    /// Creates a new [`GramatronSpliceMutator`].
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

/// A mutator that uses Gramatron for grammar fuzzing and mutation.
#[derive(Default, Debug)]
pub struct GramatronRecursionMutator {
    counters: HashMap<usize, (usize, usize, usize)>,
    states: Vec<usize>,
    suffix: Vec<Terminal>,
    feature: Vec<Terminal>,
}

impl<S> Mutator<GramatronInput, S> for GramatronRecursionMutator
where
    S: HasRand + HasMetadata,
{
    fn mutate(
        &mut self,
        state: &mut S,
        input: &mut GramatronInput,
    ) -> Result<MutationResult, Error> {
        if input.terminals().is_empty() {
            return Ok(MutationResult::Skipped);
        }

        self.counters.clear();
        self.states.clear();
        for i in 0..input.terminals().len() {
            let s = input.terminals()[i].state;
            match self.counters.get_mut(&s) {
                Some(entry) => {
                    if entry.0 == 1 {
                        // Keep track only of states with more than one node
                        self.states.push(s);
                    }
                    entry.0 += 1;
                    entry.2 = max(entry.2, i);
                }
                _ => {
                    self.counters.insert(s, (1, i, i));
                }
            }
        }

        if self.states.is_empty() {
            return Ok(MutationResult::Skipped);
        }

        let chosen = *state.rand_mut().choose(&self.states).unwrap();
        let chosen_nums = self.counters.get(&chosen).unwrap().0;

        let Some(minus_one) = NonZero::new(chosen_nums - 1) else {
            return Ok(MutationResult::Skipped);
        };

        let first = state.rand_mut().below(minus_one);
        let second = state.rand_mut().between(first + 1, chosen_nums - 1);

        let mut first: isize = first.try_into().unwrap();
        let mut second: isize = second.try_into().unwrap();

        let mut idx_1 = 0;
        let mut idx_2 = 0;
        for i in (self.counters.get(&chosen).unwrap().1)..=(self.counters.get(&chosen).unwrap().2) {
            if input.terminals()[i].state == chosen {
                if first == 0 {
                    idx_1 = i;
                }
                if second == 0 {
                    idx_2 = i;
                    break;
                }
                first -= 1;
                second -= 1;
            }
        }
        debug_assert!(idx_1 < idx_2);

        self.suffix.clear();
        self.suffix.extend_from_slice(&input.terminals()[idx_2..]);

        self.feature.clear();
        self.feature
            .extend_from_slice(&input.terminals()[idx_1..idx_2]);

        input.terminals_mut().truncate(idx_1);

        for _ in 0..state.rand_mut().below(nonzero!(RECUR_THRESHOLD)) {
            input.terminals_mut().extend_from_slice(&self.feature);
        }

        input.terminals_mut().extend_from_slice(&self.suffix);

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

impl Named for GramatronRecursionMutator {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("GramatronRecursionMutator");
        &NAME
    }
}

impl GramatronRecursionMutator {
    /// Creates a new [`GramatronRecursionMutator`].
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }
}
