//! Statistics used for Monitors to display.

pub mod manager;
#[cfg(feature = "introspection")]
pub mod perf_stats;
pub mod timed;
pub mod user_stats;

use alloc::{borrow::Cow, string::String};
use core::time::Duration;

use hashbrown::HashMap;
use libafl_bolts::current_time;
pub use manager::ClientStatsManager;
#[cfg(feature = "introspection")]
pub use perf_stats::{ClientPerfStats, PerfFeature};
use serde::{Deserialize, Serialize};
pub use timed::*;
pub use user_stats::{
    AggregatorOps, TAG_AFL_STATS_IMPORTED, TAG_AFL_STATS_OWN_FINDS, TAG_AFL_STATS_PENDING,
    TAG_AFL_STATS_PENDING_FAV, TAG_CALIBRATE_STABILITY, TAG_MAP, UserStats, UserStatsTag,
    UserStatsValue,
};

#[cfg(feature = "afl_exec_sec")]
const CLIENT_STATS_TIME_WINDOW_SECS: u64 = 5; // 5 seconds

/// A simple struct to keep track of client statistics
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ClientStats {
    /// If this client is enabled. This is set to `true` the first time we see this client.
    enabled: bool,
    // monitor (maybe we need a separated struct?)
    /// The corpus size for this client
    corpus_size: u64,
    /// The time for the last update of the corpus size
    last_corpus_time: Duration,
    /// The total executions for this client
    executions: u64,
    /// The number of executions of the previous state in case a client decrease the number of execution (e.g when restarting without saving the state)
    prev_state_executions: u64,
    /// The testcase index currently being fuzzed
    pub current_testcase_idx: Option<usize>,
    /// The size of the objectives corpus for this client
    objective_size: u64,
    /// The time for the last update of the objective size
    last_objective_time: Duration,
    /// The last reported executions for this client
    #[cfg(feature = "afl_exec_sec")]
    last_window_executions: u64,
    /// The last executions per sec
    #[cfg(feature = "afl_exec_sec")]
    last_execs_per_sec: f64,
    /// The last time we got this information
    last_window_time: Duration,
    /// the start time of the client
    start_time: Duration,
    /// User-defined stats
    user_stats: HashMap<Cow<'static, str>, UserStats>,
    /// Client performance statistics
    #[cfg(feature = "introspection")]
    pub introspection_stats: ClientPerfStats,
    // This field is marked as skip_serializing and skip_deserializing,
    // which means when deserializing, its default value, i.e. all stats
    // is updated, will be filled in this field. This could help preventing
    // something unexpected, since when we find they all need update, we will
    // always invalid the cache.
    /// Status of current client stats. This field is used to check
    /// the validation of current cached global stats.
    #[serde(skip_serializing, skip_deserializing)]
    stats_status: ClientStatsStatus,
}

/// Status of client status
#[derive(Debug, Clone)]
struct ClientStatsStatus {
    /// Basic stats, which could affect the global stats, have been updated
    basic_stats_updated: bool,
}

impl Default for ClientStatsStatus {
    fn default() -> Self {
        ClientStatsStatus {
            basic_stats_updated: true,
        }
    }
}

/// Data struct to process timings
#[derive(Debug, Default, Clone)]
pub struct ProcessTiming {
    /// The start time
    pub client_start_time: Duration,
    /// The executions speed
    pub exec_speed: String,
    /// Timing of the last new corpus entry
    pub last_new_entry: Duration,
    /// Timing of the last new solution
    pub last_saved_solution: Duration,
    /// The total number of executions
    pub total_execs: u64,
}

impl ProcessTiming {
    /// Create a new [`ProcessTiming`] struct
    #[must_use]
    pub fn new() -> Self {
        Self {
            exec_speed: String::from("0"),
            ..Default::default()
        }
    }
}

