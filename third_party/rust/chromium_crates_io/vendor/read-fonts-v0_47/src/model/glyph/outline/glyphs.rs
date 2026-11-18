//! Glyph outlines loaded from the sources selected by a source order.

use super::{
    cff::CffGlyph,
    glyf::{draw_plan, plan_buffer_size, FontAt, GlyfContext, GlyfGlyph, Precision},
    type1::Type1Glyph,
    varc::{self, VarcGlyph},
    ControlBoundsSink, DrawMemory, DrawOptions, DrawnMetrics, PathSink, PointsFromPath,
    UnscaledOutlineSink,
};
use crate::model::Error;
use crate::{
    mem::with_scratch,
    model::{
        glyph::{outline::OutlineSource, GlyphSource, GlyphSourceOrder},
        metrics::GlyphExtents,
        Font,
    },
    ps::{cff::CffFontRef, type1::Type1Instance},
    tables::glyf::outline::{OutlineContext, OutlinePlan},
    types::{BoundingBox, F2Dot14, GlyphId},
};
use yoke::Yokeable;

/// Provides access to the glyph outlines in a font.
///
/// The sources used for drawing are selected when this is created. It is
/// cheap to create and copy because it only borrows tables that the font
/// has already parsed.
#[derive(Clone, Copy, Yokeable)]
pub struct OutlineGlyphs<'a> {
    font: &'a Font,
    base: Base<'a>,
    varc: Option<&'a varc::Outlines<'a>>,
}

