//! Scalable glyph outlines.
//!
//! [`OutlineGlyphs`] loads outlines from the source selected by a
//! [`GlyphSourceOrder`]. Each [`OutlineGlyph`] identifies its source: a
//! [`glyf::GlyfGlyph`], [`cff::CffGlyph`], [`type1::Type1Glyph`] or
//! [`varc::VarcGlyph`]. Each of these provides access to the source's own
//! types, which are needed for hinting. These are the `glyf` loader in
//! [`tables::glyf::outline`] and the charstrings in [`ps`].
//!
//! Outlines are drawn as paths into a [`PathSink`]. A `Vec` of
//! [`PathElement`] collects a path, [`ControlBoundsSink`] measures it,
//! [`TransformSink`] transforms it, [`SvgSink`] writes it as SVG path data,
//! and [`NullSink`] discards it.
//!
//! [`GlyphSourceOrder`]: crate::model::glyph::GlyphSourceOrder
//! [`tables::glyf::outline`]: crate::tables::glyf::outline
//! [`ps`]: crate::ps

pub mod cff;
pub mod glyf;
mod glyphs;
mod path;
pub mod type1;
mod unscaled;
pub mod varc;

#[doc(inline)]
pub use crate::tables::glyf::outline::PathContourStart;
#[doc(inline)]
pub use glyphs::{OutlineGlyph, OutlineGlyphs};
#[doc(inline)]
pub use path::{ControlBoundsSink, NullSink, PathElement, PathSink, SvgSink, TransformSink};
pub(crate) use unscaled::PointsFromPath;
#[doc(inline)]
pub use unscaled::UnscaledOutlineSink;

use super::GlyphSource;

/// A table that provides outlines.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum OutlineSource {
    /// Variable composites in `VARC`.
    ///
    /// The components are drawn from one of the other outline sources.
    Varc,
    /// TrueType outlines in `glyf`.
    Glyf,
    /// Variable PostScript outlines in `CFF2`.
    Cff2,
    /// PostScript outlines in a `CFF` table or a standalone CFF font.
    Cff,
    /// Charstrings of a Type 1 font.
    Type1,
}

impl From<OutlineSource> for GlyphSource {
    fn from(source: OutlineSource) -> Self {
        match source {
            OutlineSource::Varc => Self::Varc,
            OutlineSource::Glyf => Self::Glyf,
            OutlineSource::Cff2 => Self::Cff2,
            OutlineSource::Cff => Self::Cff,
            OutlineSource::Type1 => Self::Type1,
        }
    }
}

impl TryFrom<GlyphSource> for OutlineSource {
    /// The original source, which contains no outlines.
    type Error = GlyphSource;

    fn try_from(source: GlyphSource) -> Result<Self, GlyphSource> {
        Ok(match source {
            GlyphSource::Varc => Self::Varc,
            GlyphSource::Glyf => Self::Glyf,
            GlyphSource::Cff2 => Self::Cff2,
            GlyphSource::Cff => Self::Cff,
            GlyphSource::Type1 => Self::Type1,
            _ => return Err(source),
        })
    }
}

/// How an outline is drawn.
///
/// A `glyf` outline is scaled in 26.6 fixed point. Without
/// a size, it is drawn in font units, with an implied on-curve point between
/// two off-curve points keeping its half unit.
///
/// The variation location comes from the font. Hinting is done through the
/// source's own types.
#[derive(Copy, Clone, Default, PartialEq, Debug)]
pub struct DrawOptions {
    /// Pixels per em to draw at, or `None` for font units.
    pub ppem: Option<f32>,
    /// Where a `glyf` contour starts when its first point is off-curve.
    pub contour_start: PathContourStart,
}

impl DrawOptions {
    /// Creates options that draw in font units with the default contour start.
    pub fn new() -> Self {
        Self::default()
    }

    /// Sets the size in pixels per em, or `None` for font units.
    pub fn with_ppem(self, ppem: Option<f32>) -> Self {
        Self { ppem, ..self }
    }

    /// Sets where a `glyf` contour starts when its first point is off-curve.
    pub fn with_contour_start(self, contour_start: PathContourStart) -> Self {
        Self {
            contour_start,
            ..self
        }
    }
}

