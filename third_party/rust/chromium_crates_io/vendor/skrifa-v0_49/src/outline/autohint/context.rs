//! The font data that the autohinter reads.

use super::{
    super::{
        unscaled::UnscaledOutlineSink, DrawError, OutlineGlyph as FontRefGlyph,
        OutlineGlyphCollection,
    },
    metrics::BlueOutlineBuf,
    outline::Outline,
    QuirksMode,
};
use crate::{
    attribute::Style, charmap::Charmap, font::Font, instance::LocationRef, metrics::GlyphMetrics,
    prelude::Size, FontRef, MetadataProvider,
};
use core::cell::OnceCell;
use raw::{
    model::{
        charmap::Charmap as FontCharmap,
        glyph::outline::{OutlineGlyph, OutlineGlyphs, OutlineSource},
    },
    tables::{gdef::Gdef, gsub::Gsub},
    types::{F2Dot14, GlyphId},
    TableProvider,
};

/// Provides the font data the autohinter reads at one location in the
/// variation space.
///
/// The autohinter holds this as a trait object so that a single copy of the
/// autohinter serves every kind of font.
pub(crate) trait AutohintContext {
    /// Returns the normalized variation coordinates of the location.
    fn coords(&self) -> &[F2Dot14];

    /// Returns the number of glyphs that can be assigned a style.
    fn glyph_count(&self) -> u32;

    fn units_per_em(&self) -> i32;

    fn style(&self) -> Style;

    fn is_fixed_pitch(&self) -> bool;

    /// Returns true if the character map is a symbol map, which yields no
    /// metrics.
    fn is_symbol(&self) -> bool;

    fn map(&self, ch: u32) -> Option<GlyphId>;

    /// Calls `f` with each character and its glyph, in character order.
    fn for_each_mapping(&self, f: &mut dyn FnMut(u32, GlyphId));

    /// Returns the substitution table, which is read only when shaping uses
    /// it.
    fn gsub(&self) -> Option<Gsub<'_>>;

    /// Returns the glyph definition table, which provides the variation
    /// store.
    fn gdef(&self) -> Option<Gdef<'_>>;

    /// Returns the unscaled advance width of `glyph`.
    fn advance_width(&self, glyph: GlyphId) -> Option<f32>;

    /// Fills `outline` with `glyph` and analyzes it.
    ///
    /// Returns whether the glyph is flagged as having overlapping contours or
    /// components, or `None` if there is no outline for `glyph`.
    fn fill_outline(
        &self,
        glyph: GlyphId,
        outline: &mut Outline,
        quirks: QuirksMode,
    ) -> Option<Result<bool, DrawError>>;

    /// Draws the unscaled points of `glyph` into `buf`, or returns `None` if
    /// there is no outline for `glyph`.
    fn draw_unscaled(
        &self,
        glyph: GlyphId,
        buf: &mut BlueOutlineBuf,
    ) -> Option<Result<i32, DrawError>>;
}

/// An autohinting context for a [`FontRef`].
///
/// Data other than the glyph being drawn is read on first use. A draw of one
/// glyph rarely needs the character map or the other outlines.
pub(crate) struct FontRefContext<'a> {
    font: FontRef<'a>,
    coords: &'a [F2Dot14],
    charmap: OnceCell<Charmap<'a>>,
    /// The glyph being drawn, already resolved by the caller.
    drawn: Option<&'a FontRefGlyph<'a>>,
    /// Outlines loaded by the caller, used in place of `outlines`.
    borrowed_outlines: Option<&'a OutlineGlyphCollection<'a>>,
    outlines: OnceCell<OutlineGlyphCollection<'a>>,
    glyph_metrics: OnceCell<GlyphMetrics<'a>>,
}

impl<'a> FontRefContext<'a> {
    pub fn new(font: &FontRef<'a>, coords: &'a [F2Dot14]) -> Self {
        Self {
            font: font.clone(),
            coords,
            charmap: OnceCell::new(),
            drawn: None,
            borrowed_outlines: None,
            outlines: OnceCell::new(),
            glyph_metrics: OnceCell::new(),
        }
    }

    /// Creates a context for drawing `glyph`.
    ///
    /// The caller has already resolved `glyph`, so it is not looked up again.
    pub fn for_glyph(glyph: &'a FontRefGlyph<'a>, coords: &'a [F2Dot14]) -> Self {
        Self {
            drawn: Some(glyph),
            ..Self::new(glyph.font(), coords)
        }
    }

    /// Creates a context that reuses outlines the caller has already loaded.
    pub fn with_outlines(
        font: &FontRef<'a>,
        outlines: &'a OutlineGlyphCollection<'a>,
        coords: &'a [F2Dot14],
    ) -> Self {
        Self {
            borrowed_outlines: Some(outlines),
            ..Self::new(font, coords)
        }
    }

