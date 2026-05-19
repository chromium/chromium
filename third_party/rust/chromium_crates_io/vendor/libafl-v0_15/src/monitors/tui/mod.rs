//! [`TuiMonitor`] is a fancy-looking TUI monitor similar to `AFL`.
//!
//! It's based on [ratatui](https://ratatui.rs/)

use alloc::{
    borrow::Cow,
    boxed::Box,
    collections::VecDeque,
    string::{String, ToString},
    sync::Arc,
    vec::Vec,
};
use core::{fmt::Write as _, time::Duration};
use std::{
    io::{self, BufRead, Write},
    panic,
    sync::RwLock,
    thread,
    time::Instant,
};

use crossterm::{
    cursor::{EnableBlinking, Show},
    event::{self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode},
    execute,
    terminal::{EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode},
};
use hashbrown::HashMap;
use libafl_bolts::{ClientId, Error, current_time, format_big_number, format_duration};
use ratatui::{Terminal, backend::CrosstermBackend};
use typed_builder::TypedBuilder;

#[cfg(feature = "introspection")]
use crate::monitors::stats::perf_stats::{ClientPerfStats, PerfFeature};
use crate::monitors::{
    Monitor,
    stats::{
        ClientStats, EdgeCoverage, ItemGeometry, ProcessTiming, manager::ClientStatsManager,
        user_stats::UserStats,
    },
};

#[expect(missing_docs)]
pub mod ui;
use ui::TuiUi;

const DEFAULT_TIME_WINDOW: u64 = 60 * 10; // 10 min
const DEFAULT_LOGS_NUMBER: usize = 128;

#[derive(Debug, Clone, TypedBuilder)]
#[builder(build_method(into = TuiMonitor), builder_method(vis = "pub(crate)",
    doc = "Build the [`TuiMonitor`] from the set values"))]
/// Settings to create a new [`TuiMonitor`].
/// Use `TuiMonitor::builder()` or create this config and call `.into()` to create a new [`TuiMonitor`].
pub struct TuiMonitorConfig {
    /// The title to show
    #[builder(default_code = r#""LibAFL Fuzzer".to_string()"#, setter(into))]
    pub title: String,
    /// A version string to show for this (optional)
    #[builder(default_code = r#""default".to_string()"#, setter(into))]
    pub version: String,
    /// Enables unicode TUI graphics, Looks better but may interfere with old terminals.
    #[builder(default = true)]
    pub enhanced_graphics: bool,
}

/// A single status entry for timings
#[derive(Debug, Copy, Clone)]
pub struct TimedStat {
    /// The time
    pub time: Duration,
    /// The item
    pub item: u64,
}

/// Stats for timings
#[derive(Debug, Clone)]
pub struct TimedStats {
    /// Series of [`TimedStat`] entries
    pub series: VecDeque<TimedStat>,
    /// The time window to keep track of
    pub window: Duration,
}

impl TimedStats {
    /// Create a new [`TimedStats`] struct
    #[must_use]
    pub fn new(window: Duration) -> Self {
        Self {
            series: VecDeque::new(),
            window,
        }
    }

    /// Add a stat datapoint
    pub fn add(&mut self, time: Duration, item: u64) {
        if self.series.is_empty() || self.series.back().unwrap().item != item {
            if self.series.front().is_some()
                && time
                    .checked_sub(self.series.front().unwrap().time)
                    .unwrap_or(self.window)
                    >= self.window
            {
                self.series.pop_front();
            }
            self.series.push_back(TimedStat { time, item });
        }
    }

    /// Add a stat datapoint for the `current_time`
    pub fn add_now(&mut self, item: u64) {
        if self.series.is_empty() || self.series[self.series.len() - 1].item != item {
            let time = current_time();
            if self.series.front().is_some()
                && time
                    .checked_sub(self.series.front().unwrap().time)
                    .unwrap_or(self.window)
                    >= self.window
            {
                self.series.pop_front();
            }
            self.series.push_back(TimedStat { time, item });
        }
    }

    /// Change the window duration
    pub fn update_window(&mut self, window: Duration) {
        let default_stat = TimedStat {
            time: Duration::from_secs(0),
            item: 0,
        };

        self.window = window;
        while !self.series.is_empty()
            && self
                .series
                .back()
                .unwrap_or(&default_stat)
                .time
                .checked_sub(self.series.front().unwrap_or(&default_stat).time)
                .unwrap()
                >= window
        {
            self.series.pop_front();
        }
    }
}

