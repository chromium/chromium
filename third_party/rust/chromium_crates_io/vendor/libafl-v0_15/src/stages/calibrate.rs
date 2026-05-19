//! The calibration stage. The fuzzer measures the average exec time and the bitmap size.

use alloc::{
    borrow::{Cow, ToOwned},
    string::ToString,
    vec::Vec,
};
use core::{fmt::Debug, marker::PhantomData, time::Duration};

use hashbrown::HashSet;
use libafl_bolts::{AsIter, Named, current_time, impl_serdeany, tuples::Handle};
use num_traits::Bounded;
use serde::{Deserialize, Serialize};

use crate::{
    Error, HasMetadata, HasNamedMetadata, HasScheduler,
    corpus::{Corpus, HasCurrentCorpusId, SchedulerTestcaseMetadata},
    events::{Event, EventFirer, EventWithStats, LogSeverity},
    executors::{Executor, ExitKind, HasObservers},
    feedbacks::{HasObserverHandle, map::MapFeedbackMetadata},
    fuzzer::Evaluator,
    inputs::Input,
    monitors::stats::{AggregatorOps, UserStats, UserStatsValue},
    observers::{MapObserver, ObserversTuple},
    schedulers::powersched::SchedulerMetadata,
    stages::{Restartable, RetryCountRestartHelper, Stage},
    state::{HasCorpus, HasCurrentTestcase, HasExecutions},
};

/// AFL++'s `CAL_CYCLES_FAST` + 1
const CAL_STAGE_START: usize = 4;
/// AFL++'s `CAL_CYCLES` + 1
const CAL_STAGE_MAX: usize = 8;

/// Default name for `CalibrationStage`; derived from AFL++
pub const CALIBRATION_STAGE_NAME: &str = "calibration";

/// The metadata to keep unstable entries
/// Formula is same as AFL++: number of unstable entries divided by the number of filled entries.
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct UnstableEntriesMetadata {
    unstable_entries: HashSet<usize>,
    filled_entries_count: usize,
}
impl_serdeany!(UnstableEntriesMetadata);

impl UnstableEntriesMetadata {
    #[must_use]
    /// Create a new [`struct@UnstableEntriesMetadata`]
    pub fn new() -> Self {
        Self {
            unstable_entries: HashSet::new(),
            filled_entries_count: 0,
        }
    }

    /// Getter
    #[must_use]
    pub fn unstable_entries(&self) -> &HashSet<usize> {
        &self.unstable_entries
    }

    /// Getter
    #[must_use]
    pub fn filled_entries_count(&self) -> usize {
        self.filled_entries_count
    }
}

impl Default for UnstableEntriesMetadata {
    fn default() -> Self {
        Self::new()
    }
}

/// Runs the target with pre and post execution hooks and returns the exit kind and duration.
pub fn run_target_with_timing<E, EM, Z, S, OT, I>(
    fuzzer: &mut Z,
    executor: &mut E,
    state: &mut S,
    mgr: &mut EM,
    input: &I,
    had_errors: bool,
) -> Result<(ExitKind, Duration, bool), Error>
where
    OT: ObserversTuple<I, S>,
    E: Executor<EM, I, S, Z> + HasObservers<Observers = OT>,
    EM: EventFirer<I, S>,
    I: Input,
    S: HasExecutions,
{
    executor.observers_mut().pre_exec_all(state, input)?;

    let start = current_time();
    let exit_kind = executor.run_target(fuzzer, state, mgr, input)?;
    let mut has_errors = had_errors;
    if exit_kind != ExitKind::Ok && !had_errors {
        mgr.log(
            state,
            LogSeverity::Warn,
            "Corpus entry errored on execution!".into(),
        )?;

        has_errors = true;
    }
    let duration = current_time()
        .checked_sub(start)
        .ok_or(Error::illegal_state(format!(
            "The time seems to have jumped in ClibrationStage! {start:?}"
        )))?;

    executor
        .observers_mut()
        .post_exec_all(state, input, &exit_kind)?;

    Ok((exit_kind, duration, has_errors))
}

/// The calibration stage will measure the average exec time and the target's stability for this input.
#[derive(Debug, Clone)]
pub struct CalibrationStage<C, I, O, OT, S> {
    map_observer_handle: Handle<C>,
    map_name: Cow<'static, str>,
    name: Cow<'static, str>,
    stage_max: usize,
    /// If we should track stability
    track_stability: bool,
    phantom: PhantomData<(I, O, OT, S)>,
}

