use alloc::{string::String, vec::Vec};
use core::time::Duration;

use libafl_bolts::format_duration;
use ratatui::{
    Frame,
    layout::{Alignment, Constraint, Rect},
    style::{Color, Modifier, Style},
    symbols,
    text::{Line, Span},
    widgets::{Axis, Block, Borders, Cell, Chart, Dataset, List, ListItem, Row, Table},
};

/// Helper to create a standard Block with title, borders, and optional hints
#[must_use]
pub fn draw_main_block<'a>(
    title: &'a str,
    borders: Borders,
    top_right_text: Option<Line<'a>>,
    bottom_right_text: Option<Line<'a>>,
) -> Block<'a> {
    let mut block = Block::default().borders(borders).title(Span::styled(
        title,
        Style::default()
            .fg(Color::LightCyan)
            .add_modifier(Modifier::BOLD),
    ));

    if let Some(text) = top_right_text {
        block = block.title_top(text.alignment(Alignment::Right));
    }

    if let Some(text) = bottom_right_text {
        block = block.title_bottom(text.alignment(Alignment::Right));
    }

    block
}

/// Calculate the sliding window for tabs
#[must_use]
pub fn calculate_tab_window(
    titles: &[String],
    max_width: usize,
    selected_idx: usize,
) -> (usize, usize, usize) {
    let tab_count = titles.len();
    if tab_count == 0 {
        return (0, 0, 0);
    }
    let selected_idx = selected_idx.min(tab_count - 1);
    let get_width = |i: usize| -> usize {
        if i >= titles.len() {
            0
        } else {
            titles[i].chars().count() + 3
        }
    };

    let mut start = selected_idx;
    let mut end = selected_idx + 1;
    let mut current_width = get_width(selected_idx);

    loop {
        let mut changed = false;
        if start > 0 {
            let w = get_width(start - 1);
            if current_width + w <= max_width {
                start -= 1;
                current_width += w;
                changed = true;
            }
        }
        if end < tab_count {
            let w = get_width(end);
            if current_width + w <= max_width {
                end += 1;
                current_width += w;
                changed = true;
            }
        }
        if !changed {
            break;
        }
    }
    (start, end, selected_idx.saturating_sub(start))
}

use crate::monitors::{
    stats::{ProcessTiming, TimedStat},
    tui::ItemGeometry,
};

/// Options for drawing a time chart
#[derive(Debug)]
pub struct TimeChartOptions<'a> {
    /// The title of the chart
    pub title: &'a str,
    /// The name of the Y-axis
    pub y_name: &'a str,
    /// The stats to plot (series of points)
    pub stats: &'a [TimedStat],
    /// The window duration for the chart
    pub window: Duration,
    /// A buffer to hold the graph data (to avoid allocations)
    pub graph_data: &'a mut Vec<(f64, f64)>,
    /// Whether to use enhanced graphics (Braille) or not
    pub enhanced_graphics: bool,
    /// The current running time
    pub current_time: Duration,
    /// Optional preset range for Y axis (min, max). Data outside this range will expand it.
    pub preset_y_range: Option<(f64, f64)>,
    /// The style of the chart
    pub style: Style,
}

/// Options for drawing a time chart with multiple series
#[derive(Debug)]
pub struct MultiTimeChartOptions<'a> {
    /// The title of the chart
    pub title: &'a str,
    /// The name of the Y-axis
    pub y_name: &'a str,
    /// The stats to plot: (Name, Series, Window, Style)
    pub series: Vec<(&'a str, &'a [TimedStat], Duration, Style)>,
    /// Buffers to hold the graph data (to avoid allocations). Must match series length.
    pub graph_data: &'a mut Vec<Vec<(f64, f64)>>,
    /// Whether to use enhanced graphics (Braille) or not
    pub enhanced_graphics: bool,
    /// The current running time
    pub current_time: Duration,
    /// Optional preset range for Y axis (min, max). Data outside this range will expand it.
    pub preset_y_range: Option<(f64, f64)>,
    /// Optional style for the title
    pub title_style: Option<Style>,
}

/// Draw the time chart with the given stats
pub fn draw_time_chart(f: &mut Frame, area: Rect, options: TimeChartOptions) {
    let TimeChartOptions {
        title,
        y_name,
        stats,
        window,
        graph_data,
        enhanced_graphics,
        current_time,
        preset_y_range,
        style,
    } = options;

    // Create a temporary buffer for the single series
    let mut temp_data = vec![];
    core::mem::swap(graph_data, &mut temp_data);
    let mut buffers = vec![temp_data];

    let multi_options = MultiTimeChartOptions {
        title,
        y_name,
        series: vec![("", stats, window, style)],
        graph_data: &mut buffers,
        enhanced_graphics,
        current_time,
        preset_y_range,
        title_style: Some(style),
    };

    draw_multi_time_chart(f, area, multi_options);

    // Restore buffer
    core::mem::swap(graph_data, &mut buffers[0]);
}

