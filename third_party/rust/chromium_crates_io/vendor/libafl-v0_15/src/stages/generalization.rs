//! The tracing stage can trace the target and enrich a [`crate::corpus::Testcase`] with metadata, for example for `CmpLog`.

use alloc::{
    borrow::{Cow, ToOwned},
    vec::Vec,
};
use core::{fmt::Debug, marker::PhantomData};

use libafl_bolts::{
    AsSlice, Named,
    tuples::{Handle, Handled},
};

#[cfg(feature = "introspection")]
use crate::monitors::stats::PerfFeature;
use crate::{
    Error, HasMetadata, HasNamedMetadata,
    corpus::{Corpus, HasCurrentCorpusId},
    executors::{Executor, HasObservers},
    feedbacks::map::MapNoveltiesMetadata,
    inputs::{
        BytesInput, GeneralizedInputMetadata, GeneralizedItem, HasMutatorBytes, ResizableMutator,
    },
    mark_feature_time,
    observers::{CanTrack, MapObserver, ObserversTuple},
    require_novelties_tracking,
    stages::{Restartable, RetryCountRestartHelper, Stage},
    start_timer,
    state::{HasCorpus, HasExecutions, MaybeHasClientPerfMonitor},
};

const MAX_GENERALIZED_LEN: usize = 8192;

const fn increment_by_offset(_list: &[Option<u8>], idx: usize, off: u8) -> usize {
    idx + 1 + off as usize
}

fn find_next_char(list: &[Option<u8>], mut idx: usize, ch: u8) -> usize {
    while idx < list.len() {
        if list[idx] == Some(ch) {
            return idx + 1;
        }
        idx += 1;
    }
    idx
}

/// The name for generalization stage
pub static GENERALIZATION_STAGE_NAME: &str = "generalization";

/// A stage that runs a tracer executor
#[derive(Debug, Clone)]
pub struct GeneralizationStage<C, EM, I, O, OT, S, Z> {
    name: Cow<'static, str>,
    map_observer_handle: Handle<C>,
    phantom: PhantomData<(EM, I, O, OT, S, Z)>,
}

impl<C, EM, I, O, OT, S, Z> Named for GeneralizationStage<C, EM, I, O, OT, S, Z> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<C, EM, I, O, OT, S, Z> Restartable<S> for GeneralizationStage<C, EM, I, O, OT, S, Z>
where
    S: HasMetadata + HasNamedMetadata + HasCurrentCorpusId,
{
    #[inline]
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        // TODO: We need to be able to resume better if something crashes or times out
        RetryCountRestartHelper::should_restart::<S>(state, &self.name, 3)
    }

    #[inline]
    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        // TODO: We need to be able to resume better if something crashes or times out
        RetryCountRestartHelper::clear_progress::<S>(state, &self.name)
    }
}

impl<C, E, EM, O, S, Z> Stage<E, EM, S, Z>
    for GeneralizationStage<C, EM, BytesInput, O, E::Observers, S, Z>
where
    C: CanTrack + AsRef<O> + Named,
    E: Executor<EM, BytesInput, S, Z> + HasObservers,
    E::Observers: ObserversTuple<BytesInput, S>,
    O: MapObserver,
    S: HasExecutions
        + HasMetadata
        + HasCorpus<BytesInput>
        + HasNamedMetadata
        + HasCurrentCorpusId
        + MaybeHasClientPerfMonitor,
{
    #[inline]
    #[expect(clippy::too_many_lines)]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let Some(corpus_id) = state.current_corpus_id()? else {
            return Err(Error::illegal_state(
                "state is not currently processing a corpus index",
            ));
        };

        let (mut payload, original, novelties) = {
            start_timer!(state);
            {
                let corpus = state.corpus();
                let mut testcase = corpus.get(corpus_id)?.borrow_mut();
                if testcase.scheduled_count() > 0 {
                    return Ok(());
                }

                corpus.load_input_into(&mut testcase)?;
            }
            mark_feature_time!(state, PerfFeature::GetInputFromCorpus);
            let mut entry = state.corpus().get(corpus_id)?.borrow_mut();
            let input = entry.input_mut().as_mut().unwrap();

            let payload: Vec<_> = input.mutator_bytes().iter().map(|&x| Some(x)).collect();

            if payload.len() > MAX_GENERALIZED_LEN {
                return Ok(());
            }

            let original = input.clone();
            let meta = entry.metadata_map().get::<MapNoveltiesMetadata>().ok_or_else(|| {
                    Error::key_not_found(format!(
                        "MapNoveltiesMetadata needed for GeneralizationStage not found in testcase #{corpus_id} (check the arguments of MapFeedback::new(...))"
                    ))
                })?;
            if meta.as_slice().is_empty() {
                return Ok(()); // don't generalise inputs which don't have novelties
            }
            (payload, original, meta.as_slice().to_vec())
        };

        // Do not generalized unstable inputs
        if !self.verify_input(fuzzer, executor, state, manager, &novelties, &original)? {
            return Ok(());
        }

        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            increment_by_offset,
            255,
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            increment_by_offset,
            127,
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            increment_by_offset,
            63,
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            increment_by_offset,
            31,
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            increment_by_offset,
            0,
        )?;

        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b'.',
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b';',
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b',',
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b'\n',
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b'\r',
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b'#',
        )?;
        self.find_gaps(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            find_next_char,
            b' ',
        )?;

        self.find_gaps_in_closures(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            b'(',
            b')',
        )?;
        self.find_gaps_in_closures(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            b'[',
            b']',
        )?;
        self.find_gaps_in_closures(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            b'{',
            b'}',
        )?;
        self.find_gaps_in_closures(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            b'<',
            b'>',
        )?;
        self.find_gaps_in_closures(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            b'\'',
            b'\'',
        )?;
        self.find_gaps_in_closures(
            fuzzer,
            executor,
            state,
            manager,
            &mut payload,
            &novelties,
            b'"',
            b'"',
        )?;

        // Save the modified input in the corpus
        {
            let meta = GeneralizedInputMetadata::generalized_from_options(&payload);

            assert!(meta.generalized().first() == Some(&GeneralizedItem::Gap));
            assert!(meta.generalized().last() == Some(&GeneralizedItem::Gap));

            let mut entry = state.corpus().get(corpus_id)?.borrow_mut();
            entry.metadata_map_mut().insert(meta);
        }

        Ok(())
    }
}

