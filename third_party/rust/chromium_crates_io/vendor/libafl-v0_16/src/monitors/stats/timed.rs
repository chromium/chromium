//! Timed stats for monitors

use alloc::collections::VecDeque;
use core::time::Duration;

use libafl_bolts::current_time;

/// A single status entry for timings
#[derive(Debug, Copy, Clone)]
pub struct TimedStat {
    /// The time
    pub time: Duration,
    /// The item
    pub item: f64,
}

/// Stats for timings
#[derive(Debug, Clone)]
pub struct TimedStats {
    /// Series of [`TimedStat`] entries
    pub series: VecDeque<TimedStat>,
    /// The time window to keep track of
    pub window: Duration,
    /// The retention window (how long to keep data)
    pub retention: Duration,
}

impl TimedStats {
    /// Create a new [`TimedStats`] struct
    #[must_use]
    pub fn new(window: Duration) -> Self {
        Self {
            series: VecDeque::new(),
            window,
            retention: window,
        }
    }

    /// Add a stat datapoint
    pub fn add(&mut self, time: Duration, item: f64) {
        if self.series.is_empty() || (self.series.back().unwrap().item - item).abs() > f64::EPSILON
        {
            while self.series.front().is_some()
                && time
                    .checked_sub(self.series.front().unwrap().time)
                    .unwrap_or(self.retention)
                    >= self.retention
            {
                self.series.pop_front();
            }
            self.series.push_back(TimedStat { time, item });
        }
    }

    /// Add a stat datapoint for the `current_time`
    pub fn add_now(&mut self, item: f64) {
        let time = current_time();
        self.add(time, item);
    }

    /// Change the window duration
    pub fn update_window(&mut self, window: Duration) {
        let default_stat = TimedStat {
            time: Duration::from_secs(0),
            item: 0.0,
        };

        self.window = window;
        if window > self.retention {
            self.retention = window;
        }
        while !self.series.is_empty()
            && self
                .series
                .back()
                .unwrap_or(&default_stat)
                .time
                .checked_sub(self.series.front().unwrap_or(&default_stat).time)
                .unwrap_or(self.retention)
                >= self.retention
        {
            self.series.pop_front();
        }
    }
}
