//! Stage to compute and report AFL++ stats
use alloc::{borrow::Cow, string::String, vec::Vec};
use core::{
    fmt::{Debug, Display},
    marker::PhantomData,
    time::Duration,
};
use std::{
    fs::{File, OpenOptions},
    io::{BufRead, BufReader, Write},
    path::{Path, PathBuf},
    process,
};

#[cfg(unix)]
use libafl_bolts::os::peak_rss_mb_child_processes;
use libafl_bolts::{
    Named,
    core_affinity::CoreId,
    current_time,
    tuples::{Handle, MatchName},
};
use serde::{Deserialize, Serialize};

#[cfg(feature = "track_hit_feedbacks")]
use crate::feedbacks::{CRASH_FEEDBACK_NAME, TIMEOUT_FEEDBACK_NAME};
use crate::{
    Error, HasMetadata, HasNamedMetadata, HasScheduler,
    corpus::{Corpus, HasCurrentCorpusId, SchedulerTestcaseMetadata, Testcase},
    events::{Event, EventFirer, EventWithStats},
    executors::HasObservers,
    feedbacks::{HasObserverHandle, MapFeedbackMetadata},
    monitors::stats::{AggregatorOps, UserStats, UserStatsValue},
    mutators::Tokens,
    observers::MapObserver,
    schedulers::{HasQueueCycles, minimizer::IsFavoredMetadata},
    stages::{Restartable, Stage, calibrate::UnstableEntriesMetadata},
    state::{HasCorpus, HasExecutions, HasImported, HasStartTime, Stoppable},
    std::string::ToString,
};

/// AFL++'s default stats update interval
pub const AFL_FUZZER_STATS_UPDATE_INTERVAL_SECS: u64 = 60;

/// `CalibrationTime` - Use in conjunction with `TimeTrackingFeedback`
#[derive(Debug, Serialize, Deserialize)]
pub struct CalibrationTime(pub Duration);
impl From<Duration> for CalibrationTime {
    fn from(value: Duration) -> Self {
        Self(value)
    }
}

libafl_bolts::impl_serdeany!(CalibrationTime);

/// `SyncTime` - Use in conjunction with `TimeTrackingFeedback`
#[derive(Debug, Serialize, Deserialize)]
pub struct SyncTime(pub Duration);
impl From<Duration> for SyncTime {
    fn from(value: Duration) -> Self {
        Self(value)
    }
}

libafl_bolts::impl_serdeany!(SyncTime);

/// `FuzzTime` - Use in conjunction with `TimeTrackingFeedback`
#[derive(Debug, Serialize, Deserialize)]
pub struct FuzzTime(pub Duration);
impl From<Duration> for FuzzTime {
    fn from(value: Duration) -> Self {
        Self(value)
    }
}

libafl_bolts::impl_serdeany!(FuzzTime);

/// The [`AflStatsStage`] is a Stage that calculates and writes
/// AFL++'s `fuzzer_stats` and `plot_data` information.
#[derive(Debug, Clone)]
pub struct AflStatsStage<C, I, O> {
    map_observer_handle: Handle<C>,
    map_name: Cow<'static, str>,
    stats_file_path: Option<PathBuf>,
    plot_file_path: Option<PathBuf>,
    start_time: u64,
    // the number of testcases that have been fuzzed
    has_fuzzed_size: usize,
    // the number of "favored" testcases
    is_favored_size: usize,
    // the last time that we report all stats
    last_report_time: Duration,
    // the interval at which we report all stats
    stats_report_interval: Duration,
    pid: u32,
    slowest_exec: Duration,
    max_depth: u64,
    cycles_done: u64,
    saved_crashes: u64,
    saved_hangs: u64,
    last_find: Duration,
    last_hang: Duration,
    last_crash: Duration,
    exec_timeout: u64,
    execs_at_last_objective: u64,
    cycles_wo_finds: u64,
    /// banner text (e.g., the target name)
    afl_banner: Cow<'static, str>,
    /// the version of libafl-fuzz used
    afl_version: Cow<'static, str>,
    /// default, persistent, qemu, unicorn, non-instrumented
    target_mode: Cow<'static, str>,
    /// full command line used for the fuzzing session
    command_line: Cow<'static, str>,
    /// Amount of tokens provided by the user. Used to determine autotokens count.
    dict_count: usize,
    /// autotokens are enabled
    autotokens_enabled: bool,
    /// The core we are bound to
    core_id: CoreId,
    phantom: PhantomData<(I, O)>,
}

