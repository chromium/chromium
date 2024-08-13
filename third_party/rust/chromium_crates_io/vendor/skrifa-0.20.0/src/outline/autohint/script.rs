//! Codepoint to script mapping.

use super::style::GlyphStyle;
use raw::types::Tag;

/// Defines the basic properties for each script supported by the
/// autohinter.
#[derive(Clone, Debug)]
pub(super) struct ScriptClass {
    pub name: &'static str,
    /// Unicode tag for the script.
    pub tag: Tag,
    /// Index of self in the SCRIPT_CLASSES array.
    pub index: usize,
    /// True if outline edges are processed top to bottom.
    pub hint_top_to_bottom: bool,
    /// Characters used to define standard width and height of stems.
    pub std_chars: &'static [char],
    /// "Blue" characters used to define alignment zones.
    pub blues: &'static [(&'static [char], u32)],
}

impl ScriptClass {
    pub fn from_index(index: u16) -> Option<&'static ScriptClass> {
        SCRIPT_CLASSES.get(index as usize)
    }
}

/// Associates a basic glyph style with a range of codepoints.
#[derive(Copy, Clone, Debug)]
pub(super) struct ScriptRange {
    pub first: u32,
    pub last: u32,
    pub style: GlyphStyle,
}

impl ScriptRange {
    pub fn contains(&self, ch: u32) -> bool {
        (self.first..=self.last).contains(&ch)
    }
}

// These properties ostensibly come from
// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afblue.h#L317>
// but are modified to match those at
// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/aflatin.h#L68>
// so that when don't need to keep two sets and adjust during blue computation.
pub(super) mod blue_flags {
    pub const LATIN_ACTIVE: u32 = 1 << 0;
    pub const LATIN_TOP: u32 = 1 << 1;
    pub const LATIN_SUB_TOP: u32 = 1 << 2;
    pub const LATIN_NEUTRAL: u32 = 1 << 3;
    pub const LATIN_BLUE_ADJUSTMENT: u32 = 1 << 4;
    pub const LATIN_X_HEIGHT: u32 = 1 << 5;
    pub const LATIN_LONG: u32 = 1 << 6;
    pub const CJK_TOP: u32 = 1 << 0;
    pub const CJK_HORIZ: u32 = 1 << 1;
    pub const CJK_RIGHT: u32 = CJK_TOP;
}

// The following are helpers for generated code.
const fn base_range(first: u32, last: u32, script_index: usize) -> ScriptRange {
    ScriptRange {
        first,
        last,
        style: GlyphStyle::from_script_index_and_flags(script_index as u16, 0),
    }
}

const fn non_base_range(first: u32, last: u32, script_index: usize) -> ScriptRange {
    ScriptRange {
        first,
        last,
        style: GlyphStyle::from_script_index_and_flags(script_index as u16, GlyphStyle::NON_BASE),
    }
}

use blue_flags::*;

include!("../../../generated/generated_autohint_scripts.rs");
