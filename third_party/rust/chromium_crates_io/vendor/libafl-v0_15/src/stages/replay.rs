//! The replay stage can scan all inputs and executes them once per input

use alloc::{
    borrow::{Cow, ToOwned},
    string::ToString,
    vec::Vec,
};
use core::marker::PhantomData;

use hashbrown::HashSet;
use libafl_bolts::{Named, impl_serdeany};
use serde::{Deserialize, Serialize};

use crate::{
    Error, Evaluator, HasMetadata,
    corpus::{Corpus, CorpusId},
    stages::{Restartable, Stage},
    state::{HasCorpus, HasSolutions},
};

/// Replay all inputs
#[derive(Debug)]
pub struct ReplayStage<I> {
    name: Cow<'static, str>,
    phantom: PhantomData<I>,
}

impl<I> Default for ReplayStage<I> {
    fn default() -> Self {
        Self::new()
    }
}

impl<I> Named for ReplayStage<I> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

#[derive(Serialize, Deserialize, Debug, Clone, Default)]
/// Maintains the list of processed corpus or solution entries till now
pub struct ReplayRestarterMetadata {
    done_corpus: HashSet<CorpusId>,
    done_solution: HashSet<CorpusId>,
}

impl_serdeany!(ReplayRestarterMetadata);

impl ReplayRestarterMetadata {
    /// constructor
    #[must_use]
    pub fn new() -> Self {
        Self {
            done_corpus: HashSet::default(),
            done_solution: HashSet::default(),
        }
    }

    /// clear history
    pub fn clear(&mut self) {
        self.done_corpus.clear();
        self.done_solution.clear();
    }

    /// check we've scaned this corpus entry
    pub fn corpus_probe(&mut self, id: &CorpusId) -> bool {
        self.done_corpus.contains(id)
    }

    /// check we've scaned this solution entry
    pub fn solution_probe(&mut self, id: &CorpusId) -> bool {
        self.done_solution.contains(id)
    }

    /// mark this corpus entry as finished
    pub fn corpus_finish(&mut self, id: CorpusId) {
        self.done_corpus.insert(id);
    }

    /// mark this solution entry as finished
    pub fn solution_finish(&mut self, id: CorpusId) {
        self.done_solution.insert(id);
    }
}

/// The counter for giving this stage unique id
static mut REPLAY_STAGE_ID: usize = 0;
/// The name for tracing stage
pub static REPLAY_STAGE_NAME: &str = "tracing";

impl<I> ReplayStage<I> {
    #[must_use]
    /// Create a new replay stage
    pub fn new() -> Self {
        // unsafe but impossible that you create two threads both instantiating this instance
        let stage_id = unsafe {
            let ret = REPLAY_STAGE_ID;
            REPLAY_STAGE_ID += 1;
            ret
        };

        Self {
            name: Cow::Owned(REPLAY_STAGE_NAME.to_owned() + ":" + stage_id.to_string().as_ref()),
            phantom: PhantomData,
        }
    }
}

impl<E, EM, I, S, Z> Stage<E, EM, S, Z> for ReplayStage<I>
where
    S: HasCorpus<I> + HasSolutions<I> + HasMetadata,
    Z: Evaluator<E, EM, I, S>,
    I: Clone,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let corpus_ids: Vec<CorpusId> = state.corpus().ids().collect();
        for id in corpus_ids {
            {
                let helper = state.metadata_mut::<ReplayRestarterMetadata>()?;
                if helper.corpus_probe(&id) {
                    continue;
                }
                helper.corpus_finish(id);
            }

            log::info!("Replaying corpus: {id}");
            let input = {
                let mut tc = state.corpus().get(id)?.borrow_mut();
                let input = tc.load_input(state.corpus())?;
                input.clone()
            };

            fuzzer.evaluate_input(state, executor, manager, &input)?;
        }

        let solution_ids: Vec<CorpusId> = state.solutions().ids().collect();
        for id in solution_ids {
            {
                let helper = state.metadata_mut::<ReplayRestarterMetadata>()?;
                if helper.solution_probe(&id) {
                    continue;
                }
                helper.solution_finish(id);
            }
            log::info!("Replaying solution: {id}");
            let input = {
                let mut tc = state.solutions().get(id)?.borrow_mut();
                let input = tc.load_input(state.corpus())?;
                input.clone()
            };

            fuzzer.evaluate_input(state, executor, manager, &input)?;
        }
        log::info!("DONE :)");
        Ok(())
    }
}

impl<I, S> Restartable<S> for ReplayStage<I>
where
    S: HasMetadata,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        state.metadata_or_insert_with(ReplayRestarterMetadata::default);
        Ok(true)
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        state.remove_metadata::<ReplayRestarterMetadata>();
        Ok(())
    }
}