/// AFL++'s `fuzzer_stats`
#[derive(Debug, Clone)]
pub struct AflFuzzerStats<'a> {
    /// unix time indicating the start time of afl-fuzz
    start_time: u64,
    /// unix time corresponding to the last interval
    last_update: u64,
    /// run time in seconds to the last update of this file
    run_time: u64,
    /// process id of the fuzzer process
    fuzzer_pid: u32,
    /// queue cycles completed so far
    cycles_done: u64,
    /// number of queue cycles without any new paths found
    cycles_wo_find: u64,
    /// longest time in seconds no new path was found
    time_wo_finds: u64,
    /// Time spent fuzzing
    fuzz_time: u64,
    /// Time spent calibrating inputs
    calibration_time: u64,
    /// Time spent syncing with foreign fuzzers
    /// NOTE: Syncing between our own instances is not counted.
    sync_time: u64,
    /// TODO
    trim_time: u64,
    /// number of fuzzer executions attempted (what does attempted mean here?)
    execs_done: u64,
    /// overall number of execs per second
    execs_per_sec: u64,
    /// TODO
    execs_ps_last_min: u64,
    /// total number of entries in the queue
    corpus_count: usize,
    /// number of queue entries that are favored
    corpus_favored: usize,
    /// number of entries discovered through local fuzzing
    corpus_found: usize,
    /// number of entries imported from other instances
    corpus_imported: usize,
    /// number of levels in the generated data set
    max_depth: u64,
    /// currently processed entry number
    cur_item: usize,
    /// number of favored entries still waiting to be fuzzed
    pending_favs: usize,
    /// number of all entries waiting to be fuzzed
    pending_total: usize,
    /// number of test cases showing variable behavior
    corpus_variable: u64,
    /// percentage of bitmap bytes that behave consistently
    stability: f64,
    /// percentage of edge coverage found in the map so far,
    bitmap_cvg: f64,
    /// number of unique crashes recorded
    saved_crashes: u64,
    /// number of unique hangs encountered
    saved_hangs: u64,
    /// seconds since the last find was found
    last_find: Duration,
    /// seconds since the last crash was found
    last_crash: Duration,
    /// seconds since the last hang was found
    last_hang: Duration,
    /// execs since the last crash was found
    execs_since_crash: u64,
    /// the -t command line value
    exec_timeout: u64,
    /// real time of the slowest execution in ms
    slowest_exec_ms: u128,
    /// max rss usage reached during fuzzing in MB
    peak_rss_mb: i64,
    /// TODO
    cpu_affinity: usize,
    /// how many edges have been found
    edges_found: usize,
    /// Size of our edges map
    total_edges: usize,
    /// how many edges are non-deterministic
    var_byte_count: usize,
    /// TODO:
    havoc_expansion: usize,
    /// Amount of automatic dict entries found
    auto_dict_entries: usize,
    /// TODO:
    testcache_size: usize,
    /// TODO:
    testcache_count: usize,
    /// TODO:
    testcache_evict: usize,
    /// banner text (e.g., the target name)
    afl_banner: &'a Cow<'static, str>,
    /// the version of AFL++ used
    afl_version: &'a Cow<'static, str>,
    /// default, persistent, qemu, unicorn, non-instrumented
    target_mode: &'a Cow<'static, str>,
    /// full command line used for the fuzzing session
    command_line: &'a str,
}
/// AFL++'s `plot_data`
#[derive(Debug, Clone)]
pub struct AFLPlotData<'a> {
    relative_time: &'a u64,
    cycles_done: &'a u64,
    cur_item: &'a usize,
    corpus_count: &'a usize,
    pending_total: &'a usize,
    pending_favs: &'a usize,
    /// Note: renamed `map_size` -> `total_edges` for consistency with `fuzzer_stats`
    total_edges: &'a usize,
    saved_crashes: &'a u64,
    saved_hangs: &'a u64,
    max_depth: &'a u64,
    execs_per_sec: &'a u64,
    /// Note: renamed `total_execs` -> `execs_done` for consistency with `fuzzer_stats`
    execs_done: &'a u64,
    edges_found: &'a usize,
}

