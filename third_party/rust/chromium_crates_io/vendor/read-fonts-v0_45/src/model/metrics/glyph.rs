//! Per-glyph metrics.

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;

use crate::{
    mem::with_scratch,
    model::{
        metrics::{LineExtents, Metrics, Scale, ScaledGlyphMetrics},
        Font, Kind,
    },
    ps::{
        cff::{CffFontRef, Subfont},
        cs::CommandSink,
        type1::Type1Font,
    },
    tables::{
        glyf::outline::{Buffers, OutlineContext, OutlinePlan, ScaleF32 as OutlineScaleF32},
        hmtx::LongMetric,
    },
    TableProvider,
};
use types::{BigEndian, BoundingBox, F2Dot14, F48Dot16, Fixed, GlyphId, Point};

/// Where a glyph's ink sits, relative to its origin.
///
/// A bearing and a size rather than two corners, which is the shape the
/// metrics built on it use.
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct GlyphExtents<T> {
    /// Distance from the origin to the left edge of the ink.
    pub x_bearing: T,
    /// Distance from the origin to the top edge of the ink.
    pub y_bearing: T,
    /// Width of the ink, rightward from `x_bearing`.
    pub width: T,
    /// Height of the ink, downward from `y_bearing`.
    ///
    /// Never negative. A caller whose own extents count upward negates it.
    pub height: T,
}

/// Measurements of individual glyphs at one location.
///
/// Cheap to obtain, so it can be taken per query rather than held.
#[derive(Clone, Copy)]
pub struct GlyphMetrics<'a> {
    h_metrics: &'a RawGlyphMetrics<'a>,
    font: &'a Font,
    global: &'a Metrics,
    coords: &'a [F2Dot14],
    num_glyphs: u32,
    units_per_em: u16,
}

impl<'a> GlyphMetrics<'a> {
    /// Binds a font to the location its glyphs are measured at.
    #[inline]
    pub(crate) fn new(font: &'a Font, global: &'a Metrics, coords: &'a [F2Dot14]) -> Self {
        Self {
            h_metrics: font.h_metrics(),
            font,
            global,
            coords,
            num_glyphs: global.num_glyphs,
            units_per_em: global.units_per_em,
        }
    }

    /// Returns these metrics in the units `scale` describes.
    #[inline]
    pub fn scaled<S: Scale>(self, scale: S) -> ScaledGlyphMetrics<'a, 'static, S> {
        ScaledGlyphMetrics::new(self, scale)
    }

    /// Returns the advance width of `glyph`, in design units.
    ///
    /// See [`h_advance_exact`](Self::h_advance_exact) for the width before
    /// it is narrowed to `f32`.
    #[inline]
    pub fn h_advance(&self, glyph: GlyphId) -> f32 {
        self.h_advance_exact(glyph).to_f32()
    }

    /// Returns the exact advance width of `glyph`, in design units.
    ///
    /// `hmtx` states a whole number of units and a location adds a fraction,
    /// so the sum carries one. How to round it is the caller's to decide, as
    /// is what to do with a location that carries an advance below zero.
    #[inline]
    pub fn h_advance_exact(&self, glyph: GlyphId) -> F48Dot16 {
        let mut width = F48Dot16::ZERO;
        self.h_advance_batched(|value| value, core::iter::once((glyph, &mut width)));
        width
    }

