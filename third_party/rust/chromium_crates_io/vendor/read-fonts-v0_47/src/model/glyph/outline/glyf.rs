//! Outlines from the `glyf` table.

use crate::model::Error;
use crate::{
    model::{
        glyph::outline::{DrawOptions, DrawnMetrics, PathSink, UnscaledOutlineSink},
        Font,
    },
    tables::{
        glyf::Glyf,
        glyf::{
            outline::{
                Buffers, OutlineContext, OutlinePlan, Scale, Scale26Dot6, ScaleF32, Unscaled,
            },
            PointCoord, PHANTOM_POINT_COUNT,
        },
        gvar::{GlyphVariationData, SelectedGvar},
        hmtx::GlyphMetricRecords,
        loca::{Loca, LocaGlyph},
    },
    types::{F2Dot14, Fixed, GlyphId, Point},
};

/// The arithmetic used to scale an outline.
///
/// Drawing uses fixed point. Floating point is used for measurements that
/// match HarfBuzz, which keeps every fraction.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum Precision {
    /// 26.6 fixed point, as used by FreeType.
    ///
    /// Values are rounded to 1/64 of a pixel during scaling. Without a size,
    /// this preserves implied midpoints that fall on half units.
    Fixed,
    /// Single precision floating point.
    ///
    /// This keeps the fraction of every coordinate and variation delta.
    Float,
}

/// A TrueType outline from a [`Font`], ready to load.
///
/// Contains the [`OutlinePlan`] used for both sizing and loading, so the
/// glyph is only walked once no matter how many times it is drawn. The font
/// is the [`OutlineContext`] used for loading. To hint, pass a
/// [`Hinter`](crate::tables::glyf::outline::Hinter) to [`OutlinePlan::load`]
/// with the same context.
#[derive(Clone)]
pub struct GlyfGlyph<'a> {
    context: GlyfContext<'a>,
    plan: OutlinePlan<'a>,
}

impl<'a> GlyfGlyph<'a> {
    pub(crate) fn new(context: GlyfContext<'a>, glyph: GlyphId) -> Option<Self> {
        let plan = OutlinePlan::new(&context, glyph).ok()?;
        Some(Self { context, plan })
    }

    /// Returns the glyph identifier.
    pub fn glyph(&self) -> GlyphId {
        self.plan.glyph
    }

    /// Returns the font, which is the context used to load the plan.
    pub fn font(&self) -> &'a Font {
        self.context.font
    }

    /// Returns the plan that describes the outline's structure and memory
    /// requirements.
    pub fn plan(&self) -> &OutlinePlan<'a> {
        &self.plan
    }

    /// Returns the size in bytes of the working memory needed for drawing.
    pub fn required_memory_size(&self) -> usize {
        plan_buffer_size(&self.plan)
    }

    pub(crate) fn draw(
        &self,
        options: &DrawOptions,
        precision: Precision,
        buf: &mut [u8],
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, Error> {
        draw_plan(&self.plan, &self.context, options, precision, buf, sink)
    }

    /// Returns the size in bytes of the working memory needed to load the
    /// outline in font units.
    pub fn required_memory_size_unscaled(&self) -> usize {
        self.plan
            .buffer_lengths::<Unscaled>()
            .packed_len::<Unscaled>()
    }

    /// Loads the outline in font units and sends its points to `sink`.
    pub(crate) fn draw_unscaled(
        &self,
        buf: &mut [u8],
        sink: &mut dyn UnscaledOutlineSink,
    ) -> Result<DrawnMetrics, Error> {
        let lengths = self.plan.buffer_lengths::<Unscaled>();
        let buffers = Buffers::from_bytes(buf, &lengths)?;
        let outline = self.plan.load(&self.context, &Unscaled, buffers, None)?;
        let (points, flags) = (outline.points(), outline.flags());
        let mut start = 0;
        for end in outline.contours() {
            let end = *end as usize;
            if end < start || end >= points.len() {
                return Err(Error::Malformed);
            }
            sink.start_contour();
            for (point, flags) in points[start..=end].iter().zip(&flags[start..=end]) {
                sink.point(point.x, point.y, flags.without_markers());
            }
            sink.end_contour();
            start = end + 1;
        }
        Ok(DrawnMetrics {
            h_advance: Some(outline.adjusted_advance_width() as f32),
            x_bearing: Some(outline.adjusted_lsb() as f32),
            has_overlaps: self.plan.has_overlaps,
            ..Default::default()
        })
    }
}

