//! The [`MultiMonitor`] displays both cumulative and per-client stats.

use alloc::string::String;
use core::{
    fmt::{Debug, Formatter, Write},
    time::Duration,
};

use libafl_bolts::{ClientId, Error, current_time};

use crate::monitors::{Monitor, stats::ClientStatsManager};

/// Tracking monitor during fuzzing and display both per-client and cumulative info.
#[derive(Clone)]
pub struct MultiMonitor<F>
where
    F: FnMut(&str),
{
    print_fn: F,
}

impl<F> Debug for MultiMonitor<F>
where
    F: FnMut(&str),
{
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("MultiMonitor").finish_non_exhaustive()
    }
}

impl<F> Monitor for MultiMonitor<F>
where
    F: FnMut(&str),
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        let sender = format!("#{}", sender_id.0);
        let pad = if event_msg.len() + sender.len() < 13 {
            " ".repeat(13 - event_msg.len() - sender.len())
        } else {
            String::new()
        };
        let head = format!("{event_msg}{pad} {sender}");
        let global_stats = client_stats_manager.global_stats();
        let mut global_fmt = format!(
            "[{}]  (GLOBAL) run time: {}, clients: {}, corpus: {}, objectives: {}, executions: {}, exec/sec: {}",
            head,
            global_stats.run_time_pretty,
            global_stats.client_stats_count,
            global_stats.corpus_size,
            global_stats.objective_size,
            global_stats.total_execs,
            global_stats.execs_per_sec_pretty
        );
        for (key, val) in client_stats_manager.aggregated() {
            write!(global_fmt, ", {key}: {val}").unwrap();
        }

        (self.print_fn)(&global_fmt);

        client_stats_manager.client_stats_insert(sender_id)?;
        let cur_time = current_time();
        let exec_sec = client_stats_manager
            .update_client_stats_for(sender_id, |client| client.execs_per_sec_pretty(cur_time))?;
        let client = client_stats_manager.client_stats_for(sender_id)?;

        let pad = " ".repeat(head.len());
        let mut fmt = format!(
            " {}   (CLIENT) corpus: {}, objectives: {}, executions: {}, exec/sec: {}",
            pad,
            client.corpus_size(),
            client.objective_size(),
            client.executions(),
            exec_sec
        );
        for (key, val) in client.user_stats() {
            write!(fmt, ", {key}: {val}").unwrap();
        }
        (self.print_fn)(&fmt);

        // Only print perf monitor if the feature is enabled
        #[cfg(feature = "introspection")]
        {
            // Print the client performance monitor. Skip the Client 0 which is the broker
            for (i, (_, client)) in client_stats_manager
                .client_stats()
                .iter()
                .filter(|(_, x)| x.enabled())
                .enumerate()
            {
                let fmt = format!("Client {:03}:\n{}", i + 1, client.introspection_stats);
                (self.print_fn)(&fmt);
            }

            // Separate the spacing just a bit
            (self.print_fn)("\n");
        }
        Ok(())
    }
}

impl<F> MultiMonitor<F>
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