/// The context to show performance metrics
#[cfg(feature = "introspection")]
#[derive(Debug, Default, Clone)]
pub struct PerfTuiContext {
    /// Time spent in the scheduler
    pub scheduler: f64,
    /// Time spent in the event manager
    pub manager: f64,
    /// Additional time
    pub unmeasured: f64,
    /// Time spent in each individual stage
    pub stages: Vec<Vec<(String, f64)>>,
    /// Time spent in each individual feedback
    pub feedbacks: Vec<(String, f64)>,
}

#[cfg(feature = "introspection")]
impl PerfTuiContext {
    /// Get the data for performance metrics
    #[expect(clippy::cast_precision_loss)]
    pub fn grab_data(&mut self, m: &ClientPerfStats) {
        // Calculate the elapsed time from the monitor
        let elapsed: f64 = m.elapsed_cycles() as f64;

        // Calculate the percentages for each benchmark
        self.scheduler = m.scheduler_cycles() as f64 / elapsed;
        self.manager = m.manager_cycles() as f64 / elapsed;

        // Calculate the remaining percentage that has not been benchmarked
        let mut other_percent = 1.0;
        other_percent -= self.scheduler;
        other_percent -= self.manager;

        self.stages.clear();

        // Calculate each stage
        // Make sure we only iterate over used stages
        for (_stage_index, features) in m.used_stages() {
            let mut features_percentages = vec![];

            for (feature_index, feature) in features.iter().enumerate() {
                // Calculate this current stage's percentage
                let feature_percent = *feature as f64 / elapsed;

                // Ignore this feature if it isn't used
                if feature_percent == 0.0 {
                    continue;
                }

                // Update the other percent by removing this current percent
                other_percent -= feature_percent;

                // Get the actual feature from the feature index for printing its name
                let feature: PerfFeature = feature_index.into();
                features_percentages.push((format!("{feature:?}"), feature_percent));
            }

            self.stages.push(features_percentages);
        }

        self.feedbacks.clear();

        for (feedback_name, feedback_time) in m.feedbacks() {
            // Calculate this current stage's percentage
            let feedback_percent = *feedback_time as f64 / elapsed;

            // Ignore this feedback if it isn't used
            if feedback_percent == 0.0 {
                continue;
            }

            // Update the other percent by removing this current percent
            other_percent -= feedback_percent;

            self.feedbacks
                .push((feedback_name.clone(), feedback_percent));
        }

        self.unmeasured = other_percent;
    }
}

/// The context for a single client tracked in this [`TuiMonitor`]
#[derive(Debug, Default, Clone)]
pub struct ClientTuiContext {
    /// The corpus size
    pub corpus: u64,
    /// Amount of objectives
    pub objectives: u64,
    /// Amount of executions
    pub executions: u64,
    /// Float value formatted as String
    pub map_density: String,

    /// How many cycles have been done.
    /// Roughly: every testcase has been scheduled once, but highly fuzzer-specific.
    pub cycles_done: u64,

    /// Times for processing
    pub process_timing: ProcessTiming,
    /// The individual entry geometry
    pub item_geometry: ItemGeometry,
    /// Extra fuzzer-specific stats
    pub user_stats: HashMap<Cow<'static, str>, UserStats>,
}

impl ClientTuiContext {
    /// Grab data for a single client
    pub fn grab_data(&mut self, client: &mut ClientStats) {
        self.corpus = client.corpus_size();
        self.objectives = client.objective_size();
        self.executions = client.executions();
        self.process_timing = client.process_timing();

        self.map_density = client.edges_coverage().map_or(
            "0%".to_string(),
            |EdgeCoverage {
                 edges_hit,
                 edges_total,
             }| format!("{}%", edges_hit * 100 / edges_total),
        );
        self.item_geometry = client.item_geometry();

        for (key, val) in client.user_stats() {
            self.user_stats.insert(key.clone(), val.clone());
        }
    }
}

/// The [`TuiContext`] for this [`TuiMonitor`]
#[derive(Debug, Clone)]
#[expect(missing_docs)]
pub struct TuiContext {
    pub graphs: Vec<String>,

    // TODO update the window using the UI key press events (+/-)
    pub corpus_size_timed: TimedStats,
    pub objective_size_timed: TimedStats,
    pub execs_per_sec_timed: TimedStats,

    #[cfg(feature = "introspection")]
    pub introspection: HashMap<usize, PerfTuiContext>,

    pub clients: HashMap<usize, ClientTuiContext>,

    pub client_logs: VecDeque<String>,

    pub clients_num: usize,
    pub total_execs: u64,
    pub start_time: Duration,

    pub total_map_density: String,
    pub total_solutions: u64,
    pub total_corpus_count: u64,

    pub total_process_timing: ProcessTiming,
    pub total_item_geometry: ItemGeometry,
}

