//! A [`MutationalStage`] where the mutator iteration can be tuned at runtime

use alloc::string::{String, ToString};
use core::{marker::PhantomData, time::Duration};

use libafl_bolts::{current_time, impl_serdeany, rands::Rand};
use serde::{Deserialize, Serialize};

#[cfg(feature = "introspection")]
use crate::monitors::stats::PerfFeature;
use crate::{
    Error, Evaluator, HasMetadata, HasNamedMetadata, mark_feature_time,
    mutators::{MutationResult, Mutator},
    nonzero,
    stages::{
        ExecutionCountRestartHelper, MutationalStage, Restartable, Stage,
        mutational::{DEFAULT_MUTATIONAL_MAX_ITERATIONS, MutatedTransform, MutatedTransformPost},
    },
    start_timer,
    state::{HasCurrentTestcase, HasExecutions, HasRand, MaybeHasClientPerfMonitor},
};

#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
#[derive(Default, Copy, Clone, Eq, PartialEq, Debug, Serialize, Deserialize)]
struct TuneableMutationalStageMetadata {
    iters: Option<u64>,
    fuzz_time: Option<Duration>,
}

impl_serdeany!(TuneableMutationalStageMetadata);

/// The default name of the tunenable mutational stage.
pub const STD_TUNEABLE_MUTATIONAL_STAGE_NAME: &str = "TuneableMutationalStage";

/// Set the number of iterations to be used by this mutational stage by name
pub fn set_iters_by_name<S>(state: &mut S, iters: u64, name: &str) -> Result<(), Error>
where
    S: HasNamedMetadata,
{
    let metadata = state
        .named_metadata_map_mut()
        .get_mut::<TuneableMutationalStageMetadata>(name)
        .ok_or_else(|| Error::illegal_state("TuneableMutationalStage not in use"));
    metadata.map(|metadata| {
        metadata.iters = Some(iters);
    })
}

