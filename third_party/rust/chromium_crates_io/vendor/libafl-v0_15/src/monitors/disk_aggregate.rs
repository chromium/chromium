//! Monitors that log aggregated stats to disk.

use core::{
    fmt::{Debug, Formatter},
    time::Duration,
};
use std::{fs::OpenOptions, io::Write, path::PathBuf};

use libafl_bolts::{ClientId, Error, current_time};
use serde_json::json;

use crate::monitors::{Monitor, stats::ClientStatsManager};

/// A monitor that logs aggregated stats to a JSON file.
#[derive(Clone)]
pub struct OnDiskJsonAggregateMonitor {
    json_path: PathBuf,
    last_update: Duration,
    update_interval: Duration,
}

impl Debug for OnDiskJsonAggregateMonitor {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("OnDiskJsonAggregateMonitor")
            .field("last_update", &self.last_update)
            .field("update_interval", &self.update_interval)
            .field("json_path", &self.json_path)
            .finish_non_exhaustive()
    }
}

impl Monitor for OnDiskJsonAggregateMonitor {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        _event_msg: &str,
        _sender_id: ClientId,
    ) -> Result<(), Error> {
        // Write JSON stats if update interval has elapsed
        let cur_time = current_time();
        if cur_time
            .checked_sub(self.last_update)
            .unwrap_or(self.update_interval)
            >= self.update_interval
        {
            self.last_update = cur_time;

            let file = OpenOptions::new()
                .append(true)
                .create(true)
                .open(&self.json_path)
                .expect("Failed to open JSON logging file");

            let global_stats = client_stats_manager.global_stats();
            let mut json_value = json!({
                "run_time": global_stats.run_time.as_secs(),
                "clients": global_stats.client_stats_count,
                "corpus": global_stats.corpus_size,
                "objectives": global_stats.objective_size,
                "executions": global_stats.total_execs,
                "exec_sec": global_stats.execs_per_sec,
            });

            // Add all aggregated values directly to the root
            if let Some(obj) = json_value.as_object_mut() {
                obj.extend(
                    client_stats_manager
                        .aggregated()
                        .iter()
                        .map(|(k, v)| (k.clone().into_owned(), json!(v))),
                );
            }

            writeln!(&file, "{json_value}").expect("Unable to write JSON to file");
        }
        Ok(())
    }
}

impl OnDiskJsonAggregateMonitor {
    /// Creates a new [`OnDiskJsonAggregateMonitor`]
    pub fn new<P>(json_path: P) -> Self
    where
        P: Into<PathBuf>,
    {
        Self::with_interval(json_path, Duration::from_secs(10))
    }

    /// Creates a new [`OnDiskJsonAggregateMonitor`] with custom update interval
    pub fn with_interval<P>(json_path: P, update_interval: Duration) -> Self
    where
        P: Into<PathBuf>,
    {
        Self {
            json_path: json_path.into(),
            last_update: current_time()
                .checked_sub(update_interval)
                .unwrap_or_default(),
            update_interval,
        }
    }
}
