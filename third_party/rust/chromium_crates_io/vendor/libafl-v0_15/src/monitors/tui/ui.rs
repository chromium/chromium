//! The UI-specific parts of [`super::TuiMonitor`]
use alloc::{sync::Arc, vec::Vec};
use core::cmp::{max, min};
use std::sync::RwLock;

use libafl_bolts::format_big_number;
use ratatui::{
    Frame,
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    symbols,
    text::{Line, Span},
    widgets::{
        Axis, Block, Borders, Cell, Chart, Dataset, List, ListItem, Paragraph, Row, Table, Tabs,
    },
};

use super::{
    Duration, ItemGeometry, ProcessTiming, String, TimedStats, TuiContext, current_time,
    format_duration,
};

#[derive(Default, Debug)]
pub struct TuiUi {
    title: String,
    version: String,
    enhanced_graphics: bool,
    show_logs: bool,
    client_idx: usize,
    clients: Vec<usize>,
    charts_tab_idx: usize,
    graph_data: Vec<(f64, f64)>,

    pub should_quit: bool,
}

fn next_larger(sorted: &[usize], value: usize) -> Option<usize> {
    if let Some(index) = sorted.iter().position(|x| *x > value) {
        return Some(index);
    }
    None
}

fn next_smaller(sorted: &[usize], value: usize) -> Option<usize> {
    if let Some(index) = sorted.iter().rposition(|x| *x < value) {
        return Some(index);
    }
    None
}

impl TuiUi {
    #[must_use]
    pub fn new(title: String, enhanced_graphics: bool) -> Self {
        Self::with_version(title, String::from("default"), enhanced_graphics)
    }

    // create the TuiUi with a given `version`.
    #[must_use]
    pub fn with_version(title: String, version: String, enhanced_graphics: bool) -> Self {
        Self {
            title,
            version,
            enhanced_graphics,
            show_logs: true,
            client_idx: 0,
            ..TuiUi::default()
        }
    }
    pub fn on_key(&mut self, c: char) {
        match c {
            'q' => {
                self.should_quit = true;
            }
            'g' => {
                self.charts_tab_idx = (self.charts_tab_idx + 1) % 3;
            }
            't' => {
                self.show_logs = !self.show_logs;
            }
            _ => {}
        }
    }

    pub fn on_right(&mut self) {
        if let Some(idx) = next_larger(&self.clients, self.client_idx) {
            self.client_idx = self.clients[idx];
        }
    }

    pub fn on_left(&mut self) {
        if let Some(idx) = next_smaller(&self.clients, self.client_idx) {
            self.client_idx = self.clients[idx];
        }
    }

    /// Draw the current TUI context
    pub fn draw(&mut self, f: &mut Frame, app: &Arc<RwLock<TuiContext>>) {
        let new = app.read().unwrap().clients_num;
        if new != self.clients.len() {
            // get the list of all clients
            let mut all: Vec<usize> = app.read().unwrap().clients.keys().copied().collect();
            all.sort_unstable();

            // move the current client to the first one
            self.client_idx = all[0];

            // move the vector holding all clients ids
            self.clients = all;
        }

        let body = Layout::default()
            .constraints(if self.show_logs {
                if cfg!(feature = "introspection") {
                    [
                        Constraint::Percentage(41),
                        Constraint::Percentage(44),
                        Constraint::Percentage(15),
                    ]
                    .as_ref()
                } else {
                    [
                        Constraint::Percentage(20),
                        Constraint::Percentage(48),
                        Constraint::Percentage(32),
                    ]
                    .as_ref()
                }
            } else {
                [Constraint::Percentage(50), Constraint::Percentage(50)].as_ref()
            })
            .split(f.area());
        let top_body = body[0];
        let mid_body = body[1];

        self.draw_overall_ui(f, app, top_body);
        self.draw_client_ui(f, app, mid_body);

        if self.show_logs {
            let bottom_body = body[2];
            self.draw_logs(f, app, bottom_body);
        }
    }

