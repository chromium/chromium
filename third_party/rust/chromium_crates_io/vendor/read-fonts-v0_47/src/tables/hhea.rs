//! the [hhea (Horizontal Header)](https://docs.microsoft.com/en-us/typography/opentype/spec/hhea) table

include!("../../generated/generated_hhea.rs");

/// A horizontal header from HHEA or hhea.
#[derive(Clone)]
pub enum SelectedHhea<'a> {
    /// The original `hhea` table.
    Original(Hhea<'a>),
    /// The `HHEA` table (ISO/IEC 14496-22:2026).
    Extended(HheaExtended<'a>),
}

impl SelectedHhea<'_> {
    /// Number of long metric records.
    pub fn number_of_h_metrics(&self) -> u32 {
        match self {
            Self::Original(table) => table.number_of_h_metrics().into(),
            Self::Extended(table) => table.number_of_h_metrics(),
        }
    }

    selected_table_getters!(
        version -> MajorMinor,
        ascender -> FWord,
        descender -> FWord,
        line_gap -> FWord,
        advance_width_max -> UfWord,
        min_left_side_bearing -> FWord,
        min_right_side_bearing -> FWord,
        x_max_extent -> FWord,
        caret_slope_rise -> i16,
        caret_slope_run -> i16,
        caret_offset -> i16,
        metric_data_format -> i16,
    );
}

impl Hhea<'_> {
    #[deprecated(since = "0.26.0", note = "use number_of_h_metrics instead")]
    pub fn number_of_long_metrics(&self) -> u16 {
        self.number_of_h_metrics()
    }
}