impl<C, E, EM, I, O, OT, S, Z> Stage<E, EM, S, Z> for CalibrationStage<C, I, O, OT, S>
where
    E: Executor<EM, I, S, Z> + HasObservers<Observers = OT>,
    EM: EventFirer<I, S>,
    O: MapObserver,
    C: AsRef<O>,
    for<'de> <O as MapObserver>::Entry:
        Serialize + Deserialize<'de> + 'static + Default + Debug + Bounded,
    OT: ObserversTuple<I, S>,
    S: HasCorpus<I>
        + HasMetadata
        + HasNamedMetadata
        + HasExecutions
        + HasCurrentTestcase<I>
        + HasCurrentCorpusId,
    Z: Evaluator<E, EM, I, S> + HasScheduler<I, S>,
    I: Input,
{
    #[inline]
    #[expect(clippy::too_many_lines, clippy::cast_precision_loss)]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        mgr: &mut EM,
    ) -> Result<(), Error> {
        // Run this stage only once for each corpus entry and only if we haven't already inspected it
        {
            let testcase = state.current_testcase()?;
            // println!("calibration; corpus.scheduled_count() : {}", corpus.scheduled_count());

            if testcase.scheduled_count() > 0 {
                return Ok(());
            }
        }

        let mut iter = self.stage_max;

        // If we restarted after a timeout or crash, do less iterations.
        let input = state.current_input_cloned()?;
        let (_, mut total_time, _) =
            run_target_with_timing(fuzzer, executor, state, mgr, &input, false)?;

        let observers = &executor.observers();
        let map_first = observers[&self.map_observer_handle].as_ref();
        let map_first_filled_count = match state
            .named_metadata_map()
            .get::<MapFeedbackMetadata<O::Entry>>(&self.map_name)
        {
            Some(metadata) => metadata.num_covered_map_indexes,
            None => map_first.count_bytes().try_into().map_err(|len| {
                Error::illegal_state(
                    format!(
                        "map's filled entry count ({}) is greater than usize::MAX ({})",
                        len,
                        usize::MAX,
                    )
                    .as_str(),
                )
            })?,
        };
        let map_first_entries = map_first.to_vec();
        let map_first_len = map_first.to_vec().len();
        let mut unstable_entries: Vec<usize> = vec![];
        // Run CAL_STAGE_START - 1 times, increase by 2 for every time a new
        // run is found to be unstable or to crash with CAL_STAGE_MAX total runs.
        let mut i = 1;
        let mut has_errors = false;

        while i < iter {
            let (exit_kind, duration, has_errors_result) =
                run_target_with_timing(fuzzer, executor, state, mgr, &input, has_errors)?;
            has_errors = has_errors_result;

            total_time += duration;

            if self.track_stability && exit_kind != ExitKind::Timeout {
                let map = &executor.observers()[&self.map_observer_handle]
                    .as_ref()
                    .to_vec();

                let map_state = state
                    .named_metadata_map_mut()
                    .get_mut::<MapFeedbackMetadata<O::Entry>>(&self.map_name)
                    .unwrap();
                let history_map = &mut map_state.history_map;

                if history_map.len() < map_first_len {
                    history_map.resize(map_first_len, O::Entry::default());
                }

                for (idx, (first, (cur, history))) in map_first_entries
                    .iter()
                    .zip(map.iter().zip(history_map.iter_mut()))
                    .enumerate()
                {
                    if *first != *cur && *history != O::Entry::max_value() {
                        // If we just hit a history map entry that was not covered before, but is now flagged as flaky,
                        // we need to make sure the `num_covered_map_indexes` is kept in sync.
                        map_state.num_covered_map_indexes +=
                            usize::from(*history == O::Entry::default());
                        *history = O::Entry::max_value();
                        unstable_entries.push(idx);
                    }
                }

                if !unstable_entries.is_empty() && iter < CAL_STAGE_MAX {
                    iter += 2;
                }
            }
            i += 1;
        }

        let mut send_default_stability = false;
        let unstable_found = !unstable_entries.is_empty();
        if unstable_found {
            let metadata = state.metadata_or_insert_with(UnstableEntriesMetadata::new);

            // If we see new unstable entries executing this new corpus entries, then merge with the existing one
            for item in unstable_entries {
                metadata.unstable_entries.insert(item); // Insert newly found items
            }
            metadata.filled_entries_count = map_first_filled_count;
        } else if !state.has_metadata::<UnstableEntriesMetadata>() && map_first_filled_count > 0 {
            send_default_stability = true;
            state.add_metadata(UnstableEntriesMetadata::new());
        }

        // If weighted scheduler or powerscheduler is used, update it
        if state.has_metadata::<SchedulerMetadata>() {
            let observers = executor.observers();
            let map = observers[&self.map_observer_handle].as_ref();

            let bitmap_size = map.count_bytes();

            if bitmap_size < 1 {
                return Err(Error::invalid_corpus(
                    "This testcase does not trigger any edges. Check your instrumentation!"
                        .to_string(),
                ));
            }

            let psmeta = state
                .metadata_map_mut()
                .get_mut::<SchedulerMetadata>()
                .unwrap();
            let handicap = psmeta.queue_cycles();

            psmeta.set_exec_time(psmeta.exec_time() + total_time);
            psmeta.set_cycles(psmeta.cycles() + (iter as u64));
            psmeta.set_bitmap_size(psmeta.bitmap_size() + bitmap_size);
            psmeta.set_bitmap_size_log(psmeta.bitmap_size_log() + libm::log2(bitmap_size as f64));
            psmeta.set_bitmap_entries(psmeta.bitmap_entries() + 1);

            let mut testcase = state.current_testcase_mut()?;

            testcase.set_exec_time(total_time / (iter as u32));
            // log::trace!("time: {:#?}", testcase.exec_time());

            // If the testcase doesn't have its own `SchedulerTestcaseMetadata`, create it.
            let data = if let Ok(metadata) = testcase.metadata_mut::<SchedulerTestcaseMetadata>() {
                metadata
            } else {
                let depth = match testcase.parent_id() {
                    Some(parent_id) => {
                        match (*state.corpus().get(parent_id)?)
                            .borrow()
                            .metadata_map()
                            .get::<SchedulerTestcaseMetadata>()
                        {
                            Some(parent_metadata) => parent_metadata.depth() + 1,
                            _ => 0,
                        }
                    }
                    _ => 0,
                };
                testcase.add_metadata(SchedulerTestcaseMetadata::new(depth));
                testcase
                    .metadata_mut::<SchedulerTestcaseMetadata>()
                    .unwrap()
            };

            data.set_cycle_and_time((total_time, iter));
            data.set_bitmap_size(bitmap_size);
            data.set_handicap(handicap);
        }

        // Send the stability event to the broker
        if unstable_found {
            if let Some(meta) = state.metadata_map().get::<UnstableEntriesMetadata>() {
                let unstable_entries = meta.unstable_entries().len();
                debug_assert_ne!(
                    map_first_filled_count, 0,
                    "The map's filled count must never be 0"
                );
                // In theory `map_first_filled_count - unstable_entries` could be negative.
                // Because `map_first_filled_count` is the filled count of just one single run.
                // While the `unstable_entries` is the number of all the unstable entries across multiple runs.
                // If the target is very unstable (~100%) then this would hit more edges than `map_first_filled_count`.
                // But even in that case, we don't allow negative stability and just show 0% here.
                let stable_count: u64 =
                    map_first_filled_count.saturating_sub(unstable_entries) as u64;
                mgr.fire(
                    state,
                    EventWithStats::with_current_time(
                        Event::UpdateUserStats {
                            name: Cow::from("stability"),
                            value: UserStats::new(
                                UserStatsValue::Ratio(stable_count, map_first_filled_count as u64),
                                AggregatorOps::Avg,
                            ),
                            phantom: PhantomData,
                        },
                        *state.executions(),
                    ),
                )?;
            }
        } else if send_default_stability {
            mgr.fire(
                state,
                EventWithStats::with_current_time(
                    Event::UpdateUserStats {
                        name: Cow::from("stability"),
                        value: UserStats::new(
                            UserStatsValue::Ratio(
                                map_first_filled_count as u64,
                                map_first_filled_count as u64,
                            ),
                            AggregatorOps::Avg,
                        ),
                        phantom: PhantomData,
                    },
                    *state.executions(),
                ),
            )?;
        }

        Ok(())
    }
}

