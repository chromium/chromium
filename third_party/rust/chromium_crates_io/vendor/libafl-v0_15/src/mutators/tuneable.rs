//! An extension to the `ScheduledMutator` which schedules multiple mutations internally.
//!
//! Instead of a random mutator for a random amount of iterations, we can run
//! a specific mutator for a specified amount of iterations

use alloc::{borrow::Cow, vec::Vec};
use core::{fmt::Debug, num::NonZero};

use libafl_bolts::{
    Named, impl_serdeany, math::calculate_cumulative_distribution_in_place, rands::Rand,
    tuples::NamedTuple,
};
use serde::{Deserialize, Serialize};

pub use crate::mutators::{mutations::*, token_mutations::*};
use crate::{
    Error, HasMetadata,
    mutators::{
        ComposedByMutations, MutationId, MutationResult, Mutator, MutatorsTuple, ScheduledMutator,
    },
    state::HasRand,
};

/// Metadata in the state, that controls the behavior of the [`TuneableScheduledMutator`] at runtime
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct TuneableScheduledMutatorMetadata {
    /// The offsets of mutators to run, in order. Clear to fall back to random,
    /// or use `mutation_probabilities`
    pub mutation_ids: Vec<MutationId>,
    /// The next index to read from in the `next` vec
    pub next_id: MutationId,
    /// The cumulative probability distribution for each mutation.
    /// Will not be used when `mutation_ids` are set.
    /// Clear to fall back to random.
    pub mutation_probabilities_cumulative: Vec<f32>,
    /// The count of mutations to stack.
    /// If `mutation_ids` is of length `10`, and this number is `20`,
    /// the mutations will be iterated through twice.
    pub iters: Option<u64>,
    /// The probability of each number of mutations to stack.
    pub iter_probabilities_pow_cumulative: Vec<f32>,
}

impl_serdeany!(TuneableScheduledMutatorMetadata);

impl Default for TuneableScheduledMutatorMetadata {
    fn default() -> Self {
        Self {
            mutation_ids: Vec::default(),
            next_id: 0.into(),
            mutation_probabilities_cumulative: Vec::default(),
            iters: None,
            iter_probabilities_pow_cumulative: Vec::default(),
        }
    }
}

impl TuneableScheduledMutatorMetadata {
    /// Gets the stored metadata, used to alter the [`TuneableScheduledMutator`] behavior
    pub fn get<S: HasMetadata>(state: &S) -> Result<&Self, Error> {
        state
            .metadata_map()
            .get::<Self>()
            .ok_or_else(|| Error::illegal_state("TuneableScheduledMutator not in use"))
    }

    /// Gets the stored metadata, used to alter the [`TuneableScheduledMutator`] behavior, mut
    pub fn get_mut<S: HasMetadata>(state: &mut S) -> Result<&mut Self, Error> {
        state
            .metadata_map_mut()
            .get_mut::<Self>()
            .ok_or_else(|| Error::illegal_state("TuneableScheduledMutator not in use"))
    }
}

/// A [`Mutator`] that schedules one of the embedded mutations on each call.
/// The index of the next mutation can be set.
#[derive(Debug)]
pub struct TuneableScheduledMutator<MT> {
    name: Cow<'static, str>,
    mutations: MT,
    max_stack_pow: usize,
}

