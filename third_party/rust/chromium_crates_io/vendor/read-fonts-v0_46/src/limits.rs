//! Limits applied when interpreting font data.
//!
//! Font files can describe structures that recurse, refer to themselves, or are
//! simply enormous. Nothing here comes from the OpenType specification: these
//! are the bounds this crate places on how much work one operation will do
//! before giving up, so that a malformed or hostile font cannot turn a parse
//! into a hang.

/// Maximum depth for structures that nest.
///
/// Applies to composite glyph trees, nested variation data, layout lookup
/// nesting, and colour graph traversal.
///
/// Matches HarfBuzz's [`HB_MAX_NESTING_LEVEL`].
///
/// [`HB_MAX_NESTING_LEVEL`]: https://github.com/harfbuzz/harfbuzz/blob/724ef405f3a0a0b2792c7a575d1f3341b41ca950/src/hb-limits.hh#L53
pub const MAX_RECURSION_DEPTH: usize = 64;

/// Maximum number of points in a single assembled outline.
///
/// A composite glyph accumulates the points of every component in its tree, so
/// unlike the point count of one simple glyph this is not bounded by the
/// format. A contour end point is a `u16`, so the last point an outline can
/// name is `u16::MAX`, and it holds one more than that.
pub const MAX_OUTLINE_POINTS: usize = u16::MAX as usize + 1;

/// Maximum number of references followed while assembling one composite
/// glyph, whether a [`glyf`] component tree or a [`VARC`] graph.
///
/// Bounds the total work for a graph that fans out at every level, which the
/// depth limit alone does not.
///
/// [`glyf`]: crate::tables::glyf
/// [`VARC`]: crate::tables::varc
pub const MAX_COMPOSITE_EDGES: usize = 2048;

/// Maximum number of scripts visited in a layout table's script list.
///
/// Matches HarfBuzz's `HB_MAX_SCRIPTS` in [`hb-limits.hh`].
///
/// [`hb-limits.hh`]: https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-limits.hh
#[cfg(feature = "std")]
pub const MAX_SCRIPTS: u16 = 500;

/// Maximum number of language systems visited in a layout table's script list.
///
/// Matches HarfBuzz's `HB_MAX_LANGSYS` in [`hb-limits.hh`].
///
/// [`hb-limits.hh`]: https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-limits.hh
#[cfg(feature = "std")]
pub const MAX_LANGSYS: u16 = 2000;

/// Maximum total number of feature indices referenced by the language systems
/// visited when pruning a layout table's language systems.
///
/// Matches HarfBuzz's `HB_MAX_LANGSYS_FEATURE_COUNT` in [`hb-limits.hh`].
///
/// [`hb-limits.hh`]: https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-limits.hh
#[cfg(feature = "std")]
pub const MAX_LANGSYS_FEATURE_COUNT: u32 = 50000;

/// Maximum total number of feature indices visited across the language
/// systems of a layout table.
///
/// Matches HarfBuzz's `HB_MAX_FEATURE_INDICES` in [`hb-limits.hh`].
///
/// [`hb-limits.hh`]: https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-limits.hh
#[cfg(feature = "std")]
pub const MAX_FEATURE_INDICES: u16 = 8000;

/// Maximum number of lookups visited by one layout closure operation.
///
/// Matches HarfBuzz's `HB_MAX_LOOKUP_VISIT_COUNT` in [`hb-limits.hh`].
///
/// [`hb-limits.hh`]: https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-limits.hh
#[cfg(feature = "std")]
pub const MAX_LOOKUP_VISIT_COUNT: u16 = 35000;