/// Returns the size in bytes of the working memory needed to draw `plan`.
///
/// The size is computed for fixed point, which has the larger layout
/// because it keeps a copy of the unscaled points. A floating point draw
/// fits in the same buffer.
pub(crate) fn plan_buffer_size(plan: &OutlinePlan<'_>) -> usize {
    plan.buffer_lengths::<Scale26Dot6>()
        .packed_len::<Scale26Dot6>()
}

/// Loads `plan` into `buf` with the given options and precision, and draws
/// it into `sink`.
pub(crate) fn draw_plan<'a>(
    plan: &OutlinePlan<'a>,
    context: &'a dyn OutlineContext<'a>,
    options: &DrawOptions,
    precision: Precision,
    buf: &mut [u8],
    sink: &mut dyn PathSink,
) -> Result<DrawnMetrics, Error> {
    let units_per_em = context.units_per_em();
    match precision {
        Precision::Fixed => load_and_draw(
            plan,
            context,
            &Scale26Dot6::new(options.ppem, units_per_em),
            options,
            buf,
            sink,
        ),
        Precision::Float => load_and_draw(
            plan,
            context,
            &ScaleF32::new(options.ppem, units_per_em),
            options,
            buf,
            sink,
        ),
    }
}

fn load_and_draw<'a, S: Scale>(
    plan: &OutlinePlan<'a>,
    context: &'a dyn OutlineContext<'a>,
    scale: &S,
    options: &DrawOptions,
    buf: &mut [u8],
    mut sink: &mut dyn PathSink,
) -> Result<DrawnMetrics, Error> {
    let lengths = plan.buffer_lengths::<S>();
    let buffers = Buffers::from_bytes(buf, &lengths)?;
    let outline = plan.load(context, scale, buffers, None)?;
    let metrics = DrawnMetrics {
        h_advance: Some(outline.adjusted_advance_width().to_f32()),
        x_bearing: Some(outline.adjusted_lsb().to_f32()),
        has_overlaps: plan.has_overlaps,
        ..Default::default()
    };
    if !outline.to_path(options.contour_start, &mut sink) {
        return Err(Error::Malformed);
    }
    Ok(metrics)
}

/// The tables, location and scalars a `glyf` outline loads from, looked up
/// once for a font.
///
/// Loading asks for these for every glyph, so a draw reads them from here
/// rather than from the font's caches each time.
#[derive(Clone, Copy)]
pub(crate) struct GlyfContext<'a> {
    pub(crate) font: &'a Font,
    tables: &'a (Glyf<'a>, Loca<'a>),
    gvar: Option<&'a SelectedGvar<'a>>,
    h_metrics: &'a GlyphMetricRecords<'a>,
    v_metrics: &'a GlyphMetricRecords<'a>,
    coords: &'a [F2Dot14],
    gvar_scalars: &'a [Fixed],
    /// The ascender and descender that place vertical phantom points for a
    /// font with no vertical metrics.
    h_line: (i32, i32),
    num_glyphs: u32,
    units_per_em: u16,
    has_hvar: bool,
}

impl<'a> GlyfContext<'a> {
    /// Returns the context for `font`, or `None` if it has no `glyf` table.
    pub(crate) fn new(font: &'a Font) -> Option<Self> {
        let tables = font.selected_glyf_loca()?;
        let v_metrics = font.v_metric_records();
        let h_line = if v_metrics.is_empty() {
            OutlineContext::h_line_metrics(&font)
        } else {
            (0, 0)
        };
        Some(Self {
            font,
            tables,
            gvar: font.selected_gvar(),
            h_metrics: font.h_metric_records(),
            v_metrics,
            coords: font.normalized_coords(),
            gvar_scalars: font.gvar_scalars(),
            h_line,
            num_glyphs: font.num_glyphs(),
            units_per_em: font.units_per_em(),
            has_hvar: font.hvar().is_some(),
        })
    }

