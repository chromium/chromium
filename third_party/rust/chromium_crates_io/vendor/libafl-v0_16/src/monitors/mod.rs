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
#[cfg(feature = "std")]
use core::str::FromStr;
use core::{
    fmt,
    fmt::{Debug, Write},
    time::Duration,
};
#[cfg(feature = "std")]
use std::sync::OnceLock;

use libafl_bolts::ClientId;
#[cfg(feature = "prometheus_monitor")]
pub use prometheus::PrometheusMonitor;
#[cfg(feature = "statsd_monitor")]
pub use statsd::StatsdMonitor;

/// Returns if we're cooking.
#[cfg(feature = "std")]
#[must_use]
pub(crate) fn pizza_is_served() -> bool {
    static PIZZA_IS_SERVED: OnceLock<bool> = OnceLock::new();
    *PIZZA_IS_SERVED.get_or_init(|| {
        match std::env::var("AFL_PIZZA_MODE")
            .map(|s| i64::from_str(&s).expect("AFL_PIZZA_MODE must be set to a signed integer!"))
        {
            Ok(v) if v < 1 => false,
            Ok(_) => true,
            Err(_) => {
                #[cfg(unix)]
                // SAFETY: `localtime` and `time` are standard libc functions. `t` is initialized.
                unsafe {
                    let mut t = 0;
                    libc::time(&raw mut t);
                    let tm = libc::localtime(&raw const t);
                    !tm.is_null() && (*tm).tm_mon == 3 && (*tm).tm_mday == 1
                }
                #[cfg(windows)]
                // SAFETY: `GetLocalTime` is a standard Win32 API.
                unsafe {
                    let lt = windows::Win32::System::SystemInformation::GetLocalTime();
                    lt.wMonth == 4 && lt.wDay == 1
                }
                #[cfg(not(any(unix, windows)))]
                false
            }
        }
    })
}

#[cfg(not(feature = "std"))]
/// Returns `true` if it is currently pizza mode.
#[must_use]
pub fn pizza_is_served() -> bool {
    false
}

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
        let (run, customers, corpus, objectives, executions, speed) = if pizza_is_served() {
            (
                "time to bake",
                "customers",
                "pizzas",
                "deliveries",
                "doughs",
                "p/s",
            )
        } else {
            (
                "run time",
                "clients",
                "corpus",
                "objectives",
                "executions",
                "exec/sec",
            )
        };
        println!(
            "[{} #{}] {}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}",
            event_msg,
            sender_id.0,
            run,
            global_stats.run_time_pretty,
            customers,
            global_stats.client_stats_count,
            corpus,
            global_stats.corpus_size,
            objectives,
            global_stats.objective_size,
            executions,
            global_stats.total_execs,
            speed,
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
        let (run, customers, corpus, objectives, executions, speed) = if pizza_is_served() {
            (
                "time to bake",
                "customers",
                "pizzas",
                "deliveries",
                "doughs",
                "p/s",
            )
        } else {
            (
                "run time",
                "clients",
                "corpus",
                "objectives",
                "executions",
                "exec/sec",
            )
        };
        let mut fmt = format!(
            "[{} #{}] {}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}: {}",
            event_msg,
            sender_id.0,
            run,
            global_stats.run_time_pretty,
            customers,
            global_stats.client_stats_count,
            corpus,
            global_stats.corpus_size,
            objectives,
            global_stats.objective_size,
            executions,
            global_stats.total_execs,
            speed,
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

    #[test]
    #[cfg(feature = "std")]
    fn test_pizza_mode() {
        let _ = super::pizza_is_served();
    }

    #[test]
    #[cfg(feature = "std")]
    fn test_multi_monitor_pizza_mode() {
        use alloc::string::String;
        use core::cell::RefCell;
        let output = RefCell::new(String::new());
        let _monitor = super::MultiMonitor::new(|s| output.borrow_mut().push_str(s));
    }
}
