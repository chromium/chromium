//! [`TuiMonitor`] is a fancy-looking TUI monitor similar to `AFL`.
//!
//! It's based on [ratatui](https://ratatui.rs/)

use alloc::{
    boxed::Box,
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
use libafl_bolts::{ClientId, Error, current_time, format_big_number};
use ratatui::{Terminal, backend::CrosstermBackend};
use typed_builder::TypedBuilder;

use crate::monitors::{
    Monitor,
    stats::{EdgeCoverage, manager::ClientStatsManager},
};

/// The context modules for the TUI, containing the context and helper structs
pub mod context;
pub use context::{
    ClientTuiContext, ItemGeometry, ProcessTiming, TimedStat, TimedStats, TuiContext,
};

/// Layout logic for the TUI
pub mod layout;
/// The main UI logic, handling the drawing and events
pub mod ui;
/// Widgets used in the TUI, like charts and tables
pub mod widgets;
use ui::TuiUi;

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

/// Tracking monitor during fuzzing and display with [`ratatui`](https://ratatui.rs/)
#[derive(Debug, Clone)]
pub struct TuiMonitor {
    pub(crate) context: Arc<RwLock<TuiContext>>,
    config: TuiMonitorConfig,
    started: bool,
}

impl From<TuiMonitorConfig> for TuiMonitor {
    fn from(builder: TuiMonitorConfig) -> Self {
        Self {
            context: Arc::new(RwLock::new(TuiContext::new(current_time()))),
            started: false,
            config: builder,
        }
    }
}

impl Monitor for TuiMonitor {
    #[expect(clippy::cast_sign_loss, clippy::cast_precision_loss)]
    fn display(
        &mut self,
        client_stats_manager: &mut ClientStatsManager,
        event_msg: &str,
        sender_id: ClientId,
    ) -> Result<(), Error> {
        self.ensure_started();

        let cur_time = current_time();

        // 1. Prepare global stats (no lock needed yet)
        let (execsec, totalexec, run_time, exec_per_sec_pretty, corpus_size, objective_size) = {
            let global_stats = client_stats_manager.global_stats();
            (
                global_stats.execs_per_sec as u64,
                global_stats.total_execs,
                global_stats.run_time,
                global_stats.execs_per_sec_pretty.clone(),
                global_stats.corpus_size,
                global_stats.objective_size,
            )
        };

        // 2. Prepare client stats (no lock needed yet)
        client_stats_manager.client_stats_insert(sender_id)?;

        let (exec_sec, client_process_timing, client_item_geometry, client_snapshot) =
            client_stats_manager.update_client_stats_for(sender_id, |client| {
                (
                    client.execs_per_sec_pretty(cur_time),
                    client.process_timing(),
                    client.item_geometry(),
                    client.clone(),
                )
            })?;

        // 3. Prepare log message
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
            format_big_number(client_snapshot.corpus_size()),
            format_big_number(client_snapshot.objective_size()),
            format_big_number(client_snapshot.executions()),
            exec_sec
        );

        #[cfg(feature = "introspection")]
        if event_msg.contains("PerfMonitor") {
            let stats = &client_snapshot.introspection_stats;
            #[allow(clippy::cast_precision_loss)]
            let elapsed = stats.elapsed_cycles() as f64;
            if elapsed > 0.0 {
                #[allow(clippy::cast_precision_loss)]
                let scheduler_percent = stats.scheduler_cycles() as f64 / elapsed;
                #[allow(clippy::cast_precision_loss)]
                let manager_percent = stats.manager_cycles() as f64 / elapsed;
                write!(
                    fmt,
                    ", scheduler: {:.2}%, manager: {:.2}%",
                    scheduler_percent * 100.0,
                    manager_percent * 100.0
                )
                .unwrap();
            }
        }

        let client_run_time = cur_time.saturating_sub(client_stats_manager.start_time());

        // Collect stats to update in context (to avoid locking inside loops)
        // Store Cow to avoid allocation if possible
        let mut chart_updates = Vec::new();
        let mut tag_updates = Vec::new();

        for (key, val) in client_snapshot.user_stats() {
            write!(fmt, ", {key}: {val}").unwrap();

            // Collect tags for better coloring
            if let Some(tag) = val.tag() {
                tag_updates.push((key.clone(), tag));
            }

            if let Some(val_f64) = val.value().as_f64() {
                chart_updates.push((key, val_f64));
            }
        }
        for (key, val) in client_stats_manager.aggregated() {
            write!(fmt, ", {key}: {val}").unwrap();
            if let Some(val) = val.as_f64() {
                chart_updates.push((key, val));
            }
        }

        // 4. Update TuiContext (Lock ONCE)
        {
            let mut ctx = self.context.write().unwrap();

            // Global updates
            ctx.total_corpus_count = corpus_size;
            ctx.total_solutions = objective_size;
            ctx.corpus_size_timed.add(run_time, corpus_size as f64);
            ctx.objective_size_timed
                .add(run_time, objective_size as f64);

            let total_process_timing =
                client_stats_manager.process_timing(exec_per_sec_pretty, totalexec);
            ctx.total_process_timing = total_process_timing;

            ctx.execs_per_sec_timed.add(run_time, execsec as f64);
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

            // Tag updates
            for (key, tag) in tag_updates {
                ctx.tags.insert(key.into_owned(), tag);
            }

            // Chart updates
            for (key, val) in chart_updates {
                if !ctx.graphs.iter().any(|k| k.as_str() == key.as_ref()) {
                    ctx.graphs.push(key.to_string());
                }

                // Optimization: Try to get mutable reference without allocating String
                if let Some(stats) = ctx.custom_timed.get_mut(key.as_ref()) {
                    stats.add(client_run_time, val);
                } else {
                    ctx.custom_timed
                        .entry(key.to_string())
                        .or_insert_with(|| {
                            TimedStats::new(Duration::from_secs(context::DEFAULT_TIME_WINDOW))
                        })
                        .add(client_run_time, val);
                }
            }

            // Client data update
            if let Some(c) = ctx.clients.get_mut(&(sender_id.0 as usize)) {
                c.process_timing = client_process_timing;
                c.item_geometry = client_item_geometry;
                c.client_stats = client_snapshot;
            } else {
                let c = ClientTuiContext {
                    process_timing: client_process_timing,
                    item_geometry: client_item_geometry,
                    client_stats: client_snapshot,
                };
                ctx.clients.insert(sender_id.0 as usize, c);
            }

            // Client logs
            while ctx.client_logs.len() >= DEFAULT_LOGS_NUMBER {
                ctx.client_logs.pop_front();
            }
            ctx.client_logs.push_back(fmt);
        }

        Ok(())
    }
}