/// The geometry of a single data point
#[derive(Debug, Default, Clone)]
pub struct ItemGeometry {
    /// Pending entries
    pub pending: u64,
    /// Favored pending entries
    pub pend_fav: u64,
    /// How much entries we found
    pub own_finds: u64,
    /// How much entries were imported
    pub imported: u64,
    /// The stability, ranges from 0.0 to 1.0.
    ///
    /// If there is no such data, this field will be `None`.
    pub stability: Option<f64>,
}

impl ItemGeometry {
    /// Create a new [`ItemGeometry`]
    #[must_use]
    pub fn new() -> Self {
        ItemGeometry::default()
    }
}

/// Stats of edge coverage
#[derive(Debug, Default, Clone)]
pub struct EdgeCoverage {
    /// Count of hit edges
    pub edges_hit: u64,
    /// Count of total edges
    pub edges_total: u64,
}

impl ClientStats {
    /// If this client is enabled. This is set to `true` the first time we see this client.
    #[must_use]
    pub fn enabled(&self) -> bool {
        self.enabled
    }
    /// The corpus size for this client
    #[must_use]
    pub fn corpus_size(&self) -> u64 {
        self.corpus_size
    }
    /// The total executions for this client
    #[must_use]
    pub fn last_corpus_time(&self) -> Duration {
        self.last_corpus_time
    }
    /// The total executions for this client
    #[must_use]
    pub fn executions(&self) -> u64 {
        self.executions
    }
    /// The number of executions of the previous state in case a client decrease the number of execution (e.g when restarting without saving the state)
    #[must_use]
    pub fn prev_state_executions(&self) -> u64 {
        self.prev_state_executions
    }
    /// The size of the objectives corpus for this client
    #[must_use]
    pub fn objective_size(&self) -> u64 {
        self.objective_size
    }
    /// The time for the last update of the objective size
    #[must_use]
    pub fn last_objective_time(&self) -> Duration {
        self.last_objective_time
    }
    /// The last time we got this information
    #[must_use]
    pub fn last_window_time(&self) -> Duration {
        self.last_window_time
    }
    /// the start time of the client
    #[must_use]
    pub fn start_time(&self) -> Duration {
        self.start_time
    }
    /// User-defined stats
    #[must_use]
    pub fn user_stats(&self) -> &HashMap<Cow<'static, str>, UserStats> {
        &self.user_stats
    }

    /// Clear current stats status. This is used before user update `ClientStats`.
    fn clear_stats_status(&mut self) {
        self.stats_status.basic_stats_updated = false;
    }

    /// We got new information about executions for this client, insert them.
    #[cfg(feature = "afl_exec_sec")]
    pub fn update_executions(&mut self, executions: u64, cur_time: Duration) {
        let diff = cur_time
            .checked_sub(self.last_window_time)
            .map_or(CLIENT_STATS_TIME_WINDOW_SECS, |d| d.as_secs());
        if diff >= CLIENT_STATS_TIME_WINDOW_SECS {
            let _: f64 = self.execs_per_sec(cur_time);
            self.last_window_time = cur_time;
            self.last_window_executions = self.executions;
        }
        if self.executions > self.prev_state_executions + executions {
            // Something is strange here, sum the executions
            self.prev_state_executions = self.executions;
        }
        self.executions = self.prev_state_executions + executions;
        self.stats_status.basic_stats_updated = true;
    }

    /// We got a new information about executions for this client, insert them.
    #[cfg(not(feature = "afl_exec_sec"))]
    pub fn update_executions(&mut self, executions: u64, _cur_time: Duration) {
        if self.executions > self.prev_state_executions + executions {
            // Something is strange here, sum the executions
            self.prev_state_executions = self.executions;
        }
        self.executions = self.prev_state_executions + executions;
        self.stats_status.basic_stats_updated = true;
    }

    /// We got new information about corpus size for this client, insert them.
    pub fn update_corpus_size(&mut self, corpus_size: u64) {
        self.corpus_size = corpus_size;
        self.last_corpus_time = current_time();
        self.stats_status.basic_stats_updated = true;
    }

    /// We got a new information about objective corpus size for this client, insert them.
    pub fn update_objective_size(&mut self, objective_size: u64) {
        self.objective_size = objective_size;
        self.last_objective_time = current_time();
        self.stats_status.basic_stats_updated = true;
    }

    // This will not update stats status, since the value this function changed
    // does not affect global stats.
    /// Get the calculated executions per second for this client
    #[expect(clippy::cast_precision_loss, clippy::cast_sign_loss)]
    #[cfg(feature = "afl_exec_sec")]
    pub fn execs_per_sec(&mut self, cur_time: Duration) -> f64 {
        if self.executions == 0 {
            return 0.0;
        }

        let elapsed = cur_time.saturating_sub(self.last_window_time).as_secs_f64();
        if elapsed as u64 == 0 {
            return self.last_execs_per_sec;
        }

        let cur_avg = ((self.executions - self.last_window_executions) as f64) / elapsed;
        if self.last_window_executions == 0 {
            self.last_execs_per_sec = cur_avg;
            return self.last_execs_per_sec;
        }

        // If there is a dramatic (5x+) jump in speed, reset the indicator more quickly
        if cur_avg * 5.0 < self.last_execs_per_sec || cur_avg / 5.0 > self.last_execs_per_sec {
            self.last_execs_per_sec = cur_avg;
        }

        self.last_execs_per_sec =
            self.last_execs_per_sec * (1.0 - 1.0 / 16.0) + cur_avg * (1.0 / 16.0);
        self.last_execs_per_sec
    }

    // This will not update stats status, since there is no value changed
    /// Get the calculated executions per second for this client
    #[expect(clippy::cast_precision_loss, clippy::cast_sign_loss)]
    #[cfg(not(feature = "afl_exec_sec"))]
    pub fn execs_per_sec(&mut self, cur_time: Duration) -> f64 {
        if self.executions == 0 {
            return 0.0;
        }

        let elapsed = cur_time.saturating_sub(self.last_window_time).as_secs_f64();
        if elapsed as u64 == 0 {
            return 0.0;
        }

        (self.executions as f64) / elapsed
    }

    // This will not update stats status, since the value this function changed
    // does not affect global stats.
    /// Executions per second
    pub fn execs_per_sec_pretty(&mut self, cur_time: Duration) -> String {
        prettify_float(self.execs_per_sec(cur_time))
    }

    // This will not update stats status, since the value this function changed
    // does not affect global stats.
    /// Update the user-defined stat with name and value
    pub fn update_user_stats(
        &mut self,
        name: Cow<'static, str>,
        value: UserStats,
    ) -> Option<UserStats> {
        self.user_stats.insert(name, value)
    }
    /// Get a user-defined stat using the name
    #[must_use]
    pub fn get_user_stats(&self, name: &str) -> Option<&UserStats> {
        self.user_stats.get(name)
    }

    /// Get user-defined stats using the tag
    pub fn user_stats_by_tag(&self, tag: UserStatsTag) -> impl Iterator<Item = &UserStats> {
        self.user_stats
            .values()
            .filter(move |v| v.tag() == Some(tag))
    }

    /// Update the current [`ClientPerfStats`] with the given [`ClientPerfStats`]
    #[cfg(feature = "introspection")]
    pub fn update_introspection_stats(&mut self, introspection_stats: &ClientPerfStats) {
        #[allow(clippy::cast_precision_loss)]
        let elapsed = introspection_stats.elapsed_cycles() as f64;
        if elapsed == 0.0 {
            return;
        }

        self.introspection_stats = introspection_stats.clone();
    }

    /// Get process timing of current client.
    pub fn process_timing(&mut self) -> ProcessTiming {
        let client_start_time = self.start_time();
        let last_new_entry = if self.last_corpus_time() > self.start_time() {
            current_time().saturating_sub(self.last_corpus_time())
        } else {
            Duration::default()
        };

        let last_saved_solution = if self.last_objective_time() > self.start_time() {
            current_time().saturating_sub(self.last_objective_time())
        } else {
            Duration::default()
        };

        let exec_speed = self.execs_per_sec_pretty(current_time());
        let total_execs = self.executions;

        ProcessTiming {
            client_start_time,
            exec_speed,
            last_new_entry,
            last_saved_solution,
            total_execs,
        }
    }

    /// Get edge coverage of current client
    ///
    /// This is using userstats set by [`crate::feedbacks::MapFeedback`]
    #[must_use]
    pub fn edges_coverage(&self) -> Option<EdgeCoverage> {
        let mut edges_hit = 0;
        let mut edges_total = 0;
        let mut found = false;

        for stat in self.user_stats_by_tag(TAG_MAP) {
            if let UserStatsValue::Ratio(hit, total) = stat.value() {
                edges_hit += hit;
                edges_total += total;
                found = true;
            }
        }

        if found {
            Some(EdgeCoverage {
                edges_hit,
                edges_total,
            })
        } else {
            None
        }
    }

    /// Get item geometry of current client
    ///
    /// This is using userstats set by [`crate::stages::AflStatsStage`].
    #[must_use]
    pub fn item_geometry(&self) -> Option<ItemGeometry> {
        let pending = self
            .user_stats_by_tag(TAG_AFL_STATS_PENDING)
            .map(|s| s.value().as_u64().unwrap_or(0))
            .sum();
        let pend_fav = self
            .user_stats_by_tag(TAG_AFL_STATS_PENDING_FAV)
            .map(|s| s.value().as_u64().unwrap_or(0))
            .sum();
        let imported = self
            .user_stats_by_tag(TAG_AFL_STATS_IMPORTED)
            .map(|s| s.value().as_u64().unwrap_or(0))
            .sum();
        let own_finds = self
            .user_stats_by_tag(TAG_AFL_STATS_OWN_FINDS)
            .map(|s| s.value().as_u64().unwrap_or(0))
            .sum();

        let (stability_sum, stability_count) = self
            .user_stats
            .values()
            .filter(|v| v.tag() == Some(TAG_CALIBRATE_STABILITY))
            .fold((0.0, 0), |(sum, count), s| {
                (
                    sum + s.value().as_ratio().map_or(0.0, |(a, b)| {
                        #[expect(clippy::cast_precision_loss)]
                        let ret = a as f64 / b as f64;
                        ret
                    }),
                    count + 1,
                )
            });

        let stability = if stability_count > 0 {
            Some(stability_sum / f64::from(stability_count))
        } else {
            None
        };

        // We want to return None if we have NO relevant stats, to avoid showing empty charts/info
        // "pending" is a good indicator if we have data.
        if self
            .user_stats_by_tag(TAG_AFL_STATS_PENDING)
            .next()
            .is_none()
            && stability.is_none()
        {
            return None;
        }

        Some(ItemGeometry {
            pending,
            pend_fav,
            own_finds,
            imported,
            stability,
        })
    }
}

/// Prettifies float values for human-readable output
fn prettify_float(value: f64) -> String {
    let (value, suffix) = match value {
        value if value >= 1_000_000.0 => (value / 1_000_000.0, "M"),
        value if value >= 1_000.0 => (value / 1_000.0, "k"),
        value => (value, ""),
    };
    match value {
        value if value >= 1_000_000.0 => {
            format!("{value:.2}{suffix}")
        }
        value if value >= 1_000.0 => {
            format!("{value:.1}{suffix}")
        }
        value if value >= 100.0 => {
            format!("{value:.1}{suffix}")
        }
        value if value >= 10.0 => {
            format!("{value:.2}{suffix}")
        }
        value => {
            format!("{value:.3}{suffix}")
        }
    }
}