impl<C, E, EM, I, O, S, Z> Stage<E, EM, S, Z> for AflStatsStage<C, I, O>
where
    C: AsRef<O> + Named,
    E: HasObservers,
    <E as HasObservers>::Observers: MatchName,
    EM: EventFirer<I, S>,
    Z: HasScheduler<I, S>,
    S: HasImported
        + HasCorpus<I>
        + HasMetadata
        + HasStartTime
        + HasExecutions
        + HasNamedMetadata
        + Stoppable
        + HasCurrentCorpusId,
    O: MapObserver,
    for<'de> <O as MapObserver>::Entry: Serialize + Deserialize<'de> + 'static + Debug,
    C: AsRef<O> + Named,
    Z::Scheduler: HasQueueCycles,
{
    #[expect(clippy::too_many_lines)]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        executor: &mut E,
        state: &mut S,
        manager: &mut EM,
    ) -> Result<(), Error> {
        let Some(corpus_idx) = state.current_corpus_id()? else {
            return Err(Error::illegal_state(
                "state is not currently processing a corpus index",
            ));
        };
        let testcase = state.corpus().get(corpus_idx)?.borrow();
        // NOTE: scheduled_count represents the amount of fuzz runs a
        // testcase has had. Since this stage is kept at the very end of stage list,
        // the entry would have been fuzzed already (and should contain IsFavoredMetadata) but would have a scheduled count of zero
        // since the scheduled count is incremented after all stages have been run.
        if testcase.scheduled_count() == 0 {
            // New testcase!
            self.cycles_wo_finds = 0;
            self.update_last_find();
            #[cfg(feature = "track_hit_feedbacks")]
            {
                self.maybe_update_last_crash(&testcase, state);
                self.maybe_update_last_hang(&testcase, state);
            }
            self.update_has_fuzzed_size();
            self.maybe_update_is_favored_size(&testcase);
        }
        self.maybe_update_slowest_exec(&testcase);
        self.maybe_update_max_depth(&testcase);

        // See if we actually need to run the stage, if not, avoid dynamic value computation.
        if !self.check_interval() {
            return Ok(());
        }

        let corpus_size = state.corpus().count();
        let total_executions = *state.executions();

        let scheduler = fuzzer.scheduler();
        let queue_cycles = scheduler.queue_cycles();
        self.maybe_update_cycles(queue_cycles);
        self.maybe_update_cycles_wo_finds(queue_cycles);

        let map_feedback = state
            .named_metadata_map()
            .get::<MapFeedbackMetadata<<O as MapObserver>::Entry>>(&self.map_name)
            .unwrap();

        let filled_entries_in_map = map_feedback.num_covered_map_indexes;
        let observers = executor.observers();
        let map = observers[&self.map_observer_handle].as_ref();
        let map_size = map.usable_count();

        // Since we do not calibrate when using `QueueScheduler`; we cannot calculate unstable entries.
        let unstable_entries_in_map = state
            .metadata_map()
            .get::<UnstableEntriesMetadata>()
            .map_or(0, |m| m.unstable_entries().len());

        let auto_dict_entries = if self.autotokens_enabled {
            state
                .metadata::<Tokens>()?
                .len()
                .saturating_sub(self.dict_count)
        } else {
            0
        };
        let stats = AflFuzzerStats {
            start_time: self.start_time,
            last_update: self.last_report_time.as_secs(),
            run_time: self.last_report_time.as_secs() - self.start_time,
            fuzzer_pid: self.pid,
            cycles_done: queue_cycles,
            cycles_wo_find: self.cycles_wo_finds,
            fuzz_time: state
                .metadata::<FuzzTime>()
                .map_or(Duration::from_secs(0), |d| d.0)
                .as_secs(),
            calibration_time: state
                .metadata::<CalibrationTime>()
                .map_or(Duration::from_secs(0), |d| d.0)
                .as_secs(),
            sync_time: state
                .metadata::<SyncTime>()
                .map_or(Duration::from_secs(0), |d| d.0)
                .as_secs(),
            trim_time: 0, // TODO
            execs_done: total_executions,
            execs_per_sec: *state.executions(),     // TODO
            execs_ps_last_min: *state.executions(), // TODO
            max_depth: self.max_depth,
            corpus_count: corpus_size,
            corpus_favored: corpus_size - self.is_favored_size,
            corpus_found: corpus_size - state.imported(),
            corpus_imported: *state.imported(),
            cur_item: corpus_idx.into(),
            pending_total: corpus_size - self.has_fuzzed_size,
            pending_favs: 0, // TODO
            time_wo_finds: current_time()
                .checked_sub(self.last_find)
                .unwrap_or_default()
                .as_secs(),
            corpus_variable: 0,
            stability: self.calculate_stability(unstable_entries_in_map, filled_entries_in_map),
            #[expect(clippy::cast_precision_loss)]
            bitmap_cvg: (filled_entries_in_map as f64 / map_size as f64) * 100.0,
            saved_crashes: self.saved_crashes,
            saved_hangs: self.saved_hangs,
            last_find: self.last_find,
            last_hang: self.last_hang,
            last_crash: self.last_crash,
            execs_since_crash: total_executions - self.execs_at_last_objective,
            exec_timeout: self.exec_timeout,
            slowest_exec_ms: self.slowest_exec.as_millis(),
            // TODO: getting rss_mb may take some extra millis, so might make sense to make this optional
            #[cfg(unix)]
            peak_rss_mb: peak_rss_mb_child_processes()?,
            #[cfg(not(unix))]
            peak_rss_mb: 0, // TODO for Windows
            cpu_affinity: self.core_id.0,
            total_edges: map_size,
            edges_found: filled_entries_in_map,
            var_byte_count: unstable_entries_in_map,
            havoc_expansion: 0, // TODO
            auto_dict_entries,
            testcache_size: 0,
            testcache_count: 0,
            testcache_evict: 0,
            afl_banner: &self.afl_banner,
            afl_version: &self.afl_version,
            target_mode: &self.target_mode,
            command_line: &self.command_line,
        };
        let plot_data = AFLPlotData {
            corpus_count: &stats.corpus_count,
            cur_item: &stats.cur_item,
            cycles_done: &stats.cycles_done,
            edges_found: &stats.edges_found,
            total_edges: &stats.total_edges,
            execs_per_sec: &stats.execs_per_sec,
            pending_total: &stats.pending_total,
            pending_favs: &stats.pending_favs,
            max_depth: &stats.max_depth,
            relative_time: &stats.run_time,
            saved_hangs: &stats.saved_hangs,
            saved_crashes: &stats.saved_crashes,
            execs_done: &stats.execs_done,
        };
        self.maybe_write_fuzzer_stats(&stats)?;
        if self.plot_file_path.is_some() {
            self.write_plot_data(&plot_data)?;
        }

        drop(testcase);

        // We construct this simple json by hand to squeeze out some extra speed.
        let json = format!(
            "{{\
                \"pending\":{},\
                \"pending_fav\":{},\
                \"own_finds\":{},\
                \"imported\":{}\
            }}",
            stats.pending_total, stats.pending_favs, stats.corpus_found, stats.corpus_imported
        );

        manager.fire(
            state,
            EventWithStats::with_current_time(
                Event::UpdateUserStats {
                    name: Cow::Borrowed("AflStats"),
                    value: UserStats::new(
                        UserStatsValue::String(Cow::Owned(json)),
                        AggregatorOps::None,
                    ),
                    phantom: PhantomData,
                },
                *state.executions(),
            ),
        )?;

        Ok(())
    }
}

