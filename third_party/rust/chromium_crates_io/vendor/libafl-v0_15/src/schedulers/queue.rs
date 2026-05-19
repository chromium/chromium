//! The queue corpus scheduler implements an AFL-like queue mechanism

use alloc::borrow::ToOwned;

use crate::{
    Error,
    corpus::{Corpus, CorpusId},
    schedulers::{HasQueueCycles, RemovableScheduler, Scheduler},
    state::HasCorpus,
};

/// Walk the corpus in a queue-like fashion
#[derive(Debug, Clone)]
pub struct QueueScheduler {
    queue_cycles: u64,
    runs_in_current_cycle: u64,
}

impl<I, S> RemovableScheduler<I, S> for QueueScheduler {}

impl<I, S> Scheduler<I, S> for QueueScheduler
where
    S: HasCorpus<I>,
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

    /// Gets the next entry in the queue
    fn next(&mut self, state: &mut S) -> Result<CorpusId, Error> {
        if state.corpus().count() == 0 {
            Err(Error::empty(
                "No entries in corpus. This often implies the target is not properly instrumented."
                    .to_owned(),
            ))
        } else {
            let id = state
                .corpus()
                .current()
                .map(|id| state.corpus().next(id))
                .flatten()
                .unwrap_or_else(|| state.corpus().first().unwrap());

            self.runs_in_current_cycle += 1;
            // TODO deal with corpus_counts decreasing due to removals
            if self.runs_in_current_cycle >= state.corpus().count() as u64 {
                self.queue_cycles += 1;
                self.runs_in_current_cycle = 0;
            }
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

impl QueueScheduler {
    /// Creates a new `QueueScheduler`
    #[must_use]
    pub fn new() -> Self {
        Self {
            runs_in_current_cycle: 0,
            queue_cycles: 0,
        }
    }
}

impl Default for QueueScheduler {
    fn default() -> Self {
        Self::new()
    }
}

impl HasQueueCycles for QueueScheduler {
    fn queue_cycles(&self) -> u64 {
        self.queue_cycles
    }
}

#[cfg(test)]
#[cfg(feature = "std")]
mod tests {

    use std::{fs, path::PathBuf};

    use libafl_bolts::rands::StdRand;

    use crate::{
        corpus::{Corpus, OnDiskCorpus, Testcase},
        feedbacks::ConstFeedback,
        inputs::bytes::BytesInput,
        schedulers::{QueueScheduler, Scheduler},
        state::{HasCorpus, StdState},
    };

    #[test]
    fn test_queuecorpus() {
        let rand = StdRand::with_seed(4);
        let mut scheduler: QueueScheduler = QueueScheduler::new();

        let mut q =
            OnDiskCorpus::<BytesInput>::new(PathBuf::from("target/.test/fancy/path")).unwrap();
        let t = Testcase::with_filename(BytesInput::new(vec![0_u8; 4]), "fancyfile".into());
        q.add(t).unwrap();

        let objective_q =
            OnDiskCorpus::<BytesInput>::new(PathBuf::from("target/.test/fancy/objective/path"))
                .unwrap();

        let mut feedback = ConstFeedback::new(false);
        let mut objective = ConstFeedback::new(false);

        let mut state = StdState::new(rand, q, objective_q, &mut feedback, &mut objective).unwrap();

        let next_id =
            <QueueScheduler as Scheduler<BytesInput, _>>::next(&mut scheduler, &mut state).unwrap();
        let filename = state
            .corpus()
            .get(next_id)
            .unwrap()
            .borrow()
            .filename()
            .as_ref()
            .unwrap()
            .clone();

        assert_eq!(filename, "fancyfile");

        fs::remove_dir_all("target/.test/fancy/path").unwrap();
    }
}