    fn charmap(&self) -> &Charmap<'a> {
        self.charmap.get_or_init(|| self.font.charmap())
    }

    fn outlines(&self) -> &OutlineGlyphCollection<'a> {
        match self.borrowed_outlines {
            Some(outlines) => outlines,
            None => self.outlines.get_or_init(|| self.font.outline_glyphs()),
        }
    }

    /// Resolves `glyph`, using the glyph being drawn if it matches and the
    /// collection otherwise.
    fn resolve(&self, glyph: GlyphId) -> Option<FontRefGlyph<'a>> {
        match self.drawn {
            Some(drawn) if drawn.glyph_id() == glyph => Some(drawn.clone()),
            _ => self.outlines().get(glyph),
        }
    }
}

impl AutohintContext for FontRefContext<'_> {
    fn coords(&self) -> &[F2Dot14] {
        self.coords
    }

    fn glyph_count(&self) -> u32 {
        // The style table is sized by the glyph count, which a font can
        // overstate; it never needs more entries than there are outlines.
        self.font
            .selected_maxp()
            .map(|maxp| maxp.num_glyphs())
            .unwrap_or_default()
            .min(self.outlines().stored_glyph_count())
    }

    fn units_per_em(&self) -> i32 {
        self.font
            .head()
            .map(|head| head.units_per_em() as i32)
            .unwrap_or_default()
    }

    fn style(&self) -> Style {
        self.font.attributes().style
    }

    fn is_fixed_pitch(&self) -> bool {
        self.font
            .post()
            .map(|post| post.is_fixed_pitch() != 0)
            .unwrap_or_default()
    }

    fn is_symbol(&self) -> bool {
        self.charmap().is_symbol()
    }

    fn map(&self, ch: u32) -> Option<GlyphId> {
        self.charmap().map(ch)
    }

    fn for_each_mapping(&self, f: &mut dyn FnMut(u32, GlyphId)) {
        for (ch, glyph) in self.charmap().mappings() {
            f(ch, glyph);
        }
    }

    fn gsub(&self) -> Option<Gsub<'_>> {
        self.font.gsub().ok()
    }

    fn gdef(&self) -> Option<Gdef<'_>> {
        self.font.gdef().ok()
    }

    fn advance_width(&self, glyph: GlyphId) -> Option<f32> {
        self.glyph_metrics
            .get_or_init(|| {
                self.font
                    .glyph_metrics(Size::unscaled(), LocationRef::new(self.coords))
            })
            .advance_width(glyph)
    }

    fn fill_outline(
        &self,
        glyph: GlyphId,
        outline: &mut Outline,
        quirks: QuirksMode,
    ) -> Option<Result<bool, DrawError>> {
        let glyph = self.resolve(glyph)?;
        Some(
            outline
                .fill(&glyph, self.coords, quirks)
                .map(|_| glyph.has_overlaps().unwrap_or_default()),
        )
    }

    fn draw_unscaled(
        &self,
        glyph: GlyphId,
        buf: &mut BlueOutlineBuf,
    ) -> Option<Result<i32, DrawError>> {
        let glyph = self.resolve(glyph)?;
        Some(glyph.draw_unscaled(self.coords, None, buf))
    }
}

/// An autohinting context for a [`Font`] at the font's location.
pub(crate) struct FontContext<'a> {
    font: &'a Font,
    charmap: FontCharmap<'a>,
    outlines: OutlineGlyphs<'a>,
    /// The glyph being drawn, already resolved by the caller.
    drawn: Option<&'a OutlineGlyph<'a>>,
}

impl<'a> FontContext<'a> {
    /// Creates a context that reads glyphs from `outlines`.
    pub fn new(font: &'a Font, outlines: OutlineGlyphs<'a>) -> Self {
        Self {
            font,
            charmap: font.charmap(),
            outlines,
            drawn: None,
        }
    }

    /// Creates a context for drawing `outline`, one of `outlines`.
    ///
    /// The caller has already resolved `outline`, so it is not looked up
    /// again.
    pub fn for_glyph(
        font: &'a Font,
        outlines: OutlineGlyphs<'a>,
        outline: &'a OutlineGlyph<'a>,
    ) -> Self {
        Self {
            drawn: Some(outline),
            ..Self::new(font, outlines)
        }
    }

    /// Resolves `glyph`, using the glyph being drawn if it matches and the
    /// collection otherwise.
    fn resolve(&self, glyph: GlyphId) -> Option<OutlineGlyph<'a>> {
        match self.drawn {
            Some(drawn) if drawn.glyph() == glyph => Some(drawn.clone()),
            _ => self.outlines.get(glyph),
        }
    }

    /// Writes `outline` into `sink` in font units and returns its advance.
    ///
    /// A `glyf` outline provides its advance through its phantom points, with
    /// variations applied. The other formats read it from the metrics
    /// tables.
    fn draw_unscaled_into(
        &self,
        outline: &OutlineGlyph<'_>,
        sink: &mut impl UnscaledOutlineSink,
    ) -> Result<i32, DrawError> {
        let glyph = outline.glyph();
        let metrics = outline
            .draw_unscaled(raw::model::glyph::outline::DrawMemory::Scratch, sink)
            .map_err(|error| DrawError::from_outline_error(error, glyph))?;
        Ok(match outline {
            OutlineGlyph::Glyf(_) => metrics.h_advance.unwrap_or_default() as i32,
            // A location can add a fraction, which FreeType rounds to a
            // whole unit.
            _ => self
                .font
                .glyph_metrics()
                .h_advance_exact(glyph)
                .round()
                .to_i32(),
        })
    }
}