/// The source used to draw glyphs that are not `VARC` composites.
#[derive(Clone, Copy)]
enum Base<'a> {
    None,
    Glyf(GlyfContext<'a>),
    Cff(&'a CffFontRef<'a>, OutlineSource),
    Type1(Type1Instance<'a>),
}

impl<'a> OutlineGlyphs<'a> {
    pub(crate) fn new(font: &'a Font, order: GlyphSourceOrder) -> Self {
        let mut base = Base::None;
        let mut varc = None;
        if let Some(type1) = font.type1_instance() {
            if order.contains(GlyphSource::Type1) {
                base = Base::Type1(type1);
            }
            return Self { font, base, varc };
        }
        // A standalone CFF font has no tables, so it finds no composites or
        // `glyf` here, only its charstrings as `CFF`.
        for source in order
            .iter()
            .filter_map(|source| OutlineSource::try_from(source).ok())
        {
            match source {
                // Covers only some glyphs, so it layers over a base that
                // comes after it rather than replacing one.
                OutlineSource::Varc if varc.is_none() => varc = font.varc_outlines(),
                OutlineSource::Glyf => {
                    if let Some(context) = GlyfContext::new(font) {
                        base = Base::Glyf(context);
                        break;
                    }
                }
                OutlineSource::Cff2 | OutlineSource::Cff => {
                    if let Some(cff) = font.charstrings(source) {
                        base = Base::Cff(cff, source);
                        break;
                    }
                }
                _ => {}
            }
        }
        // The components of a composite come from the base, so without one
        // there is nothing to draw.
        if matches!(base, Base::None) {
            varc = None;
        }
        Self { font, base, varc }
    }

    /// Returns the font that contains these outlines.
    pub fn font(&self) -> &'a Font {
        self.font
    }

    /// Returns the table that provides the outlines.
    ///
    /// This is [`OutlineSource::Varc`] if `VARC` composites are drawn. The
    /// composites cover only some glyphs, and the rest come from the
    /// [`underlying_source`](Self::underlying_source). Returns `None` if the
    /// font has no outlines from the requested sources.
    pub fn source(&self) -> Option<OutlineSource> {
        if self.varc.is_some() {
            return Some(OutlineSource::Varc);
        }
        self.underlying_source()
    }

    /// Returns the table that draws the glyphs `VARC` does not cover and the
    /// components of those it does.
    ///
    /// This is the same as [`source`](Self::source) unless `VARC`
    /// composites are drawn, and is never [`OutlineSource::Varc`]. Returns
    /// `None` if the font has no outlines from the requested sources.
    pub fn underlying_source(&self) -> Option<OutlineSource> {
        match self.base {
            Base::None => None,
            Base::Glyf(_) => Some(OutlineSource::Glyf),
            Base::Cff(_, source) => Some(source),
            Base::Type1(_) => Some(OutlineSource::Type1),
        }
    }

    /// Returns true if `VARC` composites are drawn.
    pub(crate) fn has_varc(&self) -> bool {
        self.varc.is_some()
    }

    /// Returns true if `glyph` is drawn as a `VARC` composite.
    pub(crate) fn varc_covers(&self, glyph: GlyphId) -> bool {
        self.varc.is_some_and(|varc| varc.covers(glyph))
    }

    /// Returns the `CFF` or `CFF2` font used to draw the base glyphs.
    ///
    /// Returns `None` if the base source is not `CFF` or `CFF2`.
    #[doc(hidden)]
    pub fn cff_font(&self) -> Option<&'a CffFontRef<'a>> {
        match self.base {
            Base::Cff(cff, _) => Some(cff),
            _ => None,
        }
    }

    /// Returns true if the font has no outlines from the requested sources.
    pub fn is_empty(&self) -> bool {
        matches!(self.base, Base::None)
    }

    /// Returns the number of glyphs in the font.
    ///
    /// Returns zero if the font has no outlines from the requested sources.
    pub fn len(&self) -> u32 {
        if self.is_empty() {
            0
        } else {
            self.font.num_glyphs()
        }
    }

    /// Returns the outline for `glyph`, or `None` if it has no outline or
    /// its data in the base source cannot be read.
    ///
    /// If `VARC` is enabled and the table covers the glyph, this returns a
    /// `VARC` composite. Otherwise, the outline comes from the base source.
    /// A composite that is malformed or nests past the recursion or edge
    /// limits returns an error when it is drawn.
    pub fn get(&self, glyph: GlyphId) -> Option<OutlineGlyph<'a>> {
        if glyph.to_u32() >= self.len() {
            return None;
        }
        if let Some(varc) = self.varc {
            if let Some(outline) = varc.outline(glyph) {
                return Some(OutlineGlyph::Varc(VarcGlyph {
                    outlines: varc,
                    outline,
                    base: *self,
                }));
            }
        }
        self.get_base(glyph)
    }

    /// Returns the outline for `glyph` from the base source only.
    fn get_base(&self, glyph: GlyphId) -> Option<OutlineGlyph<'a>> {
        match self.base {
            Base::None => None,
            Base::Glyf(context) => GlyfGlyph::new(context, glyph).map(OutlineGlyph::Glyf),
            Base::Cff(cff, source) => {
                let subfont_index = cff.subfont_index(glyph)?;
                let subfont = self.font.cff_subfont(source, subfont_index)?;
                Some(OutlineGlyph::Cff(CffGlyph {
                    cff,
                    coords: self.font.normalized_coords(),
                    glyph,
                    subfont_index,
                    subfont,
                    source,
                }))
            }
            Base::Type1(font) => Some(OutlineGlyph::Type1(Type1Glyph { font, glyph })),
        }
    }

    /// Returns an iterator over every glyph that has an outline, in glyph
    /// identifier order.
    pub fn iter(&self) -> impl Iterator<Item = (GlyphId, OutlineGlyph<'a>)> + 'a {
        let this = *self;
        (0..this.len())
            .map(GlyphId::new)
            .filter_map(move |glyph_id| this.get(glyph_id).map(|glyph| (glyph_id, glyph)))
    }

    /// Draws `glyph` from the base source at `coords`.
    ///
    /// This is used for the components of a composite. The `coords` may
    /// differ from the font's location.
    pub(super) fn draw_base(
        &self,
        glyph: GlyphId,
        coords: &[F2Dot14],
        options: &DrawOptions,
        precision: Precision,
        buf: &mut [u8],
        mut sink: &mut dyn PathSink,
    ) -> Result<(), Error> {
        let font = self.font;
        let at_own_location = same_location(coords, font.normalized_coords());
        match self.base {
            Base::None => Err(Error::MissingResource),
            Base::Glyf(context) => {
                if at_own_location {
                    let plan = OutlinePlan::new(&context, glyph)?;
                    draw_plan(&plan, &context, options, precision, buf, sink)?;
                } else {
                    let at = FontAt { context, coords };
                    let plan = OutlinePlan::new(&at, glyph)?;
                    draw_plan(&plan, &at, options, precision, buf, sink)?;
                }
                Ok(())
            }
            Base::Cff(cff, source) => {
                let index = cff.subfont_index(glyph).ok_or(Error::MissingResource)?;
                let subfont = if at_own_location {
                    font.cff_subfont(source, index).ok_or(Error::Malformed)?
                } else {
                    cff.subfont(index, coords)?
                };
                cff.draw(&subfont, glyph, coords, options.ppem, &mut sink)?;
                Ok(())
            }
            Base::Type1(type1) => {
                type1.draw(glyph, options.ppem, &mut sink)?;
                Ok(())
            }
        }
    }

    /// Returns the size in bytes of the working memory needed to draw
    /// `glyph` from the base source.
    pub(super) fn base_buffer_size(&self, glyph: GlyphId) -> usize {
        match self.base {
            Base::Glyf(context) => {
                let Ok(mut plan) = OutlinePlan::new(&context, glyph) else {
                    return 0;
                };
                // A component may be drawn away from the font's location, so
                // the deltas need room whenever the font has any.
                plan.applies_variations = context.has_gvar();
                plan_buffer_size(&plan)
            }
            _ => 0,
        }
    }
}