    /// Writes the advance width of each glyph to its slot, in order.
    ///
    /// Each width passes through `convert`, so a caller working in another
    /// number type writes into that type directly.
    #[inline]
    pub fn h_advance_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        let raw = self.h_metrics;
        if raw.is_empty() {
            // A Type 1 font states its widths in the charstrings rather than
            // a table, so it reaches here and is measured another way. The
            // test sits inside this branch so that a font with `hmtx`, which
            // is nearly all of them, never makes it.
            if let Kind::Type1(font) = self.font.kind() {
                return self.h_advance_batched_type1(font, convert, glyphs);
            }
            // Otherwise the font states no widths at all, and every glyph
            // gets half an em; no location can move that.
            let half_em = F48Dot16::from_i32(self.units_per_em as i32 / 2);
            for (_, out) in glyphs {
                *out = convert(half_em);
            }
            return;
        }
        let coords = self.coords;
        if coords.is_empty() {
            return raw.run(self.num_glyphs, convert, glyphs);
        }
        self.h_advance_batched_varied(raw, coords, convert, glyphs)
    }

    /// Returns the advance height of `glyph`, in design units.
    ///
    /// See [`v_advance_exact`](Self::v_advance_exact) for the height before
    /// it is narrowed to `f32`.
    #[inline]
    pub fn v_advance(&self, glyph: GlyphId) -> f32 {
        self.v_advance_exact(glyph).to_f32()
    }

    /// Returns the exact advance height of `glyph`, in design units.
    ///
    /// The height is positive, as the font states it. A caller whose axis
    /// runs down the page negates it.
    #[inline]
    pub fn v_advance_exact(&self, glyph: GlyphId) -> F48Dot16 {
        let mut height = F48Dot16::ZERO;
        self.v_advance_batched(|value| value, core::iter::once((glyph, &mut height)));
        height
    }

    /// Writes the advance height of each glyph to its slot, in order.
    ///
    /// Each height passes through `convert`, so a caller working in another
    /// number type writes into that type directly.
    #[inline]
    pub fn v_advance_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        let raw = self.font.v_metrics();
        if raw.is_empty() {
            // Such a font stacks its glyphs by the line it lays horizontal
            // text on, so every glyph advances the same and no location
            // moves it.
            let height = self.line_height();
            for (_, out) in glyphs {
                *out = convert(height);
            }
            return;
        }
        let coords = self.coords;
        if coords.is_empty() {
            return raw.run(self.num_glyphs, convert, glyphs);
        }
        self.v_advance_batched_varied(raw, coords, convert, glyphs)
    }

    /// Returns the y coordinate of `glyph`'s vertical origin, in design units.
    ///
    /// The x coordinate is half its advance width. See
    /// [`v_origin_y_exact`](Self::v_origin_y_exact) for the value before it is
    /// narrowed to `f32`.
    #[inline]
    pub fn v_origin_y(&self, glyph: GlyphId) -> f32 {
        self.v_origin_y_exact(glyph).to_f32()
    }

    /// Returns the exact y coordinate of `glyph`'s vertical origin.
    #[inline]
    pub fn v_origin_y_exact(&self, glyph: GlyphId) -> F48Dot16 {
        let mut origin = F48Dot16::ZERO;
        self.v_origin_y_batched(|value| value, core::iter::once((glyph, &mut origin)));
        origin
    }

    /// Writes the vertical origin of each glyph in design units to its slot.
    ///
    /// `VORG` answers first, followed by the glyf top phantom point. Other
    /// glyphs are centered in the horizontal line by their extents, or
    /// placed on the ascender when extents are unavailable.
    pub fn v_origin_y_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        let mut line = None;
        self.v_origins_batched(
            |glyph, origin| {
                convert(origin.unwrap_or_else(|| {
                    self.origin_against_ink(glyph, *line.get_or_insert_with(|| self.origin_line()))
                }))
            },
            |glyphs| {
                let line = self.origin_line();
                self.extents_batched(
                    |extents| convert(Self::origin_from_extents(extents, line)),
                    glyphs,
                );
            },
            glyphs,
        );
    }

    /// Returns where `glyph`'s ink sits, in design units.
    ///
    /// An empty glyph has zero extents. `None` means the glyph is unavailable
    /// or its extents could not be read.
    ///
    /// See [`extents_exact`](Self::extents_exact) for the values before they
    /// are narrowed to `f32`.
    #[inline]
    pub fn extents(&self, glyph: GlyphId) -> Option<GlyphExtents<f32>> {
        self.extents_exact(glyph).map(|extents| GlyphExtents {
            x_bearing: extents.x_bearing.to_f32(),
            y_bearing: extents.y_bearing.to_f32(),
            width: extents.width.to_f32(),
            height: extents.height.to_f32(),
        })
    }

    /// Returns the exact extents of `glyph` in design units.
    ///
    /// See [`extents_batched`](Self::extents_batched) to compute several
    /// glyphs' extents with one choice of source for the batch.
    #[inline]
    pub fn extents_exact(&self, glyph: GlyphId) -> Option<GlyphExtents<F48Dot16>> {
        let mut extents = None;
        self.extents_batched(|value| value, core::iter::once((glyph, &mut extents)));
        extents
    }

    /// Writes where each glyph's ink sits to its slot, in order.
    ///
    /// Each measurement passes through `convert`, so a caller working in
    /// another number type writes into that type directly.
    ///
    /// The source is settled once for the batch, since which table answers
    /// depends on the font rather than on any one glyph.
    pub fn extents_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(Option<GlyphExtents<F48Dot16>>) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        if let Some((glyf, loca)) = self.font.glyf_loca() {
            if self.coords.is_empty() {
                // Every glyph states its own box, so nothing has to be
                // loaded to find it.
                for (glyph, out) in glyphs {
                    if glyph.to_u32() >= self.num_glyphs {
                        *out = convert(None);
                        continue;
                    }
                    *out = convert(loca.get_glyf(glyph, glyf).ok().map(|outline| {
                        let Some(outline) = outline else {
                            return GlyphExtents::default();
                        };
                        if outline.number_of_contours() == 0 {
                            return GlyphExtents::default();
                        }
                        let mut extents = extents_from_corners(
                            F48Dot16::from_i32(outline.x_min() as i32),
                            F48Dot16::from_i32(outline.y_min() as i32),
                            F48Dot16::from_i32(outline.x_max() as i32),
                            F48Dot16::from_i32(outline.y_max() as i32),
                        );
                        extents.x_bearing = self
                            .h_metrics
                            .stated_side_bearing(glyph)
                            .unwrap_or(F48Dot16::ZERO);
                        extents
                    }));
                }
                return;
            }
            return self.extents_batched_glyf_varied(convert, glyphs);
        }
        if let Some(cff) = self.font.cff() {
            return self.extents_batched_cff(cff, convert, glyphs);
        }
        if let Kind::Type1(font) = self.font.kind() {
            return self.extents_batched_type1(font, convert, glyphs);
        }
        // A font stating its outlines in a way none of the above reads.
        for (_, out) in glyphs {
            *out = convert(None);
        }
    }

    /// Returns the line this font's glyphs stack by, where it provides one.
    #[inline]
    pub(crate) fn line_extents(&self) -> Option<LineExtents<F48Dot16>> {
        self.global.h_line().map(|line| line.extents())
    }

    /// The vertical origin `VORG` states for `glyph`, in design units.
    ///
    /// `None` where the font has no `VORG`, which is every font that states
    /// its outlines in `glyf`.
    #[cfg(test)]
    pub(crate) fn stated_v_origin(&self, glyph: GlyphId) -> Option<F48Dot16> {
        let vorg = self.font.vorg()?;
        let origin = F48Dot16::from_i32(vorg.vertical_origin_y(glyph) as i32);
        let delta = if self.coords.is_empty() {
            F48Dot16::ZERO
        } else {
            self.font
                .vvar()
                .and_then(|vvar| vvar.v_origin_y_delta(glyph, self.coords))
                .unwrap_or(F48Dot16::ZERO)
        };
        Some(origin.saturating_add(delta))
    }

    /// The top side bearing `vmtx` states for `glyph`, before any location.
    ///
    /// `None` for a font that states none and past the last glyph one covers.
    #[cfg(test)]
    pub(crate) fn stated_v_bearing(&self, glyph: GlyphId) -> Option<F48Dot16> {
        self.font.v_metrics().stated_side_bearing(glyph)
    }

    /// The top phantom point of a glyf glyph with its vertical metrics.
    /// Loading the outline also handles composite `USE_MY_METRICS` and gvar.
    #[cfg(test)]
    pub(crate) fn glyf_v_origin(&self, glyph: GlyphId) -> Option<F48Dot16> {
        if self.font.v_metrics().is_empty() || self.font.glyf_loca().is_none() {
            return None;
        }
        let context = self.font;
        glyf_v_origin_from_context(
            &context,
            &OutlineScaleF32::new(None, self.units_per_em),
            glyph,
        )
    }

    /// Selects the source of vertical origins once for the whole run.
    ///
    /// `fallback` receives the whole run when neither table answers. A
    /// malformed glyf glyph can still reach `convert` with `None`.
    pub(crate) fn v_origins_batched<'o, V: 'o, I>(
        &self,
        mut convert: impl FnMut(GlyphId, Option<F48Dot16>) -> V,
        fallback: impl FnOnce(I),
        glyphs: I,
    ) where
        I: Iterator<Item = (GlyphId, &'o mut V)>,
    {
        if let Some(vorg) = self.font.vorg() {
            let vvar = if self.coords.is_empty() {
                None
            } else {
                self.font.vvar()
            };
            for (glyph, out) in glyphs {
                let origin = F48Dot16::from_i32(vorg.vertical_origin_y(glyph) as i32);
                let delta = vvar
                    .and_then(|vvar| vvar.v_origin_y_delta(glyph, self.coords))
                    .unwrap_or(F48Dot16::ZERO);
                *out = convert(glyph, Some(origin.saturating_add(delta)));
            }
            return;
        }
        if !self.font.v_metrics().is_empty() && self.font.glyf_loca().is_some() {
            let context = self.font;
            let scale = OutlineScaleF32::new(None, self.units_per_em);
            for (glyph, out) in glyphs {
                *out = convert(glyph, glyf_v_origin_from_context(&context, &scale, glyph));
            }
            return;
        }
        fallback(glyphs);
    }

    /// Returns `true` where the font provides vertical metrics of its own.
    ///
    /// One that does not stacks its glyphs by the line, which is the only
    /// part of a vertical advance a caller can supply.
    #[inline]
    pub(crate) fn states_vertical_advances(&self) -> bool {
        !self.font.v_metrics().is_empty()
    }

    pub(crate) fn origin_line(&self) -> LineExtents<F48Dot16> {
        let line = self.line_extents().unwrap_or_else(|| {
            let em = self.units_per_em as f64;
            let ascender = F48Dot16::from_f64(em * 0.8);
            LineExtents {
                ascender,
                descender: ascender - F48Dot16::from_i32(self.units_per_em as i32),
            }
        });
        LineExtents {
            ascender: F48Dot16::from_bits(line.ascender.to_bits().saturating_abs()),
            descender: F48Dot16::from_bits(-line.descender.to_bits().saturating_abs()),
        }
    }

    fn origin_against_ink(&self, glyph: GlyphId, line: LineExtents<F48Dot16>) -> F48Dot16 {
        Self::origin_from_extents(self.extents_exact(glyph), line)
    }

    fn origin_from_extents(
        extents: Option<GlyphExtents<F48Dot16>>,
        line: LineExtents<F48Dot16>,
    ) -> F48Dot16 {
        let Some(extents) = extents else {
            return line.ascender;
        };
        let height = line.ascender - line.descender;
        extents.y_bearing + F48Dot16::from_bits((height - extents.height).to_bits() >> 1)
    }

    /// The varied half of [`extents_batched`](Self::extents_batched).
    ///
    /// A location moves the points, and the box a glyph states is the one it
    /// has at the default location, so the answer comes from loading the
    /// outline. Floating-point deltas are kept until the four bounds are
    /// measured; the scale decides how to round them.
    /// `#[inline(never)]` because that pulls in the loader, whose size no
    /// unvaried measurement should pay for.
    #[inline(never)]
    fn extents_batched_glyf_varied<'o, V: 'o>(
        &self,
        convert: impl Fn(Option<GlyphExtents<F48Dot16>>) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        let context = self.font;
        for (glyph, out) in glyphs {
            *out = convert(
                (glyph.to_u32() < self.num_glyphs)
                    .then(|| varied_control_box(&context, glyph))
                    .flatten(),
            );
        }
    }

    /// The charstring half of [`extents_batched`](Self::extents_batched).
    ///
    /// A charstring states no box, so reading one means running it.
    /// `#[inline(never)]` for the same reason as the varied arm.
    #[inline(never)]
    fn extents_batched_cff<'o, V: 'o>(
        &self,
        cff: &CffFontRef<'_>,
        convert: impl Fn(Option<GlyphExtents<F48Dot16>>) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        // Nearly every font has one subfont, and a CID font groups its
        // glyphs, so the last one read is usually the next one wanted.
        let mut last: Option<(u16, Subfont)> = None;
        for (glyph, out) in glyphs {
            let subfont = cff.subfont_index(glyph).and_then(|index| match last {
                Some((cached, subfont)) if cached == index => Some(subfont),
                _ => {
                    let subfont = cff.subfont(index, self.coords).ok()?;
                    last = Some((index, subfont));
                    Some(subfont)
                }
            });
            *out = convert(subfont.and_then(|subfont| {
                cff.evaluate_extents(&subfont, glyph, self.coords)
                    .ok()
                    .map(|bounds| bounds.map(extents_from_bounds).unwrap_or_default())
            }));
        }
    }

    /// The Type 1 half of [`extents_batched`](Self::extents_batched).
    ///
    /// `#[inline(never)]` for the same reason as
    /// [`h_advance_batched_type1`](Self::h_advance_batched_type1).
    #[inline(never)]
    fn extents_batched_type1<'o, V: 'o>(
        &self,
        font: &Type1Font,
        convert: impl Fn(Option<GlyphExtents<F48Dot16>>) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        for (glyph, out) in glyphs {
            *out = convert(
                font.evaluate_extents(glyph)
                    .ok()
                    .map(|bounds| bounds.map(extents_from_bounds).unwrap_or_default()),
            );
        }
    }

    /// The height a glyph advances where the font has no `vmtx`.
    ///
    /// The extent of the line, without the gap after it, as HarfBuzz and
    /// FreeType both take it. A font that states no line at all measures one
    /// em.
    fn line_height(&self) -> F48Dot16 {
        match self.global.h_line() {
            Some(line) => line.ascender - line.descender,
            None => F48Dot16::from_i32(self.units_per_em as i32),
        }
    }

    /// The varied half of [`v_advance_batched`](Self::v_advance_batched).
    ///
    /// `#[inline(never)]` for the same reason as the horizontal one.
    #[inline(never)]
    fn v_advance_batched_varied<'o, V: 'o>(
        &self,
        raw: &RawGlyphMetrics<'_>,
        coords: &[F2Dot14],
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        // Ask the table that answers directly, and stop there if it does.
        if let Some(vvar) = self.font.vvar() {
            return raw.run_varied(
                self.num_glyphs,
                |gid| vvar.advance_delta(gid, coords).unwrap_or(F48Dot16::ZERO),
                convert,
                glyphs,
            );
        }
        // Without `VVAR`, the phantom points answer. The vertical pair runs
        // downward from the origin, so the height is the top less the
        // bottom, the reverse of the width.
        if let (Some(gvar), Some((glyf, loca))) = (self.font.gvar(), self.font.glyf_loca()) {
            return raw.run_varied(
                self.num_glyphs,
                |gid| {
                    // A glyph the table says nothing readable about does
                    // not move.
                    gvar.phantom_point_deltas(glyf, loca, coords, gid)
                        .map_or(F48Dot16::ZERO, |deltas| {
                            (deltas[2].y - deltas[3].y).to_f48dot16()
                        })
                },
                convert,
                glyphs,
            );
        }
        // A location, but nothing stating what it changes.
        raw.run(self.num_glyphs, convert, glyphs)
    }

    /// The varied half of [`h_advance_batched`](Self::h_advance_batched).
    ///
    /// `#[inline(never)]` because reading `gvar` pulls in a large amount of
    /// code, whose size would otherwise be charged to every unvaried
    /// measurement.
    #[inline(never)]
    fn h_advance_batched_varied<'o, V: 'o>(
        &self,
        raw: &RawGlyphMetrics<'_>,
        coords: &[F2Dot14],
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        // Ask the table that answers directly, and stop there if it does.
        if let Some(hvar) = self.font.hvar() {
            return raw.run_varied(
                self.num_glyphs,
                |gid| hvar.advance_delta(gid, coords).unwrap_or(F48Dot16::ZERO),
                convert,
                glyphs,
            );
        }
        // Without `HVAR`, the answer comes from the phantom points on the
        // outline: an advance spans the two horizontal ones, so a change in
        // it is a change in that span.
        if let (Some(gvar), Some((glyf, loca))) = (self.font.gvar(), self.font.glyf_loca()) {
            return raw.run_varied(
                self.num_glyphs,
                |gid| {
                    // A glyph the table says nothing readable about does
                    // not move.
                    gvar.phantom_point_deltas(glyf, loca, coords, gid)
                        .map_or(F48Dot16::ZERO, |deltas| {
                            (deltas[1].x - deltas[0].x).to_f48dot16()
                        })
                },
                convert,
                glyphs,
            );
        }
        // A location, but nothing stating what it changes.
        raw.run(self.num_glyphs, convert, glyphs)
    }

    /// The Type 1 half of [`h_advance_batched`](Self::h_advance_batched).
    ///
    /// A Type 1 charstring states its own width, so reading one means
    /// running it. `#[inline(never)]` because that has nothing in common with
    /// reading a table, and no `sfnt` should pay for its presence.
    #[inline(never)]
    fn h_advance_batched_type1<'o, V: 'o>(
        &self,
        font: &Type1Font,
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        /// The width is all that is wanted, so the outline is discarded.
        struct WidthOnly;

        impl CommandSink for WidthOnly {
            fn move_to(&mut self, _x: Fixed, _y: Fixed) {}
            fn line_to(&mut self, _x: Fixed, _y: Fixed) {}
            fn curve_to(
                &mut self,
                _cx0: Fixed,
                _cy0: Fixed,
                _cx1: Fixed,
                _cy1: Fixed,
                _x: Fixed,
                _y: Fixed,
            ) {
            }
            fn close(&mut self) {}
        }

        // A charstring states its width in its own space, which the font
        // matrix maps to design units. No size is applied: that is the
        // caller's to do.
        let transform = font.transform(None);
        for (gid, out) in glyphs {
            // A charstring states its width before it draws anything, so one
            // that states none is malformed and measures as nothing.
            let width = font
                .evaluate_charstring(gid, &mut WidthOnly)
                .ok()
                .flatten()
                .map(|width| transform.transform_h_metric(width).to_f48dot16())
                .unwrap_or(F48Dot16::ZERO)
                .max(F48Dot16::ZERO);
            *out = convert(width);
        }
    }
}

