//! The [`MinimizerScheduler`]`s` are a family of corpus schedulers that feed the fuzzer
//! with [`Testcase`]`s` only from a subset of the total [`Corpus`].

use alloc::vec::Vec;
use core::{any::type_name, cmp::Ordering, marker::PhantomData};

use hashbrown::{HashMap, HashSet};
use libafl_bolts::{AsIter, HasRefCnt, rands::Rand, serdeany::SerdeAny, tuples::MatchName};
use serde::{Deserialize, Serialize};

use super::HasQueueCycles;
use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId, Testcase},
    feedbacks::MapIndexesMetadata,
    observers::CanTrack,
    require_index_tracking,
    schedulers::{LenTimeMulTestcasePenalty, RemovableScheduler, Scheduler, TestcasePenalty},
    state::{HasCorpus, HasRand},
};

/// Default probability to skip the non-favored values
pub const DEFAULT_SKIP_NON_FAVORED_PROB: f64 = 0.95;

/// A testcase metadata saying if a testcase is favored
#[derive(Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct IsFavoredMetadata {}

libafl_bolts::impl_serdeany!(IsFavoredMetadata);

/// A state metadata holding a map of favoreds testcases for each map entry
#[derive(Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct TopRatedsMetadata {
    /// map index -> corpus index
    pub map: HashMap<usize, CorpusId>,
}

libafl_bolts::impl_serdeany!(TopRatedsMetadata);

impl TopRatedsMetadata {
    /// Creates a new [`struct@TopRatedsMetadata`]
    #[must_use]
    pub fn new() -> Self {
        Self {
            map: HashMap::default(),
        }
    }

    /// Getter for map
    #[must_use]
    pub fn map(&self) -> &HashMap<usize, CorpusId> {
        &self.map
    }
}

impl Default for TopRatedsMetadata {
    fn default() -> Self {
        Self::new()
    }
}

/// The [`MinimizerScheduler`] employs a genetic algorithm to compute a subset of the
/// corpus that exercise all the requested features.
///
/// E.g., it can use all the coverage seen so far to prioritize [`Testcase`]`s` using a [`TestcasePenalty`].
#[derive(Debug, Clone)]
pub struct MinimizerScheduler<CS, F, I, M, S> {
    base: CS,
    skip_non_favored_prob: f64,
    remove_metadata: bool,
    phantom: PhantomData<(F, I, M, S)>,
}

impl<CS, F, M, I, O, S> RemovableScheduler<I, S> for MinimizerScheduler<CS, F, I, M, O>
where
    CS: RemovableScheduler<I, S> + Scheduler<I, S>,
    F: TestcasePenalty<I, S>,
    M: for<'a> AsIter<'a, Item = usize> + SerdeAny + HasRefCnt,
    S: HasCorpus<I> + HasMetadata + HasRand,
{
    /// Replaces the [`Testcase`] at the given [`CorpusId`]
    fn on_replace(
        &mut self,
        state: &mut S,
        id: CorpusId,
        testcase: &Testcase<I>,
    ) -> Result<(), Error> {
        self.base.on_replace(state, id, testcase)?;
        self.update_score(state, id)
    }

    /// Removes an entry from the corpus
    fn on_remove(
        &mut self,
        state: &mut S,
        id: CorpusId,
        testcase: &Option<Testcase<I>>,
    ) -> Result<(), Error> {
        self.base.on_remove(state, id, testcase)?;
        let mut entries =
            if let Some(meta) = state.metadata_map_mut().get_mut::<TopRatedsMetadata>() {
                meta.map
                    .extract_if(|_, other_id| *other_id == id)
                    .map(|(entry, _)| entry)
                    .collect::<Vec<_>>()
            } else {
                return Ok(());
            };
        entries.sort_unstable(); // this should already be sorted, but just in case
        let mut map = HashMap::new();
        for current_id in state.corpus().ids() {
            let mut old = state.corpus().get(current_id)?.borrow_mut();
            let factor = F::compute(state, &mut *old)?;
            if let Some(old_map) = old.metadata_map_mut().get_mut::<M>() {
                let mut e_iter = entries.iter();
                let mut map_iter = old_map.as_iter(); // ASSERTION: guaranteed to be in order?

                // manual set intersection
                let mut entry = e_iter.next();
                let mut map_entry = map_iter.next();
                while let Some(e) = entry {
                    match map_entry {
                        Some(ref me) => {
                            match e.cmp(me) {
                                Ordering::Less => {
                                    entry = e_iter.next();
                                }
                                Ordering::Equal => {
                                    // if we found a better factor, prefer it
                                    map.entry(*e)
                                        .and_modify(|(f, id)| {
                                            if *f > factor {
                                                *f = factor;
                                                *id = current_id;
                                            }
                                        })
                                        .or_insert((factor, current_id));
                                    entry = e_iter.next();
                                    map_entry = map_iter.next();
                                }
                                Ordering::Greater => {
                                    map_entry = map_iter.next();
                                }
                            }
                        }
                        _ => {
                            break;
                        }
                    }
                }
            }
        }
        if let Some(mut meta) = state.metadata_map_mut().remove::<TopRatedsMetadata>() {
            let map_iter = map.iter();

            let reserve = if meta.map.is_empty() {
                map_iter.size_hint().0
            } else {
                map_iter.size_hint().0.div_ceil(2)
            };
            meta.map.reserve(reserve);

            for (entry, (_, new_id)) in map_iter {
                let mut new = state.corpus().get(*new_id)?.borrow_mut();
                let new_meta = new.metadata_map_mut().get_mut::<M>().ok_or_else(|| {
                    Error::key_not_found(format!(
                        "{} needed for MinimizerScheduler not found in testcase #{new_id}",
                        type_name::<M>()
                    ))
                })?;
                *new_meta.refcnt_mut() += 1;
                meta.map.insert(*entry, *new_id);
            }

            // Put back the metadata
            state.metadata_map_mut().insert_boxed(meta);
        }
        Ok(())
    }
}