/// Working memory for drawing an outline.
///
/// A `glyf` outline and a `VARC` composite need working memory to draw; a
/// charstring needs none. The size an outline needs is given by its
/// `required_memory_size`.
#[derive(Debug, Default)]
pub enum DrawMemory<'a> {
    /// Short-lived scratch memory of the size the outline needs, which may be
    /// allocated on the heap for a large glyph.
    #[default]
    Scratch,
    /// Memory supplied by the caller, of at least the size the outline
    /// needs.
    Buffer(&'a mut [u8]),
}

impl<'a> DrawMemory<'a> {
    /// Returns the caller's memory, or `None` for scratch memory.
    pub(crate) fn into_buffer(self) -> Option<&'a mut [u8]> {
        match self {
            Self::Scratch => None,
            Self::Buffer(buffer) => Some(buffer),
        }
    }
}

impl<'a> From<&'a mut [u8]> for DrawMemory<'a> {
    fn from(buffer: &'a mut [u8]) -> Self {
        Self::Buffer(buffer)
    }
}

/// What drawing an outline reports about it.
///
/// Values are in the units the outline was drawn in: pixels at a size, font
/// units otherwise. Hinting may adjust the advance and bearing.
#[non_exhaustive]
#[derive(Copy, Clone, Default, PartialEq, Debug)]
pub struct DrawnMetrics {
    /// The horizontal advance, if the outline provides one.
    ///
    /// A `glyf` outline provides it through its phantom points, with
    /// variations applied. A charstring provides it directly. A `VARC`
    /// composite has none.
    pub h_advance: Option<f32>,
    /// The left side bearing, if the outline provides one.
    ///
    /// Only a `glyf` outline provides it, through its phantom points. A
    /// hinter that does not move phantom points, such as an automatic
    /// hinter, may not report it.
    pub x_bearing: Option<f32>,
    /// True if the glyph is flagged as having overlapping contours or
    /// components.
    ///
    /// Only `glyf` has these flags.
    pub has_overlaps: bool,
    /// The dropout control mode requested by the glyph program.
    ///
    /// This tells a bilevel rasterizer how to treat features thinner than a
    /// pixel. It is [`DropoutControl::Simple`], the TrueType default, when
    /// no TrueType hinter ran. When one ran, it is the mode the font's
    /// programs left, and [`DropoutControl::Ignore`] if they never turned
    /// dropout control on.
    pub dropout_control: DropoutControl,
}

/// The dropout control mode for a bilevel rasterizer.
///
/// Dropout control defines how to treat features too thin to register on the
/// pixel grid. A TrueType glyph program selects the mode with the `SCANCTRL`
/// and `SCANTYPE` instructions, and may select a different mode for each
/// glyph. It affects only rasterization to one bit per pixel. An antialiasing
/// rasterizer uses coverage and does not need it.
///
/// See <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructions#scan-conversion-control>.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub enum DropoutControl {
    /// Fills only pixels whose centers are covered. Thin features may vanish.
    Ignore,
    /// Restores a dropped feature that crosses a scan line, ignoring stubs.
    ///
    /// This is the default when a glyph program does not set a mode.
    #[default]
    Simple,
    /// Like [`Self::Simple`], but also restores stubs.
    SimpleWithStubs,
    /// Restores a dropped feature at the best position chosen by the scan
    /// converter rather than a fixed one, ignoring stubs.
    Smart,
    /// Like [`Self::Smart`], but also restores stubs.
    SmartWithStubs,
}

impl DropoutControl {
    /// Returns true if dropped features should be restored.
    pub fn is_enabled(self) -> bool {
        self != Self::Ignore
    }

    /// Returns true if the scan converter should choose the restore position.
    pub fn is_smart(self) -> bool {
        matches!(self, Self::Smart | Self::SmartWithStubs)
    }

    /// Returns true if stubs should be restored along with crossing features.
    pub fn includes_stubs(self) -> bool {
        matches!(self, Self::SimpleWithStubs | Self::SmartWithStubs)
    }
}

#[cfg(test)]
mod tests {
    use super::DropoutControl;

    #[test]
    fn dropout_predicates() {
        assert!(!DropoutControl::Ignore.is_enabled());
        assert!(DropoutControl::Simple.is_enabled());
        assert!(!DropoutControl::Simple.is_smart());
        assert!(DropoutControl::SmartWithStubs.is_smart());
        assert!(DropoutControl::SmartWithStubs.includes_stubs());
        assert!(!DropoutControl::Smart.includes_stubs());
        assert_eq!(DropoutControl::default(), DropoutControl::Simple);
    }
}