impl<C, EM, O, OT, S, Z> GeneralizationStage<C, EM, BytesInput, O, OT, S, Z>
where
    O: MapObserver,
    C: CanTrack + AsRef<O> + Named,
    S: HasExecutions + HasMetadata + HasCorpus<BytesInput> + MaybeHasClientPerfMonitor,
    OT: ObserversTuple<BytesInput, S>,
{
    /// Create a new [`GeneralizationStage`].
    #[must_use]
    pub fn new(map_observer: &C) -> Self {
        require_novelties_tracking!("GeneralizationStage", C);
        let name = map_observer.name().clone();
        Self {
            name: Cow::Owned(
                GENERALIZATION_STAGE_NAME.to_owned() + ":" + name.into_owned().as_str(),
            ),
            map_observer_handle: map_observer.handle(),
            phantom: PhantomData,
        }
    }

    fn verify_input<E>(
        &self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
        novelties: &[usize],
        input: &BytesInput,
    ) -> Result<bool, Error>
    where
        E: Executor<EM, BytesInput, S, Z> + HasObservers,
        E::Observers: ObserversTuple<BytesInput, S>,
    {
        start_timer!(state);
        executor.observers_mut().pre_exec_all(state, input)?;
        mark_feature_time!(state, PerfFeature::PreExecObservers);

        start_timer!(state);
        let exit_kind = executor.run_target(fuzzer, state, manager, input)?;
        mark_feature_time!(state, PerfFeature::TargetExecution);

        start_timer!(state);
        executor
            .observers_mut()
            .post_exec_all(state, input, &exit_kind)?;
        mark_feature_time!(state, PerfFeature::PostExecObservers);

        let cnt = executor.observers()[&self.map_observer_handle]
            .as_ref()
            .how_many_set(novelties);

        Ok(cnt == novelties.len())
    }

    fn trim_payload(payload: &mut Vec<Option<u8>>) {
        let mut previous = false;
        payload.retain(|&x| !(x.is_none() & core::mem::replace(&mut previous, x.is_none())));
    }

    #[expect(clippy::too_many_arguments)]
    fn find_gaps<E>(
        &self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
        payload: &mut Vec<Option<u8>>,
        novelties: &[usize],
        find_next_index: fn(&[Option<u8>], usize, u8) -> usize,
        split_char: u8,
    ) -> Result<(), Error>
    where
        E: Executor<EM, BytesInput, S, Z> + HasObservers<Observers = OT>,
    {
        let mut start = 0;
        while start < payload.len() {
            let mut end = find_next_index(payload, start, split_char);
            if end > payload.len() {
                end = payload.len();
            }
            let mut candidate = BytesInput::new(vec![]);
            candidate.extend(payload[..start].iter().flatten());
            candidate.extend(payload[end..].iter().flatten());

            if self.verify_input(fuzzer, executor, state, manager, novelties, &candidate)? {
                for item in &mut payload[start..end] {
                    *item = None;
                }
            }

            start = end;
        }

        Self::trim_payload(payload);
        Ok(())
    }

    #[expect(clippy::too_many_arguments)]
    fn find_gaps_in_closures<E>(
        &self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
        payload: &mut Vec<Option<u8>>,
        novelties: &[usize],
        opening_char: u8,
        closing_char: u8,
    ) -> Result<(), Error>
    where
        E: Executor<EM, BytesInput, S, Z> + HasObservers<Observers = OT>,
    {
        let mut index = 0;
        while index < payload.len() {
            // Find start index
            while index < payload.len() {
                if payload[index] == Some(opening_char) {
                    break;
                }
                index += 1;
            }
            let mut start = index;
            let mut end = payload.len() - 1;
            let mut endings = 0;
            // Process every ending
            while end > start {
                if payload[end] == Some(closing_char) {
                    endings += 1;
                    let mut candidate = BytesInput::new(vec![]);
                    candidate.extend(payload[..start].iter().flatten());
                    candidate.extend(payload[end..].iter().flatten());

                    if self.verify_input(fuzzer, executor, state, manager, novelties, &candidate)? {
                        for item in &mut payload[start..end] {
                            *item = None;
                        }
                    }
                    start = end;
                }
                end -= 1;
                index += 1;
            }

            if endings == 0 {
                break;
            }
        }

        Self::trim_payload(payload);
        Ok(())
    }
}