/// The per-glyph records of `hmtx` or `vmtx`.
///
/// Both tables hold the same records, so one type reads either, and which
/// direction it describes is fixed when it is read.
///
/// Nothing here depends on a location, so every location shares one parse.
#[derive(Clone, Default, yoke::Yokeable)]
pub(crate) struct RawGlyphMetrics<'a> {
    metrics: &'a [LongMetric],
    /// Bare side bearings after the last long metric.
    bearings: &'a [BigEndian<i16>],
}

impl<'a> RawGlyphMetrics<'a> {
    /// Reads what `hmtx` states.
    pub(crate) fn from_hmtx(tables: &impl TableProvider<'a>) -> Self {
        let hmtx = tables.hmtx().ok();
        Self {
            metrics: hmtx
                .as_ref()
                .map(|hmtx| hmtx.h_metrics())
                .unwrap_or_default(),
            bearings: hmtx
                .as_ref()
                .map(|hmtx| hmtx.left_side_bearings())
                .unwrap_or_default(),
        }
    }

    /// Reads what `vmtx` states.
    pub(crate) fn from_vmtx(tables: &impl TableProvider<'a>) -> Self {
        let vmtx = tables.vmtx().ok();
        Self {
            metrics: vmtx
                .as_ref()
                .map(|vmtx| vmtx.v_metrics())
                .unwrap_or_default(),
            bearings: vmtx
                .as_ref()
                .map(|vmtx| vmtx.top_side_bearings())
                .unwrap_or_default(),
        }
    }

    /// Returns `true` if the table states no metrics.
    #[inline]
    pub(crate) fn is_empty(&self) -> bool {
        self.metrics.is_empty()
    }

    /// Writes what the table states for each glyph.
    ///
    /// A stored advance is a `u16`, so unlike
    /// [`run_varied`](Self::run_varied) this can neither overflow the sum nor
    /// report anything below zero.
    #[inline]
    fn run<'o, V: 'o>(
        &self,
        num_glyphs: u32,
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        for (gid, out) in glyphs {
            *out = convert(self.stored_advance(num_glyphs, gid));
        }
    }

    /// Writes each glyph's advance with `delta` applied.
    ///
    /// `delta` is settled before the run, leaving only the branches that turn
    /// on the glyph. The sum saturates rather than wrapping, but is not
    /// otherwise bounded: a font whose deltas say so can carry an advance
    /// below zero, and what to do about that is the caller's to decide.
    ///
    /// Glyphs beyond the end of the font measure zero here, as they do
    /// unvaried.
    #[inline]
    fn run_varied<'o, V: 'o>(
        &self,
        num_glyphs: u32,
        delta: impl Fn(GlyphId) -> F48Dot16,
        convert: impl Fn(F48Dot16) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        for (gid, out) in glyphs {
            // Glyphs beyond the end of the font measure zero, and no
            // delta applies to them. The index map clamps an out of range
            // glyph onto its last entry, so asking would answer with some
            // other glyph's delta.
            let advance = if gid.to_u32() < num_glyphs {
                self.stored_advance(num_glyphs, gid)
                    .saturating_add(delta(gid))
            } else {
                F48Dot16::ZERO
            };
            *out = convert(advance);
        }
    }

