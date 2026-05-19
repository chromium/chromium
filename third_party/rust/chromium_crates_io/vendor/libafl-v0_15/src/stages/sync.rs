//! The [`SyncFromDiskStage`] is a stage that imports inputs from disk for e.g. sync with AFL

use alloc::{
    borrow::{Cow, ToOwned},
    vec::Vec,
};
use core::{marker::PhantomData, time::Duration};
use std::path::{Path, PathBuf};

use libafl_bolts::{
    Named, current_time,
    fs::find_new_files_rec,
    shmem::{ShMem, ShMemProvider},
};
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata, HasNamedMetadata,
    corpus::{Corpus, CorpusId, HasCurrentCorpusId},
    events::{Event, EventConfig, EventFirer, EventWithStats, llmp::LlmpEventConverter},
    executors::{Executor, ExitKind, HasObservers},
    fuzzer::{Evaluator, EvaluatorObservers, ExecutionProcessor, HasObjective},
    inputs::{Input, InputConverter},
    stages::{Restartable, RetryCountRestartHelper, Stage},
    state::{
        HasCorpus, HasCurrentTestcase, HasExecutions, HasRand, HasSolutions,
        MaybeHasClientPerfMonitor, Stoppable,
    },
};

/// Default name for `SyncFromDiskStage`; derived from AFL++
pub const SYNC_FROM_DISK_STAGE_NAME: &str = "sync";

/// Metadata used to store information about disk sync time
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
#[derive(Serialize, Deserialize, Debug)]
pub struct SyncFromDiskMetadata {
    /// The last time the sync was done
    pub last_time: Duration,
    /// The paths that are left to sync
    pub left_to_sync: Vec<PathBuf>,
}

libafl_bolts::impl_serdeany!(SyncFromDiskMetadata);

impl SyncFromDiskMetadata {
    /// Create a new [`struct@SyncFromDiskMetadata`]
    #[must_use]
    pub fn new(last_time: Duration, left_to_sync: Vec<PathBuf>) -> Self {
        Self {
            last_time,
            left_to_sync,
        }
    }
}

/// A stage that loads testcases from disk to sync with other fuzzers such as AFL++
/// When syncing, the stage will ignore [`Error::InvalidInput`] and will skip the file.
#[derive(Debug)]
pub struct SyncFromDiskStage<CB, E, EM, I, S, Z> {
    name: Cow<'static, str>,
    sync_dirs: Vec<PathBuf>,
    load_callback: CB,
    interval: Duration,
    phantom: PhantomData<(E, EM, I, S, Z)>,
}

impl<CB, E, EM, I, S, Z> Named for SyncFromDiskStage<CB, E, EM, I, S, Z> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<CB, E, EM, I, S, Z> Stage<E, EM, S, Z> for SyncFromDiskStage<CB, E, EM, I, S, Z>
where
    CB: FnMut(&mut Z, &mut S, &Path) -> Result<I, Error>,
    Z: Evaluator<E, EM, I, S>,
    S: HasCorpus<I>
        + HasRand
        + HasMetadata
        + HasNamedMetadata
        + HasCurrentCorpusId
        + MaybeHasClientPerfMonitor,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let last = state
            .metadata_map()
            .get::<SyncFromDiskMetadata>()
            .map(|m| m.last_time);

        if let Some(last) = last {
            if current_time().saturating_sub(last) < self.interval {
                return Ok(());
            }
        }

        let new_max_time = current_time();

        let mut new_files = vec![];
        for dir in &self.sync_dirs {
            log::debug!("Syncing from dir: {}", dir.display());
            let new_dir_files = find_new_files_rec(dir, &last)?;
            new_files.extend(new_dir_files);
        }

        let sync_from_disk_metadata = state
            .metadata_or_insert_with(|| SyncFromDiskMetadata::new(new_max_time, new_files.clone()));

        // At the very first sync, last_time and file_to_sync are set twice
        sync_from_disk_metadata.last_time = new_max_time;
        sync_from_disk_metadata.left_to_sync = new_files;

        // Iterate over the paths of files left to sync.
        // By keeping track of these files, we ensure that no file is missed during synchronization,
        // even in the event of a target restart.
        let to_sync = sync_from_disk_metadata.left_to_sync.clone();
        log::debug!("Number of files to sync: {:?}", to_sync.len());
        for path in to_sync {
            // Removing each path from the `left_to_sync` Vec before evaluating
            // prevents duplicate processing and ensures that each file is evaluated only once. This approach helps
            // avoid potential infinite loops that may occur if a file is an objective or an invalid input.
            state
                .metadata_mut::<SyncFromDiskMetadata>()
                .unwrap()
                .left_to_sync
                .retain(|p| p != &path);
            let input = match (self.load_callback)(fuzzer, state, &path) {
                Ok(input) => input,
                Err(Error::InvalidInput(reason, _)) => {
                    log::warn!(
                        "Invalid input found in {} when syncing; reason {reason}; skipping;",
                        path.display()
                    );
                    continue;
                }
                Err(e) => return Err(e),
            };
            log::debug!("Syncing and evaluating {}", path.display());
            fuzzer.evaluate_input(state, executor, manager, &input)?;
        }

        Ok(())
    }
}

impl<CB, E, EM, I, S, Z> Restartable<S> for SyncFromDiskStage<CB, E, EM, I, S, Z>
where
    S: HasMetadata + HasNamedMetadata + HasCurrentCorpusId,
{
    #[inline]
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        // TODO: Needs proper crash handling for when an imported testcase crashes
        // For now, Make sure we don't get stuck crashing on this testcase
        RetryCountRestartHelper::no_retry(state, &self.name)
    }

    #[inline]
    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        RetryCountRestartHelper::clear_progress(state, &self.name)
    }
}

