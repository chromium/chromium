use alloc::{
    collections::VecDeque,
    string::{String, ToString},
    vec::Vec,
};
use core::time::Duration;

use hashbrown::HashMap;

pub use crate::monitors::stats::{
    ClientStats, EdgeCoverage, ItemGeometry, ProcessTiming, TimedStat, TimedStats, UserStatsTag,
    user_stats::{TAG_AFL_STATS_CYCLES_DONE, UserStats},
};

/// The default time window for charts (10 minutes)
pub const DEFAULT_TIME_WINDOW: u64 = 60 * 10; // 10 min
const DEFAULT_LOGS_NUMBER: usize = 128;

/// The default charts to show (Corpus, Objectives, Execs/Sec)
pub const DEFAULT_CHARTS: &[&str] = &["corpus", "objectives", "exec/sec"];

// TimedStat and TimedStats moved to crate::monitors::stats::timed

/// The context for a single client tracked in this [`crate::monitors::tui::TuiMonitor`]
#[derive(Debug, Default, Clone)]
pub struct ClientTuiContext {
    /// The client stats
    pub client_stats: ClientStats,

    /// Times for processing
    pub process_timing: ProcessTiming,
    /// The individual entry geometry
    pub item_geometry: Option<ItemGeometry>,
}

impl ClientTuiContext {
    /// Grab data for a single client
    pub fn grab_data(&mut self, client: &mut ClientStats) {
        self.process_timing = client.process_timing();
        self.item_geometry = client.item_geometry();
        self.client_stats = client.clone();
    }

    /// Get the number of cycles done
    #[must_use]
    pub fn cycles_done(&self) -> Option<u64> {
        self.client_stats
            .user_stats_by_tag(TAG_AFL_STATS_CYCLES_DONE)
            .next()
            .and_then(|s: &UserStats| s.value().as_u64())
    }

    /// Get the number of executions
    #[must_use]
    pub fn executions(&self) -> u64 {
        self.client_stats.executions()
    }

    /// Get the number of corpus items
    #[must_use]
    pub fn corpus(&self) -> u64 {
        self.client_stats.corpus_size()
    }

    /// Get the number of objectives found
    #[must_use]
    pub fn objectives(&self) -> u64 {
        self.client_stats.objective_size()
    }
}

/// The [`TuiContext`] for this [`crate::monitors::tui::TuiMonitor`]
#[derive(Debug, Clone)]
pub struct TuiContext {
    /// The graphs to display
    pub graphs: Vec<String>,
    pub(crate) tags: HashMap<String, UserStatsTag>,
    /// Timed corpus size
    pub corpus_size_timed: TimedStats,
    /// Timed objective size
    pub objective_size_timed: TimedStats,
    /// Timed execs per sec
    /// Timed execs per sec
    pub execs_per_sec_timed: TimedStats,
    /// Timed custom user stats
    pub custom_timed: HashMap<String, TimedStats>,

    /// Clients stats
    pub clients: HashMap<usize, ClientTuiContext>,

    /// Logs from clients
    pub client_logs: VecDeque<String>,

    /// Total number of clients
    pub clients_num: usize,
    /// Total executions
    pub total_execs: u64,
    /// Start time
    pub start_time: Duration,

    /// Total map density
    pub total_map_density: String,

    /// Total solutions
    pub total_solutions: u64,
    /// Total corpus count
    pub total_corpus_count: u64,

    /// Total item geometry
    pub total_item_geometry: Option<ItemGeometry>,

    /// Total process timing
    pub total_process_timing: ProcessTiming,
}

impl TuiContext {
    /// Create a new [`TuiContext`]
    #[must_use]
    pub fn new(start_time: Duration) -> Self {
        Self {
            start_time,
            clients: HashMap::default(),
            client_logs: VecDeque::with_capacity(DEFAULT_LOGS_NUMBER),
            total_execs: 0,
            clients_num: 0,
            total_map_density: String::new(),
            total_solutions: 0,
            total_corpus_count: 0,
            total_item_geometry: None,
            total_process_timing: ProcessTiming::new(),
            graphs: DEFAULT_CHARTS.iter().map(ToString::to_string).collect(),
            tags: HashMap::default(),
            corpus_size_timed: TimedStats::new(Duration::from_secs(DEFAULT_TIME_WINDOW)),
            objective_size_timed: TimedStats::new(Duration::from_secs(DEFAULT_TIME_WINDOW)),
            execs_per_sec_timed: TimedStats::new(Duration::from_secs(DEFAULT_TIME_WINDOW)),
            custom_timed: HashMap::default(),
        }
    }
}
