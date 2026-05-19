//! Coverage accounting corpus scheduler, more details at <https://www.ndss-symposium.org/wp-content/uploads/2020/02/24422-paper.pdf>

use alloc::vec::Vec;
use core::{
    fmt::Debug,
    ops::{Deref, DerefMut},
};

use hashbrown::HashMap;
use libafl_bolts::{HasLen, HasRefCnt, rands::Rand, tuples::MatchName};
use serde::{Deserialize, Serialize};

use super::IndexesLenTimeMinimizerScheduler;
use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId},
    observers::CanTrack,
    schedulers::{
        Scheduler,
        minimizer::{DEFAULT_SKIP_NON_FAVORED_PROB, IsFavoredMetadata, MinimizerScheduler},
    },
    state::{HasCorpus, HasRand},
};

/// A testcase metadata holding a list of indexes of a map
#[derive(Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct AccountingIndexesMetadata {
    /// The list of indexes.
    pub list: Vec<usize>,
    /// A refcount used to know when remove this meta
    pub tcref: isize,
}

libafl_bolts::impl_serdeany!(AccountingIndexesMetadata);

impl Deref for AccountingIndexesMetadata {
    type Target = [usize];
    fn deref(&self) -> &[usize] {
        &self.list
    }
}
impl DerefMut for AccountingIndexesMetadata {
    fn deref_mut(&mut self) -> &mut [usize] {
        &mut self.list
    }
}

impl HasRefCnt for AccountingIndexesMetadata {
    fn refcnt(&self) -> isize {
        self.tcref
    }

    fn refcnt_mut(&mut self) -> &mut isize {
        &mut self.tcref
    }
}

impl AccountingIndexesMetadata {
    /// Creates a new [`struct@AccountingIndexesMetadata`].
    #[must_use]
    pub fn new(list: Vec<usize>) -> Self {
        Self { list, tcref: 0 }
    }

    /// Creates a new [`struct@AccountingIndexesMetadata`] specifying the refcount.
    #[must_use]
    pub fn with_tcref(list: Vec<usize>, tcref: isize) -> Self {
        Self { list, tcref }
    }
}

/// A state metadata holding a map of favoreds testcases for each map entry
#[derive(Debug, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct TopAccountingMetadata {
    /// map index -> corpus index
    pub map: HashMap<usize, CorpusId>,
    /// If changed sicne the previous add to the corpus
    pub changed: bool,
    /// The max accounting seen so far
    pub max_accounting: Vec<u32>,
}

libafl_bolts::impl_serdeany!(TopAccountingMetadata);

impl TopAccountingMetadata {
    /// Creates a new [`struct@TopAccountingMetadata`]
    #[must_use]
    pub fn new(acc_len: usize) -> Self {
        Self {
            map: HashMap::default(),
            changed: false,
            max_accounting: vec![0; acc_len],
        }
    }
}

/// A minimizer scheduler using coverage accounting
#[derive(Debug)]
pub struct CoverageAccountingScheduler<'a, CS, I, O> {
    accounting_map: &'a [u32],
    skip_non_favored_prob: f64,
    inner: IndexesLenTimeMinimizerScheduler<CS, I, O>,
}

