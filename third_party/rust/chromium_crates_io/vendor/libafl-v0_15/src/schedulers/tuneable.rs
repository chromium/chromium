//! The queue corpus scheduler implements an AFL-like queue mechanism
//! The [`TuneableScheduler`] extends the queue scheduler with a method to
//! chose the next corpus entry manually

use alloc::borrow::ToOwned;

use libafl_bolts::impl_serdeany;
use serde::{Deserialize, Serialize};

use super::RemovableScheduler;
use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId},
    schedulers::Scheduler,
    state::HasCorpus,
};

#[derive(Default, Copy, Clone, Eq, PartialEq, Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
struct TuneableSchedulerMetadata {
    next: Option<CorpusId>,
}

impl_serdeany!(TuneableSchedulerMetadata);

/// Walk the corpus in a queue-like fashion
/// With the specific `set_next` method, we can chose the next corpus entry manually
#[derive(Debug, Clone)]
pub struct TuneableScheduler {}

impl TuneableScheduler {
    /// Creates a new `TuneableScheduler`
    #[must_use]
    pub fn new<S>(state: &mut S) -> Self
    where
        S: HasMetadata,
    {
        if !state.has_metadata::<TuneableSchedulerMetadata>() {
            state.add_metadata(TuneableSchedulerMetadata::default());
        }
        Self {}
    }

    fn metadata_mut<S>(state: &mut S) -> &mut TuneableSchedulerMetadata
    where
        S: HasMetadata,
    {
        state
            .metadata_map_mut()
            .get_mut::<TuneableSchedulerMetadata>()
            .unwrap()
    }

    fn metadata<S>(state: &S) -> &TuneableSchedulerMetadata
    where
        S: HasMetadata,
    {
        state
            .metadata_map()
            .get::<TuneableSchedulerMetadata>()
            .unwrap()
    }

    /// Sets the next corpus id to be used
    pub fn set_next<S>(state: &mut S, next: CorpusId)
    where
        S: HasMetadata,
    {
        Self::metadata_mut(state).next = Some(next);
    }

    /// Gets the next set corpus id
    pub fn get_next<S>(state: &S) -> Option<CorpusId>
    where
        S: HasMetadata,
    {
        Self::metadata(state).next
    }

    /// Resets this to a queue scheduler
    pub fn reset<S>(state: &mut S)
    where
        S: HasMetadata,
    {
        let metadata = Self::metadata_mut(state);
        metadata.next = None;
    }

    /// Gets the current corpus entry id
    pub fn get_current<I, S>(state: &S) -> CorpusId
    where
        S: HasCorpus<I>,
    {
        state
            .corpus()
            .current()
            .unwrap_or_else(|| state.corpus().first().expect("Empty corpus"))
    }
}

impl<I, S> RemovableScheduler<I, S> for TuneableScheduler {}

impl<I, S> Scheduler<I, S> for TuneableScheduler
where
    S: HasCorpus<I> + HasMetadata,
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
            return Err(Error::empty(
                "No entries in corpus. This often implies the target is not properly instrumented."
                    .to_owned(),
            ));
        }
        let id = if let Some(next) = Self::get_next(state) {
            // next was set
            next
        } else if let Some(next) = state.corpus().next(Self::get_current(state)) {
            next
        } else {
            state.corpus().first().unwrap()
        };
        <Self as Scheduler<I, S>>::set_current_scheduled(self, state, Some(id))?;
        Ok(id)
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
