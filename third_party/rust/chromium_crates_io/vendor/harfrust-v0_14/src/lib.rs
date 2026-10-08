/*!
Text shaping with a Rust port of [HarfBuzz](https://harfbuzz.github.io/).

Create a [`ShaperFont`] from a [`Font`], add Unicode text to a [`Buffer`],
and call [`shape`] to replace the text with positioned glyphs.
*/

#![cfg_attr(not(feature = "std"), no_std)]
// Forbidding unsafe code only applies to the lib
// examples continue to use it, so this cannot be placed into Cargo.toml
#![forbid(unsafe_code)]
#![warn(missing_docs)]
#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]
#![allow(non_snake_case)]
#![allow(clippy::collapsible_if)]
#![allow(clippy::collapsible_else_if)]
#![allow(clippy::comparison_chain)]
#![allow(clippy::needless_range_loop)]
#![allow(clippy::non_canonical_partial_ord_impl)]
#![allow(clippy::upper_case_acronyms)]
#![allow(clippy::too_many_arguments)]
#![allow(clippy::wildcard_in_or_patterns)]
#![allow(clippy::identity_op)]
#![allow(clippy::inline_always)]
#![allow(clippy::mut_range_bound)]
#![allow(clippy::enum_variant_names)]
#![allow(clippy::manual_range_patterns)]
#![allow(clippy::type_complexity)]
#![allow(clippy::wrong_self_convention)]
#![allow(clippy::match_like_matches_macro)]
#![allow(clippy::manual_range_contains)]

extern crate alloc;

mod algs;
pub mod unicode;
#[macro_use]
mod buffer;
mod aat;
mod cache;
mod direction;
mod error;
pub(crate) mod fallback;
mod feature;
mod language;
pub(crate) mod normalize;
mod once;
mod options;
pub(crate) mod ot;
mod parse_setting;
mod plan;
pub(crate) mod planner;
mod scale;
mod script;
pub(crate) mod set_digest;
mod shape;
mod shaper_font;
mod tag;
#[allow(clippy::collapsible_match)]
mod tag_table;
mod text_parser;

type Mask = u32;

#[inline(always)]
fn clamp_i64_to_i32(value: i64) -> i32 {
    value.clamp(i64::from(i32::MIN), i64::from(i32::MAX)) as i32
}

pub(crate) type U32Set = read_fonts::collections::int_set::U32Set;

pub use error::ShapeError;
pub use options::ShapeOptions;
#[doc(hidden)]
pub use parse_setting::ParseSetting;
pub use plan::{ShapePlan, ShapePlanKey};
pub(crate) use planner::ShapePlanner;
pub use scale::Scale;
pub use shape::shape;
pub(crate) use shaper_font::LayoutData;

pub use buffer::{Buffer, ContentType, GlyphFlags, GlyphInfo, GlyphPosition};
pub use direction::Direction;
pub use feature::Feature;
pub use language::Language;
pub use script::Script;
pub use shaper_font::{
    Advances, FontFuncs, GlyphExtents, NominalGlyphs, RawAdvances, RawNominalGlyphs, ShaperFont,
};

/// Font types supplied by `read-fonts`.
pub mod font {
    pub use read_fonts::model::*;
}

#[doc(inline)]
pub use font::Font;

pub use read_fonts::types::{GlyphId, Tag};

// /// An OpenType tag.
// pub type Tag = read_fonts::types::Tag;

// /// A 32-bit glyph identifier.
// pub type GlyphId = read_fonts::types::GlyphId;