impl<I, MT, S> Mutator<I, S> for TuneableScheduledMutator<MT>
where
    MT: MutatorsTuple<I, S>,
    S: HasRand + HasMetadata,
{
    #[inline]
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        self.scheduled_mutate(state, input)
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

impl<MT> ComposedByMutations for TuneableScheduledMutator<MT> {
    type Mutations = MT;
    /// Get the mutations
    #[inline]
    fn mutations(&self) -> &MT {
        &self.mutations
    }

    // Get the mutations (mutable)
    #[inline]
    fn mutations_mut(&mut self) -> &mut MT {
        &mut self.mutations
    }
}

impl<MT> Named for TuneableScheduledMutator<MT> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<I, MT, S> ScheduledMutator<I, S> for TuneableScheduledMutator<MT>
where
    MT: MutatorsTuple<I, S>,
    S: HasRand + HasMetadata,
{
    /// Compute the number of iterations used to apply stacked mutations
    fn iterations(&self, state: &mut S, _: &I) -> u64 {
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();

        if metadata.iter_probabilities_pow_cumulative.is_empty() {
            if let Some(iters) = metadata.iters {
                iters
            } else {
                // fall back to random
                1 << (1 + state.rand_mut().below_or_zero(self.max_stack_pow))
            }
        } else {
            // We will sample using the mutation probabilities.
            // Doing this outside of the original if branch to make the borrow checker happy.
            let coin = state.rand_mut().next_float() as f32;

            let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
            let power = metadata
                .iter_probabilities_pow_cumulative
                .iter()
                .position(|i| *i >= coin)
                .unwrap();

            1 << (1 + power)
        }
    }

    /// Get the next mutation to apply
    fn schedule(&self, state: &mut S, _: &I) -> MutationId {
        // Assumption: we can not reach this code path without previously adding this metadatum.
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();

        if !metadata.mutation_ids.is_empty() {
            // using pre-set ids.
            let ret = metadata.mutation_ids[metadata.next_id.0];
            metadata.next_id.0 += 1_usize;
            if metadata.next_id.0 >= metadata.mutation_ids.len() {
                metadata.next_id = 0.into();
            }
            debug_assert!(
                self.mutations.len() > ret.0,
                "TuneableScheduler: next vec may not contain id larger than number of mutations!"
            );
            return ret;
        }

        if !metadata.mutation_probabilities_cumulative.is_empty() {
            // We will sample using the mutation probabilities.
            // Doing this outside of the original if branch to make the borrow checker happy.
            let coin = state.rand_mut().next_float() as f32;

            let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
            debug_assert_eq!(
                self.mutations.len(),
                metadata.mutation_probabilities_cumulative.len(),
                "TuneableScheduler: mutation probabilities do not match with number of mutations"
            );

            let mutation_id = metadata
                .mutation_probabilities_cumulative
                .iter()
                .position(|i| *i >= coin)
                .unwrap()
                .into();

            return mutation_id;
        }

        // fall back to random if no entries in either vec, the scheduling is not tuned.
        state
            .rand_mut()
            .below(NonZero::new(self.mutations.len()).expect("No mutations provided!"))
            .into()
    }
}

impl<MT> TuneableScheduledMutator<MT> {
    /// Create a new [`TuneableScheduledMutator`] instance specifying mutations
    pub fn new<S>(state: &mut S, mutations: MT) -> Self
    where
        MT: NamedTuple,
        S: HasRand + HasMetadata,
    {
        if !state.has_metadata::<TuneableScheduledMutatorMetadata>() {
            state.add_metadata(TuneableScheduledMutatorMetadata::default());
        }
        TuneableScheduledMutator {
            name: Cow::from(format!("TuneableMutator[{}]", mutations.names().join(", "))),
            mutations,
            max_stack_pow: 7,
        }
    }
}

impl<MT> TuneableScheduledMutator<MT> {
    /// Sets the next iterations count, i.e., how many times to mutate the input
    ///
    /// Using `set_mutation_ids_and_iter` to set multiple values at the same time
    /// will be faster than setting them individually
    /// as it internally only needs a single metadata lookup
    pub fn set_iters<S>(&self, state: &mut S, iters: u64)
    where
        S: HasMetadata,
    {
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();

        metadata.iters = Some(iters);
        metadata.iter_probabilities_pow_cumulative.clear();
    }

    /// Sets the probability of next iteration counts,
    /// i.e., how many times the mutation is likely to get mutated.
    ///
    /// So, setting the `iter_probabilities` to `vec![0.1, 0.7, 0.2]`
    /// would apply 2^1 mutation with the likelihood of 10%, 2^2 mutations with the
    /// a probability of 70% (0.7), and 2^3 mutations with the likelihood of 20%.
    /// These will be applied for each call of this `mutate` function.
    ///
    /// Setting this function will unset everything previously set in `set_iters`.
    pub fn set_iter_probabilities_pow<S>(
        &self,
        state: &mut S,
        mut iter_probabilities_pow: Vec<f32>,
    ) -> Result<(), Error>
    where
        S: HasMetadata,
    {
        if iter_probabilities_pow.len() >= 32 {
            return Err(Error::illegal_argument(
                "Cannot stack more than 2^32 mutations",
            ));
        }
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
        metadata.iters = None;

        // we precalculate the cumulative probability to be faster when sampling later.
        calculate_cumulative_distribution_in_place(&mut iter_probabilities_pow)?;
        metadata.iter_probabilities_pow_cumulative = iter_probabilities_pow;

        Ok(())
    }

    /// Gets the set amount of iterations
    pub fn get_iters<S>(&self, state: &S) -> Option<u64>
    where
        S: HasMetadata,
    {
        let metadata = TuneableScheduledMutatorMetadata::get(state).unwrap();
        metadata.iters
    }

    /// Sets the mutation ids
    pub fn set_mutation_ids<S>(&self, state: &mut S, mutations: Vec<MutationId>)
    where
        S: HasMetadata,
    {
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
        metadata.mutation_ids = mutations;
        metadata.next_id = 0.into();
    }

    /// Sets the mutation probabilities.
    /// The `Vec` contains a probability per [`MutationId`]: between 0 and 1, and they have to add
    /// up to 1.
    /// Setting the probabilities will remove the value set through `set_mutation_ids`.
    pub fn set_mutation_probabilities<S>(
        &self,
        state: &mut S,
        mut mutation_probabilities: Vec<f32>,
    ) -> Result<(), Error>
    where
        S: HasMetadata,
    {
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
        metadata.mutation_ids.clear();
        metadata.next_id = 0.into();

        // we precalculate the cumulative probability to be faster when sampling later.
        calculate_cumulative_distribution_in_place(&mut mutation_probabilities)?;
        metadata.mutation_probabilities_cumulative = mutation_probabilities;
        Ok(())
    }

    /// mutation ids and iterations
    pub fn set_mutation_ids_and_iters<S>(
        &self,
        state: &mut S,
        mutations: Vec<MutationId>,
        iters: u64,
    ) where
        S: HasMetadata,
    {
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
        metadata.mutation_ids = mutations;
        metadata.next_id = 0.into();
        metadata.iters = Some(iters);
    }

    /// Appends a mutation id to the end of the mutations
    pub fn push_mutation_id<S>(state: &mut S, mutation_id: MutationId)
    where
        S: HasMetadata,
    {
        let metadata = TuneableScheduledMutatorMetadata::get_mut(state).unwrap();
        metadata.mutation_ids.push(mutation_id);
    }

    /// Resets this to a randomic mutational stage
    pub fn reset<S>(self, state: &mut S)
    where
        S: HasMetadata,
    {
        let metadata = state
            .metadata_map_mut()
            .get_mut::<TuneableScheduledMutatorMetadata>()
            .unwrap();
        metadata.mutation_ids.clear();
        metadata.next_id = 0.into();
        metadata.iters = None;
        metadata.mutation_probabilities_cumulative.clear();
        metadata.iter_probabilities_pow_cumulative.clear();
    }
}

#[cfg(test)]
mod test {
    use libafl_bolts::tuples::tuple_list;

    use super::{
        BitFlipMutator, ByteDecMutator, TuneableScheduledMutator, TuneableScheduledMutatorMetadata,
    };
    use crate::{
        inputs::BytesInput,
        mutators::{ByteRandMutator, ScheduledMutator},
        state::NopState,
    };

    #[test]
    fn test_tuning() {
        // # Safety
        // No concurrency per testcase
        #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
        unsafe {
            TuneableScheduledMutatorMetadata::register();
        }

        let mut state: NopState<BytesInput> = NopState::new();
        let mutators = tuple_list!(
            BitFlipMutator::new(),
            ByteDecMutator::new(),
            ByteRandMutator::new()
        );
        let tuneable = TuneableScheduledMutator::new(&mut state, mutators);
        let input = BytesInput::new(vec![42]);
        let metadata = TuneableScheduledMutatorMetadata::get_mut(&mut state).unwrap();
        metadata.mutation_ids.push(1.into());
        metadata.mutation_ids.push(2.into());
        assert_eq!(tuneable.schedule(&mut state, &input), 1.into());
        assert_eq!(tuneable.schedule(&mut state, &input), 2.into());
        assert_eq!(tuneable.schedule(&mut state, &input), 1.into());
    }

    #[test]
    fn test_mutation_distribution() {
        // # Safety
        // No concurrency per testcase
        #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
        unsafe {
            TuneableScheduledMutatorMetadata::register();
        }

        let mut state: NopState<BytesInput> = NopState::new();
        let mutators = tuple_list!(
            BitFlipMutator::new(),
            ByteDecMutator::new(),
            ByteRandMutator::new()
        );
        let tuneable = TuneableScheduledMutator::new(&mut state, mutators);
        let input = BytesInput::new(vec![42]);

        // Basic tests over the probability distribution.
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![0.0])
                .is_err()
        );
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![1.0; 3])
                .is_err()
        );
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![-1.0, 1.0, 1.0])
                .is_err()
        );
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![])
                .is_err()
        );

        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![0.0, 0.0, 1.0])
                .is_ok()
        );
        assert_eq!(tuneable.schedule(&mut state, &input), 2.into());
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![0.0, 1.0, 0.0])
                .is_ok()
        );
        assert_eq!(tuneable.schedule(&mut state, &input), 1.into());
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![1.0, 0.0, 0.0])
                .is_ok()
        );
        assert_eq!(tuneable.schedule(&mut state, &input), 0.into());

        // We should not choose a mutation with p=0.
        assert!(
            tuneable
                .set_mutation_probabilities(&mut state, vec![0.5, 0.0, 0.5])
                .is_ok()
        );
        assert!(tuneable.schedule(&mut state, &input) != 1.into());
    }
}