/// Set the number of iterations to be used by this mutational stage with a default name
pub fn set_iters_std<S>(state: &mut S, iters: u64) -> Result<(), Error>
where
    S: HasNamedMetadata,
{
    set_iters_by_name(state, iters, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
}

/// Get the set iterations by name
pub fn get_iters_by_name<S>(state: &S, name: &str) -> Result<Option<u64>, Error>
where
    S: HasNamedMetadata,
{
    state
        .named_metadata_map()
        .get::<TuneableMutationalStageMetadata>(name)
        .ok_or_else(|| Error::illegal_state("TuneableMutationalStage not in use"))
        .map(|metadata| metadata.iters)
}

/// Get the set iterations with a default name
pub fn get_iters_std<S>(state: &S) -> Result<Option<u64>, Error>
where
    S: HasNamedMetadata,
{
    get_iters_by_name(state, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
}

/// Set the time for a single seed to be used by this mutational stage
pub fn set_seed_fuzz_time_by_name<S>(
    state: &mut S,
    fuzz_time: Duration,
    name: &str,
) -> Result<(), Error>
where
    S: HasNamedMetadata,
{
    let metadata = state
        .named_metadata_map_mut()
        .get_mut::<TuneableMutationalStageMetadata>(name)
        .ok_or_else(|| Error::illegal_state("TuneableMutationalStage not in use"));
    metadata.map(|metadata| {
        metadata.fuzz_time = Some(fuzz_time);
    })
}

/// Set the time for a single seed to be used by this mutational stage with a default name
pub fn set_seed_fuzz_time_std<S>(state: &mut S, fuzz_time: Duration) -> Result<(), Error>
where
    S: HasNamedMetadata,
{
    set_seed_fuzz_time_by_name(state, fuzz_time, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
}

/// Get the time for a single seed to be used by this mutational stage by name
pub fn get_seed_fuzz_time_by_name<S>(state: &S, name: &str) -> Result<Option<Duration>, Error>
where
    S: HasNamedMetadata,
{
    state
        .named_metadata_map()
        .get::<TuneableMutationalStageMetadata>(name)
        .ok_or_else(|| Error::illegal_state("TuneableMutationalStage not in use"))
        .map(|metadata| metadata.fuzz_time)
}

/// Get the time for a single seed to be used by this mutational stage with a default name
pub fn get_seed_fuzz_time_std<S>(state: &S) -> Result<Option<Duration>, Error>
where
    S: HasNamedMetadata,
{
    get_seed_fuzz_time_by_name(state, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
}

/// Reset this to a normal, randomized, stage by name
pub fn reset_by_name<S>(state: &mut S, name: &str) -> Result<(), Error>
where
    S: HasNamedMetadata,
{
    state
        .named_metadata_map_mut()
        .get_mut::<TuneableMutationalStageMetadata>(name)
        .ok_or_else(|| Error::illegal_state("TuneableMutationalStage not in use"))
        .map(|metadata| {
            metadata.iters = None;
            metadata.fuzz_time = None;
        })
}

/// Reset this to a normal, randomized, stage with a default name
pub fn reset_std<S>(state: &mut S) -> Result<(), Error>
where
    S: HasNamedMetadata,
{
    reset_by_name(state, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
}

/// A [`MutationalStage`] where the mutator iteration can be tuned at runtime
#[derive(Debug, Clone)]
pub struct TuneableMutationalStage<E, EM, I, M, S, Z> {
    /// The mutator we use
    mutator: M,
    /// The name of this stage
    name: String,
    /// The progress helper we use to keep track of progress across restarts
    restart_helper: ExecutionCountRestartHelper,
    phantom: PhantomData<(E, EM, I, S, Z)>,
}

impl<E, EM, I, M, S, Z> MutationalStage<S> for TuneableMutationalStage<E, EM, I, M, S, Z>
where
    M: Mutator<I, S>,
    Z: Evaluator<E, EM, I, S>,
    S: HasRand + HasNamedMetadata + HasMetadata + HasExecutions + HasCurrentTestcase<I>,
    I: MutatedTransform<I, S> + Clone,
{
    type Mutator = M;
    /// The mutator, added to this stage
    #[inline]
    fn mutator(&self) -> &Self::Mutator {
        &self.mutator
    }

    /// The list of mutators, added to this stage (as mutable ref)
    #[inline]
    fn mutator_mut(&mut self) -> &mut Self::Mutator {
        &mut self.mutator
    }

    /// Gets the number of iterations as a random number
    fn iterations(&self, state: &mut S) -> Result<usize, Error> {
        Ok(
            // fall back to random
            1 + state
                .rand_mut()
                .below(nonzero!(DEFAULT_MUTATIONAL_MAX_ITERATIONS)),
        )
    }
}

impl<E, EM, I, M, S, Z> Stage<E, EM, S, Z> for TuneableMutationalStage<E, EM, I, M, S, Z>
where
    M: Mutator<I, S>,
    Z: Evaluator<E, EM, I, S>,
    S: HasRand
        + HasNamedMetadata
        + HasMetadata
        + HasExecutions
        + HasCurrentTestcase<I>
        + MaybeHasClientPerfMonitor,
    I: MutatedTransform<I, S> + Clone,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        self.perform_mutational(fuzzer, executor, state, manager)
    }
}

impl<E, EM, I, M, S, Z> Restartable<S> for TuneableMutationalStage<E, EM, I, M, S, Z>
where
    S: HasNamedMetadata + HasExecutions,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        self.restart_helper.should_restart(state, &self.name)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        self.restart_helper.clear_progress(state, &self.name)
    }
}

impl<E, EM, I, M, S, Z> TuneableMutationalStage<E, EM, I, M, S, Z>
where
    M: Mutator<I, S>,
    Z: Evaluator<E, EM, I, S>,
    S: HasRand
        + HasNamedMetadata
        + HasExecutions
        + HasMetadata
        + HasCurrentTestcase<I>
        + MaybeHasClientPerfMonitor,
    I: MutatedTransform<I, S> + Clone,
{
    /// Runs this (mutational) stage for the given `testcase`
    /// Exactly the same functionality as [`MutationalStage::perform_mutational`], but with added timeout support.
    fn perform_mutational(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let fuzz_time = self.seed_fuzz_time(state)?;
        let iters = self.fixed_iters(state)?;

        start_timer!(state);
        let mut testcase = state.current_testcase_mut()?;
        let Ok(input) = I::try_transform_from(&mut testcase, state) else {
            return Ok(());
        };
        drop(testcase);
        mark_feature_time!(state, PerfFeature::GetInputFromCorpus);

        match (fuzz_time, iters) {
            (Some(fuzz_time), Some(iters)) => {
                // perform n iterations or fuzz for provided time, whichever comes first
                let start_time = current_time();
                for _ in 1..=iters {
                    if current_time().checked_sub(start_time).unwrap_or(fuzz_time) >= fuzz_time {
                        break;
                    }

                    self.perform_mutation(fuzzer, executor, state, manager, &input)?;
                }
            }
            (Some(fuzz_time), None) => {
                // fuzz for provided time
                let start_time = current_time();
                for _ in 1.. {
                    if current_time().checked_sub(start_time).unwrap_or(fuzz_time) >= fuzz_time {
                        break;
                    }

                    self.perform_mutation(fuzzer, executor, state, manager, &input)?;
                }
            }
            (None, Some(iters)) => {
                // perform n iterations
                for _ in 1..=iters {
                    self.perform_mutation(fuzzer, executor, state, manager, &input)?;
                }
            }
            (None, None) => {
                // fall back to random
                let iters = self
                    .iterations(state)?
                    .saturating_sub(self.execs_since_progress_start(state)? as usize);
                for _ in 1..=iters {
                    self.perform_mutation(fuzzer, executor, state, manager, &input)?;
                }
            }
        }
        Ok(())
    }

    fn execs_since_progress_start(&mut self, state: &mut S) -> Result<u64, Error> {
        self.restart_helper
            .execs_since_progress_start(state, &self.name)
    }

    /// Creates a new default tuneable mutational stage
    #[must_use]
    pub fn new(state: &mut S, mutator: M) -> Self {
        Self::transforming(state, mutator, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
    }

    /// Crates a new tuneable mutational stage with the given name
    pub fn with_name(state: &mut S, mutator: M, name: &str) -> Self {
        Self::transforming(state, mutator, name)
    }

    /// Set the number of iterations to be used by this [`TuneableMutationalStage`]
    pub fn set_iters(&self, state: &mut S, iters: u64) -> Result<(), Error>
    where
        S: HasNamedMetadata,
    {
        set_iters_by_name(state, iters, &self.name)
    }

    /// Set the number of iterations to be used by the std [`TuneableMutationalStage`]
    pub fn set_iters_std(state: &mut S, iters: u64) -> Result<(), Error> {
        set_iters_by_name(state, iters, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
    }

    /// Set the number of iterations to be used by the [`TuneableMutationalStage`] with the given name
    pub fn set_iters_by_name(state: &mut S, iters: u64, name: &str) -> Result<(), Error>
    where
        S: HasNamedMetadata,
    {
        set_iters_by_name(state, iters, name)
    }

    /// Get the set iterations for this [`TuneableMutationalStage`], if any
    pub fn fixed_iters(&self, state: &S) -> Result<Option<u64>, Error>
    where
        S: HasNamedMetadata,
    {
        get_iters_by_name(state, &self.name)
    }

    /// Get the set iterations for the std [`TuneableMutationalStage`], if any
    pub fn iters_std(state: &S) -> Result<Option<u64>, Error> {
        get_iters_by_name(state, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
    }

    /// Get the set iterations for the [`TuneableMutationalStage`] with the given name, if any
    pub fn iters_by_name(state: &S, name: &str) -> Result<Option<u64>, Error>
    where
        S: HasNamedMetadata,
    {
        get_iters_by_name(state, name)
    }

    /// Set the time to mutate a single input in this [`TuneableMutationalStage`]
    pub fn set_seed_fuzz_time(&self, state: &mut S, fuzz_time: Duration) -> Result<(), Error>
    where
        S: HasNamedMetadata,
    {
        set_seed_fuzz_time_by_name(state, fuzz_time, &self.name)
    }

    /// Set the time to mutate a single input in the std [`TuneableMutationalStage`]
    pub fn set_seed_fuzz_time_std(state: &mut S, fuzz_time: Duration) -> Result<(), Error> {
        set_seed_fuzz_time_by_name(state, fuzz_time, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
    }

    /// Set the time to mutate a single input in the [`TuneableMutationalStage`] with the given name
    pub fn set_seed_fuzz_time_by_name(
        state: &mut S,
        fuzz_time: Duration,
        name: &str,
    ) -> Result<(), Error>
    where
        S: HasNamedMetadata,
    {
        set_seed_fuzz_time_by_name(state, fuzz_time, name)
    }

    /// Set the time to mutate a single input in this [`TuneableMutationalStage`]
    pub fn seed_fuzz_time(&self, state: &S) -> Result<Option<Duration>, Error>
    where
        S: HasNamedMetadata,
    {
        get_seed_fuzz_time_by_name(state, &self.name)
    }

    /// Set the time to mutate a single input for the std [`TuneableMutationalStage`]
    pub fn seed_fuzz_time_std(&self, state: &S) -> Result<Option<Duration>, Error> {
        get_seed_fuzz_time_by_name(state, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
    }

    /// Set the time to mutate a single input for the [`TuneableMutationalStage`] with a given name
    pub fn seed_fuzz_time_by_name(&self, state: &S, name: &str) -> Result<Option<Duration>, Error>
    where
        S: HasNamedMetadata,
    {
        get_seed_fuzz_time_by_name(state, name)
    }

    /// Reset this to a normal, randomized, stage with
    pub fn reset(&self, state: &mut S) -> Result<(), Error>
    where
        S: HasNamedMetadata,
    {
        reset_by_name(state, &self.name)
    }

    /// Reset the std stage to a normal, randomized, stage
    pub fn reset_std(state: &mut S) -> Result<(), Error> {
        reset_by_name(state, STD_TUNEABLE_MUTATIONAL_STAGE_NAME)
    }

    /// Reset this to a normal, randomized, stage by name
    pub fn reset_by_name(state: &mut S, name: &str) -> Result<(), Error>
    where
        S: HasNamedMetadata,
    {
        reset_by_name(state, name)
    }

    fn perform_mutation(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
        input: &I,
    ) -> Result<(), Error> {
        let mut input = input.clone();

        start_timer!(state);
        let mutated = self.mutator_mut().mutate(state, &mut input)?;
        mark_feature_time!(state, PerfFeature::Mutate);

        if mutated == MutationResult::Skipped {
            return Ok(());
        }

        let (untransformed, post) = input.try_transform_into(state)?;
        let (_, corpus_id) = fuzzer.evaluate_filtered(state, executor, manager, &untransformed)?;

        start_timer!(state);
        self.mutator_mut().post_exec(state, corpus_id)?;
        post.post_exec(state, corpus_id)?;
        mark_feature_time!(state, PerfFeature::MutatePostExec);

        Ok(())
    }
}

impl<E, EM, I, M, S, Z> TuneableMutationalStage<E, EM, I, M, S, Z>
where
    M: Mutator<I, S>,
    Z: Evaluator<E, EM, I, S>,
    S: HasRand + HasNamedMetadata,
{
    /// Creates a new transforming mutational stage
    #[must_use]
    pub fn transforming(state: &mut S, mutator: M, name: &str) -> Self {
        let _ = state.named_metadata_or_insert_with(name, TuneableMutationalStageMetadata::default);
        Self {
            mutator,
            name: name.to_string(),
            restart_helper: ExecutionCountRestartHelper::default(),
            phantom: PhantomData,
        }
    }
}