    /// Returns the advance the table states for `glyph`, before any location.
    ///
    /// A glyph past the end of the font advances by nothing. The last record
    /// covers every glyph after it, and clamping the index rather than
    /// falling back on a failed lookup keeps that bound provable, so a run
    /// compiles without a branch per glyph.
    ///
    /// `self.metrics` is never empty here: a caller settles that font before
    /// the run starts.
    #[inline]
    pub(crate) fn stored_advance(&self, num_glyphs: u32, glyph: GlyphId) -> F48Dot16 {
        if glyph.to_u32() >= num_glyphs {
            return F48Dot16::ZERO;
        }
        let index = (glyph.to_u32() as usize).min(self.metrics.len().saturating_sub(1));
        match self.metrics.get(index) {
            Some(metric) => F48Dot16::from_i32(metric.advance() as i32),
            None => F48Dot16::ZERO,
        }
    }

    /// Returns the side bearing stated for `glyph`, including the bare
    /// bearings following the last long metric.
    pub(crate) fn stated_side_bearing(&self, glyph: GlyphId) -> Option<F48Dot16> {
        let index = glyph.to_u32() as usize;
        if let Some(metric) = self.metrics.get(index) {
            return Some(F48Dot16::from_i32(metric.side_bearing() as i32));
        }
        let tail = index.checked_sub(self.metrics.len())?;
        Some(F48Dot16::from_i32(self.bearings.get(tail)?.get() as i32))
    }
}

/// Returns the metrics of a font that states none.
pub(crate) fn empty() -> &'static RawGlyphMetrics<'static> {
    static EMPTY: RawGlyphMetrics<'static> = RawGlyphMetrics {
        metrics: &[],
        bearings: &[],
    };
    &EMPTY
}

/// Turns the corners of a box into a bearing and a size.
///
/// The corners are taken either way round, so a font stating a reversed box
/// still measures a size rather than a negative one.
fn extents_from_corners(
    x_min: F48Dot16,
    y_min: F48Dot16,
    x_max: F48Dot16,
    y_max: F48Dot16,
) -> GlyphExtents<F48Dot16> {
    let (left, right) = if x_min <= x_max {
        (x_min, x_max)
    } else {
        (x_max, x_min)
    };
    let (bottom, top) = if y_min <= y_max {
        (y_min, y_max)
    } else {
        (y_max, y_min)
    };
    GlyphExtents {
        x_bearing: left,
        y_bearing: top,
        width: right - left,
        height: top - bottom,
    }
}

/// Turns a charstring's box into a bearing and a size.
fn extents_from_bounds(bounds: BoundingBox<Fixed>) -> GlyphExtents<F48Dot16> {
    extents_from_corners(
        bounds.x_min.to_f48dot16(),
        bounds.y_min.to_f48dot16(),
        bounds.x_max.to_f48dot16(),
        bounds.y_max.to_f48dot16(),
    )
}

/// The box a loaded outline's points stay within.
///
/// The control points count, so this is what the glyph states as its own box
/// rather than the tighter box its curves keep to. A scale can round the
/// finished bounds after applying floating-point variation deltas.
fn control_box(points: &[Point<f32>]) -> Option<GlyphExtents<F48Dot16>> {
    let first = points.first()?;
    let (mut x_min, mut y_min) = (first.x, first.y);
    let (mut x_max, mut y_max) = (first.x, first.y);
    for point in &points[1..] {
        x_min = x_min.min(point.x);
        y_min = y_min.min(point.y);
        x_max = x_max.max(point.x);
        y_max = y_max.max(point.y);
    }
    let as_fixed = |value: f32| F48Dot16::from_f64(value as f64);
    Some(extents_from_corners(
        as_fixed(x_min),
        as_fixed(y_min),
        as_fixed(x_max),
        as_fixed(y_max),
    ))
}

/// The box a glyph's points stay within, at the location `tables` states.
///
/// How much scratch space a glyph takes is known only once its plan is read,
/// and it is finished with by the time this returns, so the space comes from
/// [`with_scratch`] rather than an allocation per glyph.
fn varied_control_box<'a>(
    context: &'a dyn OutlineContext<'a>,
    glyph: GlyphId,
) -> Option<GlyphExtents<F48Dot16>> {
    let plan = OutlinePlan::new(context, glyph).ok()?;
    let scale = OutlineScaleF32::new(None, context.units_per_em());
    let lengths = plan.buffer_lengths::<OutlineScaleF32>();
    with_scratch(lengths.packed_len::<OutlineScaleF32>(), |block| {
        let buffers = Buffers::from_bytes(block, &lengths).ok()?;
        let outline = plan.load(context, &scale, buffers, None).ok()?;
        Some(control_box(outline.points()).unwrap_or_default())
    })
}