impl<CS, F, I, M, O, S> Scheduler<I, S> for MinimizerScheduler<CS, F, I, M, O>
where
    CS: Scheduler<I, S>,
    F: TestcasePenalty<I, S>,
    M: for<'a> AsIter<'a, Item = usize> + SerdeAny + HasRefCnt,
    S: HasCorpus<I> + HasMetadata + HasRand,
{
    /// Called when a [`Testcase`] is added to the corpus
    fn on_add(&mut self, state: &mut S, id: CorpusId) -> Result<(), Error> {
        self.base.on_add(state, id)?;
        self.update_score(state, id)
    }

    /// An input has been evaluated
    fn on_evaluation<OT>(&mut self, state: &mut S, input: &I, observers: &OT) -> Result<(), Error>
    where
        OT: MatchName,
    {
        self.base.on_evaluation(state, input, observers)
    }

    /// Gets the next entry
    fn next(&mut self, state: &mut S) -> Result<CorpusId, Error> {
        self.cull(state)?;
        let mut id = self.base.next(state)?;
        while {
            !state
                .corpus()
                .get(id)?
                .borrow()
                .has_metadata::<IsFavoredMetadata>()
        } && state.rand_mut().coinflip(self.skip_non_favored_prob)
        {
            id = self.base.next(state)?;
        }
        Ok(id)
    }

    /// Set current fuzzed corpus id and `scheduled_count`
    fn set_current_scheduled(
        &mut self,
        _state: &mut S,
        _next_id: Option<CorpusId>,
    ) -> Result<(), Error> {
        // We do nothing here, the inner scheduler will take care of it
        Ok(())
    }
}

