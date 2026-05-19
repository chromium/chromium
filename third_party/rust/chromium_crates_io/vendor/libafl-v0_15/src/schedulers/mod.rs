//! Schedule the access to the Corpus.

use alloc::{borrow::ToOwned, string::ToString};
use core::{hash::Hash, marker::PhantomData};

pub mod testcase_score;
pub use testcase_score::{LenTimeMulTestcasePenalty, TestcasePenalty, TestcaseScore};

pub mod queue;
pub use queue::QueueScheduler;

pub mod minimizer;
pub use minimizer::{
    IndexesLenTimeMinimizerScheduler, LenTimeMinimizerScheduler, MinimizerScheduler,
};

pub mod powersched;
pub use powersched::{PowerQueueScheduler, SchedulerMetadata};

pub mod probabilistic_sampling;
pub use probabilistic_sampling::ProbabilitySamplingScheduler;

pub mod accounting;
pub use accounting::CoverageAccountingScheduler;

pub mod weighted;
pub use weighted::{StdWeightedScheduler, WeightedScheduler};

pub mod tuneable;
use libafl_bolts::{
    generic_hash_std,
    rands::Rand,
    tuples::{Handle, MatchName, MatchNameRef},
};
pub use tuneable::*;

use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId, HasTestcase, SchedulerTestcaseMetadata, Testcase},
    random_corpus_id,
    state::{HasCorpus, HasRand},
};

/// The scheduler also implements `on_remove` and `on_replace` if it implements this stage.
pub trait RemovableScheduler<I, S> {
    /// Removed the given entry from the corpus at the given index
    /// When you remove testcases, make sure that that testcase is not currently fuzzed one!
    fn on_remove(
        &mut self,
        _state: &mut S,
        _id: CorpusId,
        _testcase: &Option<Testcase<I>>,
    ) -> Result<(), Error> {
        Ok(())
    }

    /// Replaced the given testcase at the given idx
    fn on_replace(
        &mut self,
        _state: &mut S,
        _id: CorpusId,
        _prev: &Testcase<I>,
    ) -> Result<(), Error> {
        Ok(())
    }
}

/// Called when a [`Testcase`] is evaluated
pub fn on_add_metadata_default<CS, I, S>(
    scheduler: &mut CS,
    state: &mut S,
    id: CorpusId,
) -> Result<(), Error>
where
    CS: AflScheduler,
    S: HasTestcase<I> + HasCorpus<I>,
{
    let current_id = *state.corpus().current();

    let mut depth = match current_id {
        Some(parent_idx) => state
            .testcase(parent_idx)?
            .metadata::<SchedulerTestcaseMetadata>()?
            .depth(),
        None => 0,
    };

    // TODO increase perf_score when finding new things like in AFL
    // https://github.com/google/AFL/blob/master/afl-fuzz.c#L6547

    // Attach a `SchedulerTestcaseMetadata` to the queue entry.
    depth += 1;
    let mut testcase = state.testcase_mut(id)?;
    testcase.add_metadata(SchedulerTestcaseMetadata::with_n_fuzz_entry(
        depth,
        scheduler.last_hash(),
    ));
    testcase.set_parent_id_optional(current_id);
    Ok(())
}

/// Called when a [`Testcase`] is evaluated
pub fn on_evaluation_metadata_default<CS, O, OT, S>(
    scheduler: &mut CS,
    state: &mut S,
    observers: &OT,
) -> Result<(), Error>
where
    CS: AflScheduler,
    CS::ObserverRef: AsRef<O>,
    S: HasMetadata,
    O: Hash,
    OT: MatchName,
{
    let observer = observers
        .get(scheduler.observer_handle())
        .ok_or_else(|| Error::key_not_found("Observer not found".to_string()))?
        .as_ref();

    let mut hash = generic_hash_std(observer) as usize;

    let psmeta = state.metadata_mut::<SchedulerMetadata>()?;

    hash %= psmeta.n_fuzz().len();
    // Update the path frequency
    psmeta.n_fuzz_mut()[hash] = psmeta.n_fuzz()[hash].saturating_add(1);

    scheduler.set_last_hash(hash);

    Ok(())
}