impl TuiMonitor {
    /// Create a builder for [`TuiMonitor`]
    pub fn builder() -> TuiMonitorConfigBuilder {
        TuiMonitorConfig::builder()
    }

    /// Start the tui thread if it is not running already
    pub fn ensure_started(&mut self) {
        if self.started {
            return;
        }

        if let Err(e) = enable_raw_mode() {
            println!("Could not enable raw mode, TUI will be disabled: {e:?}");
            self.started = true;
            return;
        }

        let tui_ui = TuiUi::with_version(
            self.config.title.clone(),
            self.config.version.clone(),
            self.config.enhanced_graphics,
        );

        #[cfg(unix)]
        {
            #[cfg(feature = "std")]
            use std::fs::File;
            use std::os::fd::{AsRawFd, FromRawFd};

            let stdout = unsafe { libc::dup(io::stdout().as_raw_fd()) };
            let stdout = unsafe { File::from_raw_fd(stdout) };
            run_tui_thread(
                self.context.clone(),
                Duration::from_millis(250),
                tui_ui,
                move || stdout.try_clone().unwrap(),
            );
        }
        #[cfg(not(unix))]
        {
            run_tui_thread(
                self.context.clone(),
                Duration::from_millis(250),
                tui_ui,
                io::stdout,
            );
        }

        self.started = true;
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
        let mut last_repaint = Instant::now();
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
            } else {
                let clients_count = context.read().unwrap().clients.len();
                // Only redraw the UI every 10 seconds if there are not more clients registered.
                // If there are more clients registered, they won't print stdout or stderr as they are other processes.
                if clients_count <= 1 && last_repaint.elapsed() > Duration::from_secs(10) {
                    drop(terminal.clear());
                    last_repaint = Instant::now();
                }
            }
            terminal.draw(|f| ui.draw(f, &context))?;

            let timeout = tick_rate.saturating_sub(last_tick.elapsed());
            if event::poll(timeout)?
                && let Event::Key(key) = event::read()?
            {
                match key.code {
                    KeyCode::Char(c) => ui.on_key(c, &context),
                    KeyCode::Left => ui.on_left(),
                    //KeyCode::Up => ui.on_up(),
                    KeyCode::Right => ui.on_right(),
                    //KeyCode::Down => ui.on_down(),
                    _ => {}
                }
            }
            if last_tick.elapsed() >= tick_rate {
                //context.on_tick();
                last_tick = Instant::now();
            }
            if ui.should_refresh {
                ui.should_refresh = false;
                cnt = 0;
                last_repaint = Instant::now();
                drop(terminal.clear());
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
