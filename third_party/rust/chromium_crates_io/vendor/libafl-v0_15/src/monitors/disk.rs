//! Monitors that log to disk using different formats like `JSON` and `TOML`.

use alloc::{string::String, vec::Vec};
use core::time::Duration;
use std::{
    fs::{File, OpenOptions},
    io::Write,
    path::PathBuf,
};

use libafl_bolts::{ClientId, Error, current_time};
use serde_json::json;

use crate::monitors::{Monitor, stats::ClientStatsManager};

/// Wrap a monitor and log the current state of the monitor into a Toml file.
#[derive(Debug, Clone)]
pub struct OnDiskTomlMonitor {
    filename: PathBuf,
    last_update: Duration,
    update_interval: Duration,
}

impl Monitor for OnDiskTomlMonitor {
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        _event_msg: &str,
        _sender_id: ClientId,
    ) -> Result<(), Error> {
        let cur_time = current_time();

        if cur_time
            .checked_sub(self.last_update)
            .unwrap_or(self.update_interval)
            >= self.update_interval
        {
            self.last_update = cur_time;

            let global_stats = client_stats_manager.global_stats();

            let mut file = File::create(&self.filename).expect("Failed to open the Toml file");
            write!(
                &mut file,
                "# This Toml is generated using the OnDiskMonitor component of LibAFL

[global]
run_time = \"{}\"
clients = {}
corpus = {}
objectives = {}
executions = {}
exec_sec = {}
",
                global_stats.run_time_pretty,
                global_stats.client_stats_count,
                global_stats.corpus_size,
                global_stats.objective_size,
                global_stats.total_execs,
                global_stats.execs_per_sec
            )
            .expect("Failed to write to the Toml file");

            let all_clients: Vec<ClientId> = client_stats_manager
                .client_stats()
                .keys()
                .copied()
                .collect();

            for client_id in &all_clients {
                let exec_sec = client_stats_manager
                    .update_client_stats_for(*client_id, |client_stat| {
                        client_stat.execs_per_sec(cur_time)
                    })?;

                let client = client_stats_manager.client_stats_for(*client_id)?;

                write!(
                    &mut file,
                    "
[client_{}]
corpus = {}
objectives = {}
executions = {}
exec_sec = {}
",
                    client_id.0,
                    client.corpus_size(),
                    client.objective_size(),
                    client.executions(),
                    exec_sec
                )
                .expect("Failed to write to the Toml file");

                for (key, val) in client.user_stats() {
                    let k: String = key
                        .chars()
                        .map(|c| if c.is_whitespace() { '_' } else { c })
                        .filter(|c| c.is_alphanumeric() || *c == '_')
                        .collect();
                    writeln!(&mut file, "{k} = \"{val}\"")
                        .expect("Failed to write to the Toml file");
                }
            }

            drop(file);
        }
        Ok(())
    }
}

impl OnDiskTomlMonitor {
    /// Create new [`OnDiskTomlMonitor`]
    #[must_use]
    pub fn new<P>(filename: P) -> Self
    where
        P: Into<PathBuf>,
    {
        Self::with_update_interval(filename, Duration::from_secs(60))
    }

    /// Create new [`OnDiskTomlMonitor`] with custom update interval
    #[must_use]
    pub fn with_update_interval<P>(filename: P, update_interval: Duration) -> Self
    where
        P: Into<PathBuf>,
    {
        Self {
            filename: filename.into(),
            last_update: current_time()
                .checked_sub(update_interval)
                .unwrap_or_default(),
            update_interval,
        }
    }
}

impl OnDiskTomlMonitor {
    /// Create new [`OnDiskTomlMonitor`] without a base
    #[must_use]
    #[deprecated(since = "0.16.0", note = "Use new directly")]
    pub fn nop<P>(filename: P) -> Self
    where
        P: Into<PathBuf>,
    {
        Self::new(filename)
    }
}

#[derive(Debug, Clone)]
/// Continuously appends the current statistics to a Json lines file.
pub struct OnDiskJsonMonitor<F>
where
    F: FnMut(&mut ClientStatsManager) -> bool,
{
    path: PathBuf,
    /// A function that has the current runtime as argument and decides, whether a record should be logged
    log_record: F,
}

impl<F> OnDiskJsonMonitor<F>
where
    F: FnMut(&mut ClientStatsManager) -> bool,
{
    /// Create a new [`OnDiskJsonMonitor`]
    pub fn new<P>(filename: P, log_record: F) -> Self
    where
        P: Into<PathBuf>,
    {
        let path = filename.into();

        Self { path, log_record }
    }
}

impl<F> Monitor for OnDiskJsonMonitor<F>
where
    F: FnMut(&mut ClientStatsManager) -> bool,
{
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        _event_msg: &str,
        _sender_id: ClientId,
    ) -> Result<(), Error> {
        if (self.log_record)(client_stats_manager) {
            let file = OpenOptions::new()
                .append(true)
                .create(true)
                .open(&self.path)
                .expect("Failed to open logging file");

            let global_stats = client_stats_manager.global_stats();
            let line = json!({
                "run_time": global_stats.run_time,
                "clients": global_stats.client_stats_count,
                "corpus": global_stats.corpus_size,
                "objectives": global_stats.objective_size,
                "executions": global_stats.total_execs,
                "exec_sec": global_stats.execs_per_sec,
                "client_stats": client_stats_manager.client_stats(),
            });
            writeln!(&file, "{line}").expect("Unable to write Json to file");
        }
        Ok(())
    }
}