impl TuiContext {
    /// Create a new TUI context
    #[must_use]
    pub fn new(start_time: Duration) -> Self {
        Self {
            graphs: vec!["corpus".into(), "objectives".into(), "exec/sec".into()],
            corpus_size_timed: TimedStats::new(Duration::from_secs(DEFAULT_TIME_WINDOW)),
            objective_size_timed: TimedStats::new(Duration::from_secs(DEFAULT_TIME_WINDOW)),
            execs_per_sec_timed: TimedStats::new(Duration::from_secs(DEFAULT_TIME_WINDOW)),

            #[cfg(feature = "introspection")]
            introspection: HashMap::default(),
            clients: HashMap::default(),

            client_logs: VecDeque::with_capacity(DEFAULT_LOGS_NUMBER),

            clients_num: 0,
            total_execs: 0,
            start_time,

            total_map_density: "0%".to_string(),
            total_solutions: 0,
            total_corpus_count: 0,
            total_item_geometry: ItemGeometry::new(),
            total_process_timing: ProcessTiming::new(),
        }
    }
}

/// Tracking monitor during fuzzing and display with [`ratatui`](https://ratatui.rs/)
#[derive(Debug, Clone)]
pub struct TuiMonitor {
    pub(crate) context: Arc<RwLock<TuiContext>>,
}

impl From<TuiMonitorConfig> for TuiMonitor {
    #[expect(deprecated)]
    fn from(builder: TuiMonitorConfig) -> Self {
        Self::with_time(
            TuiUi::with_version(builder.title, builder.version, builder.enhanced_graphics),
            current_time(),
        )
    }
}

impl Monitor for TuiMonitor {
    #[expect(clippy::cast_sign_loss)]
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        let cur_time = current_time();

        {
            let global_stats = client_stats_manager.global_stats();
            // TODO implement floating-point support for TimedStat
            let execsec = global_stats.execs_per_sec as u64;
            let totalexec = global_stats.total_execs;
            let run_time = global_stats.run_time;
            let exec_per_sec_pretty = global_stats.execs_per_sec_pretty.clone();
            let total_execs = global_stats.total_execs;
            let mut ctx = self.context.write().unwrap();
            ctx.total_corpus_count = global_stats.corpus_size;
            ctx.total_solutions = global_stats.objective_size;
            ctx.corpus_size_timed
                .add(run_time, global_stats.corpus_size);
            ctx.objective_size_timed
                .add(run_time, global_stats.objective_size);
            let total_process_timing =
                client_stats_manager.process_timing(exec_per_sec_pretty, total_execs);

            ctx.total_process_timing = total_process_timing;
            ctx.execs_per_sec_timed.add(run_time, execsec);
            ctx.start_time = client_stats_manager.start_time();
            ctx.total_execs = totalexec;
            ctx.clients_num = client_stats_manager.client_stats().len();
            ctx.total_map_density = client_stats_manager.edges_coverage().map_or(
                "0%".to_string(),
                |EdgeCoverage {
                     edges_hit,
                     edges_total,
                 }| format!("{}%", edges_hit * 100 / edges_total),
            );
            ctx.total_item_geometry = client_stats_manager.item_geometry();
        }

        client_stats_manager.client_stats_insert(sender_id)?;
        let exec_sec = client_stats_manager
            .update_client_stats_for(sender_id, |client| client.execs_per_sec_pretty(cur_time))?;
        let client = client_stats_manager.client_stats_for(sender_id)?;

        let sender = format!("#{}", sender_id.0);
        let pad = if event_msg.len() + sender.len() < 13 {
            " ".repeat(13 - event_msg.len() - sender.len())
        } else {
            String::new()
        };
        let head = format!("{event_msg}{pad} {sender}");
        let mut fmt = format!(
            "[{}] corpus: {}, objectives: {}, executions: {}, exec/sec: {}",
            head,
            format_big_number(client.corpus_size()),
            format_big_number(client.objective_size()),
            format_big_number(client.executions()),
            exec_sec
        );
        for (key, val) in client.user_stats() {
            write!(fmt, ", {key}: {val}").unwrap();
        }
        for (key, val) in client_stats_manager.aggregated() {
            write!(fmt, ", {key}: {val}").unwrap();
        }

        {
            let mut ctx = self.context.write().unwrap();
            client_stats_manager.update_client_stats_for(sender_id, |client| {
                ctx.clients
                    .entry(sender_id.0 as usize)
                    .or_default()
                    .grab_data(client);
            })?;
            while ctx.client_logs.len() >= DEFAULT_LOGS_NUMBER {
                ctx.client_logs.pop_front();
            }
            ctx.client_logs.push_back(fmt);
        }