/// The loaded top phantom point, retaining fractional gvar deltas.
fn glyf_v_origin_from_context<'a>(
    context: &'a dyn OutlineContext<'a>,
    scale: &OutlineScaleF32,
    glyph: GlyphId,
) -> Option<F48Dot16> {
    let plan = OutlinePlan::new(context, glyph).ok()?;
    let lengths = plan.buffer_lengths::<OutlineScaleF32>();
    with_scratch(lengths.packed_len::<OutlineScaleF32>(), |block| {
        let buffers = Buffers::from_bytes(block, &lengths).ok()?;
        let outline = plan.load(context, scale, buffers, None).ok()?;
        Some(F48Dot16::from_f64(outline.phantom_points()[2].y as f64))
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        model::{
            glyph::outline::{ControlBoundsPen, NullPen},
            Blob, NormalizedCoord,
        },
        tables::glyf::outline::{Outline, OutlineTables, PathContourStart, Unscaled},
        FontRef,
    };
    use alloc::{sync::Arc, vec, vec::Vec};
    use std::sync::Mutex;
    use types::Tag;

    const STATIC: &[u8] = font_test_data::TINOS_SUBSET;
    /// Has both `HVAR` and `gvar`, so it can answer either way.
    const VAR: &[u8] = font_test_data::VAZIRMATN_VAR;
    /// Eleven glyphs but one long metric, so ten of them are in the tail.
    const TAIL: &[u8] = font_test_data::MATERIAL_SYMBOLS_SUBSET;
    /// Charstring outlines, with no `glyf` to answer ahead of them.
    const CFF: &[u8] = font_test_data::NOTO_SANS_JP_CFF;
    const CFF2: &[u8] = font_test_data::ift::CFF2_FONT;

    #[test]
    fn vertical_origins_in_design_units_match_their_sources_and_batch() {
        for data in [
            font_test_data::VORG,
            font_test_data::MPLUS1CODE_VERTICAL_SUBSET,
            STATIC,
        ] {
            let font = Font::new(data, 0).unwrap();
            let metrics = font.glyph_metrics();
            let glyphs: Vec<_> = (0..font.num_glyphs()).map(GlyphId::new).collect();
            let mut batched = vec![F48Dot16::ZERO; glyphs.len()];
            metrics.v_origin_y_batched(
                |origin| origin,
                glyphs.iter().copied().zip(batched.iter_mut()),
            );
            for (glyph, origin) in glyphs.into_iter().zip(batched) {
                assert_eq!(origin, metrics.v_origin_y_exact(glyph), "glyph {glyph}");
                assert_eq!(origin.to_f32(), metrics.v_origin_y(glyph));
                if let Some(stated) = metrics.stated_v_origin(glyph) {
                    assert_eq!(origin, stated);
                } else if let Some(phantom) = metrics.glyf_v_origin(glyph) {
                    assert_eq!(origin, phantom);
                }
            }
        }
    }

    #[test]
    fn a_width_is_what_hmtx_stores() {
        // A font with a tail, so this covers both halves of `hmtx`.
        let font = Font::new(TAIL, 0).unwrap();
        let direct = FontRef::new(TAIL).unwrap();
        let hmtx = direct.hmtx().unwrap();
        let num_glyphs = direct.maxp().unwrap().num_glyphs();
        assert!(num_glyphs > 1);
        for gid in 0..num_glyphs as u32 {
            let expected = hmtx.advance(GlyphId::new(gid)).unwrap();
            assert_eq!(
                font.glyph_metrics().h_advance_exact(GlyphId::new(gid)),
                F48Dot16::from_i32(expected as i32)
            );
        }
    }

    #[test]
    fn a_glyph_past_the_end_advances_by_nothing() {
        // Deliberately unlike `Hmtx::advance`, which clamps to the last long
        // metric and so reports a width for a glyph the font does not have.
        // HarfBuzz returns zero once a font has metrics at all, and a shaper
        // handed a bad glyph id is better served by nothing than by whatever
        // the last real glyph happened to measure.
        let font = Font::new(STATIC, 0).unwrap();
        let direct = FontRef::new(STATIC).unwrap();
        let past = GlyphId::new(direct.maxp().unwrap().num_glyphs() as u32);

        assert_eq!(font.glyph_metrics().h_advance_exact(past), F48Dot16::ZERO);
        assert!(direct.hmtx().unwrap().advance(past).unwrap() > 0);
        assert_eq!(
            font.glyph_metrics().h_advance_exact(GlyphId::new(60000)),
            F48Dot16::ZERO
        );
    }

    #[test]
    fn a_font_with_no_widths_gives_every_glyph_half_an_em() {
        let font = Font::new(font_test_data::NAMES_ONLY, 0).unwrap();
        // That font has no `head` either, so the em is zero. The shape of
        // the answer is what matters.
        assert_eq!(
            font.glyph_metrics().h_advance_exact(GlyphId::new(1)),
            F48Dot16::ZERO
        );
    }

    #[test]
    fn glyphs_past_the_long_metrics_share_the_last_advance() {
        // `hmtx` stores a full metric for the first `numberOfHMetrics`
        // glyphs and a bare side bearing for the rest, which all advance by
        // the last stored width. That tail is how a font records a run of
        // glyphs of equal width, and it is most of this font.
        let font = Font::new(TAIL, 0).unwrap();
        let direct = FontRef::new(TAIL).unwrap();
        let num_glyphs = direct.maxp().unwrap().num_glyphs() as u32;
        let num_long = direct.hhea().unwrap().number_of_h_metrics() as u32;
        assert!(num_long < num_glyphs, "this font has no tail to test");

        let last = font
            .glyph_metrics()
            .h_advance_exact(GlyphId::new(num_long - 1));
        assert!(last > F48Dot16::ZERO);
        for gid in num_long..num_glyphs {
            assert_eq!(
                font.glyph_metrics().h_advance_exact(GlyphId::new(gid)),
                last,
                "glyph {gid} is in the tail and should share the last advance"
            );
        }
        // And the tail stops at the end of the font rather than running on.
        assert_eq!(
            font.glyph_metrics()
                .h_advance_exact(GlyphId::new(num_glyphs)),
            F48Dot16::ZERO
        );
    }

    #[test]
    fn glyphs_beyond_the_font_measure_zero_at_any_location() {
        // The delta set index map clamps an out of range glyph onto its
        // last entry, so without a check these would take the last real
        // glyph's delta and report an advance, and a negative one at that.
        let font = Font::new(VAR, 0).unwrap();
        let num_glyphs = font.num_glyphs();
        let instance = at(&font, -1.0);
        assert!(
            instance
                .glyph_metrics()
                .h_advance_exact(GlyphId::new(num_glyphs - 1))
                != font
                    .glyph_metrics()
                    .h_advance_exact(GlyphId::new(num_glyphs - 1)),
            "the last real glyph should move, or this proves nothing"
        );
        for gid in [num_glyphs, num_glyphs + 1, u32::MAX] {
            let gid = GlyphId::new(gid);
            assert_eq!(
                instance.glyph_metrics().h_advance_exact(gid),
                F48Dot16::ZERO,
                "glyph {gid} is beyond the font"
            );
            assert_eq!(font.glyph_metrics().h_advance_exact(gid), F48Dot16::ZERO);
        }
    }

    /// Has `vmtx`, `VVAR` and one axis, so it can vary vertically.
    const VERT_VAR: &[u8] = font_test_data::ift::CFF2_FONT;
    /// Has `vmtx`, `VVAR` and `gvar`, which disagree about the advance.
    const VERT: &[u8] = font_test_data::MPLUS1CODE_VERTICAL_SUBSET;

    #[test]
    fn a_type1_glyph_stacks_by_the_line() {
        // Type 1 has no `vmtx`, so its glyphs take the line, which such a
        // font derives from its bounding box. Every glyph gets the same
        // height while their widths differ, and no location moves either.
        //
        // FreeType instead gives each Type 1 glyph 12/10 of its own ink
        // height. Nothing here follows it: one rule for every font without
        // vertical metrics is easier to predict than a per-glyph heuristic,
        // and HarfBuzz has no opinion because it does not read Type 1.
        let font = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, 0).unwrap();
        let line = font.metrics().h_line().unwrap();
        let height = line.ascender - line.descender;
        assert!(height > F48Dot16::ZERO);
        let metrics = font.glyph_metrics();
        let mut widths = std::collections::HashSet::new();
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert_eq!(metrics.v_advance_exact(gid), height, "glyph {gid}");
            widths.insert(metrics.h_advance_exact(gid).to_bits());
        }
        assert!(
            widths.len() > 1,
            "every glyph is the same width, so this proves nothing"
        );
    }

    #[test]
    fn vertical_advances_come_from_vmtx() {
        let font = Font::new(VERT_VAR, 0).unwrap();
        let direct = FontRef::new(VERT_VAR).unwrap();
        let vmtx = direct.vmtx().unwrap();
        let metrics = font.glyph_metrics();
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let Some(stated) = vmtx.advance(gid) else {
                continue;
            };
            assert_eq!(
                metrics.v_advance_exact(gid),
                F48Dot16::from_i32(stated as i32),
                "glyph {gid}"
            );
            checked += 1;
        }
        assert!(checked > 1, "this font states no vertical advances");
    }

    #[test]
    fn vorg_reads_the_vvar_origin_delta_at_a_location() {
        // Pair the VORG fixture with a variable CFF2 font so both tables
        // answer. This VVAR fixture's origin deltas are zero, but requesting
        // the table proves the varied branch is taken.
        let asked = Arc::new(Mutex::new(Vec::new()));
        let calls = asked.clone();
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            calls.lock().unwrap().push(tag);
            let data = if tag == Tag::new(b"VORG") {
                font_test_data::VORG
            } else {
                VERT_VAR
            };
            let font = FontRef::new(data).ok()?;
            Some(Blob::from(font.table_data(tag)?.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let instance = at(&font, -1.0);
        let gid = GlyphId::new(1);
        let vorg = FontRef::new(font_test_data::VORG).unwrap().vorg().unwrap();
        assert_eq!(
            instance.glyph_metrics().stated_v_origin(gid),
            Some(F48Dot16::from_i32(vorg.vertical_origin_y(gid) as i32))
        );
        assert!(asked_for(&asked, b"VVAR"));
    }

    #[test]
    fn without_vmtx_glyphs_advance_by_the_line() {
        // A horizontal font stacks by the line it lays text on, so every
        // glyph gets the same height whatever its width.
        let font = Font::new(STATIC, 0).unwrap();
        let line = font.metrics().h_line().unwrap();
        let expected = line.ascender - line.descender;
        assert!(expected > F48Dot16::ZERO);
        let metrics = font.glyph_metrics();
        for gid in (0..font.num_glyphs().min(16)).map(GlyphId::new) {
            assert_eq!(metrics.v_advance_exact(gid), expected, "glyph {gid}");
        }
    }

    #[test]
    fn a_location_moves_a_vertical_advance() {
        let font = Font::new(VERT, 0).unwrap();
        let instance = at(&font, -1.0);
        let (varied, plain) = (instance.glyph_metrics(), font.glyph_metrics());
        let moved = (0..font.num_glyphs())
            .map(GlyphId::new)
            .filter(|gid| varied.v_advance_exact(*gid) != plain.v_advance_exact(*gid))
            .count();
        assert!(moved > 0, "no glyph moved at the light end of the axis");
    }

    #[test]
    fn phantom_points_measure_a_glyph_the_same_as_vvar() {
        // Both describe the same font, so where the font is written
        // consistently they agree. Withholding `VVAR` drives the phantom
        // point path rather than reimplementing it alongside.
        let with = Font::new(VERT, 0).unwrap();
        let asked = Arc::new(Mutex::new(Vec::new()));
        let without = callback_font(VERT, asked.clone(), &[b"VVAR"]);
        let (a, b) = (at(&with, -1.0), at(&without, -1.0));
        let (ma, mb) = (a.glyph_metrics(), b.glyph_metrics());
        // The composite is the exception: it carries its component's metrics
        // through `USE_MY_METRICS`, and this font states a delta there that
        // its own `vmtx` has already counted, so the two differ by it.
        let composite = GlyphId::new(2);
        for gid in (0..with.num_glyphs()).map(GlyphId::new) {
            if gid == composite {
                continue;
            }
            assert_eq!(
                ma.v_advance_exact(gid),
                mb.v_advance_exact(gid),
                "glyph {gid}"
            );
        }
        assert_eq!(
            mb.v_advance_exact(composite) - ma.v_advance_exact(composite),
            F48Dot16::from_i32(235)
        );
    }

    #[test]
    fn vvar_answering_keeps_gvar_and_the_outlines_unread() {
        let asked = Arc::new(Mutex::new(Vec::new()));
        let font = callback_font(VERT, asked.clone(), &[]);
        let instance = at(&font, -1.0);
        let metrics = instance.glyph_metrics();
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert!(metrics.v_advance_exact(gid) > F48Dot16::ZERO);
        }
        assert!(asked_for(&asked, b"VVAR"));
        for unread in [b"gvar", b"glyf", b"loca"] {
            assert!(
                !asked_for(&asked, unread),
                "{:?} was read",
                Tag::new(unread)
            );
        }
    }

    #[test]
    fn a_vertical_batch_agrees_with_one_at_a_time() {
        for (data, coord) in [(VERT_VAR, 0.0), (VERT_VAR, -1.0), (STATIC, 0.0)] {
            let font = Font::new(data, 0).unwrap();
            let instance = at(&font, coord);
            let metrics = instance.glyph_metrics();
            let gids: Vec<_> = (0..16u32).map(GlyphId::new).collect();
            let mut batch = vec![F48Dot16::ZERO; 16];
            metrics.v_advance_batched(|v| v, gids.iter().copied().zip(batch.iter_mut()));
            for (gid, expected) in gids.iter().zip(&batch) {
                assert_eq!(metrics.v_advance_exact(*gid), *expected, "glyph {gid}");
            }
        }
    }

    #[test]
    fn the_tables_are_parsed_once_for_the_font() {
        let font = Font::new(VAR, 0).unwrap();
        let first = font.h_metrics() as *const RawGlyphMetrics<'_>;
        for _ in 0..8 {
            assert!(core::ptr::eq(
                font.h_metrics() as *const RawGlyphMetrics<'_>,
                first
            ));
        }
        // And clones share it, since they share one `Arc<FontRepr>`.
        assert!(core::ptr::eq(
            font.clone().h_metrics() as *const RawGlyphMetrics<'_>,
            first
        ));
    }

    #[test]
    fn a_batch_agrees_with_one_at_a_time() {
        let font = Font::new(STATIC, 0).unwrap();
        let gids: Vec<_> = (0..16u32).map(GlyphId::new).collect();
        let mut batch = vec![F48Dot16::ZERO; 16];
        font.glyph_metrics()
            .h_advance_batched(|v| v, gids.iter().copied().zip(batch.iter_mut()));
        for (gid, expected) in gids.iter().zip(&batch) {
            assert_eq!(font.glyph_metrics().h_advance_exact(*gid), *expected);
        }
    }

    #[test]
    fn a_type1_glyph_is_measured_by_its_charstring() {
        // Type 1 states no metrics table, so a width that is neither zero
        // nor half an em can only have come from running the charstring.
        let font = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB, 0).unwrap();
        let metrics = font.glyph_metrics();
        let half_em = F48Dot16::from_i32(font.units_per_em() as i32 / 2);
        assert!(font.num_glyphs() > 1);

        let widths: Vec<_> = (0..font.num_glyphs())
            .map(|gid| metrics.h_advance_exact(GlyphId::new(gid)))
            .collect();
        assert!(widths.iter().any(|width| *width > F48Dot16::ZERO));
        assert!(
            widths.iter().any(|width| *width != half_em),
            "every glyph reported the fallback, so no charstring was run"
        );
    }

    /// The fixture with its font matrix replaced, same length so the rest of
    /// the header is untouched. `xx` becomes 2, so every advance doubles.
    fn type1_with_a_stretched_matrix() -> Vec<u8> {
        const FROM: &[u8] = b"/FontMatrix [0.001 0 0 0.001 0 0 ]";
        const TO: &[u8] = b"/FontMatrix [0.002 0 0 0.001 0 0 ]";
        let base = font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA;
        let at = base
            .windows(FROM.len())
            .position(|window| window == FROM)
            .expect("the fixture states the matrix this test rewrites");
        let mut data = base.to_vec();
        data[at..at + TO.len()].copy_from_slice(TO);
        data
    }

    #[test]
    fn a_type1_width_is_mapped_by_the_font_matrix() {
        // The charstring states a width in its own space, which the matrix
        // maps to design units. The fixture states the usual matrix, where
        // that mapping is the identity and a missing one would go unnoticed,
        // so this stretches it: every advance must double.
        let plain = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, 0).unwrap();
        let stretched = Font::new(type1_with_a_stretched_matrix(), 0).unwrap();
        let (a, b) = (plain.glyph_metrics(), stretched.glyph_metrics());
        assert_eq!(plain.num_glyphs(), stretched.num_glyphs());

        let mut stretched_any = false;
        for gid in (0..plain.num_glyphs()).map(GlyphId::new) {
            let width = a.h_advance_exact(gid);
            assert_eq!(
                b.h_advance_exact(gid),
                width.saturating_add(width),
                "glyph {gid}"
            );
            stretched_any |= width > F48Dot16::ZERO;
        }
        assert!(stretched_any, "no glyph had a width to stretch");
    }

    #[test]
    fn a_type1_advance_is_what_drawing_the_glyph_reports() {
        // `draw` is the reference: it maps the charstring's own space to
        // design units through the font matrix. Note that this font states
        // the usual matrix for an em of 1000, where that mapping is the
        // identity, so this pins agreement with `draw` rather than proving
        // the matrix is applied.
        let data = font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA;
        let font = Font::new(data, 0).unwrap();
        let direct = crate::ps::type1::Type1Font::new(data).unwrap();
        let metrics = font.glyph_metrics();
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let expected = direct.draw(gid, None, &mut NullPen).ok().flatten();
            assert_eq!(
                metrics.h_advance(gid),
                expected.unwrap_or(0.0),
                "glyph {gid}"
            );
        }
    }

    #[test]
    fn the_narrowed_advance_agrees_with_the_exact_one() {
        for data in [STATIC, TAIL, VAR] {
            let font = Font::new(data, 0).unwrap();
            let metrics = font.glyph_metrics();
            for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                assert_eq!(
                    metrics.h_advance(gid),
                    metrics.h_advance_exact(gid).to_f32()
                );
            }
        }
    }

    #[test]
    fn the_conversion_decides_what_a_caller_gets_back() {
        let font = Font::new(STATIC, 0).unwrap();
        let gids = [GlyphId::new(1), GlyphId::new(2)];
        let mut exact = [F48Dot16::ZERO; 2];
        let mut floats = [0.0f32; 2];
        font.glyph_metrics()
            .h_advance_batched(|v| v, gids.iter().copied().zip(exact.iter_mut()));
        font.glyph_metrics()
            .h_advance_batched(|v| v.to_f32(), gids.iter().copied().zip(floats.iter_mut()));
        for i in 0..2 {
            assert_eq!(floats[i], exact[i].to_f32());
        }
    }

    #[test]
    fn a_box_is_the_one_the_glyph_states() {
        // Nothing has to be loaded at the default location: the glyph header
        // carries the box.
        let font = Font::new(STATIC, 0).unwrap();
        let metrics = font.glyph_metrics();
        let direct = FontRef::new(STATIC).unwrap();
        let (glyf, loca, hmtx) = (
            direct.glyf().unwrap(),
            direct.loca(None).unwrap(),
            direct.hmtx().unwrap(),
        );
        let mut drawn = 0;
        let mut empty = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            match loca.get_glyf(gid, &glyf).unwrap() {
                Some(glyph) => {
                    if glyph.number_of_contours() == 0 {
                        assert_eq!(
                            metrics.extents_exact(gid),
                            Some(GlyphExtents::default()),
                            "glyph {gid}"
                        );
                        empty += 1;
                        continue;
                    }
                    let unit = |value: i16| F48Dot16::from_i32(value as i32);
                    assert_eq!(
                        metrics.extents_exact(gid),
                        Some(GlyphExtents {
                            x_bearing: hmtx.side_bearing(gid).map(unit).unwrap_or(F48Dot16::ZERO),
                            y_bearing: unit(glyph.y_max()),
                            width: unit(glyph.x_max()) - unit(glyph.x_min()),
                            height: unit(glyph.y_max()) - unit(glyph.y_min()),
                        }),
                        "glyph {gid}"
                    );
                    drawn += 1;
                }
                // A valid empty glyph has a zero box.
                None => {
                    assert_eq!(
                        metrics.extents_exact(gid),
                        Some(GlyphExtents::default()),
                        "glyph {gid}"
                    );
                    empty += 1;
                }
            }
        }
        assert!(drawn > 0 && empty > 0, "{drawn} drawn, {empty} empty");
    }

    #[test]
    fn glyf_ink_starts_at_the_stated_left_side_bearing() {
        let direct = FontRef::new(STATIC).unwrap();
        let loca = direct.loca(None).unwrap();
        let glyf = direct.glyf().unwrap();
        let gid = (0..direct.hmtx().unwrap().h_metrics().len() as u32)
            .map(GlyphId::new)
            .find(|gid| {
                loca.get_glyf(*gid, &glyf)
                    .ok()
                    .flatten()
                    .is_some_and(|glyph| glyph.number_of_contours() > 0)
            })
            .unwrap();
        let outline = loca.get_glyf(gid, &glyf).unwrap().unwrap();
        let bearing = outline.x_min() + 17;
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            let font = FontRef::new(STATIC).ok()?;
            let mut data = font.table_data(tag)?.as_bytes().to_vec();
            if tag == Tag::new(b"hmtx") {
                let offset = gid.to_u32() as usize * 4 + 2;
                data[offset..offset + 2].copy_from_slice(&bearing.to_be_bytes());
            }
            Some(Blob::from(data))
        });
        let font = Font::new(source, 0).unwrap();
        assert_ne!(bearing, outline.x_min());
        let tables = OutlineTables::new(&font.tables()).unwrap();
        let mut outline = Outline::<Unscaled>::new();
        let loaded = outline.load(&tables, gid).unwrap();
        let points = loaded.points();
        let x_min = points.iter().map(|point| point.x).min().unwrap();
        let x_max = points.iter().map(|point| point.x).max().unwrap();
        let y_min = points.iter().map(|point| point.y).min().unwrap();
        let y_max = points.iter().map(|point| point.y).max().unwrap();
        let unit = F48Dot16::from_i32;
        assert_eq!(
            font.glyph_metrics().extents_exact(gid),
            Some(GlyphExtents {
                x_bearing: unit(x_min),
                y_bearing: unit(y_max),
                width: unit(x_max - x_min),
                height: unit(y_max - y_min),
            })
        );
    }

    #[test]
    fn glyf_bearing_uses_hmtx_tail_then_falls_back_to_zero() {
        let direct = FontRef::new(TAIL).unwrap();
        let loca = direct.loca(None).unwrap();
        let glyf = direct.glyf().unwrap();
        let hmtx = direct.hmtx().unwrap();
        let long_count = hmtx.h_metrics().len();
        let gid = (long_count as u32..direct.maxp().unwrap().num_glyphs() as u32)
            .map(GlyphId::new)
            .find(|gid| loca.get_glyf(*gid, &glyf).ok().flatten().is_some())
            .unwrap();
        let font = Font::new(TAIL, 0).unwrap();
        assert_eq!(
            font.glyph_metrics().extents_exact(gid).unwrap().x_bearing,
            F48Dot16::from_i32(hmtx.side_bearing(gid).unwrap() as i32)
        );

        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            let font = FontRef::new(TAIL).ok()?;
            let mut data = font.table_data(tag)?.as_bytes().to_vec();
            if tag == Tag::new(b"hmtx") {
                data.truncate(long_count * 4);
            }
            Some(Blob::from(data))
        });
        let shortened = Font::new(source, 0).unwrap();
        assert_eq!(
            shortened
                .glyph_metrics()
                .extents_exact(gid)
                .unwrap()
                .x_bearing,
            F48Dot16::ZERO
        );
        let tables = OutlineTables::new(&shortened.tables()).unwrap();
        let mut outline = Outline::<Unscaled>::new();
        let loaded = outline.load(&tables, gid).unwrap();
        assert_eq!(loaded.points().iter().map(|point| point.x).min(), Some(0));
    }

    #[test]
    fn a_varied_box_holds_the_outline_it_describes() {
        // The stated box is the one the glyph has at the default location, so
        // a location away from it is measured from the loaded points. Drawing
        // those points reaches the same box by another route, which is what
        // catches a phantom point counted as ink.
        let font = Font::new(VAR, 0).unwrap();
        let instance = at(&font, -1.0);
        let metrics = instance.glyph_metrics();
        let direct = FontRef::new(VAR).unwrap();
        let tables = OutlineTables::new(&direct)
            .unwrap()
            .at(instance.normalized_coords(), &[]);
        let mut outline = Outline::<OutlineScaleF32>::new();
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let Ok(loaded) = outline.load(&tables, gid, None) else {
                continue;
            };
            let mut pen = ControlBoundsPen::new();
            let _ = loaded.to_path(PathContourStart::ScanBackward, &mut pen);
            let expected = pen
                .bounding_box()
                .map(|b| {
                    extents_from_corners(
                        F48Dot16::from_f64(b.x_min as f64),
                        F48Dot16::from_f64(b.y_min as f64),
                        F48Dot16::from_f64(b.x_max as f64),
                        F48Dot16::from_f64(b.y_max as f64),
                    )
                })
                .or(Some(GlyphExtents::default()));
            assert_eq!(metrics.extents_exact(gid), expected, "glyph {gid}");
            checked += expected.is_some() as u32;
        }
        assert!(checked > 0);
    }

    #[test]
    fn a_location_moves_the_box() {
        let font = Font::new(VAR, 0).unwrap();
        let instance = at(&font, -1.0);
        let (varied, plain) = (instance.glyph_metrics(), font.glyph_metrics());
        let moved = (0..font.num_glyphs())
            .map(GlyphId::new)
            .filter(|gid| varied.extents_exact(*gid) != plain.extents_exact(*gid))
            .count();
        assert!(moved > 0, "no glyph ink moved at the end of the axis");
    }

    #[test]
    fn a_charstring_box_is_the_box_it_draws_within() {
        // A charstring states no box, so the two have to agree by running it
        // the same way twice: once collecting bounds, once drawing.
        for data in [CFF, CFF2] {
            let font = Font::new(data, 0).unwrap();
            let metrics = font.glyph_metrics();
            let direct = FontRef::new(data).unwrap();
            let cff = font.cff().expect("no charstrings");
            assert!(direct.glyf().is_err(), "glyf would answer first");
            let mut checked = 0;
            for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                let subfont = cff
                    .subfont(cff.subfont_index(gid).unwrap(), &[])
                    .expect("no subfont");
                let mut pen = ControlBoundsPen::new();
                cff.draw(&subfont, gid, &[], None, &mut pen).unwrap();
                let expected = pen
                    .bounding_box()
                    .map(|b| GlyphExtents {
                        x_bearing: F48Dot16::from_f64(b.x_min as f64),
                        y_bearing: F48Dot16::from_f64(b.y_max as f64),
                        width: F48Dot16::from_f64((b.x_max - b.x_min) as f64),
                        height: F48Dot16::from_f64((b.y_max - b.y_min) as f64),
                    })
                    .or(Some(GlyphExtents::default()));
                assert_eq!(metrics.extents_exact(gid), expected, "glyph {gid}");
                checked += expected.is_some() as u32;
            }
            assert!(checked > 0);
        }
    }

    #[test]
    fn a_type1_box_is_the_box_it_draws_within() {
        let data = font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA;
        let font = Font::new(data, 0).unwrap();
        let metrics = font.glyph_metrics();
        let direct = crate::ps::type1::Type1Font::new(data).unwrap();
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let mut pen = ControlBoundsPen::new();
            direct.draw(gid, None, &mut pen).unwrap();
            let expected = pen
                .bounding_box()
                .map(|b| GlyphExtents {
                    x_bearing: F48Dot16::from_f64(b.x_min as f64),
                    y_bearing: F48Dot16::from_f64(b.y_max as f64),
                    width: F48Dot16::from_f64((b.x_max - b.x_min) as f64),
                    height: F48Dot16::from_f64((b.y_max - b.y_min) as f64),
                })
                .or(Some(GlyphExtents::default()));
            assert_eq!(metrics.extents_exact(gid), expected, "glyph {gid}");
            checked += expected.is_some() as u32;
        }
        assert!(checked > 0);
    }

    #[test]
    fn every_source_measures_a_run_as_it_measures_one() {
        // Each source keeps something across a run: buffers, a subfont, a
        // table read once. None of it may change an answer.
        for (data, coord) in [(STATIC, 0.0), (VAR, -1.0), (CFF, 0.0), (CFF2, 0.0)] {
            let font = Font::new(data, 0).unwrap();
            let instance = if coord == 0.0 {
                font.clone()
            } else {
                at(&font, coord)
            };
            let metrics = instance.glyph_metrics();
            let glyphs: Vec<_> = (0..font.num_glyphs()).map(GlyphId::new).collect();
            let mut batched = vec![None; glyphs.len()];
            metrics.extents_batched(
                |extents| extents,
                glyphs.iter().copied().zip(batched.iter_mut()),
            );
            for (gid, batched) in glyphs.iter().copied().zip(batched) {
                assert_eq!(metrics.extents_exact(gid), batched, "glyph {gid}");
            }
        }
    }

    #[test]
    fn a_font_stating_no_outlines_measures_nothing() {
        let font = Font::new(font_test_data::NAMES_ONLY, 0).unwrap();
        let metrics = font.glyph_metrics();
        for gid in (0..8).map(GlyphId::new) {
            assert_eq!(metrics.extents_exact(gid), None);
        }
    }

    #[test]
    fn a_glyph_past_the_font_has_no_extents() {
        for data in [
            STATIC,
            VAR,
            CFF,
            CFF2,
            font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA,
        ] {
            let font = Font::new(data, 0).unwrap();
            let past = GlyphId::new(font.num_glyphs());
            assert_eq!(font.glyph_metrics().extents_exact(past), None);
            if data == VAR {
                assert_eq!(at(&font, -1.0).glyph_metrics().extents_exact(past), None);
            }
        }
    }

    #[test]
    fn a_reversed_box_still_measures_a_size() {
        // A size is never negative, whichever way round a font states its
        // corners.
        let unit = F48Dot16::from_i32;
        assert_eq!(
            extents_from_corners(unit(70), unit(50), unit(10), unit(-30)),
            GlyphExtents {
                x_bearing: unit(10),
                y_bearing: unit(50),
                width: unit(60),
                height: unit(80),
            }
        );
    }

    fn at(font: &Font, coord: f32) -> Font {
        font.instance_builder()
            .normalized_coords([NormalizedCoord::from_f32(coord)])
            .build()
    }

    /// A font whose tables arrive one at a time, recording what was asked
    /// for and withholding any tag in `hide`.
    fn callback_font(
        data: &'static [u8],
        asked: Arc<Mutex<Vec<Tag>>>,
        hide: &'static [&[u8; 4]],
    ) -> Font {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag: Tag| {
            asked.lock().unwrap().push(tag);
            if hide.iter().any(|hidden| Tag::new(hidden) == tag) {
                return None;
            }
            let font = FontRef::new(data).ok()?;
            Some(Blob::from(font.table_data(tag)?.as_bytes().to_vec()))
        });
        Font::new(source, 0).unwrap()
    }

    fn asked_for(asked: &Arc<Mutex<Vec<Tag>>>, tag: &[u8; 4]) -> bool {
        asked.lock().unwrap().contains(&Tag::new(tag))
    }

    #[test]
    fn an_instance_varies_what_the_font_does_not() {
        let font = Font::new(VAR, 0).unwrap();
        let instance = at(&font, -1.0);
        let (varied, plain) = (instance.glyph_metrics(), font.glyph_metrics());
        let moved = (0..font.num_glyphs())
            .map(GlyphId::new)
            .any(|gid| varied.h_advance_exact(gid) != plain.h_advance_exact(gid));
        assert!(moved, "no glyph moved at the far end of the axis");
    }

    #[test]
    fn an_advance_keeps_the_fraction_a_location_adds() {
        // The stored width is whole and the delta is not, so a varied advance
        // should carry a fraction that rounding would have thrown away. If
        // nothing here is fractional the exactness is untested.
        let font = Font::new(VAR, 0).unwrap();
        let metrics = at(&font, -0.4);
        let metrics = metrics.glyph_metrics();
        let fractional = (0..font.num_glyphs())
            .map(GlyphId::new)
            .any(|gid| metrics.h_advance_exact(gid).to_bits() & 0xFFFF != 0);
        assert!(fractional, "no advance carried a fraction");
    }

    #[test]
    fn a_batch_agrees_with_one_at_a_time_at_a_location() {
        let font = Font::new(VAR, 0).unwrap();
        let instance = at(&font, -0.6);
        let metrics = instance.glyph_metrics();
        let gids: Vec<_> = (0..16u32).map(GlyphId::new).collect();
        let mut batch = vec![F48Dot16::ZERO; 16];
        metrics.h_advance_batched(|v| v, gids.iter().copied().zip(batch.iter_mut()));
        for (gid, expected) in gids.iter().zip(&batch) {
            assert_eq!(metrics.h_advance_exact(*gid), *expected);
        }
    }

    #[test]
    fn gvar_answers_the_same_as_hvar() {
        // The two rungs of the ladder describe the same font, so a font that
        // has both must measure the same either way. Withholding `HVAR`
        // forces the second rung, which exercises the real fallback rather
        // than a reimplementation of it.
        let font = Font::new(VAR, 0).unwrap();
        let asked = Arc::new(Mutex::new(Vec::new()));
        let no_hvar = callback_font(VAR, asked.clone(), &[b"HVAR"]);
        for coord in [-1.0, -0.8, -0.25, 0.75, 1.0] {
            let (with, without) = (at(&font, coord), at(&no_hvar, coord));
            let (with, without) = (with.glyph_metrics(), without.glyph_metrics());
            for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                assert_eq!(
                    with.h_advance_exact(gid),
                    without.h_advance_exact(gid),
                    "glyph {gid} disagrees at {coord}"
                );
            }
        }
        // And it really did take the other path.
        assert!(asked_for(&asked, b"gvar"));
    }

    #[test]
    fn a_default_location_reads_no_variation_table() {
        // The point of the whole arrangement: `gvar` is typically about half
        // a variable font, and on a platform that hands tables over one at a
        // time, asking for it means copying it. Static text must not.
        let asked = Arc::new(Mutex::new(Vec::new()));
        let font = callback_font(VAR, asked.clone(), &[]);
        let metrics = font.glyph_metrics();
        for gid in (0..8).map(GlyphId::new) {
            let _ = metrics.h_advance_exact(gid);
        }
        assert!(!asked_for(&asked, b"gvar"));
        assert!(!asked_for(&asked, b"HVAR"));
        assert!(asked_for(&asked, b"hmtx"), "but it should read hmtx");
    }

    #[test]
    fn an_instance_at_the_default_location_reads_no_variation_table() {
        let asked = Arc::new(Mutex::new(Vec::new()));
        let font = callback_font(VAR, asked.clone(), &[]);
        // All-zero coordinates are the default location, and an instance
        // collapses them to none.
        let instance = at(&font, 0.0);
        assert!(instance.normalized_coords().is_empty());
        let _ = instance.glyph_metrics().h_advance_exact(GlyphId::new(1));
        assert!(!asked_for(&asked, b"gvar"));
        assert!(!asked_for(&asked, b"HVAR"));
    }

    #[test]
    fn hvar_answering_keeps_gvar_and_the_outlines_unread() {
        let asked = Arc::new(Mutex::new(Vec::new()));
        let font = callback_font(VAR, asked.clone(), &[]);
        let _ = at(&font, -0.75)
            .glyph_metrics()
            .h_advance_exact(GlyphId::new(1));
        assert!(asked_for(&asked, b"HVAR"));
        for cold in [b"gvar", b"glyf", b"loca"] {
            assert!(
                !asked_for(&asked, cold),
                "HVAR answered, so {} should never have been copied",
                Tag::new(cold)
            );
        }
    }

    #[test]
    fn taking_the_metrics_reads_hmtx_but_nothing_that_varies() {
        // Every measurement needs `hmtx`, so it is read up front. What a
        // location changes is not: that waits until something asks.
        let asked = Arc::new(Mutex::new(Vec::new()));
        let font = callback_font(VAR, asked.clone(), &[]);
        let instance = at(&font, -0.75);
        let metrics = instance.glyph_metrics();
        assert!(asked_for(&asked, b"hmtx"));
        for cold in [b"HVAR", b"gvar", b"glyf", b"loca"] {
            assert!(!asked_for(&asked, cold), "read {}", Tag::new(cold));
        }
        // And measuring one glyph then reads only what answers.
        let _ = metrics.h_advance_exact(GlyphId::new(1));
        assert!(asked_for(&asked, b"HVAR"));
        assert!(!asked_for(&asked, b"gvar"));
    }
}
