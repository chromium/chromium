use alloc::{
    string::{String, ToString},
    sync::Arc,
    vec::Vec,
};
use core::{
    hash::{Hash, Hasher},
    time::Duration,
};
use std::sync::RwLock;

use libafl_bolts::{current_time, format_big_number};
use ratatui::{
    Frame,
    layout::{Alignment, Constraint, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Borders, Paragraph, Tabs},
};

#[cfg(feature = "introspection")]
use super::widgets::draw_scrolled_stats;
use super::{
    layout::{split_client, split_main, split_title, split_top},
    widgets::{
        MultiTimeChartOptions, TimeChartOptions, calculate_tab_window, draw_item_geometry_text,
        draw_key_value_block, draw_logs, draw_main_block, draw_multi_time_chart,
        draw_process_timing_text, draw_time_chart, draw_user_stats,
    },
};
#[cfg(feature = "introspection")]
use crate::monitors::stats::PerfFeature;
use crate::monitors::{
    stats::{
        ProcessTiming, TimedStat,
        user_stats::{
            TAG_AFL_STATS_CYCLES_DONE, TAG_AFL_STATS_CYCLES_WO_FINDS, TAG_AFL_STATS_IMPORTED,
            TAG_AFL_STATS_OWN_FINDS, TAG_AFL_STATS_PENDING, TAG_AFL_STATS_PENDING_FAV,
            TAG_AFL_STATS_PENDING_FAVORED, UserStatsTag, UserStatsValue,
        },
    },
    tui::{ItemGeometry, TuiContext},
};

/// The UI for the TUI monitor
#[derive(Default, Debug)]
#[allow(clippy::struct_excessive_bools)]
pub struct TuiUi {
    title: String,
    version: String,
    enhanced_graphics: bool,
    show_logs: bool,
    client_idx: usize,
    clients: Vec<usize>,
    charts_tab_idx: usize,
    graph_data: Vec<(f64, f64)>,
    multi_graph_data: Vec<Vec<(f64, f64)>>,
    is_narrow: bool,
    logs_wrap: bool,
    /// Cached tabs for lock-free navigation
    tabs: Vec<String>,

    /// If the UI should quit
    pub should_quit: bool,
    /// If the UI should refresh
    pub(crate) should_refresh: bool,
    user_stats_scroll: usize,
    pub(crate) user_stats_page_size: usize,

    #[cfg(feature = "introspection")]
    perf_stats_scroll: usize,
    #[cfg(feature = "introspection")]
    pub(crate) perf_stats_page_size: usize,

    show_geometry: bool,
    item_geometry_scroll: usize,
    pub(crate) item_geometry_page_size: usize,

    pizza_mode: bool,
}

#[derive(Debug)]
struct GenericStats {
    labels: Vec<(Span<'static>, String)>,
}

#[derive(Debug)]
struct PreparedFrameData {
    pizza_mode: bool,
    // Overall
    generic_stats: GenericStats,
    total_timing: Option<(ProcessTiming, Duration)>,
    total_geometry: Option<ItemGeometry>,