    /// Returns the bearing and advance in `records` for `glyph`.
    fn metrics(&self, records: &GlyphMetricRecords<'_>, glyph: GlyphId) -> (i32, i32) {
        (
            records
                .stated_side_bearing(glyph)
                .unwrap_or_default()
                .to_i32(),
            records.stated_advance(self.num_glyphs, glyph).to_i32(),
        )
    }
}

impl<'a> OutlineContext<'a> for GlyfContext<'a> {
    fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>> {
        let (glyf, loca) = self.tables;
        loca.get(glyph, glyf)
    }

    fn glyph_variation_data(&self, glyph: GlyphId) -> Option<GlyphVariationData<'a>> {
        self.gvar?.glyph_variation_data(glyph).ok()?
    }

    fn has_gvar(&self) -> bool {
        self.gvar.is_some()
    }

    fn phantom_point_deltas(&self, glyph: GlyphId) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]> {
        let (glyf, loca) = self.tables;
        self.gvar?.phantom_point_deltas_with_scalars(
            glyf,
            loca,
            self.coords,
            self.gvar_scalars,
            glyph,
        )
    }

    fn gvar_scalars(&self) -> &[Fixed] {
        self.gvar_scalars
    }

    fn coords(&self) -> &[F2Dot14] {
        self.coords
    }

    fn units_per_em(&self) -> u16 {
        self.units_per_em
    }

    fn h_metrics(&self, glyph: GlyphId) -> (i32, i32) {
        self.metrics(self.h_metrics, glyph)
    }

    fn v_metrics(&self, glyph: GlyphId) -> Option<(i32, i32)> {
        (!self.v_metrics.is_empty()).then(|| self.metrics(self.v_metrics, glyph))
    }

    fn h_line_metrics(&self) -> (i32, i32) {
        self.h_line
    }

    fn has_hvar(&self) -> bool {
        self.has_hvar
    }
}

/// A font read at a location other than its own.
///
/// A `VARC` component defines its own coordinates, so its glyph is loaded
/// through this type rather than the font. The font's cached scalars only
/// apply to its own location, so this type does not provide them.
#[derive(Clone, Copy)]
pub(crate) struct FontAt<'a> {
    pub(crate) context: GlyfContext<'a>,
    pub(crate) coords: &'a [F2Dot14],
}

impl<'a> OutlineContext<'a> for FontAt<'a> {
    fn glyph(&self, glyph: GlyphId) -> Option<LocaGlyph<'a>> {
        self.context.glyph(glyph)
    }

    fn glyph_variation_data(&self, glyph: GlyphId) -> Option<GlyphVariationData<'a>> {
        self.context.glyph_variation_data(glyph)
    }

    fn has_gvar(&self) -> bool {
        self.context.has_gvar()
    }

    fn phantom_point_deltas(&self, glyph: GlyphId) -> Option<[Point<Fixed>; PHANTOM_POINT_COUNT]> {
        let (glyf, loca) = self.context.tables;
        self.context
            .gvar?
            .phantom_point_deltas_with_scalars(glyf, loca, self.coords, &[], glyph)
    }

    fn coords(&self) -> &[F2Dot14] {
        self.coords
    }

    fn units_per_em(&self) -> u16 {
        self.context.units_per_em
    }

    fn h_metrics(&self, glyph: GlyphId) -> (i32, i32) {
        self.context.h_metrics(glyph)
    }

    fn v_metrics(&self, glyph: GlyphId) -> Option<(i32, i32)> {
        self.context.v_metrics(glyph)
    }

    fn h_line_metrics(&self) -> (i32, i32) {
        self.context.h_line
    }

    fn has_hvar(&self) -> bool {
        self.context.has_hvar
    }
}
