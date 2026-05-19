//! Keep stats, and display them to the user. Usually used in a broker, or main node, of some sort.

pub mod multi;
use libafl_bolts::Error;
pub use multi::MultiMonitor;
pub mod stats;

pub mod logics;
pub use logics::{IfElseMonitor, IfMonitor, OptionalMonitor, WhileMonitor};

#[cfg(feature = "std")]
pub mod disk;
#[cfg(feature = "std")]
pub use disk::{OnDiskJsonMonitor, OnDiskTomlMonitor};

#[cfg(feature = "std")]
pub mod disk_aggregate;
#[cfg(feature = "std")]
pub use disk_aggregate::OnDiskJsonAggregateMonitor;

#[cfg(all(feature = "tui_monitor", feature = "std"))]
pub mod tui;
#[cfg(all(feature = "tui_monitor", feature = "std"))]
pub use tui::TuiMonitor;

#[cfg(feature = "prometheus_monitor")]
pub mod prometheus;

#[cfg(feature = "statsd_monitor")]
pub mod statsd;

#[cfg(feature = "std")]
use alloc::vec::Vec;
use core::{
    fmt,
    fmt::{Debug, Write},
    time::Duration,
};

use libafl_bolts::ClientId;
#[cfg(feature = "prometheus_monitor")]
pub use prometheus::PrometheusMonitor;
#[cfg(feature = "statsd_monitor")]
pub use statsd::StatsdMonitor;

use crate::monitors::stats::ClientStatsManager;

/// The monitor trait keeps track of all the client's monitor, and offers methods to display them.
pub trait Monitor {
    /// Show the monitor to the user
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error>;
}

/// Monitor that print exactly nothing.
/// Not good for debugging, very good for speed.
#[derive(Debug, Copy, Clone)]
pub struct NopMonitor {}

impl Monitor for NopMonitor {
    #[inline]
    fn display(
        &mut self,
        _client_stats_manager: &mut ClientStatsManager,
        _event_msg: &str,
        _sender_id: ClientId,
    ) -> Result<(), Error> {
        Ok(())
    }
}

impl NopMonitor {
    /// Create new [`NopMonitor`]
    #[must_use]
    pub fn new() -> Self {
        Self {}
    }
}

impl Default for NopMonitor {
    fn default() -> Self {
        Self::new()
    }
}

/// Tracking monitor during fuzzing that just prints to `stdout`.
#[cfg(feature = "std")]
#[derive(Debug, Clone, Default)]
pub struct SimplePrintingMonitor {}

#[cfg(feature = "std")]
impl SimplePrintingMonitor {
    /// Create a new [`SimplePrintingMonitor`]
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }
}

#[cfg(feature = "std")]
impl Monitor for SimplePrintingMonitor {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        let mut userstats = client_stats_manager
            .get(sender_id)?
            .user_stats()
            .iter()
            .map(|(key, value)| format!("{key}: {value}"))
            .collect::<Vec<_>>();
        userstats.sort();
        let global_stats = client_stats_manager.global_stats();
        println!(
            "[{} #{}] run time: {}, clients: {}, corpus: {}, objectives: {}, executions: {}, exec/sec: {}, {}",
            event_msg,
            sender_id.0,
            global_stats.run_time_pretty,
            global_stats.client_stats_count,
            global_stats.corpus_size,
            global_stats.objective_size,
            global_stats.total_execs,
            global_stats.execs_per_sec_pretty,
            userstats.join(", ")
        );

        // Only print perf monitor if the feature is enabled
        #[cfg(feature = "introspection")]
        {
            // Print the client performance monitor.
            println!(
                "Client {:03}:\n{}",
                sender_id.0,
                client_stats_manager.get(sender_id)?.introspection_stats
            );
            // Separate the spacing just a bit
            println!();
        }
        Ok(())
    }
}

/// Tracking monitor during fuzzing.
#[derive(Clone)]
pub struct SimpleMonitor<F>
where
    F: FnMut(&str),
{
    print_fn: F,
}