    // Client
    client_idx: usize,
    client_generic_stats: GenericStats,
    client_timing: Option<(ProcessTiming, Duration)>,
    client_geometry: Option<ItemGeometry>,
    client_user_stats: Vec<(Span<'static>, String)>,

    // Logs
    logs: Vec<String>,

    // Charts
    tabs: Vec<String>,
    active_chart: Option<ChartKind>,

    #[cfg(feature = "introspection")]
    client_perf_stats: Vec<(Span<'static>, String)>,
}

#[derive(Debug)]
enum ChartKind {
    Single {
        name: String,
        series: Vec<TimedStat>,
        window: Duration,
        run_time: Duration,
        style: Style,
    },
    Multi {
        name: String,
        series: Vec<(String, Vec<TimedStat>, Duration, Style)>,
        run_time: Duration,
    },
}

fn next_larger(sorted: &[usize], value: usize) -> Option<usize> {
    sorted.iter().position(|x| *x > value).map(|i| sorted[i])
}

fn next_smaller(sorted: &[usize], value: usize) -> Option<usize> {
    sorted.iter().rposition(|x| *x < value).map(|i| sorted[i])
}

impl TuiUi {
    /// Create a new [`TuiUi`] with the given title and enhanced graphics flag
    #[must_use]
    pub fn new(title: String, enhanced_graphics: bool) -> Self {
        Self::with_version(title, String::from("default"), enhanced_graphics)
    }

    /// create a new [`TuiUi`] with a given `version` string.
    #[must_use]
    pub fn with_version(mut title: String, version: String, enhanced_graphics: bool) -> Self {
        let pizza_mode = crate::monitors::pizza_is_served();
        if pizza_mode {
            title = "Mozzarbella Pizzeria management system".into();
        }
        Self {
            title,
            version,
            enhanced_graphics,
            show_logs: true,
            client_idx: 0,
            clients: vec![],
            charts_tab_idx: 0,
            graph_data: vec![],
            multi_graph_data: vec![],
            is_narrow: false,
            logs_wrap: false,
            tabs: vec!["Overview".to_string()],
            should_quit: false,
            should_refresh: false,
            user_stats_scroll: 0,
            user_stats_page_size: 10,
            #[cfg(feature = "introspection")]
            perf_stats_scroll: 0,
            #[cfg(feature = "introspection")]
            perf_stats_page_size: 10,
            show_geometry: false,
            item_geometry_scroll: 0,
            item_geometry_page_size: 10,
            pizza_mode: crate::monitors::pizza_is_served(),
        }
    }

    /// Adds pineapple.
    pub fn add_pineapple(&mut self) {
        self.pizza_mode = false;
    }

    fn get_tabs(&self, ctx: &TuiContext) -> Vec<String> {
        let mut tabs = vec![if self.pizza_mode {
            "Baking overview".to_string()
        } else {
            "Overview".to_string()
        }];
        if self.pizza_mode {
            for graph in &ctx.graphs {
                tabs.push(match graph.as_str() {
                    "corpus" => "pizzas".to_string(),
                    "objectives" => "deliveries".to_string(),
                    "exec/sec" => "p/s".to_string(),
                    "User Stats" => "Kitchen Stats".to_string(),
                    _ => graph.clone(),
                });
            }
        } else {
            tabs.extend(ctx.graphs.clone());
        }
        tabs
    }

    /// Some stats are already displayed in other contexts. We can filter those.
    fn is_redundant_userstat(tag: Option<UserStatsTag>) -> bool {
        if let Some(tag) = tag {
            matches!(
                tag,
                TAG_AFL_STATS_CYCLES_DONE
                    | TAG_AFL_STATS_CYCLES_WO_FINDS
                    | TAG_AFL_STATS_IMPORTED
                    | TAG_AFL_STATS_OWN_FINDS
                    | TAG_AFL_STATS_PENDING
                    | TAG_AFL_STATS_PENDING_FAV
                    | TAG_AFL_STATS_PENDING_FAVORED
            )
        } else {
            false
        }
    }

    fn update_clients_list(&mut self, ctx: &TuiContext) {
        if ctx.clients_num != self.clients.len() {
            let mut all: Vec<usize> = ctx.clients.keys().copied().collect();
            all.sort_unstable();
            if !all.is_empty() && !all.contains(&self.client_idx) {
                self.client_idx = all[0];
            }
            self.clients = all;
        }
    }

    fn prepare_overall_stats(&self, ctx: &TuiContext) -> GenericStats {
        let (corpus, objectives, executions) = if self.pizza_mode {
            ("pizzas on the menu", "deliveries", "doughs")
        } else {
            ("corpus count", "solutions", "total execs")
        };

        let mut labels = vec![
            (
                Span::raw(if self.pizza_mode {
                    "customers"
                } else {
                    "clients"
                }),
                format!("{}", ctx.clients.len()),
            ),
            (Span::raw(executions), format_big_number(ctx.total_execs)),
            (
                Span::raw(objectives),
                format_big_number(ctx.total_solutions),
            ),
            (Span::raw(corpus), format_big_number(ctx.total_corpus_count)),
        ];

        if self.pizza_mode {
            labels.push((Span::raw("Pineapple pizzas"), "0".to_string()));
        }

        GenericStats { labels }
    }

    fn prepare_total_timing(
        ctx: &TuiContext,
        run_time: Duration,
    ) -> Option<(ProcessTiming, Duration)> {
        if ctx.total_process_timing.exec_speed != "0/sec" {
            Some((ctx.total_process_timing.clone(), run_time))
        } else if let Some(client) = ctx.clients.get(&0) {
            if client.process_timing.exec_speed == "0/sec" {
                None
            } else {
                Some((client.process_timing.clone(), run_time))
            }
        } else {
            None
        }
    }

    #[allow(clippy::cast_precision_loss, clippy::type_complexity)]
    fn prepare_client_data(
        &self,
        ctx: &TuiContext,
    ) -> (
        GenericStats,
        Option<(ProcessTiming, Duration)>,
        Option<ItemGeometry>,
        Vec<(Span<'static>, String)>,
    ) {
        if let Some(client) = ctx.clients.get(&self.client_idx) {
            let timing = if client.process_timing.exec_speed == "0/sec" {
                None
            } else {
                Some((
                    client.process_timing.clone(),
                    current_time().saturating_sub(client.process_timing.client_start_time),
                ))
            };

            let (corpus, objectives, executions) = if self.pizza_mode {
                ("pizzas", "deliveries", "doughs")
            } else {
                ("corpus count", "solutions", "total execs")
            };

            let generic = GenericStats {
                labels: vec![
                    (
                        Span::raw(corpus),
                        format_big_number(client.client_stats.corpus_size()),
                    ),
                    (
                        Span::raw(executions),
                        format_big_number(client.client_stats.executions()),
                    ),
                    (
                        Span::raw(if self.pizza_mode {
                            "ovens heated"
                        } else {
                            "cycles done"
                        }),
                        client
                            .cycles_done()
                            .map_or(String::new(), |c| c.to_string()),
                    ),
                    (
                        Span::raw(objectives),
                        format_big_number(client.client_stats.objective_size()),
                    ),
                    (
                        Span::raw(if self.pizza_mode {
                            "current order"
                        } else {
                            "current testcase"
                        }),
                        client
                            .client_stats
                            .user_stats()
                            .get("current_testcase_idx")
                            .and_then(|s| s.value().as_u64())
                            .map_or_else(|| "-".to_string(), |v| v.to_string()),
                    ),
                ],
            };

            // Format user stats
            let mut keys: Vec<_> = client.client_stats.user_stats().keys().collect();
            keys.sort();
            let user_stats_vec = keys
                .into_iter()
                .filter(|k| {
                    let val = client.client_stats.user_stats().get(*k).unwrap();
                    !Self::is_redundant_userstat(val.tag())
                })
                .map(|k| {
                    let val = client.client_stats.user_stats().get(k).unwrap();
                    let val_str = match val.value() {
                        UserStatsValue::Number(n) => format_big_number(*n),
                        UserStatsValue::Float(f) => format!("{f:.2}"),
                        UserStatsValue::String(s) => s.to_string(),
                        UserStatsValue::Ratio(a, b) => {
                            if *b == 0 {
                                "0/0".into()
                            } else {
                                let percentage = (*a as f64 / *b as f64) * 100.0;
                                format!("{a}/{b} ({percentage:.2}%)")
                            }
                        }
                        UserStatsValue::Percent(p) => format!("{:.2}%", p * 100.0),
                    };

                    let mut hasher = std::collections::hash_map::DefaultHasher::new();

                    if let Some(tag) = val.tag() {
                        tag.hash(&mut hasher);
                    } else {
                        k.hash(&mut hasher);
                    }
                    let color_idx = hasher.finish() as usize % 4;
                    let colors = [Color::Green, Color::Blue, Color::Magenta, Color::Cyan];
                    let style = Style::default().fg(colors[color_idx]);

                    // Map keys if pizza mode is active
                    let key_str = if self.pizza_mode {
                        match k.as_ref() {
                            "cycles done" => "seasons done",
                            "unique crashes" => "at table",
                            "unique hangs" => "number of Peroni",
                            "map density" => "Baking progress",
                            "edges" => "toppings",
                            "stability" => "dough consistency",
                            "havoc_inc" => "spice level",
                            "havoc_mod" => "topping variety",
                            "imported" => "takeouts",
                            "favored" => "chef's choice",
                            "pending" => "waiting orders",
                            _ => k.as_ref(),
                        }
                    } else {
                        k.as_ref()
                    };

                    (Span::styled(key_str.to_string(), style), val_str)
                })
                .collect();

            (
                generic,
                timing,
                client.item_geometry.clone(),
                user_stats_vec,
            )
        } else {
            (GenericStats { labels: vec![] }, None, None, vec![])
        }
    }

    #[cfg(feature = "introspection")]
    #[allow(clippy::cast_precision_loss)]
    fn prepare_client_perf_stats(&self, ctx: &TuiContext) -> Vec<(Span<'static>, String)> {
        if let Some(client) = ctx.clients.get(&self.client_idx) {
            let stats = &client.client_stats.introspection_stats;
            let mut vec = Vec::new();
            let elapsed = stats.elapsed_cycles() as f64;
            let scheduler_percent = if elapsed == 0.0 {
                0.0
            } else {
                stats.scheduler_cycles() as f64 / elapsed
            };
            vec.push((
                Span::raw(if self.pizza_mode {
                    "pizza chef"
                } else {
                    "scheduler"
                }),
                format!("{:.2}%", scheduler_percent * 100.0),
            ));

            let manager_percent = if elapsed == 0.0 {
                0.0
            } else {
                stats.manager_cycles() as f64 / elapsed
            };
            vec.push((
                Span::raw(if self.pizza_mode {
                    "delivery boy"
                } else {
                    "manager"
                }),
                format!("{:.2}%", manager_percent * 100.0),
            ));

            if elapsed != 0.0 {
                for (stage_index, features) in stats.used_stages() {
                    for (feature_index, feature) in features.iter().enumerate() {
                        let feature_percent = *feature as f64 / elapsed;
                        if feature_percent > 0.0 {
                            let feature_name: PerfFeature = feature_index.into();
                            let label = if self.pizza_mode {
                                format!("topping {stage_index}: {feature_name:?}")
                            } else {
                                format!("stage {stage_index}: {feature_name:?}")
                            };
                            vec.push((
                                Span::raw(label),
                                format!("{:.2}%", feature_percent * 100.0),
                            ));
                        }
                    }
                }

                for (name, val) in stats.feedbacks() {
                    let feedback_percent = *val as f64 / elapsed;
                    if feedback_percent > 0.0 {
                        let label = if self.pizza_mode {
                            format!("taste test: {name}")
                        } else {
                            format!("feedback: {name}")
                        };
                        vec.push((
                            Span::raw(label),
                            format!("{:.2}%", feedback_percent * 100.0),
                        ));
                    }
                }
            }
            vec
        } else {
            vec![]
        }
    }

    fn prepare_charts(
        &mut self,
        ctx: &TuiContext,
        run_time: Duration,
    ) -> (Vec<String>, Option<ChartKind>) {
        let tabs = self.get_tabs(ctx);
        if self.charts_tab_idx >= tabs.len() {
            self.charts_tab_idx = 0;
        }

        let mut active_chart = None;
        if tabs.len() > 1 {
            let chart_idx = if self.charts_tab_idx == 0 {
                1
            } else {
                self.charts_tab_idx
            };

            if chart_idx < tabs.len() {
                let key = &tabs[chart_idx];

                active_chart = match key.as_str() {
                    "Overview" | "Baking overview" => None,
                    "User Stats" | "Kitchen Stats" => {
                        let mut series = vec![];
                        let colors = [Color::Green, Color::Blue, Color::Magenta, Color::Cyan];
                        for (key, stats) in &ctx.custom_timed {
                            let mut hasher = std::collections::hash_map::DefaultHasher::new();
                            key.hash(&mut hasher);
                            let color_idx = hasher.finish() as usize % 4;
                            let style = Style::default().fg(colors[color_idx]);
                            series.push((
                                key.clone(),
                                stats.series.clone().into(),
                                stats.window,
                                style,
                            ));
                        }
                        series.sort_by(|a, b| a.0.cmp(&b.0));
                        Some(ChartKind::Multi {
                            name: if self.pizza_mode {
                                "Kitchen Stats".to_string()
                            } else {
                                "User Stats".to_string()
                            },
                            series,
                            run_time,
                        })
                    }
                    "corpus" | "pizzas" => Some(ChartKind::Single {
                        name: if self.pizza_mode {
                            "pizzas".to_string()
                        } else {
                            "corpus".to_string()
                        },
                        series: ctx.corpus_size_timed.series.clone().into(),
                        window: ctx.corpus_size_timed.window,
                        run_time,
                        style: Style::default()
                            .fg(Color::LightYellow)
                            .add_modifier(Modifier::BOLD),
                    }),
                    "objectives" | "deliveries" => Some(ChartKind::Single {
                        name: if self.pizza_mode {
                            "deliveries".to_string()
                        } else {
                            "objectives".to_string()
                        },
                        series: ctx.objective_size_timed.series.clone().into(),
                        window: ctx.objective_size_timed.window,
                        run_time,
                        style: Style::default()
                            .fg(Color::LightYellow)
                            .add_modifier(Modifier::BOLD),
                    }),
                    "exec/sec" | "p/s" => Some(ChartKind::Single {
                        name: if self.pizza_mode {
                            "p/s".to_string()
                        } else {
                            "exec/sec".to_string()
                        },
                        series: ctx.execs_per_sec_timed.series.clone().into(),
                        window: ctx.execs_per_sec_timed.window,
                        run_time,
                        style: Style::default()
                            .fg(Color::LightYellow)
                            .add_modifier(Modifier::BOLD),
                    }),
                    custom => ctx.custom_timed.get(custom).map(|stats| {
                        let mut hasher = std::collections::hash_map::DefaultHasher::new();
                        if let Some(tag) = ctx.tags.get(custom) {
                            tag.hash(&mut hasher);
                        } else {
                            custom.hash(&mut hasher);
                        }
                        let color_idx = hasher.finish() as usize % 4;
                        let colors = [Color::Green, Color::Blue, Color::Magenta, Color::Cyan];
                        let style = Style::default().fg(colors[color_idx]);

                        ChartKind::Single {
                            name: custom.to_string(),
                            series: stats.series.clone().into(),
                            window: stats.window,
                            run_time,
                            style,
                        }
                    }),
                };
            }
        }
        (tabs, active_chart)
    }

    /// Prepare the data for the next frame
    #[allow(clippy::cast_precision_loss)]
    fn prepare_data(&mut self, ctx: &TuiContext) -> PreparedFrameData {
        self.update_clients_list(ctx);

        let run_time = current_time().saturating_sub(ctx.start_time);

        // 1. Overall Stats
        let generic_stats = self.prepare_overall_stats(ctx);

        let total_timing = Self::prepare_total_timing(ctx, run_time);

        let total_geometry = ctx.total_item_geometry.clone();

        // 2. Client Stats
        let (client_generic_stats, client_timing, client_geometry, client_user_stats) =
            self.prepare_client_data(ctx);

        #[cfg(feature = "introspection")]
        let client_perf_stats = self.prepare_client_perf_stats(ctx);

        // 3. Logs
        let logs: Vec<String> = ctx.client_logs.iter().cloned().collect();

        // 4. Charts
        let (tabs, active_chart) = self.prepare_charts(ctx, run_time);

        PreparedFrameData {
            pizza_mode: self.pizza_mode,
            generic_stats,
            total_timing,
            total_geometry,
            client_idx: self.client_idx,
            client_generic_stats,
            client_timing,
            client_geometry,
            client_user_stats,
            logs,
            tabs,
            active_chart,
            #[cfg(feature = "introspection")]
            client_perf_stats,
        }
    }

    /// Handle a key event
    pub fn on_key(&mut self, c: char, app: &Arc<RwLock<TuiContext>>) {
        match c {
            'q' => self.should_quit = true,
            'r' => self.should_refresh = true,
            'g' if !self.tabs.is_empty() => {
                self.charts_tab_idx = (self.charts_tab_idx + 1) % self.tabs.len();
                // In wide mode, index 0 (Overview) and index 1 (First Chart) show the same thing on the right pane.
                // So we skip 0 to prevent "double press" feeling.
                if !self.is_narrow && self.charts_tab_idx == 0 && self.tabs.len() > 1 {
                    self.charts_tab_idx = 1;
                }
            }
            'G' if !self.tabs.is_empty() => {
                self.charts_tab_idx = (self.charts_tab_idx + self.tabs.len() - 1) % self.tabs.len();
                // In wide mode, skip 0 when going backwards too
                if !self.is_narrow && self.charts_tab_idx == 0 && self.tabs.len() > 1 {
                    self.charts_tab_idx = self.tabs.len() - 1;
                }
            }
            't' => {
                self.show_logs = !self.show_logs;
            }
            'w' => {
                self.logs_wrap = !self.logs_wrap;
            }
            '+' => {
                let mut ctx = app.write().unwrap();
                let w = ctx.corpus_size_timed.window * 2;
                ctx.corpus_size_timed.update_window(w);
                ctx.objective_size_timed.update_window(w);
                ctx.execs_per_sec_timed.update_window(w);
                for timer in ctx.custom_timed.values_mut() {
                    let w = timer.window * 2;
                    timer.update_window(w);
                }
            }
            '-' => {
                let mut ctx = app.write().unwrap();
                let w = ctx.corpus_size_timed.window / 2;
                ctx.corpus_size_timed.update_window(w);
                ctx.objective_size_timed.update_window(w);
                ctx.execs_per_sec_timed.update_window(w);
                for timer in ctx.custom_timed.values_mut() {
                    let w = timer.window / 2;
                    timer.update_window(w);
                }
            }
            'u' => {
                let ctx = app.read().unwrap();
                if let Some(client) = ctx.clients.get(&self.client_idx) {
                    let total = client.client_stats.user_stats().len();
                    if total > 0 {
                        self.user_stats_scroll += self.user_stats_page_size;
                        if self.user_stats_scroll >= total {
                            self.user_stats_scroll = 0;
                        }
                    }
                }
            }
            'U' => {
                let ctx = app.read().unwrap();
                if let Some(client) = ctx.clients.get(&self.client_idx) {
                    let total = client.client_stats.user_stats().len();
                    if total > 0 {
                        if self.user_stats_scroll == 0 {
                            let remainder = total % self.user_stats_page_size;
                            if remainder == 0 {
                                self.user_stats_scroll =
                                    total.saturating_sub(self.user_stats_page_size);
                            } else {
                                self.user_stats_scroll = total - remainder;
                            }
                        } else {
                            self.user_stats_scroll = self
                                .user_stats_scroll
                                .saturating_sub(self.user_stats_page_size);
                        }
                    }
                }
            }
            #[cfg(feature = "introspection")]
            'i' => {
                let ctx = app.read().unwrap();
                #[allow(clippy::cast_precision_loss)]
                if let Some(client) = ctx.clients.get(&self.client_idx) {
                    let has_geometry = client.item_geometry.is_some();
                    let stats = &client.client_stats.introspection_stats;

                    let mut total = 2;
                    let elapsed = stats.elapsed_cycles() as f64;
                    if elapsed != 0.0 {
                        for (_, features) in stats.used_stages() {
                            for feature in features {
                                #[allow(clippy::cast_precision_loss)]
                                if *feature as f64 / elapsed > 0.0 {
                                    total += 1;
                                }
                            }
                        }
                        for (_, val) in stats.feedbacks() {
                            #[allow(clippy::cast_precision_loss)]
                            if *val as f64 / elapsed > 0.0 {
                                total += 1;
                            }
                        }
                    }

                    if self.show_geometry {
                        let geo_total = 5;
                        let next = self.item_geometry_scroll + self.item_geometry_page_size;
                        if next >= geo_total {
                            if total > 0 {
                                self.show_geometry = false;
                                self.perf_stats_scroll = 0;
                            } else {
                                self.item_geometry_scroll = 0;
                            }
                        } else {
                            self.item_geometry_scroll = next;
                        }
                    } else {
                        let next = self.perf_stats_scroll + self.perf_stats_page_size;
                        if next >= total {
                            if has_geometry {
                                self.show_geometry = true;
                                self.item_geometry_scroll = 0;
                            } else {
                                self.perf_stats_scroll = 0;
                            }
                        } else {
                            self.perf_stats_scroll = next;
                        }
                    }
                }
            }
            #[cfg(feature = "introspection")]
            'I' => {
                let ctx = app.read().unwrap();
                #[allow(clippy::cast_precision_loss)]
                if let Some(client) = ctx.clients.get(&self.client_idx) {
                    let has_geometry = client.item_geometry.is_some();
                    let stats = &client.client_stats.introspection_stats;

                    let mut total = 2;
                    let elapsed = stats.elapsed_cycles() as f64;
                    if elapsed != 0.0 {
                        for (_, features) in stats.used_stages() {
                            for feature in features {
                                #[allow(clippy::cast_precision_loss)]
                                if *feature as f64 / elapsed > 0.0 {
                                    total += 1;
                                }
                            }
                        }
                        for (_, val) in stats.feedbacks() {
                            #[allow(clippy::cast_precision_loss)]
                            if *val as f64 / elapsed > 0.0 {
                                total += 1;
                            }
                        }
                    }

                    if self.show_geometry {
                        if self.item_geometry_scroll == 0 {
                            if total > 0 {
                                self.show_geometry = false;
                                let remainder = total % self.perf_stats_page_size;
                                if remainder == 0 {
                                    self.perf_stats_scroll =
                                        total.saturating_sub(self.perf_stats_page_size);
                                } else {
                                    self.perf_stats_scroll = total - remainder;
                                }
                            } else {
                                let geo_total = 5;
                                let remainder = geo_total % self.item_geometry_page_size;
                                if remainder == 0 {
                                    self.item_geometry_scroll =
                                        geo_total.saturating_sub(self.item_geometry_page_size);
                                } else {
                                    self.item_geometry_scroll = geo_total - remainder;
                                }
                            }
                        } else {
                            self.item_geometry_scroll = self
                                .item_geometry_scroll
                                .saturating_sub(self.item_geometry_page_size);
                        }
                    } else if self.perf_stats_scroll == 0 {
                        if has_geometry {
                            self.show_geometry = true;
                            let geo_total = 5;
                            let remainder = geo_total % self.item_geometry_page_size;
                            if remainder == 0 {
                                self.item_geometry_scroll =
                                    geo_total.saturating_sub(self.item_geometry_page_size);
                            } else {
                                self.item_geometry_scroll = geo_total - remainder;
                            }
                        } else if total > 0 {
                            let remainder = total % self.perf_stats_page_size;
                            if remainder == 0 {
                                self.perf_stats_scroll =
                                    total.saturating_sub(self.perf_stats_page_size);
                            } else {
                                self.perf_stats_scroll = total - remainder;
                            }
                        }
                    } else {
                        self.perf_stats_scroll = self
                            .perf_stats_scroll
                            .saturating_sub(self.perf_stats_page_size);
                    }
                }
            }
            '0'..='9' => {
                if let Some(i) = c.to_digit(10) {
                    let i = i as usize;
                    if i < self.tabs.len() {
                        self.charts_tab_idx = i;
                    }
                }
            }
            _ => {}
        }
    }

    const NARROW_WIDTH_THRESHOLD: u16 = 75;
    const SHORT_HEIGHT_THRESHOLD: u16 = 28;

    /// Move to the next client
    pub fn on_right(&mut self) {
        if let Some(idx) = next_larger(&self.clients, self.client_idx) {
            self.client_idx = idx; // next_larger returns explicit client ID in revised version
        } else if !self.clients.is_empty() {
            self.client_idx = self.clients[0];
        }
    }

    /// Move to the previous client
    pub fn on_left(&mut self) {
        if let Some(idx) = next_smaller(&self.clients, self.client_idx) {
            self.client_idx = idx; // next_smaller returns explicit client ID
        } else if !self.clients.is_empty() {
            self.client_idx = *self.clients.last().unwrap();
        }
    }

    /// Draw the current TUI context
    pub fn draw(&mut self, f: &mut Frame, app: &Arc<RwLock<TuiContext>>) {
        let ctx = app.read().unwrap();
        let prepared = self.prepare_data(&ctx);
        drop(ctx);
        self.tabs.clone_from(&prepared.tabs);

        #[cfg(feature = "introspection")]
        let introspection = true;
        #[cfg(not(feature = "introspection"))]
        let introspection = false;

        let has_charts = prepared.tabs.len() > 1;

        let area = f.area();
        let new_is_narrow = area.width < Self::NARROW_WIDTH_THRESHOLD;
        if new_is_narrow && !self.is_narrow {
            self.show_logs = false;
            self.charts_tab_idx = 0;
        } else if !new_is_narrow && self.charts_tab_idx == 0 && self.tabs.len() > 1 {
            self.charts_tab_idx = 1;
        }
        self.is_narrow = new_is_narrow;

        let is_short = area.height < Self::SHORT_HEIGHT_THRESHOLD;
        let replace_client_with_logs = (self.is_narrow || is_short) && self.show_logs;

        if replace_client_with_logs {
            let body = split_main(area, true, introspection, has_charts);
            let top_body = body[0];
            let logs_area = Rect::new(
                area.x,
                top_body.bottom(),
                area.width,
                area.height.saturating_sub(top_body.height),
            );

            self.draw_overall_ui(f, top_body, has_charts, &prepared);
            draw_logs(
                f,
                logs_area,
                &prepared.logs,
                self.logs_wrap,
                prepared.pizza_mode,
            );
        } else {
            let body = split_main(area, self.show_logs, introspection, has_charts);
            let top_body = body[0];
            let mid_body = body[1];

            self.draw_overall_ui(f, top_body, has_charts, &prepared);
            self.draw_client_ui(f, mid_body, self.show_logs, &prepared);

            if self.show_logs && body.len() > 2 {
                draw_logs(
                    f,
                    body[2],
                    &prepared.logs,
                    self.logs_wrap,
                    prepared.pizza_mode,
                );
            }
        }
    }

    fn draw_overall_ui(
        &mut self,
        f: &mut Frame,
        area: Rect,
        has_charts: bool,
        data: &PreparedFrameData,
    ) {
        let overall_layout = area;
        let (tab_area, content_area) = split_title(overall_layout);

        if self.is_narrow {
            let available_width = area.width.saturating_sub(4) as usize;
            let (start, end, selected) =
                calculate_tab_window(&data.tabs, available_width, self.charts_tab_idx);
            let visible_titles: Vec<Span> = data.tabs[start..end]
                .iter()
                .map(|s| Span::from(s.clone()))
                .collect();

            let tabs_widget = Tabs::new(visible_titles)
                .block(draw_main_block(
                    &self.title,
                    Borders::TOP | Borders::LEFT | Borders::RIGHT,
                    Some(Line::from(Span::styled(
                        " G/g ",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))),
                    None,
                ))
                .select(selected)
                .highlight_style(Style::default().fg(if self.charts_tab_idx > 0 {
                    if let Some(ChartKind::Single { style, .. }) = &data.active_chart {
                        style.fg.unwrap_or(Color::LightYellow)
                    } else {
                        Color::LightYellow
                    }
                } else {
                    Color::LightYellow
                }));
            f.render_widget(tabs_widget, tab_area);

            if self.charts_tab_idx == 0 {
                Self::draw_stats_column(f, content_area, data, true);
            } else {
                self.draw_chart(f, content_area, data);
            }
        } else {
            let (left_col, right_col) = if has_charts {
                split_top(overall_layout)
            } else {
                (overall_layout, Rect::default())
            };

            let has_charts = data.tabs.len() > 1;

            let (left_title, left_stats) = split_title(left_col);
            let status_bar = format!("{} ({})", self.title, self.version);
            let p = Paragraph::new(Line::from(Span::styled(
                status_bar,
                Style::default()
                    .fg(Color::LightMagenta)
                    .add_modifier(Modifier::BOLD),
            )))
            .block(draw_main_block(&self.title, Borders::ALL, None, None))
            .alignment(Alignment::Center);
            f.render_widget(p, left_title);

            Self::draw_stats_column(f, left_stats, data, false);

            if !has_charts || data.tabs.len() <= 1 {
                return;
            }

            let chart_titles: Vec<String> = data.tabs[1..].to_vec();
            let available_width = right_col.width.saturating_sub(4) as usize;
            let visual_selected_idx = self.charts_tab_idx.saturating_sub(1);

            let (start, end, selected) =
                calculate_tab_window(&chart_titles, available_width, visual_selected_idx);

            let visible_titles: Vec<Span> = chart_titles[start..end]
                .iter()
                .map(|s| Span::from(s.clone()))
                .collect();

            let tabs_widget = Tabs::new(visible_titles)
                .block(draw_main_block(
                    "charts",
                    Borders::TOP | Borders::LEFT | Borders::RIGHT,
                    Some(Line::from(Span::styled(
                        format!(
                            " {}/{} (switch g/G) ",
                            self.charts_tab_idx.max(1),
                            data.tabs.len().saturating_sub(1)
                        ),
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))),
                    None,
                ))
                .select(selected)
                .highlight_style(Style::default().fg(
                    if let Some(ChartKind::Single { style, .. }) = &data.active_chart {
                        style.fg.unwrap_or(Color::LightYellow)
                    } else {
                        Color::LightYellow
                    },
                ));

            let (right_title, right_chart) = split_title(right_col);
            f.render_widget(tabs_widget, right_title);

            self.draw_chart(f, right_chart, data);
        }
    }

    fn draw_stats_column(f: &mut Frame, area: Rect, data: &PreparedFrameData, is_narrow: bool) {
        let has_timing = data.total_timing.is_some();
        let mut constraints = vec![Constraint::Length(4)];
        if has_timing {
            constraints.push(Constraint::Length(6));
        }
        constraints.push(Constraint::Min(0));

        let chunks = Layout::default()
            .direction(ratatui::layout::Direction::Vertical)
            .constraints(constraints)
            .split(area);

        let title = if is_narrow {
            ""
        } else if data.pizza_mode {
            "Baking overview"
        } else {
            "Overview"
        };
        draw_key_value_block(
            f,
            chunks[0],
            title,
            data.generic_stats.labels.clone(),
            &[
                Constraint::Length(10),
                Constraint::Min(5),
                Constraint::Length(14),
                Constraint::Min(5),
            ],
        );

        let mut next_idx = 1;
        if let Some((timing, run_time)) = &data.total_timing {
            let title = if data.pizza_mode {
                "Baking status"
            } else {
                "General"
            };
            draw_process_timing_text(
                f,
                chunks[next_idx],
                title,
                timing,
                *run_time,
                data.pizza_mode,
            );
            next_idx += 1;
        }

        if let (Some(geometry), Some(chunk)) = (&data.total_geometry, chunks.get(next_idx)) {
            draw_item_geometry_text(f, *chunk, geometry, "", 0, false, data.pizza_mode);
        }
    }

    fn draw_chart(&mut self, f: &mut Frame, area: Rect, data: &PreparedFrameData) {
        if let Some(chart) = &data.active_chart {
            match chart {
                ChartKind::Single {
                    name,
                    series,
                    window,
                    run_time,
                    style,
                } => {
                    draw_time_chart(
                        f,
                        area,
                        TimeChartOptions {
                            title: "",
                            y_name: name,
                            stats: series,
                            window: *window,
                            graph_data: &mut self.graph_data,
                            enhanced_graphics: self.enhanced_graphics,
                            current_time: *run_time,
                            preset_y_range: None,
                            style: *style,
                        },
                    );
                }
                ChartKind::Multi {
                    name,
                    series,
                    run_time,
                } => {
                    let series_refs: Vec<(&str, &[TimedStat], Duration, Style)> = series
                        .iter()
                        .map(|(n, s, w, st)| (n.as_str(), s.as_slice(), *w, *st))
                        .collect();
                    if self.multi_graph_data.len() < series.len() {
                        self.multi_graph_data.resize(series.len(), vec![]);
                    }
                    draw_multi_time_chart(
                        f,
                        area,
                        MultiTimeChartOptions {
                            title: name,
                            y_name: "value",
                            series: series_refs,
                            graph_data: &mut self.multi_graph_data,
                            enhanced_graphics: self.enhanced_graphics,
                            current_time: *run_time,
                            preset_y_range: None,
                            title_style: None,
                        },
                    );
                }
            }
        }
    }

    fn draw_client_ui(
        &mut self,
        f: &mut Frame,
        area: Rect,
        show_logs: bool,
        data: &PreparedFrameData,
    ) {
        let client_pfx = if self.pizza_mode {
            "customer"
        } else {
            "client"
        };
        let title = format!(
            "{} #{}{}",
            client_pfx,
            data.client_idx,
            if self.clients.len() > 1 {
                " (←/→ arrows to switch)"
            } else {
                ""
            }
        );

        let top_right_hint = if show_logs {
            None
        } else {
            Some(Line::from(Span::styled(
                "`t` for logs, `q` to quit",
                Style::default()
                    .fg(Color::LightMagenta)
                    .add_modifier(Modifier::BOLD),
            )))
        };

        let block = draw_main_block(&title, Borders::ALL, top_right_hint, None);

        let client_area = block.inner(area);
        f.render_widget(block, area);

        // Split client area
        let (left, right) = split_client(client_area);

        // Left: Generic + User Stats
        // Limit height for generic to matching General (6)
        let left_chunks = Layout::default()
            .direction(ratatui::layout::Direction::Vertical)
            .constraints([Constraint::Length(6), Constraint::Min(0)])
            .split(left);

        let title = if self.pizza_mode { "Stats" } else { "Overview" };
        draw_key_value_block(
            f,
            left_chunks[0],
            title,
            data.client_generic_stats.labels.clone(),
            &[Constraint::Length(20), Constraint::Min(5)],
        );

        self.user_stats_page_size = draw_user_stats(
            f,
            left_chunks[1],
            self.client_idx,
            &data.client_user_stats,
            self.user_stats_scroll,
        );

        // Right: Timing + Geometry/Perf Stats
        let mut right_constraints = vec![];
        if data.client_timing.is_some() {
            right_constraints.push(Constraint::Length(6));
        }
        right_constraints.push(Constraint::Min(0));

        let right_chunks = Layout::default()
            .direction(ratatui::layout::Direction::Vertical)
            .constraints(right_constraints)
            .split(right);

        let mut next_idx = 0;
        if let Some((timing, run_time)) = &data.client_timing {
            let title = if self.pizza_mode {
                "Baking status"
            } else {
                "General"
            };
            draw_process_timing_text(
                f,
                right_chunks[next_idx],
                title,
                timing,
                *run_time,
                self.pizza_mode,
            );
            next_idx += 1;
        }

        if let Some(chunk) = right_chunks.get(next_idx) {
            #[cfg(feature = "introspection")]
            let has_perf_stats = !data.client_perf_stats.is_empty();
            #[cfg(not(feature = "introspection"))]
            let has_perf_stats = false;

            let has_geometry = data.client_geometry.is_some();
            let show_geo = has_geometry && (self.show_geometry || !has_perf_stats);

            if show_geo {
                if let Some(geometry) = &data.client_geometry {
                    let hint = if has_perf_stats { " (i/I)" } else { "" };
                    self.item_geometry_page_size = draw_item_geometry_text(
                        f,
                        *chunk,
                        geometry,
                        hint,
                        self.item_geometry_scroll,
                        has_perf_stats,
                        self.pizza_mode,
                    );
                }
            } else {
                #[cfg(feature = "introspection")]
                if has_perf_stats {
                    let title = if self.pizza_mode {
                        "chef's performance"
                    } else {
                        "Client Perf Stats"
                    };
                    self.perf_stats_page_size = draw_scrolled_stats(
                        f,
                        *chunk,
                        title,
                        &data.client_perf_stats,
                        self.perf_stats_scroll,
                        " (i/I)",
                        true,
                    );
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use core::time::Duration;

    use ratatui::{Terminal, backend::TestBackend};

    use super::*;
    use crate::monitors::tui::TuiContext;

    #[test]
    fn test_ui_render_lock_pattern() {
        let mut tui_ui = TuiUi::new("Test".into(), false);
        // Start time 0
        let ctx = Arc::new(RwLock::new(TuiContext::new(Duration::from_secs(0))));

        // Just verify it doesn't deadlock or panic
        let backend = TestBackend::new(80, 40);
        let mut terminal = Terminal::new(backend).unwrap();

        // Render once
        terminal.draw(|f| tui_ui.draw(f, &ctx)).unwrap();
    }

    #[test]
    fn test_logs_visibility() {
        let mut tui_ui = TuiUi::new("Test".into(), true); // show logs
        let mut ctx_struct = TuiContext::new(Duration::from_secs(0));
        ctx_struct.client_logs.push_back("Test log".into());
        let ctx = Arc::new(RwLock::new(ctx_struct));

        let backend = TestBackend::new(80, 40);
        let mut terminal = Terminal::new(backend).unwrap();

        terminal.draw(|f| tui_ui.draw(f, &ctx)).unwrap();

        let buffer = terminal.backend().buffer();
        let content = format!("{buffer:?}");
        assert!(content.contains("Test log"), "Logs should be visible");
    }
}