/// Draw a time chart with multiple series
#[expect(clippy::too_many_lines, clippy::cast_precision_loss)]
pub fn draw_multi_time_chart(f: &mut Frame, area: Rect, options: MultiTimeChartOptions) {
    let MultiTimeChartOptions {
        title,
        y_name,
        series,
        graph_data,
        enhanced_graphics,
        current_time,
        preset_y_range,
        title_style,
    } = options;

    if series.is_empty() {
        return;
    }

    // Determine global min/max
    let window = series[0].2; // Use first series window as reference

    let end = current_time;
    let start = end.saturating_sub(window);

    // Calculate time unit and X conversion
    let max_x = u64::from(area.width);

    let window_dur = if start == end {
        Duration::from_secs(1)
    } else {
        end.saturating_sub(start)
    };

    let time_unit = if max_x > window_dur.as_secs() {
        0 // millis / 10
    } else if max_x > window_dur.as_secs() * 60 {
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

    let window_unit = convert_time(&window_dur).max(1);
    let to_x =
        |d: &Duration| (convert_time(d).saturating_sub(convert_time(&start))) * max_x / window_unit;

    // Process each series
    let mut global_min_y: Option<f64> = None;
    let mut global_max_y: Option<f64> = None;

    for (idx, (_, stats, _, _)) in series.iter().enumerate() {
        if idx >= graph_data.len() {
            break; // Should not happen if caller provides correct buffers
        }
        let data = &mut graph_data[idx];
        data.clear();

        // stats is &[TimedStat] (sorted by time presumably)
        let s = *stats;
        let start_index = s.partition_point(|x| x.time < start).saturating_sub(1);

        let mut prev = (0, 0.0);
        if let Some(first) = s.get(start_index) {
            prev = (to_x(&first.time), first.item);
        }

        for ts in s.iter().skip(start_index) {
            let x = to_x(&ts.time);

            global_max_y = Some(global_max_y.map_or(ts.item, |m| m.max(ts.item)));
            global_min_y = Some(global_min_y.map_or(ts.item, |m| m.min(ts.item)));

            if x > prev.0 || data.is_empty() {
                if x > prev.0 + 1 && !data.is_empty() {
                    for v in (prev.0 + 1)..x {
                        data.push((v as f64, prev.1));
                    }
                }
                data.push((x as f64, ts.item));
                prev = (x, ts.item);
            } else {
                if let Some(last) = data.last_mut() {
                    last.1 = ts.item;
                }
                prev.1 = ts.item;
            }
        }

        // Extrapolate
        let end_x = to_x(&end);
        if end_x > prev.0 {
            for v in (prev.0 + 1)..=end_x.min(max_x) {
                data.push((v as f64, prev.1));
            }
        }
        if max_x > prev.0 + 1 {
            for v in (prev.0 + 1)..max_x {
                data.push((v as f64, prev.1));
            }
        }
    }

    if let Some((p_min, p_max)) = preset_y_range {
        global_min_y = Some(global_min_y.map_or(p_min, |m| m.min(p_min)));
        global_max_y = Some(global_max_y.map_or(p_max, |m| m.max(p_max)));
    }

    let min_y = global_min_y.unwrap_or(0.0);
    let mut max_y = global_max_y.unwrap_or(0.0);

    if preset_y_range.is_none() && min_y >= 0.0 {
        max_y = max_y.max(1.0);
    }
    if (min_y - max_y).abs() < f64::EPSILON {
        max_y = min_y + 1.0;
    }

    // Create Datasets
    let datasets: Vec<Dataset> = series
        .iter()
        .enumerate()
        .map(|(idx, (name, _, _, style))| {
            Dataset::default()
                .name(*name)
                .marker(if enhanced_graphics {
                    symbols::Marker::Braille
                } else {
                    symbols::Marker::Dot
                })
                .style(*style)
                .data(&graph_data[idx])
        })
        .collect();

    let mut block = Block::default().borders(Borders::BOTTOM | Borders::LEFT | Borders::RIGHT);
    if !title.is_empty() {
        block = block.title(Span::styled(
            title,
            title_style.unwrap_or_else(|| {
                Style::default()
                    .fg(Color::LightCyan)
                    .add_modifier(Modifier::BOLD)
            }),
        ));
    }

    let min_lbl_x = format_duration(&start);
    let med_lbl_x = format_duration(&(start + window_dur / 2));
    let max_lbl_x = format_duration(&end);
    let x_labels = vec![
        Span::styled(min_lbl_x, Style::default().add_modifier(Modifier::BOLD)),
        Span::raw(med_lbl_x),
        Span::styled(max_lbl_x, Style::default().add_modifier(Modifier::BOLD)),
    ];

    let chart = Chart::new(datasets)
        .block(block)
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
                .bounds([min_y, max_y])
                .labels(vec![
                    Span::styled(
                        format!("{min_y:.2}"),
                        Style::default().add_modifier(Modifier::BOLD),
                    ),
                    Span::raw(format!("{:.2}", (max_y - min_y) / 2.0 + min_y)),
                    Span::styled(
                        format!("{max_y:.2}"),
                        Style::default().add_modifier(Modifier::BOLD),
                    ),
                ]),
        );

    f.render_widget(chart, area);
}