/// Called when choosing the next [`Testcase`]
pub fn on_next_metadata_default<I, S>(state: &mut S) -> Result<(), Error>
where
    S: HasCorpus<I> + HasTestcase<I>,
{
    let current_id = *state.corpus().current();

    if let Some(id) = current_id {
        let mut testcase = state.testcase_mut(id)?;
        let tcmeta = testcase.metadata_mut::<SchedulerTestcaseMetadata>()?;

        if tcmeta.handicap() >= 4 {
            tcmeta.set_handicap(tcmeta.handicap() - 4);
        } else if tcmeta.handicap() > 0 {
            tcmeta.set_handicap(tcmeta.handicap() - 1);
        }
    }

    Ok(())
}

/// Defines the common metadata operations for the AFL-style schedulers
pub trait AflScheduler {
    /// The type of [`crate::observers::Observer`] that this scheduler will use as reference
    type ObserverRef;

    /// Return the last hash
    fn last_hash(&self) -> usize;

    /// Set the last hash
    fn set_last_hash(&mut self, value: usize);

    /// Get the observer handle
    fn observer_handle(&self) -> &Handle<Self::ObserverRef>;
}

/// Trait for Schedulers which track queue cycles
pub trait HasQueueCycles {
    /// The amount of cycles the scheduler has completed.
    fn queue_cycles(&self) -> u64;
}

/// The scheduler define how the fuzzer requests a testcase from the corpus.
/// It has hooks to corpus add/replace/remove to allow complex scheduling algorithms to collect data.
pub trait Scheduler<I, S> {
    /// Called when a [`Testcase`] is added to the corpus
    fn on_add(&mut self, _state: &mut S, _id: CorpusId) -> Result<(), Error>;
    // Add parent_id here if it has no inner

    /// An input has been evaluated
    fn on_evaluation<OT>(
        &mut self,
        _state: &mut S,
        _input: &I,
        _observers: &OT,
    ) -> Result<(), Error>
    where
        OT: MatchName,
    {
        Ok(())
    }

    /// Gets the next entry
    fn next(&mut self, state: &mut S) -> Result<CorpusId, Error>;
    // Increment corpus.current() here if it has no inner

    /// Set current fuzzed corpus id and `scheduled_count`
    fn set_current_scheduled(
        &mut self,
        state: &mut S,
        next_id: Option<CorpusId>,
    ) -> Result<(), Error>;

    //    *state.corpus_mut().current_mut() = next_id;
    //    Ok(())
}

/// Feed the fuzzer simply with a random testcase on request
#[derive(Debug, Clone)]
pub struct RandScheduler<S> {
    phantom: PhantomData<S>,
}

impl<I, S> Scheduler<I, S> for RandScheduler<S>
where
    S: HasCorpus<I> + HasRand,
{
    fn on_add(&mut self, state: &mut S, id: CorpusId) -> Result<(), Error> {
        // Set parent id
        let current_id = *state.corpus().current();
        state
            .corpus()
            .get(id)?
            .borrow_mut()
            .set_parent_id_optional(current_id);

        Ok(())
    }

    /// Gets the next entry at random
    fn next(&mut self, state: &mut S) -> Result<CorpusId, Error> {
        if state.corpus().count() == 0 {
            Err(Error::empty(
                "No entries in corpus. This often implies the target is not properly instrumented."
                    .to_owned(),
            ))
        } else {
            let id = random_corpus_id!(state.corpus(), state.rand_mut());
            <Self as Scheduler<I, S>>::set_current_scheduled(self, state, Some(id))?;
            Ok(id)
        }
    }

    fn set_current_scheduled(
        &mut self,
        state: &mut S,
        next_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        *state.corpus_mut().current_mut() = next_id;
        Ok(())
    }
}

impl<S> RandScheduler<S> {
    /// Create a new [`RandScheduler`] that just schedules randomly.
    #[must_use]
    pub fn new() -> Self {
        Self {
            phantom: PhantomData,
        }
    }
}

impl<S> Default for RandScheduler<S> {
    fn default() -> Self {
        Self::new()
    }
}

/// A [`StdScheduler`] uses the default scheduler in `LibAFL` to schedule [`Testcase`]s.
///
/// The current `Std` is a [`RandScheduler`], although this may change in the future, if another [`Scheduler`] delivers better results.
pub type StdScheduler<S> = RandScheduler<S>;
