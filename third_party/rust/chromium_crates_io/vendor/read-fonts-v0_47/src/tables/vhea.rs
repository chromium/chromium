//! the [vhea (Vertical Header)](https://docs.microsoft.com/en-us/typography/opentype/spec/vhea) table

include!("../../generated/generated_vhea.rs");

/// A vertical header from VHEA or vhea.
#[derive(Clone)]
pub enum SelectedVhea<'a> {
    /// The original `vhea` table.
    Original(Vhea<'a>),
    /// The `VHEA` table (ISO/IEC 14496-22:2026).
    Extended(VheaExtended<'a>),
}

impl SelectedVhea<'_> {
    /// Number of long metric records.
    pub fn number_of_long_ver_metrics(&self) -> u32 {
        match self {
            Self::Original(table) => table.number_of_long_ver_metrics().into(),
            Self::Extended(table) => table.number_of_long_ver_metrics(),
        }
    }

    /// Maximum advance height.
    ///
    /// VHEA states this as a signed value, which is read as unsigned like
    /// that of vhea, HHEA and hhea.
    pub fn advance_height_max(&self) -> UfWord {
        match self {
            Self::Original(table) => table.advance_height_max(),
            Self::Extended(table) => UfWord::new(table.advance_height_max().to_i16() as u16),
        }
    }

    selected_table_getters!(
        version -> Version16Dot16,
        ascender -> FWord,
        descender -> FWord,
        line_gap -> FWord,
        min_top_side_bearing -> FWord,
        min_bottom_side_bearing -> FWord,
        y_max_extent -> FWord,
        caret_slope_rise -> i16,
        caret_slope_run -> i16,
        caret_offset -> i16,
        metric_data_format -> i16,
    );
}