impl<CS, F, I, M, O> MinimizerScheduler<CS, F, I, M, O>
where
    M: for<'a> AsIter<'a, Item = usize> + SerdeAny + HasRefCnt,
{
    /// Update the [`Corpus`] score using the [`MinimizerScheduler`]
    #[expect(clippy::cast_possible_wrap)]
    pub fn update_score<S>(&self, state: &mut S, id: CorpusId) -> Result<(), Error>
    where
        F: TestcasePenalty<I, S>,
        S: HasCorpus<I> + HasMetadata,
    {
        // Create a new top rated meta if not existing
        if state.metadata_map().get::<TopRatedsMetadata>().is_none() {
            state.add_metadata(TopRatedsMetadata::new());
        }

        let mut new_favoreds = vec![];
        {
            let mut entry = state.corpus().get(id)?.borrow_mut();
            let factor = F::compute(state, &mut *entry)?;
            let meta = entry.metadata_map_mut().get_mut::<M>().ok_or_else(|| {
                Error::key_not_found(format!(
                    "Metadata needed for MinimizerScheduler not found in testcase #{id}"
                ))
            })?;
            let top_rateds = state.metadata_map().get::<TopRatedsMetadata>().unwrap();
            for elem in meta.as_iter() {
                if let Some(old_id) = top_rateds.map.get(&*elem) {
                    if *old_id == id {
                        new_favoreds.push(*elem); // always retain current; we'll drop it later otherwise
                        continue;
                    }
                    let mut old = state.corpus().get(*old_id)?.borrow_mut();
                    if factor > F::compute(state, &mut *old)? {
                        continue;
                    }

                    let must_remove = {
                        let old_meta = old.metadata_map_mut().get_mut::<M>().ok_or_else(|| {
                            Error::key_not_found(format!(
                                "{} needed for MinimizerScheduler not found in testcase #{old_id}",
                                type_name::<M>()
                            ))
                        })?;
                        *old_meta.refcnt_mut() -= 1;
                        old_meta.refcnt() <= 0
                    };

                    if must_remove && self.remove_metadata {
                        drop(old.metadata_map_mut().remove::<M>());
                    }
                }

                new_favoreds.push(*elem);
            }

            *meta.refcnt_mut() = new_favoreds.len() as isize;
        }

        if new_favoreds.is_empty() && self.remove_metadata {
            drop(
                state
                    .corpus()
                    .get(id)?
                    .borrow_mut()
                    .metadata_map_mut()
                    .remove::<M>(),
            );
            return Ok(());
        }

        for elem in new_favoreds {
            state
                .metadata_map_mut()
                .get_mut::<TopRatedsMetadata>()
                .unwrap()
                .map
                .insert(elem, id);
        }
        Ok(())
    }

    /// Cull the [`Corpus`] using the [`MinimizerScheduler`]
    pub fn cull<S>(&self, state: &S) -> Result<(), Error>
    where
        S: HasCorpus<I> + HasMetadata,
    {
        let Some(top_rated) = state.metadata_map().get::<TopRatedsMetadata>() else {
            return Ok(());
        };

        let mut acc = HashSet::new();

        for (key, id) in &top_rated.map {
            if !acc.contains(key) {
                let mut entry = state.corpus().get(*id)?.borrow_mut();
                let meta = entry.metadata_map().get::<M>().ok_or_else(|| {
                    Error::key_not_found(format!(
                        "{} needed for MinimizerScheduler not found in testcase #{id}",
                        type_name::<M>()
                    ))
                })?;
                for elem in meta.as_iter() {
                    acc.insert(*elem);
                }

                entry.add_metadata(IsFavoredMetadata {});
            }
        }

        Ok(())
    }
}
impl<CS, F, I, M, O> HasQueueCycles for MinimizerScheduler<CS, F, I, M, O>
where
    CS: HasQueueCycles,
{
    fn queue_cycles(&self) -> u64 {
        self.base.queue_cycles()
    }
}
impl<CS, F, I, M, O> MinimizerScheduler<CS, F, I, M, O>
where
    O: CanTrack,
{
    /// Get a reference to the base scheduler
    pub fn base(&self) -> &CS {
        &self.base
    }

    /// Get a reference to the base scheduler (mut)
    pub fn base_mut(&mut self) -> &mut CS {
        &mut self.base
    }

    /// Creates a new [`MinimizerScheduler`] that wraps a `base` [`Scheduler`]
    /// and has a default probability to skip non-faved [`Testcase`]s of [`DEFAULT_SKIP_NON_FAVORED_PROB`].
    /// This will remove the metadata `M` when it is no longer needed, after consumption. This might
    /// for example be a `MapIndexesMetadata`.
    ///
    /// When calling, pass the edges observer which will provided the indexes to minimize over.
    pub fn new(_observer: &O, base: CS) -> Self {
        require_index_tracking!("MinimizerScheduler", O);
        Self {
            base,
            skip_non_favored_prob: DEFAULT_SKIP_NON_FAVORED_PROB,
            remove_metadata: true,
            phantom: PhantomData,
        }
    }

    /// Creates a new [`MinimizerScheduler`] that wraps a `base` [`Scheduler`]
    /// and has a default probability to skip non-faved [`Testcase`]s of [`DEFAULT_SKIP_NON_FAVORED_PROB`].
    /// This method will prevent the metadata `M` from being removed at the end of scoring.
    ///
    /// When calling, pass the edges observer which will provided the indexes to minimize over.
    pub fn non_metadata_removing(_observer: &O, base: CS) -> Self {
        require_index_tracking!("MinimizerScheduler", O);
        Self {
            base,
            skip_non_favored_prob: DEFAULT_SKIP_NON_FAVORED_PROB,
            remove_metadata: false,
            phantom: PhantomData,
        }
    }

    /// Creates a new [`MinimizerScheduler`] that wraps a `base` [`Scheduler`]
    /// and has a non-default probability to skip non-faved [`Testcase`]s using (`skip_non_favored_prob`).
    ///
    /// When calling, pass the edges observer which will provided the indexes to minimize over.
    pub fn with_skip_prob(_observer: &O, base: CS, skip_non_favored_prob: f64) -> Self {
        require_index_tracking!("MinimizerScheduler", O);
        Self {
            base,
            skip_non_favored_prob,
            remove_metadata: true,
            phantom: PhantomData,
        }
    }
}

/// A [`MinimizerScheduler`] with [`LenTimeMulTestcasePenalty`] to prioritize quick and small [`Testcase`]`s`.
pub type LenTimeMinimizerScheduler<CS, I, M, O> =
    MinimizerScheduler<CS, LenTimeMulTestcasePenalty, I, M, O>;

/// A [`MinimizerScheduler`] with [`LenTimeMulTestcasePenalty`] to prioritize quick and small [`Testcase`]`s`
/// that exercise all the entries registered in the [`MapIndexesMetadata`].
pub type IndexesLenTimeMinimizerScheduler<CS, I, O> =
    MinimizerScheduler<CS, LenTimeMulTestcasePenalty, I, MapIndexesMetadata, O>;