/// Returns true if two coordinate lists define the same location.
///
/// A missing axis is treated as its default value.
fn same_location(a: &[F2Dot14], b: &[F2Dot14]) -> bool {
    let (short, long) = if a.len() <= b.len() { (a, b) } else { (b, a) };
    short == &long[..short.len()] && long[short.len()..].iter().all(|c| *c == F2Dot14::ZERO)
}

/// A glyph outline from a specific source.
///
/// Each variant contains the data needed to draw that source: a `glyf`
/// plan, a CFF subfont, or a composite tree. To hint an outline, match on
/// the variant and use the source's own types.
#[derive(Clone)]
pub enum OutlineGlyph<'a> {
    /// A TrueType outline.
    Glyf(GlyfGlyph<'a>),
    /// A `CFF` or `CFF2` charstring.
    Cff(CffGlyph<'a>),
    /// A Type 1 charstring.
    Type1(Type1Glyph<'a>),
    /// A `VARC` composite.
    Varc(VarcGlyph<'a>),
}

impl OutlineGlyph<'_> {
    /// Returns the glyph identifier.
    pub fn glyph(&self) -> GlyphId {
        match self {
            Self::Glyf(glyph) => glyph.glyph(),
            Self::Cff(glyph) => glyph.glyph(),
            Self::Type1(glyph) => glyph.glyph(),
            Self::Varc(glyph) => glyph.glyph(),
        }
    }

    /// Returns the table that provides the outline.
    pub fn source(&self) -> OutlineSource {
        match self {
            Self::Glyf(_) => OutlineSource::Glyf,
            Self::Cff(glyph) => glyph.source(),
            Self::Type1(_) => OutlineSource::Type1,
            Self::Varc(_) => OutlineSource::Varc,
        }
    }

    /// Returns the size in bytes of the working memory needed by
    /// [`draw`](Self::draw).
    ///
    /// This is zero for charstrings.
    pub fn required_memory_size(&self) -> usize {
        match self {
            Self::Glyf(glyph) => glyph.required_memory_size(),
            Self::Cff(_) | Self::Type1(_) => 0,
            Self::Varc(glyph) => glyph.required_memory_size(),
        }
    }

    /// Draws the unhinted outline into `sink` with the given options.
    ///
    /// Supplied `memory` must hold at least
    /// [`required_memory_size`](Self::required_memory_size) bytes, or the draw
    /// fails with [`Error::InsufficientMemory`].
    ///
    /// Returns any metrics provided by the outline.
    pub fn draw(
        &self,
        options: &DrawOptions,
        memory: DrawMemory<'_>,
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, Error> {
        self.draw_dyn(options, Precision::Fixed, memory.into_buffer(), sink)
    }

    /// Implements [`draw`](Self::draw) with the given arithmetic precision.
    ///
    /// Takes a trait object so the sources are compiled once rather than
    /// once per sink type.
    fn draw_dyn(
        &self,
        options: &DrawOptions,
        precision: Precision,
        memory: Option<&mut [u8]>,
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, Error> {
        match self {
            Self::Cff(glyph) => glyph.draw(options, sink),
            Self::Type1(glyph) => glyph.draw(options, sink),
            Self::Glyf(glyph) => with_memory(
                memory,
                || glyph.required_memory_size(),
                |buf| glyph.draw(options, precision, buf, sink),
            ),
            Self::Varc(glyph) => with_memory(
                memory,
                || glyph.required_memory_size(),
                |buf| glyph.draw(options, precision, buf, sink),
            ),
        }
    }

    /// Returns the size in bytes of the working memory needed by
    /// [`draw_unscaled`](Self::draw_unscaled).
    pub fn required_memory_size_unscaled(&self) -> usize {
        match self {
            Self::Glyf(glyph) => glyph.required_memory_size_unscaled(),
            Self::Cff(_) | Self::Type1(_) => 0,
            Self::Varc(glyph) => glyph.required_memory_size(),
        }
    }

    /// Sends the outline's points to `sink` in font units.
    ///
    /// A `glyf` outline provides its points as stored, with variations
    /// applied. Other sources are drawn as a path in font units and converted
    /// to points. When a contour closes at its start point, that point is not
    /// repeated.
    ///
    /// `memory` works as in [`draw`](Self::draw), with a size given by
    /// [`required_memory_size_unscaled`](Self::required_memory_size_unscaled).
    ///
    /// A `VARC` composite has no advance of its own, so the returned metrics
    /// have none. Its advance is the one in the font's metrics tables, from
    /// [`GlyphMetrics::h_advance`](crate::model::metrics::GlyphMetrics::h_advance).
    pub fn draw_unscaled(
        &self,
        memory: DrawMemory<'_>,
        sink: &mut dyn UnscaledOutlineSink,
    ) -> Result<DrawnMetrics, Error> {
        self.draw_unscaled_dyn(memory.into_buffer(), sink)
    }

    fn draw_unscaled_dyn(
        &self,
        memory: Option<&mut [u8]>,
        sink: &mut dyn UnscaledOutlineSink,
    ) -> Result<DrawnMetrics, Error> {
        let options = DrawOptions::default();
        match self {
            Self::Glyf(glyph) => match memory {
                Some(buf) => glyph.draw_unscaled(buf, sink),
                None => with_scratch(glyph.required_memory_size_unscaled(), |buf| {
                    glyph.draw_unscaled(buf, sink)
                }),
            },
            Self::Cff(glyph) => {
                let mut pen = PointsFromPath::new(sink);
                let metrics = glyph.draw(&options, &mut pen)?;
                pen.finish();
                Ok(metrics)
            }
            Self::Type1(glyph) => {
                let mut pen = PointsFromPath::new(sink);
                let metrics = glyph.draw(&options, &mut pen)?;
                pen.finish();
                Ok(metrics)
            }
            Self::Varc(glyph) => {
                let mut pen = PointsFromPath::new(sink);
                let metrics = match memory {
                    Some(buf) => glyph.draw(&options, Precision::Fixed, buf, &mut pen)?,
                    None => with_scratch(glyph.required_memory_size(), |buf| {
                        glyph.draw(&options, Precision::Fixed, buf, &mut pen)
                    })?,
                };
                pen.finish();
                Ok(metrics)
            }
        }
    }

    /// Returns the extents of the outline in font units.
    ///
    /// The extents are computed from the control points, so a curve's
    /// control points are included rather than the curve itself. An empty
    /// glyph has zero extents.
    pub fn extents(&self) -> Result<GlyphExtents<f32>, Error> {
        Ok(self
            .control_box()?
            .map(GlyphExtents::from)
            .unwrap_or_default())
    }

    /// Returns the bounding box of the control points.
    ///
    /// This is used for clips that intersect boxes.
    pub(crate) fn control_box(&self) -> Result<Option<BoundingBox<f32>>, Error> {
        let mut pen = ControlBoundsSink::new();
        // HarfBuzz keeps every fraction of a varied point.
        self.draw_dyn(&DrawOptions::default(), Precision::Float, None, &mut pen)?;
        Ok(pen.bounding_box())
    }
}

/// Runs `draw` with the caller's `memory`, or with scratch memory of the
/// size `size` returns when the caller has none.
fn with_memory<R>(
    memory: Option<&mut [u8]>,
    size: impl FnOnce() -> usize,
    mut draw: impl FnMut(&mut [u8]) -> R,
) -> R {
    match memory {
        Some(buf) => draw(buf),
        None => with_scratch(size(), draw),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        model::glyph::outline::{NullSink, PathElement, SvgSink},
        FontRef, TableProvider,
    };
    use alloc::vec::Vec;

    fn font(data: &[u8]) -> Font {
        Font::new(data.to_vec(), 0).unwrap()
    }

    #[test]
    fn glyf_font_draws_from_glyf() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let glyphs = font.outline_glyphs();
        assert_eq!(glyphs.source(), Some(OutlineSource::Glyf));
        assert!(!glyphs.has_varc());
        let glyph = glyphs.get(GlyphId::new(1)).unwrap();
        assert!(matches!(glyph, OutlineGlyph::Glyf(_)));
        let mut pen = SvgSink::new();
        let metrics = glyph
            .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut pen)
            .unwrap();
        assert!(!pen.as_ref().is_empty());
        assert!(metrics.h_advance.is_some());
    }

    #[test]
    fn unscaled_fixed_and_float_agree_on_a_static_font() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let glyphs = font.outline_glyphs();
        for (glyph_id, glyph) in glyphs.iter() {
            let mut fixed = Vec::<PathElement>::new();
            glyph
                .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut fixed)
                .unwrap();
            let mut float = Vec::<PathElement>::new();
            glyph
                .draw_dyn(&DrawOptions::default(), Precision::Float, None, &mut float)
                .unwrap();
            assert_eq!(fixed, float, "glyph {glyph_id}");
        }
    }

    #[test]
    fn caller_memory_too_small_is_refused() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let glyph = font.outline_glyphs().get(GlyphId::new(1)).unwrap();
        let options = DrawOptions::default();
        let needed = glyph.required_memory_size();
        assert!(needed > 0);
        let mut small = alloc::vec![0u8; needed / 2];
        let mut pen = NullSink;
        assert!(matches!(
            glyph.draw(&options, DrawMemory::Buffer(&mut small), &mut pen),
            Err(Error::InsufficientMemory)
        ));
        let mut enough = alloc::vec![0u8; needed];
        glyph
            .draw(&options, DrawMemory::Buffer(&mut enough), &mut pen)
            .unwrap();
    }

    #[test]
    fn cff_font_draws_from_cff() {
        let font = font(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED);
        let glyphs = font.outline_glyphs();
        assert_eq!(glyphs.source(), Some(OutlineSource::Cff));
        let glyph = glyphs.get(GlyphId::new(1)).unwrap();
        let OutlineGlyph::Cff(cff) = &glyph else {
            panic!("expected a charstring");
        };
        assert_eq!(cff.source(), OutlineSource::Cff);
        assert_eq!(glyph.required_memory_size(), 0);
        let mut pen = SvgSink::new();
        let metrics = glyph
            .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut pen)
            .unwrap();
        assert!(!pen.as_ref().is_empty());
        // A CFF charstring states its width.
        assert!(metrics.h_advance.is_some());
    }

    // A standalone CFF font draws from its charstrings as an SFNT font draws
    // from its CFF table, and measures the same.
    #[test]
    fn standalone_cff_draws_like_its_table() {
        let sfnt = font(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED);
        let raw = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED)
            .unwrap()
            .cff()
            .unwrap()
            .offset_data()
            .as_bytes()
            .to_vec();
        let bare = Font::new(raw, 0).unwrap();
        let (theirs, ours) = (sfnt.outline_glyphs(), bare.outline_glyphs());
        assert_eq!(ours.source(), Some(OutlineSource::Cff));
        assert!(ours.cff_font().is_some());
        assert!(!ours.has_varc());
        assert_eq!(ours.len(), theirs.len());
        // An order without CFF finds nothing to draw.
        assert!(bare
            .outline_glyphs_with(GlyphSourceOrder::new(&[GlyphSource::Glyf]))
            .is_empty());
        let (their_metrics, our_metrics) = (sfnt.glyph_metrics(), bare.glyph_metrics());
        let mut drawn = 0;
        for (glyph, outline) in theirs.iter() {
            let mine = ours.get(glyph).unwrap();
            assert_eq!(mine.source(), OutlineSource::Cff);
            for ppem in [None, Some(16.0)] {
                let options = DrawOptions::default().with_ppem(ppem);
                let (mut expected, mut actual) = (SvgSink::new(), SvgSink::new());
                let want = outline
                    .draw(&options, DrawMemory::Scratch, &mut expected)
                    .unwrap();
                let got = mine
                    .draw(&options, DrawMemory::Scratch, &mut actual)
                    .unwrap();
                assert_eq!(
                    actual.as_ref(),
                    expected.as_ref(),
                    "glyph {glyph} at {ppem:?}"
                );
                assert_eq!(got, want, "glyph {glyph} at {ppem:?}");
            }
            assert_eq!(
                mine.extents().unwrap(),
                outline.extents().unwrap(),
                "glyph {glyph}"
            );
            assert_eq!(
                our_metrics.extents_exact(glyph),
                their_metrics.extents_exact(glyph),
                "glyph {glyph}"
            );
            drawn += 1;
        }
        assert!(drawn > 0);
    }

    #[test]
    fn order_without_the_fonts_format_is_empty() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let glyphs = font.outline_glyphs_with(GlyphSourceOrder::new(&[GlyphSource::Cff]));
        assert!(glyphs.is_empty());
        assert_eq!(glyphs.len(), 0);
        assert!(glyphs.get(GlyphId::new(1)).is_none());
        assert_eq!(glyphs.iter().count(), 0);
    }

    #[test]
    fn varc_covers_its_glyphs_and_falls_back_to_the_base() {
        let font = font(font_test_data::varc::CJK_6868);
        let glyphs = font.outline_glyphs();
        assert!(glyphs.has_varc());
        assert_eq!(glyphs.source(), Some(OutlineSource::Varc));
        assert_eq!(glyphs.underlying_source(), Some(OutlineSource::Glyf));
        let gid = font.charmap().map_unicode(0x6868_u32).unwrap();
        let glyph = glyphs.get(gid).unwrap();
        assert_eq!(glyph.source(), OutlineSource::Varc);
        // Glyph zero is a plain outline.
        assert_eq!(
            glyphs.get(GlyphId::new(0)).unwrap().source(),
            OutlineSource::Glyf
        );
        // Without VARC in the order, the composite's base glyph is drawn.
        let base_only = font.outline_glyphs_with(GlyphSourceOrder::BASE_OUTLINES);
        assert!(!base_only.has_varc());
        assert_eq!(base_only.source(), Some(OutlineSource::Glyf));
        assert_eq!(base_only.underlying_source(), Some(OutlineSource::Glyf));
        assert_eq!(base_only.get(gid).unwrap().source(), OutlineSource::Glyf);
    }

    #[test]
    fn varc_draws_in_the_memory_it_was_sized_for() {
        let font = font(font_test_data::varc::CJK_6868);
        let glyphs = font.outline_glyphs();
        let mut composites = 0;
        for (glyph_id, glyph) in glyphs.iter() {
            let OutlineGlyph::Varc(_) = &glyph else {
                continue;
            };
            composites += 1;
            let needed = glyph.required_memory_size();
            assert!(needed > 0, "glyph {glyph_id}");
            let options = DrawOptions::default();
            let mut enough = alloc::vec![0u8; needed];
            glyph
                .draw(&options, DrawMemory::Buffer(&mut enough), &mut NullSink)
                .unwrap_or_else(|error| panic!("glyph {glyph_id}: {error}"));
            // Half the block is short for the walk or for a leaf, and either
            // is refused rather than drawn into.
            let mut short = alloc::vec![0u8; needed / 2];
            assert!(
                matches!(
                    glyph.draw(&options, DrawMemory::Buffer(&mut short), &mut NullSink),
                    Err(Error::InsufficientMemory)
                ),
                "glyph {glyph_id}"
            );
        }
        assert!(composites > 0);
    }

    #[test]
    fn varc_draws_the_freetype_path_head() {
        let font = font(font_test_data::varc::CJK_6868);
        let gid = font.charmap().map_unicode(0x6868_u32).unwrap();
        let glyph = font.outline_glyphs().get(gid).unwrap();
        let mut pen = Vec::<PathElement>::new();
        glyph
            .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut pen)
            .unwrap();
        let head: Vec<_> = pen
            .iter()
            .take(3)
            .map(|el| match *el {
                PathElement::MoveTo { x, y } => alloc::format!("M{x:.2},{y:.2}"),
                PathElement::QuadTo { cx0, cy0, x, y } => {
                    alloc::format!("Q{cx0:.2},{cy0:.2} {x:.2},{y:.2}")
                }
                _ => panic!("unexpected element"),
            })
            .collect();
        assert_eq!(
            head,
            [
                "M454.56,574.77",
                "Q477.58,585.56 499.80,598.25",
                "Q522.02,610.95 543.36,625.16"
            ]
        );
    }

    #[test]
    fn varc_at_a_location_draws_with_its_deltas() {
        let font = font(font_test_data::varc::DELTA_PRECISION);
        let instance = font
            .instance_builder()
            .normalized_coords([F2Dot14::ONE, F2Dot14::ZERO])
            .build();
        let glyphs = instance.outline_glyphs();
        for (gid, expected) in [
            (2, "M600,0 L800,0 L600,200 Z"),
            (3, "M664,0 L864,0 L664,200 Z"),
            (4, "M101,0 L201,0 L101,100 Z"),
        ] {
            let glyph = glyphs.get(GlyphId::new(gid)).unwrap();
            let mut pen = SvgSink::new();
            glyph
                .draw(&DrawOptions::default(), DrawMemory::Scratch, &mut pen)
                .unwrap();
            assert_eq!(pen.as_ref(), expected, "glyph {gid}");
        }
    }

    #[test]
    fn varc_without_a_variation_store_does_not_panic() {
        use crate::types::Tag;

        let mut bytes = font_test_data::varc::CJK_6868.to_vec();
        let varc_off = {
            let font = FontRef::new(&bytes).unwrap();
            font.table_directory()
                .table_records()
                .iter()
                .find(|rec| rec.tag() == Tag::new(b"VARC"))
                .expect("VARC table present")
                .offset() as usize
        };
        // Null the nullable multi_var_store_offset.
        for b in &mut bytes[varc_off + 8..varc_off + 12] {
            *b = 0;
        }
        let font = font(&bytes);
        let gid = font.charmap().map_unicode(0x6868_u32).unwrap();
        let glyph = font.outline_glyphs().get(gid).expect("covered VARC glyph");
        let mut pen = NullSink;
        // Must complete without panicking (Ok, or a graceful error).
        let _ = glyph.draw(&DrawOptions::default(), DrawMemory::Scratch, &mut pen);
    }

    #[test]
    fn extents_of_a_glyf_glyph_match_its_header() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let (glyf, loca) = font.selected_glyf_loca().unwrap();
        let glyphs = font.outline_glyphs();
        let mut checked = 0;
        for (gid, glyph) in glyphs.iter() {
            let Some(header) = loca.get_glyf(gid, glyf).unwrap() else {
                continue;
            };
            if header.number_of_contours() == 0 {
                continue;
            }
            let extents = glyph.extents().unwrap();
            assert_eq!(extents.x_bearing, header.x_min() as f32);
            assert_eq!(extents.y_bearing, header.y_max() as f32);
            assert_eq!(extents.width, (header.x_max() - header.x_min()) as f32);
            assert_eq!(extents.height, (header.y_max() - header.y_min()) as f32);
            checked += 1;
        }
        assert!(checked > 0);
    }

    #[test]
    fn glyph_past_the_font_is_none() {
        let font = font(font_test_data::VAZIRMATN_VAR);
        let glyphs = font.outline_glyphs();
        assert!(glyphs.get(GlyphId::new(glyphs.len())).is_none());
        let font = font_ref_glyph_count(font_test_data::VAZIRMATN_VAR);
        assert_eq!(glyphs.len(), font);
    }

    fn font_ref_glyph_count(data: &[u8]) -> u32 {
        FontRef::new(data).unwrap().maxp().unwrap().num_glyphs() as u32
    }
}
