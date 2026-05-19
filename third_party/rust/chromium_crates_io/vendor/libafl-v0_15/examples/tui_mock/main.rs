//! An example for TUI that uses the TUI without any real data.
//! This is mainly to fix the UI without having to run a real fuzzer.

use core::time::Duration;
use std::thread::sleep;

use libafl::monitors::{
    Monitor,
    stats::{ClientStats, manager::ClientStatsManager},
    tui::TuiMonitor,
};
use libafl_bolts::ClientId;

pub fn main() {
    let mut monitor = TuiMonitor::builder().build();

    let _client_stats = ClientStats::default();
    let mut client_stats_manager = ClientStatsManager::default();

    let _ = monitor.display(&mut client_stats_manager, "Test", ClientId(0));
    sleep(Duration::from_secs(10));
}