impl<F> Debug for SimpleMonitor<F>
where
    F: FnMut(&str),
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("SimpleMonitor").finish_non_exhaustive()
    }
}

impl<F> Monitor for SimpleMonitor<F>
where
    F: FnMut(&str),
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        let global_stats = client_stats_manager.global_stats();
        let mut fmt = format!(
            "[{} #{}] run time: {}, clients: {}, corpus: {}, objectives: {}, executions: {}, exec/sec: {}",
            event_msg,
            sender_id.0,
            global_stats.run_time_pretty,
            global_stats.client_stats_count,
            global_stats.corpus_size,
            global_stats.objective_size,
            global_stats.total_execs,
            global_stats.execs_per_sec_pretty
        );

        client_stats_manager.client_stats_insert(sender_id)?;
        let client = client_stats_manager.client_stats_for(sender_id)?;
        for (key, val) in client.user_stats() {
            write!(fmt, ", {key}: {val}").unwrap();
        }

        (self.print_fn)(&fmt);

        // Only print perf monitor if the feature is enabled
        #[cfg(feature = "introspection")]
        {
            // Print the client performance monitor.
            let fmt = format!(
                "Client {:03}:\n{}",
                sender_id.0,
                client_stats_manager.get(sender_id)?.introspection_stats
            );
            (self.print_fn)(&fmt);

            // Separate the spacing just a bit
            (self.print_fn)("");
        }
        Ok(())
    }
}

impl<F> SimpleMonitor<F>
where
    F: FnMut(&str),
{
    /// Creates the monitor, using the `current_time` as `start_time`.
    pub fn new(print_fn: F) -> Self {
        Self { print_fn }
    }

    /// Creates the monitor with a given `start_time`.
    #[deprecated(
        since = "0.16.0",
        note = "Please use new to create. start_time is useless here."
    )]
    pub fn with_time(print_fn: F, _start_time: Duration) -> Self {
        Self::new(print_fn)
    }
}

/// Start the timer
#[macro_export]
macro_rules! start_timer {
    ($state:expr) => {{
        // Start the timer
        #[cfg(feature = "introspection")]
        $state.introspection_stats_mut().start_timer();
    }};
}

/// Mark the elapsed time for the given feature
#[macro_export]
macro_rules! mark_feature_time {
    ($state:expr, $feature:expr) => {{
        // Mark the elapsed time for the given feature
        #[cfg(feature = "introspection")]
        $state.introspection_stats_mut().mark_feature_time($feature);
    }};
}

/// Mark the elapsed time for the given feature
#[macro_export]
macro_rules! mark_feedback_time {
    ($state:expr) => {{
        // Mark the elapsed time for the given feature
        #[cfg(feature = "introspection")]
        $state.introspection_stats_mut().mark_feedback_time();
    }};
}

impl<A: Monitor, B: Monitor> Monitor for (A, B) {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        self.0.display(client_stats_manager, event_msg, sender_id)?;
        self.1.display(client_stats_manager, event_msg, sender_id)
    }
}

impl<A: Monitor> Monitor for (A, ()) {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        self.0.display(client_stats_manager, event_msg, sender_id)
    }
}

#[cfg(test)]
mod test {
    use libafl_bolts::ClientId;
    use tuple_list::tuple_list;

    use super::{Monitor, NopMonitor, SimpleMonitor, stats::ClientStatsManager};

    #[test]
    fn test_monitor_tuple_list() {
        let mut client_stats = ClientStatsManager::new();
        let mut mgr_list = tuple_list!(
            SimpleMonitor::new(|_msg| {
                #[cfg(feature = "std")]
                println!("{_msg}");
            }),
            SimpleMonitor::new(|_msg| {
                #[cfg(feature = "std")]
                println!("{_msg}");
            }),
            NopMonitor::default(),
            NopMonitor::default(),
        );
        let _ = mgr_list.display(&mut client_stats, "test", ClientId(0));
    }
}