impl<CB, E, EM, I, S, Z> SyncFromDiskStage<CB, E, EM, I, S, Z> {
    /// Creates a new [`SyncFromDiskStage`]
    /// To skip a file, you can return [`Error::invalid_input()`] from the provided `load_callback`
    #[must_use]
    pub fn new(sync_dirs: Vec<PathBuf>, load_callback: CB, interval: Duration, name: &str) -> Self {
        Self {
            name: Cow::Owned(SYNC_FROM_DISK_STAGE_NAME.to_owned() + ":" + name),
            phantom: PhantomData,
            sync_dirs,
            interval,
            load_callback,
        }
    }
}

/// Function type when the callback in `SyncFromDiskStage` is not a lambda
pub type SyncFromDiskFunction<I, S, Z> = fn(&mut Z, &mut S, &Path) -> Result<I, Error>;

impl<E, EM, I, S, Z> SyncFromDiskStage<SyncFromDiskFunction<I, S, Z>, E, EM, I, S, Z>
where
    I: Input,
    S: HasCorpus<I>,
    Z: Evaluator<E, EM, I, S>,
{
    /// Creates a new [`SyncFromDiskStage`] invoking `Input::from_file` to load inputs
    #[must_use]
    pub fn with_from_file(sync_dirs: Vec<PathBuf>, interval: Duration) -> Self {
        fn load_callback<I, S, Z>(_: &mut Z, _: &mut S, p: &Path) -> Result<I, Error>
        where
            I: Input,
            S: HasCorpus<I>,
        {
            Input::from_file(p)
        }
        Self {
            interval,
            name: Cow::Borrowed(SYNC_FROM_DISK_STAGE_NAME),
            sync_dirs,
            load_callback: load_callback::<_, _, _>,
            phantom: PhantomData,
        }
    }
}

/// Metadata used to store information about the last sent testcase with `SyncFromBrokerStage`
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
#[derive(Serialize, Deserialize, Debug)]
pub struct SyncFromBrokerMetadata {
    /// The `CorpusId` of the last sent testcase
    pub last_id: Option<CorpusId>,
}

libafl_bolts::impl_serdeany!(SyncFromBrokerMetadata);

impl SyncFromBrokerMetadata {
    /// Create a new [`struct@SyncFromBrokerMetadata`]
    #[must_use]
    pub fn new(last_id: Option<CorpusId>) -> Self {
        Self { last_id }
    }
}

/// A stage that loads testcases from disk to sync with other fuzzers such as AFL++
#[derive(Debug)]
pub struct SyncFromBrokerStage<I, IC, ICB, S, SHM, SP> {
    client: LlmpEventConverter<I, IC, ICB, S, SHM, SP>,
}

impl<E, EM, I, IC, ICB, DI, S, SHM, SP, Z> Stage<E, EM, S, Z>
    for SyncFromBrokerStage<I, IC, ICB, S, SHM, SP>
where
    DI: Input,
    EM: EventFirer<I, S>,
    E: HasObservers + Executor<EM, I, S, Z>,
    for<'a> E::Observers: Deserialize<'a>,
    I: Input + Clone,
    IC: InputConverter<From = I, To = DI>,
    ICB: InputConverter<From = DI, To = I>,
    S: HasExecutions
        + HasRand
        + HasMetadata
        + HasSolutions<I>
        + HasCurrentTestcase<I>
        + Stoppable
        + MaybeHasClientPerfMonitor,
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
    Z: EvaluatorObservers<E, EM, I, S> + ExecutionProcessor<EM, I, E::Observers, S> + HasObjective,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        if self.client.can_convert() {
            let last_id = state
                .metadata_map()
                .get::<SyncFromBrokerMetadata>()
                .and_then(|m| m.last_id);

            let mut cur_id =
                last_id.map_or_else(|| state.corpus().first(), |id| state.corpus().next(id));

            while let Some(id) = cur_id {
                let input = state.corpus().cloned_input_for_id(id)?;

                self.client.fire(
                    state,
                    EventWithStats::with_current_time(
                        Event::NewTestcase {
                            input,
                            observers_buf: None,
                            exit_kind: ExitKind::Ok,
                            corpus_size: 0, // TODO choose if sending 0 or the actual real value
                            client_config: EventConfig::AlwaysUnique,
                            forward_id: None,
                            #[cfg(all(unix, feature = "multi_machine"))]
                            node_id: None,
                        },
                        *state.executions(),
                    ),
                )?;

                cur_id = state.corpus().next(id);
            }

            let last = state.corpus().last();
            if last_id.is_none() {
                state
                    .metadata_map_mut()
                    .insert(SyncFromBrokerMetadata::new(last));
            } else {
                state
                    .metadata_map_mut()
                    .get_mut::<SyncFromBrokerMetadata>()
                    .unwrap()
                    .last_id = last;
            }
        }

        self.client.process(fuzzer, state, executor, manager)?;
        Ok(())
    }
}

impl<I, IC, ICB, S, SHM, SP> Restartable<S> for SyncFromBrokerStage<I, IC, ICB, S, SHM, SP> {
    #[inline]
    fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
        // No restart handling needed - does not execute the target.
        Ok(true)
    }

    #[inline]
    fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        // Not needed - does not execute the target.
        Ok(())
    }
}

impl<I, IC, ICB, S, SHM, SP> SyncFromBrokerStage<I, IC, ICB, S, SHM, SP> {
    /// Creates a new [`SyncFromBrokerStage`]
    #[must_use]
    pub fn new(client: LlmpEventConverter<I, IC, ICB, S, SHM, SP>) -> Self {
        Self { client }
    }
}
