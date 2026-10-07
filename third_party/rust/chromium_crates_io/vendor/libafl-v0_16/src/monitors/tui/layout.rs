use alloc::vec::Vec;

use ratatui::layout::{Constraint, Direction, Layout, Rect};

const LOGS_MIN_HEIGHT: u16 = 5;
const PROCESS_TIMING_HEIGHT: u16 = 7;
const PROCESS_TIMING_COMPACT_HEIGHT: u16 = 4;
const DEFAULT_TOP_HEIGHT_WITH_CHARTS: u16 = 15;
const DEFAULT_TOP_HEIGHT_NO_CHARTS: u16 = 12;

/// Splits the main area into top (overall), middle (client), and bottom (logs)
#[must_use]
pub fn split_main(area: Rect, show_logs: bool, introspection: bool, has_charts: bool) -> Vec<Rect> {
    if show_logs {
        if introspection {
            // Introspection mode
            Layout::default()
                .constraints(
                    [
                        Constraint::Percentage(40),
                        Constraint::Percentage(30),
                        Constraint::Percentage(30),
                    ]
                    .as_ref(),
                )
                .split(area)
                .to_vec()
        } else {
            // Normal mode
            let available = area.height.saturating_sub(LOGS_MIN_HEIGHT);

            let target_top = if has_charts {
                DEFAULT_TOP_HEIGHT_WITH_CHARTS
            } else {
                DEFAULT_TOP_HEIGHT_NO_CHARTS
            };

            let mid_len = 6;
            let (top_len, mid_len) = if available >= (target_top + mid_len) {
                (target_top, mid_len)
            } else {
                // Squeeze mode: prioritize Top, squeeze Mid
                if available > target_top {
                    (target_top, available - target_top)
                } else {
                    (available.saturating_sub(mid_len).max(1), mid_len)
                }
            };

            Layout::default()
                .constraints(
                    [
                        Constraint::Length(top_len),
                        Constraint::Length(mid_len),
                        Constraint::Min(LOGS_MIN_HEIGHT), // prioritize logs at end
                    ]
                    .as_ref(),
                )
                .split(area)
                .to_vec()
        }
    } else {
        Layout::default()
            .constraints([Constraint::Percentage(50), Constraint::Percentage(50)].as_ref())
            .split(area)
            .to_vec()
    }
}

/// Splits the top area into left (stats) and right (charts)
#[must_use]
pub fn split_top(area: Rect) -> (Rect, Rect) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)].as_ref())
        .split(area);
    (chunks[0], chunks[1])
}

/// Splits the area to reserve space for a title
#[must_use]
pub fn split_title(area: Rect) -> (Rect, Rect) {
    let chunks = Layout::default()
        .constraints([Constraint::Length(3), Constraint::Min(0)].as_ref())
        .split(area);
    (chunks[0], chunks[1])
}

/// Splits the process timing area
#[must_use]
pub fn split_process_timing(area: Rect) -> (Rect, Rect) {
    let height = if area.height < 15 {
        PROCESS_TIMING_COMPACT_HEIGHT
    } else {
        PROCESS_TIMING_HEIGHT
    };

    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Length(height), Constraint::Min(0)].as_ref())
        .split(area);
    (chunks[0], chunks[1])
}

/// Splits the client area into left and right columns
#[must_use]
pub fn split_client(area: Rect) -> (Rect, Rect) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)].as_ref())
        .split(area);
    (chunks[0], chunks[1])
}