impl AutohintContext for FontContext<'_> {
    fn coords(&self) -> &[F2Dot14] {
        self.font.normalized_coords()
    }

    fn glyph_count(&self) -> u32 {
        // The style table is sized by the glyph count, which a font can
        // overstate; it never needs more entries than there are outlines.
        let stored = match self.outlines.underlying_source() {
            Some(OutlineSource::Glyf) => self
                .font
                .tables()
                .selected_glyf_loca(None)
                .map_or(0, |(_, loca)| u32::try_from(loca.len()).unwrap_or(u32::MAX)),
            Some(OutlineSource::Cff2 | OutlineSource::Cff) => {
                self.outlines.cff_font().map_or(0, |cff| cff.num_glyphs())
            }
            _ => u32::MAX,
        };
        self.outlines.len().min(stored)
    }

    fn units_per_em(&self) -> i32 {
        self.font.units_per_em() as i32
    }

    fn style(&self) -> Style {
        // From the model, which reads a PostScript font's own dictionary
        // where an SFNT font has OS/2 and post.
        match self.font.default_attributes().style {
            raw::model::attribute::Style::Normal => Style::Normal,
            raw::model::attribute::Style::Italic => Style::Italic,
            raw::model::attribute::Style::Oblique(angle) => Style::Oblique(angle),
        }
    }

    fn is_fixed_pitch(&self) -> bool {
        self.font.style_metrics().is_fixed_pitch.unwrap_or_default()
    }

    fn is_symbol(&self) -> bool {
        self.charmap.unicode_is_symbol()
    }

    fn map(&self, ch: u32) -> Option<GlyphId> {
        self.charmap.map_unicode(ch)
    }

    fn for_each_mapping(&self, f: &mut dyn FnMut(u32, GlyphId)) {
        for (ch, glyph) in self.charmap.unicodes() {
            f(ch, glyph);
        }
    }

    fn gsub(&self) -> Option<Gsub<'_>> {
        self.font.tables().gsub().ok()
    }

    fn gdef(&self) -> Option<Gdef<'_>> {
        self.font.tables().gdef().ok()
    }

    fn advance_width(&self, glyph: GlyphId) -> Option<f32> {
        Some(self.font.glyph_metrics().h_advance(glyph))
    }

    fn fill_outline(
        &self,
        glyph: GlyphId,
        outline: &mut Outline,
        quirks: QuirksMode,
    ) -> Option<Result<bool, DrawError>> {
        let source = self.resolve(glyph)?;
        let has_overlaps =
            matches!(&source, OutlineGlyph::Glyf(glyph) if glyph.plan().has_overlaps);
        Some(
            outline
                .fill_with(glyph, self.units_per_em(), quirks, |outline| {
                    self.draw_unscaled_into(&source, outline)
                })
                .map(|_| has_overlaps),
        )
    }

    fn draw_unscaled(
        &self,
        glyph: GlyphId,
        buf: &mut BlueOutlineBuf,
    ) -> Option<Result<i32, DrawError>> {
        let source = self.resolve(glyph)?;
        Some(self.draw_unscaled_into(&source, buf))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::sync::Arc;
    use raw::{model::Blob, types::Tag};

    // The style table is bounded by the outlines a font stores, not the
    // count its maximum profile declares.
    #[test]
    fn the_glyph_count_is_bounded_by_stored_outlines() {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(|tag| {
            if tag == Tag::new(b"MAXP") {
                return Some(Blob::from(font_test_data::extended::maxp(0xFFFFFF, true)));
            }
            let base = FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).ok()?;
            base.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let context = FontContext::new(&font, font.outline_glyphs());
        let base = FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).unwrap();
        let stored = base.outline_glyphs().stored_glyph_count();
        assert!(context.glyph_count() <= stored);
    }

    /// Returns the Type 1 test font with its dictionary edited by `edit`.
    fn type1_font(edit: impl Fn(&str) -> alloc::string::String) -> Font {
        let source =
            core::str::from_utf8(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        Font::new(edit(source).into_bytes(), 0).unwrap()
    }

    // A PostScript font's own dictionary provides its style and pitch.
    #[test]
    fn a_type1_font_reports_its_italic_style_and_fixed_pitch() {
        let font = type1_font(|source| {
            source
                .replace("/ItalicAngle 0", "/ItalicAngle -12")
                .replace("/isFixedPitch false", "/isFixedPitch true")
        });
        let context = FontContext::new(&font, font.outline_glyphs());
        assert_eq!(context.style(), Style::Italic);
        assert!(context.is_fixed_pitch());
        let upright = type1_font(|source| source.into());
        let context = FontContext::new(&upright, upright.outline_glyphs());
        assert_eq!(context.style(), Style::Normal);
        assert!(!context.is_fixed_pitch());
    }
}