    #[expect(clippy::too_many_lines)]
    fn draw_overall_ui(&mut self, f: &mut Frame, app: &Arc<RwLock<TuiContext>>, area: Rect) {
        let top_layout = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(16), Constraint::Min(0)].as_ref())
            .split(area);
        let bottom_layout = top_layout[1];

        let left_top_layout = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Percentage(40), Constraint::Percentage(60)].as_ref())
            .split(top_layout[0]);

        let right_top_layout = left_top_layout[1];

        let title_layout = Layout::default()
            .constraints([Constraint::Length(3), Constraint::Min(0)].as_ref())
            .split(left_top_layout[0]);

        let status_bar: String = format!("{} ({})", self.title, self.version.as_str());

        let text = vec![Line::from(Span::styled(
            &status_bar,
            Style::default()
                .fg(Color::LightMagenta)
                .add_modifier(Modifier::BOLD),
        ))];
        let block = Block::default().borders(Borders::ALL);
        let paragraph = Paragraph::new(text)
            .block(block)
            .alignment(Alignment::Center);
        f.render_widget(paragraph, title_layout[0]);

        let process_timting_layout = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(6), Constraint::Min(0)].as_ref())
            .split(title_layout[1]);
        self.draw_process_timing_text(f, app, process_timting_layout[0], true);

        let path_geometry_layout = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(7), Constraint::Min(0)].as_ref())
            .split(process_timting_layout[1]);
        self.draw_item_geometry_text(f, app, path_geometry_layout[0], true);

        let title_chart_layout = Layout::default()
            .constraints([Constraint::Length(3), Constraint::Min(0)].as_ref())
            .split(right_top_layout);
        let titles = vec![
            Line::from(Span::styled(
                "speed",
                Style::default().fg(Color::LightGreen),
            )),
            Line::from(Span::styled(
                "corpus",
                Style::default().fg(Color::LightGreen),
            )),
            Line::from(Span::styled(
                "objectives (`g` switch)",
                Style::default().fg(Color::LightGreen),
            )),
        ];
        let tabs = Tabs::new(titles)
            .block(
                Block::default()
                    .title(Span::styled(
                        "charts",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .highlight_style(Style::default().fg(Color::LightYellow))
            .select(self.charts_tab_idx);
        f.render_widget(tabs, title_chart_layout[0]);

        let chart_layout = title_chart_layout[1];

        match self.charts_tab_idx {
            0 => {
                let ctx = app.read().unwrap();
                self.draw_time_chart(
                    "speed chart",
                    "exec/sec",
                    f,
                    chart_layout,
                    &ctx.execs_per_sec_timed,
                );
            }
            1 => {
                let ctx = app.read().unwrap();
                self.draw_time_chart(
                    "corpus chart",
                    "corpus size",
                    f,
                    chart_layout,
                    &ctx.corpus_size_timed,
                );
            }
            2 => {
                let ctx = app.read().unwrap();
                self.draw_time_chart(
                    "corpus chart",
                    "objectives",
                    f,
                    chart_layout,
                    &ctx.objective_size_timed,
                );
            }
            _ => {}
        }
        self.draw_overall_generic_text(f, app, bottom_layout);
    }

    fn draw_client_ui(&mut self, f: &mut Frame, app: &Arc<RwLock<TuiContext>>, area: Rect) {
        let client_block = Block::default()
            .title(Span::styled(
                format!("client #{} (←/→ arrows to switch)", self.client_idx),
                Style::default()
                    .fg(Color::LightCyan)
                    .add_modifier(Modifier::BOLD),
            ))
            .borders(Borders::ALL);

        #[allow(unused_mut)] // cfg dependent
        let mut client_area = client_block.inner(area);
        f.render_widget(client_block, area);

        #[cfg(feature = "introspection")]
        {
            let client_layout = Layout::default()
                .direction(Direction::Vertical)
                .constraints([Constraint::Min(11), Constraint::Percentage(50)].as_ref())
                .split(client_area);
            client_area = client_layout[0];
            let instrospection_layout = client_layout[1];
            self.draw_introspection_text(f, app, instrospection_layout);
        }

        let left_layout = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Percentage(50), Constraint::Percentage(50)].as_ref())
            .split(client_area);
        let right_layout = left_layout[1];

        let left_top_layout = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(6), Constraint::Length(5)].as_ref())
            .split(left_layout[0]);
        let left_bottom_layout = left_top_layout[1];
        self.draw_process_timing_text(f, app, left_top_layout[0], false);
        self.draw_client_generic_text(f, app, left_bottom_layout);

        let right_top_layout = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(7), Constraint::Length(5)].as_ref())
            .split(right_layout);
        let right_bottom_layout = right_top_layout[1];
        self.draw_item_geometry_text(f, app, right_top_layout[0], false);
        self.draw_client_results_text(f, app, right_bottom_layout);
    }

    #[expect(clippy::too_many_lines, clippy::cast_precision_loss)]
    fn draw_time_chart(
        &mut self,
        title: &str,
        y_name: &str,
        f: &mut Frame,
        area: Rect,
        stats: &TimedStats,
    ) {
        if stats.series.is_empty() {
            return;
        }
        let start = stats.series.front().unwrap().time;
        let end = stats.series.back().unwrap().time;
        let min_lbl_x = format_duration(&start);
        let med_lbl_x = format_duration(&(end.checked_sub(start).unwrap_or_default() / 2));
        let max_lbl_x = format_duration(&end);

        let x_labels = vec![
            Span::styled(min_lbl_x, Style::default().add_modifier(Modifier::BOLD)),
            Span::raw(med_lbl_x),
            Span::styled(max_lbl_x, Style::default().add_modifier(Modifier::BOLD)),
        ];

        let max_x = u64::from(area.width);
        let window = end.checked_sub(start).unwrap_or_default();
        let time_unit = if max_x > window.as_secs() {
            0 // millis / 10
        } else if max_x > window.as_secs() * 60 {
            1 // secs
        } else {
            2 // min
        };
        let convert_time = |d: &Duration| -> u64 {
            if time_unit == 0 {
                (d.as_millis() / 10) as u64
            } else if time_unit == 1 {
                d.as_secs()
            } else {
                d.as_secs() * 60
            }
        };
        let window_unit = convert_time(&window);
        if window_unit == 0 {
            return;
        }

        let to_x = |d: &Duration| (convert_time(d) - convert_time(&start)) * max_x / window_unit;

        self.graph_data.clear();

        let mut max_y = u64::MIN;
        let mut min_y = u64::MAX;
        let mut prev = (0, 0);
        for ts in &stats.series {
            let x = to_x(&ts.time);
            if x > prev.0 + 1 && x < max_x {
                for v in (prev.0 + 1)..x {
                    self.graph_data.push((v as f64, prev.1 as f64));
                }
            }
            prev = (x, ts.item);
            self.graph_data.push((x as f64, ts.item as f64));
            max_y = max(ts.item, max_y);
            min_y = min(ts.item, min_y);
        }
        if max_x > prev.0 + 1 {
            for v in (prev.0 + 1)..max_x {
                self.graph_data.push((v as f64, prev.1 as f64));
            }
        }

        //log::trace!("max_x: {}, len: {}", max_x, self.graph_data.len());

        let datasets = vec![
            Dataset::default()
                //.name("data")
                .marker(if self.enhanced_graphics {
                    symbols::Marker::Braille
                } else {
                    symbols::Marker::Dot
                })
                .style(
                    Style::default()
                        .fg(Color::LightYellow)
                        .add_modifier(Modifier::BOLD),
                )
                .data(&self.graph_data),
        ];
        let chart = Chart::new(datasets)
            .block(
                Block::default()
                    .title(Span::styled(
                        title,
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .x_axis(
                Axis::default()
                    .title("time")
                    .style(Style::default().fg(Color::Gray))
                    .bounds([0.0, max_x as f64])
                    .labels(x_labels),
            )
            .y_axis(
                Axis::default()
                    .title(y_name)
                    .style(Style::default().fg(Color::Gray))
                    .bounds([min_y as f64, max_y as f64])
                    .labels(vec![
                        Span::styled(
                            format!("{min_y}"),
                            Style::default().add_modifier(Modifier::BOLD),
                        ),
                        Span::raw(format!("{}", (max_y - min_y) / 2)),
                        Span::styled(
                            format!("{max_y}"),
                            Style::default().add_modifier(Modifier::BOLD),
                        ),
                    ]),
            );
        f.render_widget(chart, area);
    }

    fn draw_item_geometry_text(
        &mut self,
        f: &mut Frame,
        app: &Arc<RwLock<TuiContext>>,
        area: Rect,
        is_overall: bool,
    ) {
        let tui_context = app.read().unwrap();
        let empty_geometry: ItemGeometry = ItemGeometry::new();
        let item_geometry: &ItemGeometry = if is_overall {
            &tui_context.total_item_geometry
        } else if self.clients.is_empty() {
            &empty_geometry
        } else {
            let clients = &tui_context.clients;
            let client = clients.get(&self.client_idx);
            let client = client.as_ref();
            if let Some(client) = client {
                &client.item_geometry
            } else {
                log::warn!("Client {} was `None`. Race condition?", &self.client_idx);
                &empty_geometry
            }
        };

        let items = vec![
            Row::new(vec![
                Cell::from(Span::raw("pending")),
                Cell::from(Span::raw(format!("{}", item_geometry.pending))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("pend fav")),
                Cell::from(Span::raw(format!("{}", item_geometry.pend_fav))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("own finds")),
                Cell::from(Span::raw(format!("{}", item_geometry.own_finds))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("imported")),
                Cell::from(Span::raw(format!("{}", item_geometry.imported))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("stability")),
                Cell::from(Span::raw(format!(
                    "{:.2}%",
                    item_geometry.stability.unwrap_or(0.0) * 100.0
                ))),
            ]),
        ];

        let chunks = Layout::default()
            .constraints(
                [
                    Constraint::Length(2 + items.len() as u16),
                    Constraint::Min(0),
                ]
                .as_ref(),
            )
            .split(area);

        let table = Table::default()
            .rows(items)
            .block(
                Block::default()
                    .title(Span::styled(
                        "item geometry",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .widths([Constraint::Ratio(1, 2), Constraint::Ratio(1, 2)]);
        f.render_widget(table, chunks[0]);
    }

    fn draw_process_timing_text(
        &mut self,
        f: &mut Frame,
        app: &Arc<RwLock<TuiContext>>,
        area: Rect,
        is_overall: bool,
    ) {
        let tui_context = app.read().unwrap();
        let empty_timing: ProcessTiming = ProcessTiming::new();
        let tup: (Duration, &ProcessTiming) = if is_overall {
            (tui_context.start_time, &tui_context.total_process_timing)
        } else if self.clients.is_empty() {
            (current_time(), &empty_timing)
        } else {
            let clients = &tui_context.clients;
            let client = clients.get(&self.client_idx);
            let client = client.as_ref();
            if let Some(client) = client {
                (
                    client.process_timing.client_start_time,
                    &client.process_timing,
                )
            } else {
                log::warn!("Client {} was `None`. Race condition?", &self.client_idx);
                (current_time(), &empty_timing)
            }
        };
        let items = vec![
            Row::new(vec![
                Cell::from(Span::raw("run time")),
                Cell::from(Span::raw(format_duration(
                    &current_time().checked_sub(tup.0).unwrap_or_default(),
                ))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("exec speed")),
                Cell::from(Span::raw(&tup.1.exec_speed)),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("total execs")),
                Cell::from(Span::raw(format_big_number(tup.1.total_execs))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("last new entry")),
                Cell::from(Span::raw(format_duration(&(tup.1.last_new_entry)))),
            ]),
            Row::new(vec![
                Cell::from(Span::raw("last solution")),
                Cell::from(Span::raw(format_duration(&(tup.1.last_saved_solution)))),
            ]),
        ];

        let chunks = Layout::default()
            .constraints(
                [
                    Constraint::Length(2 + items.len() as u16),
                    Constraint::Min(0),
                ]
                .as_ref(),
            )
            .split(area);

        let table = Table::default()
            .rows(items)
            .block(
                Block::default()
                    .title(Span::styled(
                        "process timing",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .widths([Constraint::Ratio(1, 2), Constraint::Ratio(1, 2)]);
        f.render_widget(table, chunks[0]);
    }

    fn draw_overall_generic_text(
        &mut self,
        f: &mut Frame,
        app: &Arc<RwLock<TuiContext>>,
        area: Rect,
    ) {
        let items = {
            let app = app.read().unwrap();
            vec![
                Row::new(vec![
                    Cell::from(Span::raw("clients")),
                    Cell::from(Span::raw(format!("{}", self.clients.len()))),
                    Cell::from(Span::raw("total execs")),
                    Cell::from(Span::raw(format_big_number(app.total_execs))),
                ]),
                Row::new(vec![
                    Cell::from(Span::raw("solutions")),
                    Cell::from(Span::raw(format_big_number(app.total_solutions))),
                    Cell::from(Span::raw("corpus count")),
                    Cell::from(Span::raw(format_big_number(app.total_corpus_count))),
                ]),
            ]
        };

        let chunks = Layout::default()
            .constraints([Constraint::Percentage(100)].as_ref())
            .split(area);

        let table = Table::default()
            .rows(items)
            .block(
                Block::default()
                    .title(Span::styled(
                        "generic",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .widths([
                Constraint::Percentage(15),
                Constraint::Percentage(16),
                Constraint::Percentage(15),
                Constraint::Percentage(16),
                Constraint::Percentage(15),
                Constraint::Percentage(27),
            ]);
        f.render_widget(table, chunks[0]);
    }

    fn draw_client_results_text(
        &mut self,
        f: &mut Frame,
        app: &Arc<RwLock<TuiContext>>,
        area: Rect,
    ) {
        let items = {
            let app = app.read().unwrap();
            vec![
                Row::new(vec![
                    Cell::from(Span::raw("cycles done")),
                    Cell::from(Span::raw(format!(
                        "{}",
                        app.clients
                            .get(&self.client_idx)
                            .map_or(0, |x| x.cycles_done)
                    ))),
                ]),
                Row::new(vec![
                    Cell::from(Span::raw("solutions")),
                    Cell::from(Span::raw(format!(
                        "{}",
                        app.clients
                            .get(&self.client_idx)
                            .map_or(0, |x| x.objectives)
                    ))),
                ]),
            ]
        };

        let table = Table::default()
            .rows(items)
            .block(
                Block::default()
                    .title(Span::styled(
                        "overall results",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .widths([Constraint::Ratio(1, 2), Constraint::Ratio(1, 2)]);
        f.render_widget(table, area);
    }

    fn draw_client_generic_text(
        &mut self,
        f: &mut Frame,
        app: &Arc<RwLock<TuiContext>>,
        area: Rect,
    ) {
        let items = {
            let app = app.read().unwrap();
            vec![
                Row::new(vec![
                    Cell::from(Span::raw("corpus count")),
                    Cell::from(Span::raw(format_big_number(
                        app.clients.get(&self.client_idx).map_or(0, |x| x.corpus),
                    ))),
                ]),
                Row::new(vec![
                    Cell::from(Span::raw("total execs")),
                    Cell::from(Span::raw(format_big_number(
                        app.clients
                            .get(&self.client_idx)
                            .map_or(0, |x| x.executions),
                    ))),
                ]),
            ]
        };

        let table = Table::default()
            .rows(items)
            .block(
                Block::default()
                    .title(Span::styled(
                        "generic",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .widths([Constraint::Ratio(1, 2), Constraint::Ratio(1, 2)]);
        f.render_widget(table, area);
    }

    #[cfg(feature = "introspection")]
    fn draw_introspection_text(
        &mut self,
        f: &mut Frame,
        app: &Arc<RwLock<TuiContext>>,
        area: Rect,
    ) {
        let mut items = vec![];
        {
            let ctx = app.read().unwrap();
            if let Some(client) = ctx.introspection.get(&self.client_idx) {
                items.push(Row::new(vec![
                    Cell::from(Span::raw("scheduler")),
                    Cell::from(Span::raw(format!("{:.2}%", client.scheduler * 100.0))),
                ]));
                items.push(Row::new(vec![
                    Cell::from(Span::raw("manager")),
                    Cell::from(Span::raw(format!("{:.2}%", client.manager * 100.0))),
                ]));
                for i in 0..client.stages.len() {
                    items.push(Row::new(vec![
                        Cell::from(Span::raw(format!("stage {i}"))),
                        Cell::from(Span::raw("")),
                    ]));

                    for (key, val) in &client.stages[i] {
                        items.push(Row::new(vec![
                            Cell::from(Span::raw(key.clone())),
                            Cell::from(Span::raw(format!("{:.2}%", val * 100.0))),
                        ]));
                    }
                }
                for (key, val) in &client.feedbacks {
                    items.push(Row::new(vec![
                        Cell::from(Span::raw(key.clone())),
                        Cell::from(Span::raw(format!("{:.2}%", val * 100.0))),
                    ]));
                }
                items.push(Row::new(vec![
                    Cell::from(Span::raw("not measured")),
                    Cell::from(Span::raw(format!("{:.2}%", client.unmeasured * 100.0))),
                ]));
            }
        }

        let table = Table::default()
            .rows(items)
            .block(
                Block::default()
                    .title(Span::styled(
                        "introspection",
                        Style::default()
                            .fg(Color::LightCyan)
                            .add_modifier(Modifier::BOLD),
                    ))
                    .borders(Borders::ALL),
            )
            .widths([Constraint::Ratio(1, 2), Constraint::Ratio(1, 2)]);
        f.render_widget(table, area);
    }
    #[expect(clippy::unused_self)]
    fn draw_logs(&mut self, f: &mut Frame, app: &Arc<RwLock<TuiContext>>, area: Rect) {
        let app = app.read().unwrap();
        let logs: Vec<ListItem> = app
            .client_logs
            .iter()
            .map(|msg| ListItem::new(Span::raw(msg)))
            .collect();
        let logs = List::new(logs).block(
            Block::default().borders(Borders::ALL).title(Span::styled(
                "clients logs (`t` to show/hide)",
                Style::default()
                    .fg(Color::LightCyan)
                    .add_modifier(Modifier::BOLD),
            )),
        );
        f.render_widget(logs, area);
    }
}
