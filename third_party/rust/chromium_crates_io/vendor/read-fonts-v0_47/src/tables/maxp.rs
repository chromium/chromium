//! The [maxp](https://docs.microsoft.com/en-us/typography/opentype/spec/maxp) table

include!("../../generated/generated_maxp.rs");

/// A maximum profile from MAXP or the original maxp table.
#[derive(Clone)]
pub enum SelectedMaxp<'a> {
    /// The original `maxp` table.
    Original(Maxp<'a>),
    /// The `MAXP` table (ISO/IEC 14496-22:2026).
    Extended(MaxpExtended<'a>),
}

impl SelectedMaxp<'_> {
    /// Returns the glyph count without narrowing it to 16 bits.
    pub fn num_glyphs(&self) -> u32 {
        match self {
            Self::Original(table) => table.num_glyphs().into(),
            Self::Extended(table) => table.num_glyphs().into(),
        }
    }

    selected_table_getters!(
        version -> Version16Dot16,
        max_points -> Option<u16>,
        max_contours -> Option<u16>,
        max_composite_points -> Option<u16>,
        max_composite_contours -> Option<u16>,
        max_zones -> Option<u16>,
        max_twilight_points -> Option<u16>,
        max_storage -> Option<u16>,
        max_function_defs -> Option<u16>,
        max_instruction_defs -> Option<u16>,
        max_stack_elements -> Option<u16>,
        max_size_of_instructions -> Option<u16>,
        max_component_elements -> Option<u16>,
        max_component_depth -> Option<u16>,
    );
}
