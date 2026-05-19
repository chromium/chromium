//! Probabilistic sampling scheduler is a corpus scheduler that feeds the fuzzer
//! with sampled item from the corpus.

use alloc::string::String;
use core::marker::PhantomData;

use hashbrown::HashMap;
use libafl_bolts::rands::Rand;
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId, Testcase},
    schedulers::{RemovableScheduler, Scheduler, TestcaseScore},
    state::{HasCorpus, HasRand},
};

/// Conduct reservoir sampling (probabilistic sampling) over all corpus elements.
#[derive(Debug, Clone)]
pub struct ProbabilitySamplingScheduler<F> {
    phantom: PhantomData<F>,
}

/// A state metadata holding a map of probability of corpus elements.
#[derive(Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct ProbabilityMetadata {
    /// corpus index -> probability
    pub map: HashMap<CorpusId, f64>,
    /// total probability of all items in the map
    pub total_probability: f64,
}

libafl_bolts::impl_serdeany!(ProbabilityMetadata);

impl ProbabilityMetadata {
    /// Creates a new [`struct@ProbabilityMetadata`]
    #[must_use]
    pub fn new() -> Self {
        Self {
            map: HashMap::default(),
            total_probability: 0.0,
        }
    }
}

impl Default for ProbabilityMetadata {
    fn default() -> Self {
        Self::new()
    }
}

impl<F> ProbabilitySamplingScheduler<F> {
    /// Creates a new [`struct@ProbabilitySamplingScheduler`]
    #[must_use]
    pub fn new() -> Self {
        Self {
            phantom: PhantomData,
        }
    }

    /// Calculate the score and store in `ProbabilityMetadata`
    pub fn store_probability<I, S>(&self, state: &mut S, id: CorpusId) -> Result<(), Error>
    where
        F: TestcaseScore<I, S>,
        S: HasCorpus<I> + HasMetadata + HasRand,
    {
        let prob = F::compute(state, &mut *state.corpus().get(id)?.borrow_mut())?;
        debug_assert!(
            prob >= 0.0 && prob.is_finite(),
            "scheduler probability is {prob}; to work correctly it must be >= 0.0 and finite"
        );
        let meta = state
            .metadata_map_mut()
            .get_mut::<ProbabilityMetadata>()
            .unwrap();
        meta.map.insert(id, prob);
        meta.total_probability += prob;
        Ok(())
    }
}

impl<F, I, S> RemovableScheduler<I, S> for ProbabilitySamplingScheduler<F>
where
    F: TestcaseScore<I, S>,
    S: HasCorpus<I> + HasMetadata + HasRand,
{
    fn on_remove(
        &mut self,
        state: &mut S,
        id: CorpusId,
        _testcase: &Option<Testcase<I>>,
    ) -> Result<(), Error> {
        let meta = state
            .metadata_map_mut()
            .get_mut::<ProbabilityMetadata>()
            .unwrap();
        if let Some(prob) = meta.map.remove(&id) {
            meta.total_probability -= prob;
        }
        Ok(())
    }

    fn on_replace(
        &mut self,
        state: &mut S,
        id: CorpusId,
        _prev: &Testcase<I>,
    ) -> Result<(), Error> {
        let meta = state
            .metadata_map_mut()
            .get_mut::<ProbabilityMetadata>()
            .unwrap();
        if let Some(prob) = meta.map.remove(&id) {
            meta.total_probability -= prob;
        }

        self.store_probability(state, id)
    }
}

impl<F, I, S> Scheduler<I, S> for ProbabilitySamplingScheduler<F>
where
    F: TestcaseScore<I, S>,
    S: HasCorpus<I> + HasMetadata + HasRand,
{
    fn on_add(&mut self, state: &mut S, id: CorpusId) -> Result<(), Error> {
        let current_id = *state.corpus().current();
        state
            .corpus()
            .get(id)?
            .borrow_mut()
            .set_parent_id_optional(current_id);

        if state.metadata_map().get::<ProbabilityMetadata>().is_none() {
            state.add_metadata(ProbabilityMetadata::new());
        }
        self.store_probability(state, id)
    }

    /// Gets the next entry
    fn next(&mut self, state: &mut S) -> Result<CorpusId, Error> {
        if state.corpus().count() == 0 {
            Err(Error::empty(String::from(
                "No entries in corpus. This often implies the target is not properly instrumented.",
            )))
        } else {
            let rand_prob: f64 = state.rand_mut().next_float();
            let meta = state.metadata_map().get::<ProbabilityMetadata>().unwrap();
            let threshold = meta.total_probability * rand_prob;
            let mut k: f64 = 0.0;
            let mut ret = *meta.map.keys().last().unwrap();
            for (idx, prob) in &meta.map {
                k += prob;
                if k >= threshold {
                    ret = *idx;
                    break;
                }
            }
            self.set_current_scheduled(state, Some(ret))?;
            Ok(ret)
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

impl<F> Default for ProbabilitySamplingScheduler<F> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
#[cfg(feature = "std")]
mod tests {
    use core::borrow::BorrowMut;

    use libafl_bolts::rands::StdRand;

    use crate::{
        Error,
        corpus::{Corpus, InMemoryCorpus, Testcase},
        feedbacks::ConstFeedback,
        inputs::bytes::BytesInput,
        schedulers::{ProbabilitySamplingScheduler, Scheduler, TestcaseScore},
        state::{HasCorpus, StdState},
    };

    const FACTOR: f64 = 1337.0;

    #[derive(Debug, Clone)]
    pub struct UniformDistribution {}

    impl<I, S> TestcaseScore<I, S> for UniformDistribution
    where
        S: HasCorpus<I>,
    {
        fn compute(_state: &S, _: &mut Testcase<I>) -> Result<f64, Error> {
            Ok(FACTOR)
        }
    }

    pub type UniformProbabilitySamplingScheduler =
        ProbabilitySamplingScheduler<UniformDistribution>;

    #[test]
    fn test_prob_sampling() {
        // # Safety
        // No concurrency per testcase
        #[cfg(any(not(feature = "serdeany_autoreg"), miri))]
        unsafe {
            super::ProbabilityMetadata::register();
        }

        // the first 3 probabilities will be .76, .86, .36
        let rand = StdRand::with_seed(2);

        let mut scheduler: ProbabilitySamplingScheduler<_> =
            UniformProbabilitySamplingScheduler::new();

        let mut feedback = ConstFeedback::new(false);
        let mut objective = ConstFeedback::new(false);

        let mut corpus = InMemoryCorpus::new();
        let t1 = Testcase::with_filename(BytesInput::new(vec![0_u8; 4]), "1".into());
        let t2 = Testcase::with_filename(BytesInput::new(vec![1_u8; 4]), "2".into());

        let idx1 = corpus.add(t1).unwrap();
        let idx2 = corpus.add(t2).unwrap();

        let mut state = StdState::new(
            rand,
            corpus,
            InMemoryCorpus::new(),
            &mut feedback,
            &mut objective,
        )
        .unwrap();
        scheduler.on_add(state.borrow_mut(), idx1).unwrap();
        scheduler.on_add(state.borrow_mut(), idx2).unwrap();
        let next_id1 = scheduler.next(&mut state).unwrap();
        let next_id2 = scheduler.next(&mut state).unwrap();
        let next_id3 = scheduler.next(&mut state).unwrap();
        assert_eq!(next_id1, next_id2);
        assert_ne!(next_id1, next_id3);
    }
}