        #[cfg(feature = "introspection")]
        {
            // Print the client performance monitor. Skip the Client IDs that have never sent anything.
            for (i, (_, client)) in client_stats_manager
                .client_stats()
                .iter()
                .filter(|(_, x)| x.enabled())
                .enumerate()
            {
                self.context
                    .write()
                    .unwrap()
                    .introspection
                    .entry(i + 1)
                    .or_default()
                    .grab_data(&client.introspection_stats);
            }
        }

        Ok(())
    }
}

impl TuiMonitor {
    /// Create a builder for [`TuiMonitor`]
    pub fn builder() -> TuiMonitorConfigBuilder {
        TuiMonitorConfig::builder()
    }

    /// Creates the monitor.
    ///
    /// # Deprecation Note
    /// Use `TuiMonitor::builder()` instead.
    #[deprecated(
        since = "0.13.2",
        note = "Please use TuiMonitor::builder() instead of creating TuiUi directly."
    )]
    #[must_use]
    #[expect(deprecated)]
    pub fn new(tui_ui: TuiUi) -> Self {
        Self::with_time(tui_ui, current_time())
    }

    /// Creates the monitor with a given `start_time`.
    ///
    /// # Deprecation Note
    /// Use `TuiMonitor::builder()` instead.
    #[deprecated(
        since = "0.13.2",
        note = "Please use TuiMonitor::builder() instead of creating TuiUi directly."
    )]
    #[must_use]
    pub fn with_time(tui_ui: TuiUi, start_time: Duration) -> Self {
        let context = Arc::new(RwLock::new(TuiContext::new(start_time)));

        enable_raw_mode().unwrap();
        #[cfg(unix)]
        {
            use std::{
                fs::File,
                os::fd::{AsRawFd, FromRawFd},
            };

            let stdout = unsafe { libc::dup(io::stdout().as_raw_fd()) };
            let stdout = unsafe { File::from_raw_fd(stdout) };
            run_tui_thread(
                context.clone(),
                Duration::from_millis(250),
                tui_ui,
                move || stdout.try_clone().unwrap(),
            );
        }
        #[cfg(not(unix))]
        {
            run_tui_thread(
                context.clone(),
                Duration::from_millis(250),
                tui_ui,
                io::stdout,
            );
        }
        Self { context }
    }
}

fn run_tui_thread<W: Write + Send + Sync + 'static>(
    context: Arc<RwLock<TuiContext>>,
    tick_rate: Duration,
    tui_ui: TuiUi,
    stdout_provider: impl Send + Sync + 'static + Fn() -> W,
) {
    thread::spawn(move || -> io::Result<()> {
        // setup terminal
        let mut stdout = stdout_provider();
        execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;

        let backend = CrosstermBackend::new(stdout);
        let mut terminal = Terminal::new(backend)?;

        let mut ui = tui_ui;

        let mut last_tick = Instant::now();
        let mut cnt = 0;

        // Catching panics when the main thread dies
        let old_hook = panic::take_hook();
        panic::set_hook(Box::new(move |panic_info| {
            let mut stdout = stdout_provider();
            disable_raw_mode().unwrap();
            execute!(
                stdout,
                LeaveAlternateScreen,
                DisableMouseCapture,
                Show,
                EnableBlinking,
            )
            .unwrap();
            old_hook(panic_info);
        }));

        loop {
            // to avoid initial ui glitches
            if cnt < 8 {
                drop(terminal.clear());
                cnt += 1;
            }
            terminal.draw(|f| ui.draw(f, &context))?;

            let timeout = tick_rate
                .checked_sub(last_tick.elapsed())
                .unwrap_or_else(|| Duration::from_secs(0));
            if event::poll(timeout)? {
                if let Event::Key(key) = event::read()? {
                    match key.code {
                        KeyCode::Char(c) => ui.on_key(c),
                        KeyCode::Left => ui.on_left(),
                        //KeyCode::Up => ui.on_up(),
                        KeyCode::Right => ui.on_right(),
                        //KeyCode::Down => ui.on_down(),
                        _ => {}
                    }
                }
            }
            if last_tick.elapsed() >= tick_rate {
                //context.on_tick();
                last_tick = Instant::now();
            }
            if ui.should_quit {
                // restore terminal
                disable_raw_mode()?;
                execute!(
                    terminal.backend_mut(),
                    LeaveAlternateScreen,
                    DisableMouseCapture
                )?;
                terminal.show_cursor()?;

                println!(
                    "\nPress Control-C to stop the fuzzers, otherwise press Enter to resume the visualization\n"
                );

                let mut line = String::new();
                io::stdin().lock().read_line(&mut line)?;

                // setup terminal
                let mut stdout = io::stdout();
                enable_raw_mode()?;
                execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;

                cnt = 0;
                ui.should_quit = false;
            }
        }
    });
}