bitflags::bitflags! {
    /// Flags that control how a buffer is shaped.
    #[derive(Default, Debug, Clone, Copy)]
    pub struct BufferFlags: u32 {
        /// Treat the buffer as the beginning of a paragraph.
        ///
        /// Set this when the buffer includes the beginning of the text, rather
        /// than a segment shaped with surrounding context.
        const BEGINNING_OF_TEXT             = 0x0000_0001;
        /// Treat the buffer as the end of a paragraph.
        ///
        /// See [`BufferFlags::BEGINNING_OF_TEXT`].
        const END_OF_TEXT                   = 0x0000_0002;
        /// Keep glyphs for default-ignorable Unicode characters.
        ///
        /// Without this flag, shaping hides them by substituting a space glyph
        /// with zero advance. This takes precedence over
        /// [`BufferFlags::REMOVE_DEFAULT_IGNORABLES`].
        const PRESERVE_DEFAULT_IGNORABLES   = 0x0000_0004;
        /// Remove default-ignorable Unicode characters from the glyph output.
        ///
        /// [`BufferFlags::PRESERVE_DEFAULT_IGNORABLES`] takes precedence.
        const REMOVE_DEFAULT_IGNORABLES     = 0x0000_0008;
        /// Suppress dotted circles for broken character sequences.
        const DO_NOT_INSERT_DOTTED_CIRCLE   = 0x0000_0010;
        /// Reserved for HarfBuzz-compatible shaping verification.
        ///
        /// Verification is not currently implemented by this crate.
        const VERIFY                        = 0x0000_0020;
        /// Produce [`GlyphFlags::UNSAFE_TO_CONCAT`] on output glyphs.
        ///
        /// Disabled by default because it adds shaping work.
        const PRODUCE_UNSAFE_TO_CONCAT      = 0x0000_0040;
        /// Produce [`GlyphFlags::SAFE_TO_INSERT_TATWEEL`] on output glyphs.
        const PRODUCE_SAFE_TO_INSERT_TATWEEL      = 0x0000_0080;
        /// All currently defined flags.
        const DEFINED = 0x0000_00FF;
    }
}

/// How input text clusters are assigned to output glyphs.
///
/// Cluster values relate glyphs to positions in the input. Monotone levels
/// preserve cluster order, which is useful for finding line break positions.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum ClusterLevel {
    /// Keep grapheme clusters together and preserve cluster order.
    MonotoneGraphemes,
    /// Preserve cluster order, allowing characters to keep separate clusters.
    MonotoneCharacters,
    /// Allow separate character clusters without preserving cluster order.
    Characters,
    /// Keep grapheme clusters together without preserving cluster order.
    Graphemes,
}

impl ClusterLevel {
    #[inline]
    fn new(level: u32) -> Self {
        match level {
            0 => Self::MonotoneGraphemes,
            1 => Self::MonotoneCharacters,
            2 => Self::Characters,
            3 => Self::Graphemes,
            _ => Self::MonotoneGraphemes,
        }
    }
    #[inline]
    fn is_monotone(self) -> bool {
        matches!(self, Self::MonotoneGraphemes | Self::MonotoneCharacters)
    }
    #[inline]
    fn is_graphemes(self) -> bool {
        matches!(self, Self::MonotoneGraphemes | Self::Graphemes)
    }
    #[inline]
    fn _is_characters(self) -> bool {
        matches!(self, Self::MonotoneCharacters | Self::Characters)
    }
}

impl Default for ClusterLevel {
    #[inline]
    fn default() -> Self {
        ClusterLevel::MonotoneGraphemes
    }
}

bitflags::bitflags! {
    /// Flags used for serializing a buffer.
    #[derive(Default)]
    pub struct SerializeFlags: u8 {
        /// Omit glyph cluster values.
        const NO_CLUSTERS       = 0b0000_0001;
        /// Omit glyph position information.
        const NO_POSITIONS      = 0b0000_0010;
        /// Serialize glyph IDs instead of glyph names.
        const NO_GLYPH_NAMES    = 0b0000_0100;
        /// Serialize glyph extents.
        const GLYPH_EXTENTS     = 0b0000_1000;
        /// Serialize glyph flags.
        const GLYPH_FLAGS       = 0b0001_0000;
        /// Omit advances and report offsets as absolute glyph positions.
        const NO_ADVANCES       = 0b0010_0000;
        /// All currently defined flags.
        const DEFINED = 0b0011_1111;
    }
}