impl<C, I, O, OT, S> Restartable<S> for CalibrationStage<C, I, O, OT, S>
where
    S: HasMetadata + HasNamedMetadata + HasCurrentCorpusId,
{
    fn should_restart(&mut self, state: &mut S) -> Result<bool, Error> {
        // Calibration stage disallow restarts
        // If a testcase that causes crash/timeout in the queue, we need to remove it from the queue immediately.
        RetryCountRestartHelper::no_retry(state, &self.name)

        // todo
        // remove this guy from corpus queue
    }

    fn clear_progress(&mut self, state: &mut S) -> Result<(), Error> {
        // TODO: Make sure this is the correct way / there may be a better way?
        RetryCountRestartHelper::clear_progress(state, &self.name)
    }
}

impl<C, I, O, OT, S> CalibrationStage<C, I, O, OT, S>
where
    C: AsRef<O>,
    O: MapObserver,
    for<'it> O: AsIter<'it, Item = O::Entry>,
    OT: ObserversTuple<I, S>,
{
    /// Create a new [`CalibrationStage`].
    #[must_use]
    pub fn new<F>(map_feedback: &F) -> Self
    where
        F: HasObserverHandle<Observer = C> + Named,
    {
        let map_name = map_feedback.name().clone();
        Self {
            map_observer_handle: map_feedback.observer_handle().clone(),
            map_name: map_name.clone(),
            stage_max: CAL_STAGE_START,
            track_stability: true,
            phantom: PhantomData,
            name: Cow::Owned(
                CALIBRATION_STAGE_NAME.to_owned() + ":" + map_name.into_owned().as_str(),
            ),
        }
    }

    /// Create a new [`CalibrationStage`], but without checking stability.
    #[must_use]
    pub fn ignore_stability<F>(map_feedback: &F) -> Self
    where
        F: HasObserverHandle<Observer = C> + Named,
    {
        let mut ret = Self::new(map_feedback);
        ret.track_stability = false;
        ret
    }
}

impl<C, I, O, OT, S> Named for CalibrationStage<C, I, O, OT, S> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}