impl<C, I, O, S> Restartable<S> for AflStatsStage<C, I, O> {
    fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
        Ok(true)
    }

    fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        Ok(())
    }
}

impl<C, I, O> AflStatsStage<C, I, O>
where
    C: Named,
{
    /// Builder for `AflStatsStage`
    #[must_use]
    pub fn builder() -> AflStatsStageBuilder<C, I, O> {
        AflStatsStageBuilder::new()
    }

    /// Writes a stats file, if a `stats_file_path` is set.
    fn maybe_write_fuzzer_stats(&self, stats: &AflFuzzerStats) -> Result<(), Error> {
        if let Some(stats_file_path) = &self.stats_file_path {
            let tmp_file = stats_file_path
                .parent()
                .expect("fuzzer_stats file must have a parent!")
                .join(".fuzzer_stats_tmp");
            std::fs::write(&tmp_file, stats.to_string())?;
            _ = std::fs::copy(&tmp_file, stats_file_path)?;
            std::fs::remove_file(tmp_file)?;
        }
        Ok(())
    }

    fn write_plot_data(&self, plot_data: &AFLPlotData) -> Result<(), Error> {
        let mut file = OpenOptions::new().append(true).open(
            self.plot_file_path
                .as_ref()
                .expect("invariant; should never occur"),
        )?;
        writeln!(file, "{plot_data}")?;
        Ok(())
    }

    fn maybe_update_is_favored_size(&mut self, testcase: &Testcase<I>) {
        if testcase.has_metadata::<IsFavoredMetadata>() {
            self.is_favored_size += 1;
        }
    }

    fn maybe_update_slowest_exec(&mut self, testcase: &Testcase<I>) {
        if let Some(exec_time) = testcase.exec_time() {
            if exec_time > &self.slowest_exec {
                self.slowest_exec = *exec_time;
            }
        }
    }

    fn update_has_fuzzed_size(&mut self) {
        self.has_fuzzed_size += 1;
    }

    fn maybe_update_max_depth(&mut self, testcase: &Testcase<I>) {
        if let Ok(metadata) = testcase.metadata::<SchedulerTestcaseMetadata>() {
            if metadata.depth() > self.max_depth {
                self.max_depth = metadata.depth();
            }
        }
    }

    fn update_last_find(&mut self) {
        self.last_find = current_time();
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn maybe_update_last_crash<S>(&mut self, testcase: &Testcase<I>, state: &S)
    where
        S: HasExecutions,
    {
        #[cfg(feature = "track_hit_feedbacks")]
        if testcase
            .hit_objectives()
            .contains(&Cow::Borrowed(CRASH_FEEDBACK_NAME))
        {
            self.last_crash = current_time();
            self.execs_at_last_objective = *state.executions();
        }
    }

    #[cfg(feature = "track_hit_feedbacks")]
    fn maybe_update_last_hang<S>(&mut self, testcase: &Testcase<I>, state: &S)
    where
        S: HasExecutions,
    {
        if testcase
            .hit_objectives()
            .contains(&Cow::Borrowed(TIMEOUT_FEEDBACK_NAME))
        {
            self.last_hang = current_time();
            self.execs_at_last_objective = *state.executions();
        }
    }

    fn check_interval(&mut self) -> bool {
        let cur = current_time();
        if cur.checked_sub(self.last_report_time).unwrap_or_default() > self.stats_report_interval {
            self.last_report_time = cur;
            return true;
        }
        false
    }
    fn maybe_update_cycles(&mut self, queue_cycles: u64) {
        if queue_cycles > self.cycles_done {
            self.cycles_done += 1;
        }
    }

    fn maybe_update_cycles_wo_finds(&mut self, queue_cycles: u64) {
        if queue_cycles > self.cycles_done && self.last_find < current_time() {
            self.cycles_wo_finds += 1;
        }
    }

    #[expect(clippy::cast_precision_loss)]
    #[expect(clippy::unused_self)]
    fn calculate_stability(&self, unstable_entries: usize, filled_entries: usize) -> f64 {
        ((filled_entries as f64 - unstable_entries as f64) / filled_entries as f64) * 100.0
    }
}

impl Display for AFLPlotData<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{},", self.relative_time)?;
        write!(f, "{},", self.cycles_done)?;
        write!(f, "{},", self.cur_item)?;
        write!(f, "{},", self.corpus_count)?;
        write!(f, "{},", self.pending_total)?;
        write!(f, "{},", self.pending_favs)?;
        write!(f, "{},", self.total_edges)?;
        write!(f, "{},", self.saved_crashes)?;
        write!(f, "{},", self.saved_hangs)?;
        write!(f, "{},", self.max_depth)?;
        write!(f, "{},", self.execs_per_sec)?;
        write!(f, "{},", self.execs_done)?;
        write!(f, "{}", self.edges_found)?;
        Ok(())
    }
}
impl AFLPlotData<'_> {
    fn header() -> &'static str {
        "# relative_time, cycles_done, cur_item, corpus_count, pending_total, pending_favs, total_edges, saved_crashes, saved_hangs, max_depth, execs_per_sec, execs_done, edges_found"
    }
}
impl Display for AflFuzzerStats<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        writeln!(f, "start_time        : {}", &self.start_time)?;
        writeln!(f, "start_time        : {}", &self.start_time)?;
        writeln!(f, "last_update       : {}", &self.last_update)?;
        writeln!(f, "run_time          : {}", &self.run_time)?;
        writeln!(f, "fuzzer_pid        : {}", &self.fuzzer_pid)?;
        writeln!(f, "cycles_done       : {}", &self.cycles_done)?;
        writeln!(f, "cycles_wo_find    : {}", &self.cycles_wo_find)?;
        writeln!(f, "time_wo_finds     : {}", &self.time_wo_finds)?;
        writeln!(f, "fuzz_time         : {}", &self.fuzz_time)?;
        writeln!(f, "calibration_time  : {}", &self.calibration_time)?;
        writeln!(f, "sync_time         : {}", &self.sync_time)?;
        writeln!(f, "trim_time         : {}", &self.trim_time)?;
        writeln!(f, "execs_done        : {}", &self.execs_done)?;
        writeln!(f, "execs_per_sec     : {}", &self.execs_per_sec)?;
        writeln!(f, "execs_ps_last_min : {}", &self.execs_ps_last_min)?;
        writeln!(f, "corpus_count      : {}", &self.corpus_count)?;
        writeln!(f, "corpus_favored    : {}", &self.corpus_favored)?;
        writeln!(f, "corpus_found      : {}", &self.corpus_found)?;
        writeln!(f, "corpus_imported   : {}", &self.corpus_imported)?;
        writeln!(f, "max_depth         : {}", &self.max_depth)?;
        writeln!(f, "cur_item          : {}", &self.cur_item)?;
        writeln!(f, "pending_favs      : {}", &self.pending_favs)?;
        writeln!(f, "pending_total     : {}", &self.pending_total)?;
        writeln!(f, "corpus_variable   : {}", &self.corpus_variable)?;
        writeln!(f, "stability         : {:.2}%", &self.stability)?;
        writeln!(f, "bitmap_cvg        : {:.2}%", &self.bitmap_cvg)?;
        writeln!(f, "saved_crashes     : {}", &self.saved_crashes)?;
        writeln!(f, "saved_hangs       : {}", &self.saved_hangs)?;
        writeln!(f, "last_find         : {}", &self.last_find.as_secs())?;
        writeln!(f, "last_crash        : {}", &self.last_crash.as_secs())?;
        writeln!(f, "last_hang         : {}", &self.last_hang.as_secs())?;
        writeln!(f, "execs_since_crash : {}", &self.execs_since_crash)?;
        writeln!(f, "exec_timeout      : {}", &self.exec_timeout)?;
        writeln!(f, "slowest_exec_ms   : {}", &self.slowest_exec_ms)?;
        writeln!(f, "peak_rss_mb       : {}", &self.peak_rss_mb)?;
        writeln!(f, "cpu_affinity      : {}", &self.cpu_affinity)?;
        writeln!(f, "edges_found       : {}", &self.edges_found)?;
        writeln!(f, "total_edges       : {}", &self.total_edges)?;
        writeln!(f, "var_byte_count    : {}", &self.var_byte_count)?;
        writeln!(f, "havoc_expansion   : {}", &self.havoc_expansion)?;
        writeln!(f, "auto_dict_entries : {}", &self.auto_dict_entries)?;
        writeln!(f, "testcache_size    : {}", &self.testcache_size)?;
        writeln!(f, "testcache_count   : {}", &self.testcache_count)?;
        writeln!(f, "testcache_evict   : {}", &self.testcache_evict)?;
        writeln!(f, "afl_banner        : {}", self.afl_banner)?;
        writeln!(f, "afl_version       : {}", self.afl_version)?;
        writeln!(f, "target_mode       : {}", self.target_mode)?;
        writeln!(f, "command_line      : {}", self.command_line)?;
        Ok(())
    }
}
/// Get the command used to invoke the fuzzer
#[must_use]
pub fn get_run_cmdline() -> Cow<'static, str> {
    let args: Vec<String> = std::env::args().collect();
    Cow::Owned(args.join(" "))
}

