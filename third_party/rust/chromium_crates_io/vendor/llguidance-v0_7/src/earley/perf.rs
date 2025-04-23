use std::{
    fmt::{Display, Formatter},
    sync::atomic::AtomicUsize,
    time::Duration,
};

use serde::{ser::SerializeStruct as _, Serialize};

pub struct PerfTimer {
    name: String,
    max_time_us: AtomicUsize,
    time_us: AtomicUsize,
    num_calls: AtomicUsize,
}

impl PerfTimer {
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            max_time_us: AtomicUsize::new(0),
            time_us: AtomicUsize::new(0),
            num_calls: AtomicUsize::new(0),
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn get(&self) -> (usize, usize, usize) {
        (
            self.max_time_us.load(std::sync::atomic::Ordering::Relaxed),
            self.time_us.load(std::sync::atomic::Ordering::Relaxed),
            self.num_calls.load(std::sync::atomic::Ordering::Relaxed),
        )
    }

    #[inline(always)]
    pub fn record(&self, d: Duration) {
        let us = Duration::as_micros(&d) as usize;
        self.max_time_us
            .fetch_max(us, std::sync::atomic::Ordering::Relaxed);
        self.time_us
            .fetch_add(us, std::sync::atomic::Ordering::Relaxed);
        self.num_calls
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
    }
}

impl Serialize for PerfTimer {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        let (max_time_us, time_us, num_calls) = self.get();
        let avg = time_us / num_calls;
        let mut state = serializer.serialize_struct("PerfTimer", 4)?;
        state.serialize_field("name", &self.name)?;
        state.serialize_field("avg", &avg)?;
        state.serialize_field("calls", &num_calls)?;
        state.serialize_field("max", &max_time_us)?;
        state.end()
    }
}

impl Display for PerfTimer {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let (max_time_us, time_us, num_calls) = self.get();
        let avg = time_us / std::cmp::max(1, num_calls);
        write!(
            f,
            "{}: avg:{}μs calls:{} max:{}μs total:{}ms",
            self.name,
            num_with_commas(avg),
            num_with_commas(num_calls),
            num_with_commas(max_time_us),
            num_with_commas(time_us / 1000)
        )
    }
}

#[derive(Serialize)]
pub struct ParserPerfCounters {
    pub force_bytes: PerfTimer,
    pub force_bytes_empty: PerfTimer,
    pub tmp_counter: PerfTimer,
    pub tokenize_ff: PerfTimer,
    pub compute_bias: PerfTimer,
    pub compute_mask: PerfTimer,
    pub precompute: PerfTimer,
}

impl Default for ParserPerfCounters {
    fn default() -> Self {
        Self::new()
    }
}

impl ParserPerfCounters {
    pub fn new() -> Self {
        Self {
            force_bytes: PerfTimer::new("force_bytes"),
            force_bytes_empty: PerfTimer::new("force_bytes_empty"),
            tmp_counter: PerfTimer::new("tmp_counter"),
            tokenize_ff: PerfTimer::new("tokenize_ff"),
            compute_bias: PerfTimer::new("compute_bias"),
            compute_mask: PerfTimer::new("compute_mask"),
            precompute: PerfTimer::new("precompute"),
        }
    }

    pub fn counters(&self) -> Vec<&PerfTimer> {
        vec![
            &self.force_bytes,
            &self.force_bytes_empty,
            &self.tokenize_ff,
            &self.compute_bias,
            &self.compute_mask,
            &self.tmp_counter,
            &self.precompute,
        ]
    }
}

impl Display for ParserPerfCounters {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        for c in self.counters() {
            c.fmt(f)?;
            writeln!(f)?;
        }
        Ok(())
    }
}

pub fn num_with_commas(x: usize) -> String {
    let s = x.to_string();
    let len = s.len();
    let offset = len % 3;
    let mut result = String::new();

    for (i, c) in s.chars().enumerate() {
        // Insert a comma once we've passed 'offset' and every 3 digits after that.
        if i != 0 && i >= offset && (i - offset) % 3 == 0 {
            result.push(',');
        }
        result.push(c);
    }

    result
}