/// Generic helper to draw a simple Key-Value table
/// This unifies duplicated logic for generic stats
pub fn draw_key_value_block<'a, I>(
    f: &mut Frame,
    area: Rect,
    title: &str,
    data: I,
    constraints: &[Constraint],
) where
    I: IntoIterator<Item = (Span<'a>, String)>,
{
    let rows = data
        .into_iter()
        .map(|(k, v)| Row::new([Cell::from(k), Cell::from(Span::raw(v))]));

    let mut block = Block::default().borders(if title.is_empty() {
        Borders::LEFT | Borders::RIGHT | Borders::BOTTOM
    } else {
        Borders::ALL
    });

    if !title.is_empty() {
        block = block.title(Span::styled(
            title,
            Style::default()
                .fg(Color::LightCyan)
                .add_modifier(Modifier::BOLD),
        ));
    }

    let table = Table::default().rows(rows).block(block).widths(constraints);
    f.render_widget(table, area);
}

/// Draw the geometry information of the fuzzing items (testcases)
pub fn draw_item_geometry_text(
    f: &mut Frame,
    area: Rect,
    item_geometry: &ItemGeometry,
    hint: &str,
    scroll: usize,
    force_hint: bool,
    pizza_mode: bool,
) -> usize {
    let (pending, pend_fav, owns, imported, stability) = if pizza_mode {
        (
            "waiting orders",
            "chef's specials",
            "fresh ingredients",
            "takeouts",
            "dough consistency",
        )
    } else {
        ("pending", "pend fav", "own finds", "imported", "stability")
    };

    let data = vec![
        (Span::raw(pending), format!("{}", item_geometry.pending)),
        (Span::raw(pend_fav), format!("{}", item_geometry.pend_fav)),
        (Span::raw(owns), format!("{}", item_geometry.own_finds)),
        (Span::raw(imported), format!("{}", item_geometry.imported)),
        (
            Span::raw(stability),
            format!("{:.2}%", item_geometry.stability.unwrap_or(0.0) * 100.0),
        ),
    ];

    let title = if pizza_mode {
        "pizza shapes"
    } else {
        "item geometry"
    };
    draw_scrolled_stats(f, area, title, &data, scroll, hint, force_hint)
}

/// Draw the process timing information
pub fn draw_process_timing_text(
    f: &mut Frame,
    area: Rect,
    title: &str,
    data: &ProcessTiming,
    run_time: Duration,
    pizza_mode: bool,
) {
    let (run, speed, entry, solution) = if pizza_mode {
        (
            "open time",
            "baking speed",
            "last pizza baked",
            "last delivery",
        )
    } else {
        ("run time", "exec speed", "last new entry", "last solution")
    };

    let rows = [
        (Span::raw(run), format_duration(&run_time)),
        (Span::raw(speed), data.exec_speed.clone()),
        (Span::raw(entry), format_duration(&(data.last_new_entry))),
        (
            Span::raw(solution),
            format_duration(&(data.last_saved_solution)),
        ),
    ];

    draw_key_value_block(
        f,
        area,
        title,
        rows,
        &[Constraint::Length(24), Constraint::Min(5)],
    );
}

/// Draw the client logs
#[allow(deprecated)]
pub fn draw_logs(f: &mut Frame, area: Rect, logs: &[String], enable_wrap: bool, pizza_mode: bool) {
    let num_lines = area.height.saturating_sub(2) as usize;
    let mut list_items: Vec<ListItem> = Vec::with_capacity(num_lines);

    let themed_logs: Vec<String> = if pizza_mode {
        logs.iter()
            .map(|msg| {
                msg.replace("corpus", "pizzas")
                    .replace("objectives", "deliveries")
                    .replace("executions", "doughs")
                    .replace("exec/sec", "p/s")
                    .replace("current_testcase", "current_order")
                    .replace("imported", "takeouts")
                    .replace("signals_stability", "dough_consistency")
                    .replace("cycles_wo_finds", "ovens_empty")
                    .replace("pending", "waiting_orders")
            })
            .collect()
    } else {
        logs.to_vec()
    };

    if enable_wrap {
        let width = area.width.saturating_sub(2) as usize;
        if width > 0 {
            for msg in themed_logs.iter().rev() {
                let mut lines = vec![];
                let chars: Vec<char> = msg.chars().collect();
                if chars.len() <= width {
                    lines.push(msg.clone());
                } else {
                    // ... wrapping logic ...
                    // (Presumed context from previous view, re-implemented briefly)
                    let mut i = 0;
                    let mut first = true;
                    while i < chars.len() {
                        let limit = if first {
                            width
                        } else {
                            width.saturating_sub(2)
                        };
                        let remaining = chars.len() - i;
                        let chunk_len = if remaining <= limit {
                            remaining
                        } else {
                            let mut split = limit;
                            if let Some(pos) = chars[i..i + limit]
                                .iter()
                                .rposition(|c| c.is_whitespace())
                                .filter(|&p| p > 0)
                            {
                                split = pos + 1;
                            }
                            split
                        };

                        let chunk: String = chars[i..i + chunk_len].iter().collect();
                        if first {
                            lines.push(chunk);
                            first = false;
                        } else {
                            lines.push(format!("  {chunk}"));
                        }
                        i += chunk_len;
                    }
                }

                for line in lines.into_iter().rev() {
                    list_items.push(ListItem::new(Span::raw(line)));
                    if list_items.len() >= num_lines {
                        break;
                    }
                }
                if list_items.len() >= num_lines {
                    break;
                }
            }
            list_items.reverse();
        }
    } else {
        let start_index = themed_logs.len().saturating_sub(num_lines);
        list_items = themed_logs[start_index..]
            .iter()
            .map(|msg| ListItem::new(Span::raw(msg)))
            .collect();
    }

    let title = if pizza_mode {
        "kitchen logs (`t` to show/hide, `w` to wrap)"
    } else {
        "clients logs (`t` to show/hide, `w` to wrap)"
    };

    let logs_widget = List::new(list_items).block(
        Block::default()
            .borders(Borders::ALL)
            .title(Span::styled(
                title,
                Style::default()
                    .fg(Color::LightCyan)
                    .add_modifier(Modifier::BOLD),
            ))
            // Using title_top/bottom or alignment deprecated approach (will fix in ui.rs separately if needed)
            .title_top(
                Line::from(Span::styled(
                    if enable_wrap { "wrapped" } else { "raw" },
                    Style::default().fg(Color::DarkGray),
                ))
                .alignment(Alignment::Right),
            )
            .title_bottom(
                Line::from(Span::styled(
                    "`r` to refresh, `q` to quit",
                    Style::default()
                        .fg(Color::LightMagenta)
                        .add_modifier(Modifier::BOLD),
                ))
                .alignment(Alignment::Right),
            ),
    );
    f.render_widget(logs_widget, area);
}

/// Draw a scrolled list of stats
pub fn draw_scrolled_stats(
    f: &mut Frame,
    area: Rect,
    block_title: &str,
    stats: &[(Span, String)],
    scroll: usize,
    nav_hint_str: &str,
    force_hint: bool,
) -> usize {
    let total_stats = stats.len();
    let max_items = area.height.saturating_sub(2) as usize;

    if total_stats == 0 {
        return max_items;
    }

    let start_idx = if scroll >= total_stats { 0 } else { scroll };
    let end_idx = (start_idx + max_items).min(total_stats);

    let visible_stats = stats[start_idx..end_idx].iter().cloned();

    let nav_hint = match (start_idx > 0, total_stats > start_idx + max_items) {
        (true, _) | (_, true) => nav_hint_str,
        (false, false) => {
            if force_hint {
                nav_hint_str
            } else {
                ""
            }
        }
    };

    let title = format!(
        "{block_title} {}-{}/{}{}",
        start_idx + 1,
        end_idx,
        total_stats,
        nav_hint
    );

    draw_key_value_block(
        f,
        area,
        &title,
        visible_stats,
        &[Constraint::Percentage(70), Constraint::Percentage(30)],
    );
    max_items
}

/// Draw the user stats
pub fn draw_user_stats(
    f: &mut Frame,
    area: Rect,
    _client_idx: usize,
    stats: &[(Span, String)],
    scroll: usize,
) -> usize {
    draw_scrolled_stats(f, area, "User Stats", stats, scroll, " (u/U)", false)
}