/// The Builder for `AflStatsStage`
#[derive(Debug)]
pub struct AflStatsStageBuilder<C, I, O> {
    stats_file_path: Option<PathBuf>,
    plot_file_path: Option<PathBuf>,
    core_id: Option<CoreId>,
    map_observer_handle: Option<Handle<C>>,
    map_name: Option<String>,
    uses_autotokens: bool,
    report_interval: Duration,
    dict_count: usize,
    exec_timeout: u64,
    banner: String,
    version: String,
    target_mode: String,
    phantom_data: PhantomData<(I, O)>,
}

impl<C, I, O> AflStatsStageBuilder<C, I, O>
where
    C: Named,
{
    fn new() -> Self {
        Self {
            report_interval: Duration::from_secs(AFL_FUZZER_STATS_UPDATE_INTERVAL_SECS),
            stats_file_path: None,
            plot_file_path: None,
            core_id: None,
            map_observer_handle: None,
            map_name: None,
            uses_autotokens: false,
            dict_count: 0,
            exec_timeout: 0,
            banner: String::default(),
            version: String::default(),
            target_mode: String::default(),
            phantom_data: PhantomData,
        }
    }

    /// The file path to which we will write the fuzzer stats
    #[must_use]
    pub fn stats_file(mut self, path: PathBuf) -> Self {
        self.stats_file_path = Some(path);
        self
    }
    /// The file path to which we will write the plot data
    #[must_use]
    pub fn plot_file(mut self, path: PathBuf) -> Self {
        self.plot_file_path = Some(path);
        self
    }
    /// The core we are bound to
    #[must_use]
    pub fn core_id(mut self, core_id: CoreId) -> Self {
        self.core_id = Some(core_id);
        self
    }
    /// The interval with which we report stats
    #[must_use]
    pub fn report_interval(mut self, interval: Duration) -> Self {
        self.report_interval = interval;
        self
    }

    /// map name to check the filled count
    #[must_use]
    pub fn map_feedback<F>(mut self, map_feedback: &F) -> Self
    where
        F: Named + HasObserverHandle<Observer = C>,
    {
        self.map_name = Some(map_feedback.name().to_string());
        self.map_observer_handle = Some(map_feedback.observer_handle().clone());
        self
    }

    /// If we use autotokens provided by the target
    #[must_use]
    pub fn uses_autotokens(mut self, uses: bool) -> Self {
        self.uses_autotokens = uses;
        self
    }
    /// The tokens utilized by the fuzzer
    #[must_use]
    pub fn tokens(mut self, tokens: &Tokens) -> Self {
        self.dict_count = tokens.len();
        self
    }
    /// AFL++ Banner (typically the target)
    #[must_use]
    pub fn banner(mut self, banner: String) -> Self {
        self.banner = banner;
        self
    }
    /// Version of the fuzzer
    #[must_use]
    pub fn version(mut self, version: String) -> Self {
        self.version = version;
        self
    }
    /// The "timeout" value used in `TimeoutFeedback`
    #[must_use]
    pub fn exec_timeout(mut self, timeout: u64) -> Self {
        self.exec_timeout = timeout;
        self
    }
    /// Used in the UI (optional)
    /// default, persistent, qemu, unicorn, non-instrumented etc
    #[must_use]
    pub fn target_mode(mut self, target_mode: String) -> Self {
        self.target_mode = target_mode;
        self
    }

    fn create_plot_data_file(path: &Path) -> Result<(), Error> {
        if path.exists() {
            // check if it contains any data
            let file = File::open(path)?;
            if BufReader::new(file).lines().next().is_none() {
                std::fs::write(path, AFLPlotData::header())?;
            }
        } else {
            std::fs::write(path, AFLPlotData::header())?;
        }
        Ok(())
    }

    fn create_fuzzer_stats_file(path: &Path) -> Result<(), Error> {
        if !path.exists() {
            _ = OpenOptions::new().append(true).create(true).open(path)?;
        }
        Ok(())
    }
    /// Build [`AflStatsStage`]
    /// Will error if:
    /// Cannot create the stats file
    /// Cannot create the plot file (if provided)
    /// No `MapObserver` supplied to the builder
    /// No `stats_file_path` provieded
    #[allow(clippy::type_complexity)]
    pub fn build(self) -> Result<AflStatsStage<C, I, O>, Error> {
        let Some(map_name) = self.map_name else {
            return Err(Error::illegal_argument("Must set `map_feedback`"));
        };
        if let Some(ref plot_file) = self.plot_file_path {
            Self::create_plot_data_file(plot_file)?;
        }
        if let Some(stats_file_path) = &self.stats_file_path {
            Self::create_fuzzer_stats_file(stats_file_path)?;
        }
        Ok(AflStatsStage {
            stats_file_path: self.stats_file_path,
            map_name: Cow::Owned(map_name),
            plot_file_path: self.plot_file_path,
            map_observer_handle: self.map_observer_handle.unwrap(),
            start_time: current_time().as_secs(),
            stats_report_interval: self.report_interval,
            has_fuzzed_size: 0,
            is_favored_size: 0,
            cycles_done: 0,
            cycles_wo_finds: 0,
            execs_at_last_objective: 0,
            last_crash: current_time(),
            last_find: current_time(),
            last_hang: current_time(),
            max_depth: 0,
            saved_hangs: 0,
            saved_crashes: 0,
            slowest_exec: Duration::from_secs(0),
            last_report_time: current_time(),
            pid: process::id(),
            exec_timeout: self.exec_timeout,
            target_mode: Cow::Owned(self.target_mode),
            afl_banner: Cow::Owned(self.banner),
            afl_version: Cow::Owned(self.version),
            command_line: get_run_cmdline(),
            dict_count: self.dict_count,
            core_id: self.core_id.unwrap_or(CoreId(0)),
            autotokens_enabled: self.uses_autotokens,
            phantom: PhantomData,
        })
    }
}