impl<CS, I, O, S> Scheduler<I, S> for CoverageAccountingScheduler<'_, CS, I, O>
where
    CS: Scheduler<I, S>,
    S: HasCorpus<I> + HasMetadata + HasRand,
    I: HasLen,
    O: CanTrack,
{
    fn on_add(&mut self, state: &mut S, id: CorpusId) -> Result<(), Error> {
        self.update_accounting_score(state, id)?;
        self.inner.on_add(state, id)
    }

    fn on_evaluation<OT>(&mut self, state: &mut S, input: &I, observers: &OT) -> Result<(), Error>
    where
        OT: MatchName,
    {
        self.inner.on_evaluation(state, input, observers)
    }

    fn next(&mut self, state: &mut S) -> Result<CorpusId, Error> {
        if state
            .metadata_map()
            .get::<TopAccountingMetadata>()
            .is_some_and(|x| x.changed)
        {
            self.accounting_cull(state)?;
        } else {
            self.inner.cull(state)?;
        }
        let mut id = self.inner.base_mut().next(state)?;
        while {
            !state
                .corpus()
                .get(id)?
                .borrow()
                .has_metadata::<IsFavoredMetadata>()
        } && state.rand_mut().coinflip(self.skip_non_favored_prob)
        {
            id = self.inner.base_mut().next(state)?;
        }

        // Don't add corpus.curret(). The inner scheduler will take care of it

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

impl<'a, CS, I, O> CoverageAccountingScheduler<'a, CS, I, O>
where
    O: CanTrack,
{
    /// Update the `Corpus` score
    #[expect(clippy::cast_possible_wrap)]
    pub fn update_accounting_score<S>(&self, state: &mut S, id: CorpusId) -> Result<(), Error>
    where
        S: HasCorpus<I> + HasMetadata,
    {
        let mut indexes = vec![];
        let mut new_favoreds = vec![];
        {
            for idx in 0..self.accounting_map.len() {
                if self.accounting_map[idx] == 0 {
                    continue;
                }
                indexes.push(idx);

                let mut equal_score = false;
                {
                    let top_acc = state.metadata_map().get::<TopAccountingMetadata>().unwrap();

                    if let Some(old_id) = top_acc.map.get(&idx) {
                        if top_acc.max_accounting[idx] > self.accounting_map[idx] {
                            continue;
                        }

                        if top_acc.max_accounting[idx] == self.accounting_map[idx] {
                            equal_score = true;
                        }

                        let mut old = state.corpus().get_from_all(*old_id)?.borrow_mut();
                        let must_remove = {
                            let old_meta = old.metadata_map_mut().get_mut::<AccountingIndexesMetadata>().ok_or_else(|| {
                                Error::key_not_found(format!(
                                    "AccountingIndexesMetadata, needed by CoverageAccountingScheduler, not found in testcase #{old_id}"
                                ))
                            })?;
                            *old_meta.refcnt_mut() -= 1;
                            old_meta.refcnt() <= 0
                        };

                        if must_remove {
                            drop(old.metadata_map_mut().remove::<AccountingIndexesMetadata>());
                        }
                    }
                }

                let top_acc = state
                    .metadata_map_mut()
                    .get_mut::<TopAccountingMetadata>()
                    .unwrap();

                // if its accounting is equal to others', it's not favored
                if equal_score {
                    top_acc.map.remove(&idx);
                } else if top_acc.max_accounting[idx] < self.accounting_map[idx] {
                    new_favoreds.push(idx);

                    top_acc.max_accounting[idx] = self.accounting_map[idx];
                }
            }
        }

        if new_favoreds.is_empty() {
            return Ok(());
        }

        state
            .corpus()
            .get(id)?
            .borrow_mut()
            .metadata_map_mut()
            .insert(AccountingIndexesMetadata::with_tcref(
                indexes,
                new_favoreds.len() as isize,
            ));

        let top_acc = state
            .metadata_map_mut()
            .get_mut::<TopAccountingMetadata>()
            .unwrap();
        top_acc.changed = true;

        for elem in new_favoreds {
            top_acc.map.insert(elem, id);
        }

        Ok(())
    }

    /// Cull the `Corpus`
    pub fn accounting_cull<S>(&self, state: &S) -> Result<(), Error>
    where
        S: HasCorpus<I> + HasMetadata,
    {
        let Some(top_rated) = state.metadata_map().get::<TopAccountingMetadata>() else {
            return Ok(());
        };

        for (_key, id) in &top_rated.map {
            let mut entry = state.corpus().get(*id)?.borrow_mut();
            if entry.scheduled_count() > 0 {
                continue;
            }

            entry.add_metadata(IsFavoredMetadata {});
        }

        Ok(())
    }

    /// Creates a new [`CoverageAccountingScheduler`] that wraps a `base` [`Scheduler`]
    /// and has a default probability to skip non-faved Testcases of [`DEFAULT_SKIP_NON_FAVORED_PROB`].
    ///
    /// Provide the observer responsible for determining new indexes.
    pub fn new<S>(observer: &O, state: &mut S, base: CS, accounting_map: &'a [u32]) -> Self
    where
        S: HasMetadata,
    {
        match state.metadata_map().get::<TopAccountingMetadata>() {
            Some(meta) => {
                if meta.max_accounting.len() != accounting_map.len() {
                    state.add_metadata(TopAccountingMetadata::new(accounting_map.len()));
                }
            }
            None => {
                state.add_metadata(TopAccountingMetadata::new(accounting_map.len()));
            }
        }
        Self {
            accounting_map,
            inner: MinimizerScheduler::new(observer, base),
            skip_non_favored_prob: DEFAULT_SKIP_NON_FAVORED_PROB,
        }
    }

    /// Creates a new [`CoverageAccountingScheduler`] that wraps a `base` [`Scheduler`]
    /// and has a non-default probability to skip non-faved Testcases using (`skip_non_favored_prob`).
    ///
    /// Provide the observer responsible for determining new indexes.
    pub fn with_skip_prob<S>(
        observer: &O,
        state: &mut S,
        base: CS,
        skip_non_favored_prob: f64,
        accounting_map: &'a [u32],
    ) -> Self
    where
        S: HasMetadata,
    {
        match state.metadata_map().get::<TopAccountingMetadata>() {
            Some(meta) => {
                if meta.max_accounting.len() != accounting_map.len() {
                    state.add_metadata(TopAccountingMetadata::new(accounting_map.len()));
                }
            }
            None => {
                state.add_metadata(TopAccountingMetadata::new(accounting_map.len()));
            }
        }
        Self {
            accounting_map,
            inner: MinimizerScheduler::with_skip_prob(observer, base, skip_non_favored_prob),
            skip_non_favored_prob,
        }
    }
}
