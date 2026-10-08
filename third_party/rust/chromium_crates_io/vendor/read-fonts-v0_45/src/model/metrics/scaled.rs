//! Metrics in a caller's own units.

use super::{
    Decoration, GlyphExtents, GlyphMetrics, LineBox, LineExtents, Metrics, ScriptMetrics,
    StyleMetrics,
};
use types::{BoundingBox, F26Dot6, F48Dot16, Fixed, GlyphId};

/// Converts a measurement in design units into a caller's own units.
///
/// Written by whatever owns scaling in a text stack: a shaper, or the bridge
/// between one and this crate. Callers that want design units read them from
/// [`GlyphMetrics`], [`Metrics`] and [`StyleMetrics`] and never implement this.
///
/// Scaling is more than a multiplication, which is why it is a trait rather
/// than a factor this crate would apply. HarfBuzz scales each axis by its own
/// fixed point number, in a format the caller picks, and is particular about
/// how it rounds and in what order it applies things. An implementation keeps
/// all of that, while the rules for combining several measurements into one
/// metric stay here.
///
/// The arithmetic is on this trait rather than bounds on
/// [`Value`](Self::Value) for the same reason: an implementation decides what
/// an overflowing sum does, and which way a halved odd number goes. HarfBuzz
/// halves two ways within one function, so neither is safe to assume.
pub trait Scale {
    /// What a scaled measurement is expressed as.
    type Value: Copy;

    /// Returns `a` plus `b`.
    ///
    /// This and the two below take no scale. They describe
    /// [`Value`](Self::Value) itself, so every scale of that type answers
    /// them alike.
    fn add(a: Self::Value, b: Self::Value) -> Self::Value;

    /// Returns `a` less `b`.
    fn sub(a: Self::Value, b: Self::Value) -> Self::Value;

    /// Returns half of `value`.
    fn half(value: Self::Value) -> Self::Value;

    /// Scales a measurement along the x axis.
    fn scale_x(&self, value: F48Dot16) -> Self::Value;

    /// Scales a measurement along the y axis.
    ///
    /// Separate from [`scale_x`](Self::scale_x) because the two axes need
    /// not agree, and because a caller whose y runs down the page says so
    /// here.
    fn scale_y(&self, value: F48Dot16) -> Self::Value;

    /// Scales where a glyph's ink sits.
    ///
    /// One call rather than four, so that a scale running the other way up
    /// settles in one place what that means for a bearing and a size. The
    /// scale also decides whether to round the box's corners before scaling.
    fn scale_glyph_extents(&self, extents: GlyphExtents<F48Dot16>) -> GlyphExtents<Self::Value>;

    /// Scales a region given by its corners.
    ///
    /// One call for the same reason as [`scale_glyph_extents`](Self::scale_glyph_extents):
    /// a scale running the other way up decides here which corner ends up
    /// least.
    fn scale_rect(&self, bounds: BoundingBox<F48Dot16>) -> BoundingBox<Self::Value>;
}

/// Scales design-unit metrics to 26.6 pixels with FreeType-style arithmetic.
///
/// Sums saturate and halves round toward negative infinity.
///
/// FreeType scales whole design units, so a measurement carrying a fraction
/// is rounded to a unit before it is scaled. [`ScaleF32`] keeps the
/// fraction. Outline loading is chosen by the metrics implementation; this
/// scale alone does not reproduce the bounds returned by `skrifa`.
#[derive(Copy, Clone, Debug)]
pub struct Scale26Dot6 {
    x: Fixed,
    y: Fixed,
}

impl Scale26Dot6 {
    /// A scale with a size for each axis, which HarfBuzz keeps apart.
    pub fn new(x_ppem: f32, y_ppem: f32, units_per_em: u16) -> Self {
        Self {
            x: Self::factor(x_ppem, units_per_em),
            y: Self::factor(y_ppem, units_per_em),
        }
    }

    /// A scale mapping `units_per_em` design units onto `ppem` pixels.
    pub fn from_ppem(ppem: f32, units_per_em: u16) -> Self {
        Self::new(ppem, ppem, units_per_em)
    }

    /// The factor FreeType multiplies whole design units by.
    ///
    /// It folds the conversion to 1/64 pixel into itself, so that a
    /// 16.16 multiply against a whole design unit lands with 26.6 in its
    /// bits. That is a trick rather than a type, and it is the one FreeType
    /// plays.
    fn factor(ppem: f32, units_per_em: u16) -> Fixed {
        Fixed::from_bits((ppem * 64.0) as i32) / Fixed::from_bits(units_per_em.max(1) as i32)
    }

    fn scale(value: F48Dot16, factor: Fixed) -> F26Dot6 {
        F26Dot6::from_bits((Fixed::from_bits(value.to_i32()) * factor).to_bits())
    }
}

impl Scale for Scale26Dot6 {
    type Value = F26Dot6;

    fn add(a: F26Dot6, b: F26Dot6) -> F26Dot6 {
        a.saturating_add(b)
    }

    fn sub(a: F26Dot6, b: F26Dot6) -> F26Dot6 {
        a.saturating_sub(b)
    }

    fn half(value: F26Dot6) -> F26Dot6 {
        F26Dot6::from_bits(value.to_bits() >> 1)
    }

    fn scale_x(&self, value: F48Dot16) -> F26Dot6 {
        Self::scale(value, self.x)
    }

    fn scale_y(&self, value: F48Dot16) -> F26Dot6 {
        Self::scale(value, self.y)
    }

    fn scale_glyph_extents(&self, e: GlyphExtents<F48Dot16>) -> GlyphExtents<F26Dot6> {
        if e.width <= F48Dot16::ZERO || e.height <= F48Dot16::ZERO {
            return GlyphExtents::default();
        }
        // Match FreeType's FT_RoundFix: ties round away from zero.
        let round = |value: F48Dot16| {
            let bits = value.to_bits();
            F48Dot16::from_bits(bits.wrapping_add(0x8000 - i64::from(bits < 0)) & !0xffff)
        };
        let left = round(e.x_bearing);
        let top = round(e.y_bearing);
        let right = round(e.x_bearing + e.width);
        let bottom = round(e.y_bearing - e.height);
        GlyphExtents {
            x_bearing: self.scale_x(left),
            y_bearing: self.scale_y(top),
            width: self.scale_x(right - left),
            height: self.scale_y(top - bottom),
        }
    }

    fn scale_rect(&self, b: BoundingBox<F48Dot16>) -> BoundingBox<F26Dot6> {
        BoundingBox {
            x_min: self.scale_x(b.x_min),
            y_min: self.scale_y(b.y_min),
            x_max: self.scale_x(b.x_max),
            y_max: self.scale_y(b.y_max),
        }
    }
}

/// Scales design units to `f32` pixels.
///
/// Nothing here rounds, so a measurement carrying a fraction keeps it, which
/// [`Scale26Dot6`] does not.
#[derive(Copy, Clone, Debug)]
pub struct ScaleF32 {
    x: f32,
    y: f32,
}

impl ScaleF32 {
    /// A scale with a size for each axis, which HarfBuzz keeps apart.
    pub fn new(x_ppem: f32, y_ppem: f32, units_per_em: u16) -> Self {
        let per_unit = |ppem: f32| ppem / units_per_em.max(1) as f32;
        Self {
            x: per_unit(x_ppem),
            y: per_unit(y_ppem),
        }
    }

    /// A scale mapping `units_per_em` design units onto `ppem` pixels.
    pub fn from_ppem(ppem: f32, units_per_em: u16) -> Self {
        Self::new(ppem, ppem, units_per_em)
    }
}

impl Scale for ScaleF32 {
    type Value = f32;

    fn add(a: f32, b: f32) -> f32 {
        a + b
    }

    fn sub(a: f32, b: f32) -> f32 {
        a - b
    }

    fn half(value: f32) -> f32 {
        value * 0.5
    }

    fn scale_x(&self, value: F48Dot16) -> f32 {
        value.to_f32() * self.x
    }

    fn scale_y(&self, value: F48Dot16) -> f32 {
        value.to_f32() * self.y
    }

    fn scale_glyph_extents(&self, e: GlyphExtents<F48Dot16>) -> GlyphExtents<f32> {
        GlyphExtents {
            x_bearing: self.scale_x(e.x_bearing),
            y_bearing: self.scale_y(e.y_bearing),
            width: self.scale_x(e.width),
            height: self.scale_y(e.height),
        }
    }

    fn scale_rect(&self, b: BoundingBox<F48Dot16>) -> BoundingBox<f32> {
        BoundingBox {
            x_min: self.scale_x(b.x_min),
            y_min: self.scale_y(b.y_min),
            x_max: self.scale_x(b.x_max),
            y_max: self.scale_y(b.y_max),
        }
    }
}

/// Measurements of a font as a whole, in a caller's own units.
///
/// A view rather than a set of scaled fields, so reading one measurement
/// does not compute the rest. What carries no unit, such as the glyph count,
/// is reported unchanged.
#[derive(Clone, Copy)]
pub struct ScaledMetrics<'a, S: Scale> {
    metrics: &'a Metrics,
    scale: S,
}

impl<'a, S: Scale> ScaledMetrics<'a, S> {
    pub(crate) fn new(metrics: &'a Metrics, scale: S) -> Self {
        Self { metrics, scale }
    }

    /// Returns the number of glyphs in the font.
    #[inline]
    pub fn num_glyphs(&self) -> u32 {
        self.metrics.num_glyphs
    }

    /// Returns the size of the font's design em.
    #[inline]
    pub fn units_per_em(&self) -> u16 {
        self.metrics.units_per_em
    }

    /// Returns the box enclosing every glyph in the font.
    #[inline]
    pub fn bounds(&self) -> BoundingBox<S::Value> {
        self.scale.scale_rect(self.metrics.bounds)
    }

    /// Returns the ascender, descender and gap for horizontal text.
    ///
    /// Resolved as [`Metrics::h_line`] resolves it, with each end
    /// scaled on its own so that a caller subtracting them gets the height
    /// the glyph metrics report for a font that stacks by this line.
    #[inline]
    pub fn h_line(&self) -> Option<LineBox<S::Value>> {
        self.metrics.h_line().map(|line| self.scale_line_y(line))
    }

    /// Returns the line `hhea` provides.
    #[inline]
    pub fn hhea_line(&self) -> Option<LineBox<S::Value>> {
        self.metrics.hhea_line.map(|line| self.scale_line_y(line))
    }

    /// Returns the typographic line `OS/2` provides.
    #[inline]
    pub fn typo_line(&self) -> Option<LineBox<S::Value>> {
        self.metrics.typo_line.map(|line| self.scale_line_y(line))
    }

    /// Returns whether the font asks for its typographic line to be read.
    #[inline]
    pub fn use_typo_metrics(&self) -> bool {
        self.metrics.use_typo_metrics
    }

    /// Returns the line `vhea` provides, where the font has one.
    ///
    /// A vertical line runs across the page, so its measurements scale along
    /// x where a horizontal line's scale along y.
    #[inline]
    pub fn vhea_line(&self) -> Option<LineBox<S::Value>> {
        self.metrics.vhea_line.map(|line| LineBox {
            ascender: self.scale.scale_x(line.ascender),
            descender: self.scale.scale_x(line.descender),
            line_gap: self.scale.scale_x(line.line_gap),
        })
    }

    /// Returns the height of a lowercase letter, where the font provides one.
    #[inline]
    pub fn x_height(&self) -> Option<S::Value> {
        self.scaled_y(self.metrics.x_height)
    }

    /// Returns the height of a capital letter, where the font provides one.
    #[inline]
    pub fn cap_height(&self) -> Option<S::Value> {
        self.scaled_y(self.metrics.cap_height)
    }

    /// Returns the widest advance in the font, where it provides one.
    #[inline]
    pub fn max_advance_width(&self) -> Option<S::Value> {
        self.metrics
            .max_advance_width
            .map(|value| self.scale.scale_x(value))
    }

    /// Returns the tallest advance in the font, where it provides one.
    #[inline]
    pub fn max_advance_height(&self) -> Option<S::Value> {
        self.scaled_y(self.metrics.max_advance_height)
    }

    /// Returns the average advance in the font, where it provides one.
    #[inline]
    pub fn average_char_width(&self) -> Option<S::Value> {
        self.metrics
            .average_char_width
            .map(|value| self.scale.scale_x(value))
    }

    /// Returns the line outside which the font asks not to be clipped.
    #[inline]
    pub fn win_line(&self) -> Option<LineExtents<S::Value>> {
        self.metrics.win_line.map(|line| self.scale_line(line))
    }

    #[inline]
    fn scale_line_y(&self, line: LineBox<F48Dot16>) -> LineBox<S::Value> {
        LineBox {
            ascender: self.scale.scale_y(line.ascender),
            descender: self.scale.scale_y(line.descender),
            line_gap: self.scale.scale_y(line.line_gap),
        }
    }

    #[inline]
    fn scale_line(&self, line: LineExtents<F48Dot16>) -> LineExtents<S::Value> {
        LineExtents {
            ascender: self.scale.scale_y(line.ascender),
            descender: self.scale.scale_y(line.descender),
        }
    }

    #[inline]
    fn scaled_y(&self, value: Option<F48Dot16>) -> Option<S::Value> {
        value.map(|value| self.scale.scale_y(value))
    }
}

impl Metrics {
    /// Returns these metrics in the units `scale` describes.
    #[inline]
    pub fn scaled<S: Scale>(&self, scale: S) -> ScaledMetrics<'_, S> {
        ScaledMetrics::new(self, scale)
    }
}

/// Style measurements in a caller's own units.
///
/// Scaling is done when a measurement is requested. The italic angle and
/// fixed-pitch flag have no length to scale and are returned unchanged.
#[derive(Clone, Copy)]
pub struct ScaledStyleMetrics<'a, S: Scale> {
    metrics: &'a StyleMetrics,
    scale: S,
}

impl<'a, S: Scale> ScaledStyleMetrics<'a, S> {
    pub(crate) fn new(metrics: &'a StyleMetrics, scale: S) -> Self {
        Self { metrics, scale }
    }

    /// Returns the suggested subscript em box and placement.
    pub fn subscript(&self) -> Option<ScriptMetrics<S::Value>> {
        self.metrics
            .subscript
            .map(|script| self.scale_script(script))
    }

    /// Returns the suggested superscript em box and placement.
    pub fn superscript(&self) -> Option<ScriptMetrics<S::Value>> {
        self.metrics
            .superscript
            .map(|script| self.scale_script(script))
    }

    /// Returns the suggested underline position and thickness.
    pub fn underline(&self) -> Option<Decoration<S::Value>> {
        self.metrics
            .underline
            .map(|decoration| self.scale_decoration(decoration))
    }

    /// Returns the suggested strikeout position and thickness.
    pub fn strikethrough(&self) -> Option<Decoration<S::Value>> {
        self.metrics
            .strikethrough
            .map(|decoration| self.scale_decoration(decoration))
    }

    /// Returns the italic angle in degrees, unaffected by scaling.
    pub fn italic_angle(&self) -> Option<Fixed> {
        self.metrics.italic_angle
    }

    /// Returns whether the font reports a fixed pitch.
    pub fn is_fixed_pitch(&self) -> Option<bool> {
        self.metrics.is_fixed_pitch
    }

    fn scale_script(&self, script: ScriptMetrics) -> ScriptMetrics<S::Value> {
        ScriptMetrics {
            x_size: self.scale.scale_x(script.x_size),
            y_size: self.scale.scale_y(script.y_size),
            x_offset: self.scale.scale_x(script.x_offset),
            y_offset: self.scale.scale_y(script.y_offset),
        }
    }

    fn scale_decoration(&self, decoration: Decoration) -> Decoration<S::Value> {
        Decoration {
            position: self.scale.scale_y(decoration.position),
            thickness: self.scale.scale_y(decoration.thickness),
        }
    }
}

impl StyleMetrics {
    /// Returns these style measurements in the units `scale` describes.
    pub fn scaled<S: Scale>(&self, scale: S) -> ScaledStyleMetrics<'_, S> {
        ScaledStyleMetrics::new(self, scale)
    }
}

/// Measurements of individual glyphs, in a caller's own units.
///
/// Holds borrows and a scale, so it is as cheap to make as the metrics it
/// scales and can be taken per query. It remembers nothing between calls: a
/// caller wanting measurements kept holds them itself.
#[derive(Clone, Copy)]
pub struct ScaledGlyphMetrics<'a, 'extents, S: Scale> {
    metrics: GlyphMetrics<'a>,
    scale: S,
    line: Option<LineExtents<S::Value>>,
    extents: Option<&'extents dyn Fn(GlyphId) -> Option<GlyphExtents<S::Value>>>,
}

impl<'a, 'extents, S: Scale> ScaledGlyphMetrics<'a, 'extents, S> {
    pub(crate) fn new(metrics: GlyphMetrics<'a>, scale: S) -> Self {
        Self {
            metrics,
            scale,
            line: None,
            extents: None,
        }
    }

    /// Measures against a line the caller supplies rather than the font's own.
    ///
    /// A caller that reads its line from elsewhere supplies it here, so the
    /// metrics built on it agree with the rest of its layout. The line is in
    /// the same units as everything else this type reports, so nothing is
    /// converted back into design units.
    ///
    /// `None` leaves the font's own line in place, so a caller can pass
    /// whatever it happens to have without first checking.
    pub fn with_line_extents(self, line: Option<LineExtents<S::Value>>) -> Self {
        Self { line, ..self }
    }

    /// Measures ink by a function the caller supplies rather than the font.
    ///
    /// A caller that measures glyphs elsewhere supplies that here, so the
    /// metrics built on ink agree with the rest of what it reports. The
    /// function answers in the same units as everything else this type
    /// reports, so nothing is converted back into design units. `None` from
    /// the function means extents are unavailable; return
    /// `Some(GlyphExtents::default())` for a present glyph with no ink.
    ///
    /// `None` leaves the font's own measurement in place, so a caller can
    /// pass whatever it happens to have without first checking.
    ///
    /// The function is borrowed only for the lifetime of the returned view.
    pub fn with_glyph_extents<'new>(
        self,
        extents: Option<&'new dyn Fn(GlyphId) -> Option<GlyphExtents<S::Value>>>,
    ) -> ScaledGlyphMetrics<'a, 'new, S> {
        ScaledGlyphMetrics {
            metrics: self.metrics,
            scale: self.scale,
            line: self.line,
            extents,
        }
    }

    /// Returns the advance width of `glyph`.
    #[inline]
    pub fn h_advance(&self, glyph: GlyphId) -> S::Value {
        self.scale.scale_x(self.metrics.h_advance_exact(glyph))
    }

    /// Writes the advance width of each glyph to its slot, in order.
    #[inline]
    pub fn h_advance_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(S::Value) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        self.metrics
            .h_advance_batched(|value| convert(self.scale.scale_x(value)), glyphs);
    }

    /// Returns the advance height of `glyph`.
    ///
    /// A font with no vertical metrics stacks its glyphs by the line, the
    /// only measurement here a caller can supply. One that has them is
    /// measured by what it says, and a supplied line changes nothing.
    #[inline]
    pub fn v_advance(&self, glyph: GlyphId) -> S::Value {
        // Only a font with no vertical metrics stacks by the line; one
        // that has them is read below, supplied line or not.
        match self.line_height_fallback() {
            Some(height) => height,
            None => self.scale.scale_y(self.metrics.v_advance_exact(glyph)),
        }
    }

    /// Writes the advance height of each glyph to its slot, in order.
    #[inline]
    pub fn v_advance_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(S::Value) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        // Settled before the run, as the table to read is. Only a font
        // with no vertical metrics reaches this, and then all its glyphs
        // advance the same; one that has them is read below, supplied line
        // or not.
        if let Some(height) = self.line_height_fallback() {
            for (_, out) in glyphs {
                *out = convert(height);
            }
            return;
        }
        self.metrics
            .v_advance_batched(|value| convert(self.scale.scale_y(value)), glyphs);
    }

    /// Returns the y coordinate of `glyph`'s vertical origin.
    ///
    /// Where the glyph sits relative to the pen when text runs down the page.
    /// Only y: the x coordinate is half the advance width, which a caller
    /// already holds, and deriving it here would hand back half of what it
    /// passed in.
    ///
    /// Four things can answer, in the order HarfBuzz asks them: `VORG`, then
    /// the glyf top phantom point where the font has vertical metrics, then
    /// the ink centered in the line, then the ascender alone. A supplied
    /// measurement of ink is read by the last two.
    pub fn v_origin_y(&self, glyph: GlyphId) -> S::Value {
        let mut origin = self.scale.scale_y(F48Dot16::ZERO);
        self.v_origin_y_batched(|value| value, core::iter::once((glyph, &mut origin)));
        origin
    }

    /// Writes the vertical origin of each glyph to its slot, in order.
    ///
    /// The line is measured only if a glyph reaches the extents fallback, and
    /// then reused for the rest of the run.
    pub fn v_origin_y_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(S::Value) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        let mut line = None;
        self.metrics.v_origins_batched(
            |glyph, origin| {
                let value = match origin {
                    Some(origin) => self.scale.scale_y(origin),
                    None => self
                        .origin_against_ink(glyph, *line.get_or_insert_with(|| self.origin_line())),
                };
                convert(value)
            },
            |glyphs| {
                let line = self.origin_line();
                self.extents_batched(
                    |extents| convert(self.origin_from_extents(extents, line)),
                    glyphs,
                );
            },
            glyphs,
        );
    }

    /// Returns where `glyph`'s ink sits.
    ///
    /// An empty glyph has zero extents. `None` means the glyph is unavailable
    /// or its extents could not be read. A supplied extents function may use
    /// `None` for any glyph it does not measure.
    #[inline]
    pub fn extents(&self, glyph: GlyphId) -> Option<GlyphExtents<S::Value>> {
        match self.extents {
            Some(extents) => extents(glyph),
            None => {
                let mut extents = None;
                self.metrics.extents_batched(
                    |value| value.map(|extents| self.scale.scale_glyph_extents(extents)),
                    core::iter::once((glyph, &mut extents)),
                );
                extents
            }
        }
    }

    /// Writes where each glyph's ink sits to its slot, in order.
    #[inline]
    pub fn extents_batched<'o, V: 'o>(
        &self,
        convert: impl Fn(Option<GlyphExtents<S::Value>>) -> V,
        glyphs: impl Iterator<Item = (GlyphId, &'o mut V)>,
    ) {
        // Settled before the run, as the source to read is: a supplied
        // function answers for every glyph, and the font is never read.
        if let Some(extents) = self.extents {
            for (glyph, out) in glyphs {
                *out = convert(extents(glyph));
            }
            return;
        }
        self.metrics.extents_batched(
            |extents| convert(extents.map(|extents| self.scale.scale_glyph_extents(extents))),
            glyphs,
        );
    }

    /// The height a glyph takes from the line, where it takes one.
    ///
    /// `None` for two reasons that mean the same thing here: the font has
    /// vertical metrics of its own, so the line is not what its glyphs stack
    /// by; or it has no line either, which the unscaled metrics answer with
    /// an em. A supplied line applies only in the first case, so supplying
    /// one never overrides what a font says about itself.
    ///
    /// Each end is scaled before the two are subtracted, so a scale that
    /// rounds rounds them as a caller supplying its own line would, and the
    /// answer does not turn on whether one was supplied.
    #[inline]
    fn line_height_fallback(&self) -> Option<S::Value> {
        if self.metrics.states_vertical_advances() {
            return None;
        }
        let line = self.origin_line();
        Some(S::sub(line.ascender, line.descender))
    }

    /// The line a glyph stacks by, in the units this type reports.
    ///
    /// The supplied line where there is one, and the font's otherwise. A font
    /// that states no line at all is given one proportioned to its em, as
    /// HarfBuzz gives a font it can read no extents from.
    fn origin_line(&self) -> LineExtents<S::Value> {
        // A supplied line is reported as it was given: a caller stating its
        // own has already decided which way its ends run.
        if let Some(line) = self.line {
            return line;
        }
        let line = self.metrics.origin_line();
        // Each end is scaled, rather than the height between them, so a scale
        // that rounds reports ends a caller can lay out against.
        LineExtents {
            ascender: self.scale.scale_y(line.ascender),
            descender: self.scale.scale_y(line.descender),
        }
    }

    /// The last two rungs of [`v_origin_y`](Self::v_origin_y), which measure
    /// against the ink.
    fn origin_against_ink(&self, glyph: GlyphId, line: LineExtents<S::Value>) -> S::Value {
        self.origin_from_extents(self.extents(glyph), line)
    }

    fn origin_from_extents(
        &self,
        extents: Option<GlyphExtents<S::Value>>,
        line: LineExtents<S::Value>,
    ) -> S::Value {
        let Some(extents) = extents else {
            return line.ascender;
        };
        let line_height = S::sub(line.ascender, line.descender);
        S::add(
            extents.y_bearing,
            S::half(S::sub(line_height, extents.height)),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::Blob;
    use crate::model::Font;
    use alloc::sync::Arc;
    use types::{GlyphId, Tag};

    /// Scales design units to pixels at a size, halving toward zero.
    #[derive(Clone, Copy)]
    struct Ppem {
        ppem: f32,
        upem: u16,
    }

    impl Scale for Ppem {
        type Value = f32;

        fn scale_x(&self, value: F48Dot16) -> f32 {
            value.to_f32() * self.ppem / self.upem as f32
        }

        fn scale_y(&self, value: F48Dot16) -> f32 {
            self.scale_x(value)
        }

        fn scale_glyph_extents(&self, extents: GlyphExtents<F48Dot16>) -> GlyphExtents<f32> {
            GlyphExtents {
                x_bearing: self.scale_x(extents.x_bearing),
                y_bearing: self.scale_y(extents.y_bearing),
                width: self.scale_x(extents.width),
                height: self.scale_y(extents.height),
            }
        }

        fn scale_rect(&self, b: BoundingBox<F48Dot16>) -> BoundingBox<f32> {
            BoundingBox {
                x_min: self.scale_x(b.x_min),
                y_min: self.scale_y(b.y_min),
                x_max: self.scale_x(b.x_max),
                y_max: self.scale_y(b.y_max),
            }
        }

        fn add(a: f32, b: f32) -> f32 {
            a + b
        }

        fn sub(a: f32, b: f32) -> f32 {
            a - b
        }

        fn half(value: f32) -> f32 {
            value / 2.0
        }
    }

    const STATIC: &[u8] = font_test_data::TINOS_SUBSET;
    const VERT: &[u8] = font_test_data::MPLUS1CODE_VERTICAL_SUBSET;
    /// States its vertical origins outright.
    const VORG: &[u8] = font_test_data::VORG;

    fn scaled(data: &[u8], ppem: f32) -> (Font, Ppem) {
        let font = Font::new(data.to_vec(), 0).unwrap();
        let upem = font.metrics().units_per_em;
        (font, Ppem { ppem, upem })
    }

    fn line(ascender: f32, descender: f32) -> LineExtents<f32> {
        LineExtents {
            ascender,
            descender,
        }
    }

    /// Rounds every scaled value to a whole number, as a hinted scale does.
    #[derive(Clone, Copy)]
    struct Rounding(f32, u16);

    impl Scale for Rounding {
        type Value = f32;
        fn scale_x(&self, value: F48Dot16) -> f32 {
            (value.to_f32() * self.0 / self.1 as f32).round()
        }
        fn scale_y(&self, value: F48Dot16) -> f32 {
            self.scale_x(value)
        }
        fn scale_glyph_extents(&self, e: GlyphExtents<F48Dot16>) -> GlyphExtents<f32> {
            GlyphExtents {
                x_bearing: self.scale_x(e.x_bearing),
                y_bearing: self.scale_y(e.y_bearing),
                width: self.scale_x(e.width),
                height: self.scale_y(e.height),
            }
        }

        fn scale_rect(&self, b: BoundingBox<F48Dot16>) -> BoundingBox<f32> {
            BoundingBox {
                x_min: self.scale_x(b.x_min),
                y_min: self.scale_y(b.y_min),
                x_max: self.scale_x(b.x_max),
                y_max: self.scale_y(b.y_max),
            }
        }
        fn add(a: f32, b: f32) -> f32 {
            a + b
        }
        fn sub(a: f32, b: f32) -> f32 {
            a - b
        }
        fn half(value: f32) -> f32 {
            value / 2.0
        }
    }

    #[test]
    fn a_rounding_scale_rounds_the_ends_of_the_line_not_the_height() {
        // A font with no `vmtx` stacks by the line, and the height comes
        // from two ends. Scaling each before subtracting is what a caller
        // stating its own line does, so doing it the other way round would
        // answer differently depending on whether one was supplied. It shows
        // up only where the scale rounds, and then by a whole unit.
        let font = Font::new(STATIC.to_vec(), 0).unwrap();
        let upem = font.metrics().units_per_em;
        let from_font = font.metrics().h_line().unwrap().extents();
        let mut rounded_apart = 0;
        for ppem in [11.0f32, 12.0, 13.0, 14.0, 16.0, 19.0, 24.0] {
            let scale = Rounding(ppem, upem);
            let metrics = font.glyph_metrics().scaled(scale);
            let ends = LineExtents {
                ascender: scale.scale_y(from_font.ascender),
                descender: scale.scale_y(from_font.descender),
            };
            let from_ends = Rounding::sub(ends.ascender, ends.descender);
            assert_eq!(
                metrics.v_advance(GlyphId::new(1)),
                from_ends,
                "at {ppem}ppem"
            );
            // And stating the same line back gives the same answer.
            assert_eq!(
                metrics
                    .with_line_extents(Some(ends))
                    .v_advance(GlyphId::new(1)),
                from_ends,
                "at {ppem}ppem, supplied"
            );
            // The other way round: subtract in design units, scale once.
            if scale.scale_y(from_font.ascender - from_font.descender) != from_ends {
                rounded_apart += 1;
            }
        }
        assert!(
            rounded_apart > 0,
            "no size here rounds the two ways apart, so this proves nothing"
        );
    }

    #[test]
    fn the_scaled_line_is_the_one_the_glyph_metrics_stack_by() {
        // The reason both views exist. A caller reading the line from the
        // global metrics and subtracting its ends must land on the height
        // the glyph metrics give a font that stacks by that line, or the two
        // disagree about the same font at the same size.
        let font = Font::new(STATIC.to_vec(), 0).unwrap();
        let upem = font.metrics().units_per_em;
        for ppem in [11.0f32, 12.0, 13.0, 16.0, 19.0, 24.0] {
            let scale = Rounding(ppem, upem);
            let line = font.metrics().scaled(scale).h_line().unwrap();
            let height = Rounding::sub(line.ascender, line.descender);
            let glyphs = font.glyph_metrics().scaled(scale);
            for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                assert_eq!(glyphs.v_advance(gid), height, "glyph {gid} at {ppem}ppem");
            }
        }
    }

    #[test]
    fn every_measurement_is_the_unscaled_one_through_the_scale() {
        // One case per field of `Metrics`, so that a field added there
        // without one here is a gap someone has to notice.
        let font = Font::new(STATIC.to_vec(), 0).unwrap();
        let global = font.metrics();
        let scale = Ppem {
            ppem: 16.0,
            upem: global.units_per_em,
        };
        let scaled = global.scaled(scale);
        let y = |line: LineBox<F48Dot16>| LineBox {
            ascender: scale.scale_y(line.ascender),
            descender: scale.scale_y(line.descender),
            line_gap: scale.scale_y(line.line_gap),
        };
        let x = |line: LineBox<F48Dot16>| LineBox {
            ascender: scale.scale_x(line.ascender),
            descender: scale.scale_x(line.descender),
            line_gap: scale.scale_x(line.line_gap),
        };
        assert_eq!(scaled.bounds(), scale.scale_rect(global.bounds));
        assert_eq!(scaled.hhea_line(), global.hhea_line.map(y));
        assert_eq!(scaled.typo_line(), global.typo_line.map(y));
        assert_eq!(scaled.vhea_line(), global.vhea_line.map(x));
        assert_eq!(scaled.h_line(), global.h_line().map(y));
        assert_eq!(
            scaled.win_line(),
            global.win_line.map(|l| LineExtents {
                ascender: scale.scale_y(l.ascender),
                descender: scale.scale_y(l.descender),
            })
        );
        assert_eq!(scaled.x_height(), global.x_height.map(|v| scale.scale_y(v)));
        assert_eq!(
            scaled.cap_height(),
            global.cap_height.map(|v| scale.scale_y(v))
        );
        assert_eq!(
            scaled.max_advance_width(),
            global.max_advance_width.map(|v| scale.scale_x(v))
        );
        assert_eq!(
            scaled.max_advance_height(),
            global.max_advance_height.map(|v| scale.scale_y(v))
        );
        assert_eq!(
            scaled.average_char_width(),
            global.average_char_width.map(|v| scale.scale_x(v))
        );
        // What carries no unit is reported unchanged.
        assert_eq!(scaled.units_per_em(), global.units_per_em);
        assert_eq!(scaled.num_glyphs(), global.num_glyphs);
        assert_eq!(scaled.use_typo_metrics(), global.use_typo_metrics);
        // And this font provides enough for the comparison to mean something.
        assert!(scaled.hhea_line().is_some() && scaled.typo_line().is_some());
    }

    #[test]
    fn the_clipping_line_reports_its_descent_as_a_position() {
        // `OS/2` gives the descent as a positive number below the baseline.
        // Reporting it as a position keeps the pair readable as a line.
        let font = Font::new(STATIC.to_vec(), 0).unwrap();
        let global = font.metrics();
        let win = global
            .win_line
            .expect("this font provides clipping metrics");
        assert!(win.ascender > F48Dot16::ZERO);
        assert!(win.descender < F48Dot16::ZERO);
        let scale = Ppem {
            ppem: 16.0,
            upem: global.units_per_em,
        };
        let scaled = global.scaled(scale).win_line().unwrap();
        assert_eq!(scaled.ascender, scale.scale_y(win.ascender));
        assert_eq!(scaled.descender, scale.scale_y(win.descender));
    }

    #[test]
    fn the_two_scales_agree_within_what_each_can_hold() {
        // The same font at the same size, in two number types. They differ by
        // what each can represent, so the coarser rounds the finer rather
        // than disagreeing with it.
        let font = Font::new(STATIC.to_vec(), 0).unwrap();
        let upem = font.metrics().units_per_em;
        let coarse = font
            .glyph_metrics()
            .scaled(Scale26Dot6::from_ppem(16.0, upem));
        let fine = font.glyph_metrics().scaled(ScaleF32::from_ppem(16.0, upem));
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            // 26.6 resolves 1/64 pixel, so it lands within one of those.
            let apart = (coarse.h_advance(gid).to_f32() - fine.h_advance(gid)).abs();
            assert!(apart < 1.0 / 32.0, "glyph {gid} is {apart} apart");
        }
    }

    #[test]
    fn a_scale_with_a_size_per_axis_uses_each_in_its_own_direction() {
        // The axes are apart because HarfBuzz keeps them apart, so a scale
        // built from two factors has to use the right one in each direction.
        let font = Font::new(VERT.to_vec(), 0).unwrap();
        let upem = font.metrics().units_per_em;
        let em = upem as f32;
        let wide = ScaleF32::new(em * 2.0, em, upem);
        let tall = ScaleF32::new(em, em * 2.0, upem);
        let plain = ScaleF32::new(em, em, upem);
        let gid = GlyphId::new(1);
        let (w, t, p) = (
            font.glyph_metrics().scaled(wide),
            font.glyph_metrics().scaled(tall),
            font.glyph_metrics().scaled(plain),
        );
        assert_eq!(w.h_advance(gid), p.h_advance(gid) * 2.0);
        assert_eq!(w.v_advance(gid), p.v_advance(gid));
        assert_eq!(t.v_advance(gid), p.v_advance(gid) * 2.0);
        assert_eq!(t.h_advance(gid), p.h_advance(gid));
    }

    #[test]
    fn freetype_scales_whole_design_units() {
        // What separates the two: FreeType multiplies a whole design unit,
        // so a measurement carrying a fraction loses it before the multiply.
        // A varied advance carries one, which is why the other exists.
        let upem = 1000;
        let fraction = F48Dot16::from_f64(10.5);
        assert_eq!(
            Scale26Dot6::from_ppem(upem as f32, upem).scale_x(fraction),
            F26Dot6::from_f64(11.0)
        );
        assert_eq!(
            ScaleF32::from_ppem(upem as f32, upem).scale_x(fraction),
            10.5
        );
    }

    #[test]
    fn a_rounding_scale_rounds_extents_corners_before_size() {
        let extents = GlyphExtents {
            x_bearing: F48Dot16::from_f64(10.6),
            y_bearing: F48Dot16::from_f64(-0.5),
            width: F48Dot16::from_f64(9.8),
            height: F48Dot16::from_f64(20.0),
        };
        let scale = Scale26Dot6::from_ppem(1000.0, 1000);
        assert_eq!(
            scale.scale_glyph_extents(extents),
            GlyphExtents {
                x_bearing: F26Dot6::from_f64(11.0),
                y_bearing: F26Dot6::from_f64(-1.0),
                width: F26Dot6::from_f64(9.0),
                height: F26Dot6::from_f64(20.0),
            }
        );

        let below_half = F48Dot16::from_bits((100_000 << 16) + 0x7fff);
        let extents = GlyphExtents {
            x_bearing: below_half,
            y_bearing: F48Dot16::from_i32(10),
            width: F48Dot16::from_i32(2),
            height: F48Dot16::from_i32(2),
        };
        assert_eq!(
            scale.scale_glyph_extents(extents).x_bearing,
            F26Dot6::from_i32(100_000)
        );
    }

    #[test]
    fn halving_rounds_toward_negative_infinity_in_26_6() {
        // Documented, because HarfBuzz halves two ways within one function
        // and a caller writing its own needs to know which it is getting.
        assert_eq!(
            Scale26Dot6::half(F26Dot6::from_bits(-3)),
            F26Dot6::from_bits(-2)
        );
        assert_eq!(ScaleF32::half(-3.0), -1.5);
    }

    #[test]
    fn a_scaled_advance_is_the_design_unit_one_through_the_scale() {
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics();
        let scaled = metrics.scaled(scale);
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert_eq!(
                scaled.h_advance(gid),
                scale.scale_x(metrics.h_advance_exact(gid)),
                "glyph {gid}"
            );
        }
    }

    #[test]
    fn a_stated_line_measures_a_font_that_states_none() {
        // This font has no `vmtx`, so its glyphs stack by the line.
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let supplied = metrics.with_line_extents(Some(line(12.0, -4.0)));
        let gid = GlyphId::new(1);
        assert_eq!(supplied.v_advance(gid), 16.0);
        assert!(metrics.v_advance(gid) != 16.0);
    }

    #[test]
    fn a_font_that_states_its_own_ignores_the_line() {
        // This one has `vmtx`, so the line is not what its glyphs stack by,
        // and stating one changes nothing.
        let (font, scale) = scaled(VERT, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let supplied = metrics.with_line_extents(Some(line(99.0, -99.0)));
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert_eq!(
                metrics.v_advance(gid),
                supplied.v_advance(gid),
                "glyph {gid}"
            );
        }
    }

    #[test]
    fn stating_nothing_is_stating_nothing() {
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let gid = GlyphId::new(1);
        assert_eq!(
            metrics.with_line_extents(None).v_advance(gid),
            metrics.v_advance(gid)
        );
    }

    #[test]
    fn a_batch_agrees_with_one_at_a_time() {
        for (data, supplied) in [
            (STATIC, None),
            (STATIC, Some(line(12.0, -4.0))),
            (VERT, None),
            (VERT, Some(line(12.0, -4.0))),
        ] {
            let (font, scale) = scaled(data, 16.0);
            let metrics = font
                .glyph_metrics()
                .scaled(scale)
                .with_line_extents(supplied);
            let gids: Vec<_> = (0..font.num_glyphs()).map(GlyphId::new).collect();
            let mut h = vec![0.0f32; gids.len()];
            let mut v = vec![0.0f32; gids.len()];
            metrics.h_advance_batched(|value| value, gids.iter().copied().zip(h.iter_mut()));
            metrics.v_advance_batched(|value| value, gids.iter().copied().zip(v.iter_mut()));
            for (i, gid) in gids.iter().enumerate() {
                assert_eq!(metrics.h_advance(*gid), h[i], "h, glyph {gid}");
                assert_eq!(metrics.v_advance(*gid), v[i], "v, glyph {gid}");
            }
        }
    }

    #[test]
    fn a_scaled_box_is_the_font_box_through_the_scale() {
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics();
        let scaled = metrics.scaled(scale);
        let mut measured = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let expected = metrics
                .extents_exact(gid)
                .map(|extents| scale.scale_glyph_extents(extents));
            assert_eq!(scaled.extents(gid), expected, "glyph {gid}");
            measured += expected.is_some() as u32;
        }
        assert!(measured > 0);
    }

    #[test]
    fn a_supplied_measurement_replaces_the_font_one() {
        // A caller measuring ink elsewhere answers for every glyph, including
        // one it cannot measure where the font states a box.
        let (font, scale) = scaled(STATIC, 16.0);
        let ink = GlyphExtents {
            x_bearing: 1.0,
            y_bearing: 2.0,
            width: 3.0,
            height: 4.0,
        };
        let supplied = |glyph: GlyphId| (glyph.to_u32() % 2 == 0).then_some(ink);
        let metrics = font.glyph_metrics().scaled(scale);
        let overridden = metrics.with_glyph_extents(Some(&supplied));
        let mut replaced = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert_eq!(overridden.extents(gid), supplied(gid), "glyph {gid}");
            replaced += (metrics.extents(gid) != supplied(gid)) as u32;
        }
        assert!(replaced > 0, "the font already agreed everywhere");
    }

    #[test]
    fn a_supplied_measurement_answers_a_whole_run() {
        let (font, scale) = scaled(STATIC, 16.0);
        let ink = GlyphExtents {
            x_bearing: 5.0,
            y_bearing: 6.0,
            width: 7.0,
            height: 8.0,
        };
        let supplied = |glyph: GlyphId| (glyph.to_u32() % 3 != 0).then_some(ink);
        let metrics = font
            .glyph_metrics()
            .scaled(scale)
            .with_glyph_extents(Some(&supplied));
        let glyphs: Vec<_> = (0..font.num_glyphs()).map(GlyphId::new).collect();
        let mut batched = vec![None; glyphs.len()];
        metrics.extents_batched(
            |extents| extents,
            glyphs.iter().copied().zip(batched.iter_mut()),
        );
        for (gid, batched) in glyphs.iter().copied().zip(batched) {
            assert_eq!(batched, supplied(gid), "glyph {gid}");
        }
    }

    #[test]
    fn no_supplied_measurement_leaves_the_font_in_place() {
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert_eq!(
                metrics.with_glyph_extents(None).extents(gid),
                metrics.extents(gid),
                "glyph {gid}"
            );
        }
    }

    #[test]
    fn a_supplied_measurement_can_borrow_for_one_query() {
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let glyph = GlyphId::new(1);
        let original = metrics.extents(glyph);
        let supplied = {
            let ink = GlyphExtents {
                x_bearing: 1.0,
                y_bearing: 2.0,
                width: 3.0,
                height: 4.0,
            };
            let callback = |_: GlyphId| Some(ink);
            metrics.with_glyph_extents(Some(&callback)).extents(glyph)
        };
        assert_ne!(supplied, original);
        assert_eq!(metrics.extents(glyph), original);
    }

    #[test]
    fn vorg_answers_ahead_of_everything_else() {
        // A font stating this states it outright, whatever its ink or its
        // vertical metrics would otherwise say.
        let (font, scale) = scaled(VORG, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let direct = crate::FontRef::new(VORG).unwrap();
        let vorg = crate::TableProvider::vorg(&direct).unwrap();
        for gid in (0..4).map(GlyphId::new) {
            assert_eq!(
                metrics.v_origin_y(gid),
                scale.scale_y(F48Dot16::from_i32(vorg.vertical_origin_y(gid) as i32)),
                "glyph {gid}"
            );
        }
        // And it answers even where the ink is measured differently.
        let ink = GlyphExtents {
            x_bearing: 0.0,
            y_bearing: 999.0,
            width: 1.0,
            height: 1.0,
        };
        let supplied = |_: GlyphId| Some(ink);
        let overridden = metrics.with_glyph_extents(Some(&supplied));
        for gid in (0..4).map(GlyphId::new) {
            assert_eq!(overridden.v_origin_y(gid), metrics.v_origin_y(gid));
        }
    }

    #[test]
    fn vmtx_uses_the_glyf_top_phantom_point() {
        // With no `VORG`, the top phantom point includes the top side bearing.
        let (font, scale) = scaled(VERT, 16.0);
        let glyph_metrics = font.glyph_metrics();
        let metrics = glyph_metrics.scaled(scale);
        assert!(glyph_metrics.stated_v_origin(GlyphId::new(1)).is_none());
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let (Some(extents), Some(bearing)) = (
                glyph_metrics.extents_exact(gid),
                glyph_metrics.stated_v_bearing(gid),
            ) else {
                continue;
            };
            assert_eq!(
                metrics.v_origin_y(gid),
                scale.scale_y(glyph_metrics.glyf_v_origin(gid).unwrap()),
                "glyph {gid}"
            );
            assert_eq!(
                glyph_metrics.glyf_v_origin(gid).unwrap(),
                extents.y_bearing + bearing,
                "glyph {gid}"
            );
            checked += 1;
        }
        assert!(checked > 0, "no glyph stated both");

        let supplied = |_: GlyphId| {
            Some(GlyphExtents {
                x_bearing: 0.0,
                y_bearing: 999.0,
                width: 1.0,
                height: 1.0,
            })
        };
        let overridden = metrics.with_glyph_extents(Some(&supplied));
        assert_eq!(
            overridden.v_origin_y(GlyphId::new(1)),
            metrics.v_origin_y(GlyphId::new(1))
        );
    }

    #[test]
    fn without_vmtx_the_ink_is_centered_in_the_line() {
        // The leftover of the line, half above the ink and half below it.
        let (font, scale) = scaled(STATIC, 16.0);
        let glyph_metrics = font.glyph_metrics();
        let metrics = glyph_metrics.scaled(scale);
        assert!(glyph_metrics.stated_v_bearing(GlyphId::new(1)).is_none());
        let line = glyph_metrics.line_extents().unwrap();
        let height = scale.scale_y(line.ascender) - scale.scale_y(line.descender);
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let Some(extents) = glyph_metrics.extents_exact(gid) else {
                continue;
            };
            let extents = scale.scale_glyph_extents(extents);
            assert_eq!(
                metrics.v_origin_y(gid),
                extents.y_bearing + (height - extents.height) / 2.0,
                "glyph {gid}"
            );
            checked += 1;
        }
        assert!(checked > 0);
    }

    #[test]
    fn an_empty_glyph_is_centered_but_unavailable_extents_use_the_ascender() {
        let (font, scale) = scaled(STATIC, 16.0);
        let glyph_metrics = font.glyph_metrics();
        let metrics = glyph_metrics.scaled(scale);
        let line = metrics.origin_line();
        let absent = |_: GlyphId| None;
        let overridden = metrics.with_glyph_extents(Some(&absent));
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            if glyph_metrics.extents(gid) != Some(GlyphExtents::default()) {
                continue;
            }
            assert_eq!(
                metrics.v_origin_y(gid),
                (line.ascender - line.descender) / 2.0,
                "glyph {gid}"
            );
            assert_eq!(overridden.v_origin_y(gid), line.ascender);
            checked += 1;
        }
        assert!(checked > 0, "no glyph was empty");
    }

    #[test]
    fn a_supplied_measurement_moves_the_origin_with_it() {
        // The two middle rungs read the ink, so a caller substituting its own
        // gets an origin consistent with it rather than with the font.
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let ink = GlyphExtents {
            x_bearing: 0.0,
            y_bearing: 100.0,
            width: 10.0,
            height: 20.0,
        };
        let supplied = |_: GlyphId| Some(ink);
        let overridden = metrics.with_glyph_extents(Some(&supplied));
        let line = metrics.origin_line();
        let height = line.ascender - line.descender;
        let expected = ink.y_bearing + (height - ink.height) / 2.0;
        let mut moved = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert_eq!(overridden.v_origin_y(gid), expected, "glyph {gid}");
            moved += (metrics.v_origin_y(gid) != expected) as u32;
        }
        assert!(moved > 0, "the font already agreed everywhere");

        let gids: Vec<_> = (0..font.num_glyphs()).map(GlyphId::new).collect();
        let mut origins = vec![0.0; gids.len()];
        overridden.v_origin_y_batched(
            |origin| origin,
            gids.iter().copied().zip(origins.iter_mut()),
        );
        assert!(origins.iter().all(|origin| *origin == expected));
    }

    #[test]
    fn a_supplied_line_moves_the_origin_with_it() {
        // The centered rung measures against the line, so a caller laying out
        // against its own line gets an origin in that line.
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font.glyph_metrics().scaled(scale);
        let taller = metrics.with_line_extents(Some(line(100.0, -20.0)));
        let gid = (0..font.num_glyphs())
            .map(GlyphId::new)
            .find(|gid| font.glyph_metrics().extents(*gid).is_some())
            .unwrap();
        assert_ne!(taller.v_origin_y(gid), metrics.v_origin_y(gid));
    }

    #[test]
    fn a_run_of_origins_agrees_with_one_at_a_time() {
        for data in [VORG, VERT, STATIC] {
            let (font, scale) = scaled(data, 16.0);
            let metrics = font.glyph_metrics().scaled(scale);
            let glyphs: Vec<_> = (0..font.num_glyphs()).map(GlyphId::new).collect();
            let mut batched = vec![0.0; glyphs.len()];
            metrics.v_origin_y_batched(
                |origin| origin,
                glyphs.iter().copied().zip(batched.iter_mut()),
            );
            for (gid, batched) in glyphs.iter().copied().zip(batched) {
                assert_eq!(metrics.v_origin_y(gid), batched, "glyph {gid}");
            }
        }
    }

    #[test]
    fn a_font_stating_no_line_is_given_one_proportioned_to_its_em() {
        // Four fifths of the em above the baseline and the rest below, which
        // is what a glyph with no ink to measure then sits on.
        let font = font_without(&[b"hhea", b"OS/2"]);
        let glyph_metrics = font.glyph_metrics();
        assert!(glyph_metrics.line_extents().is_none());
        let em = font.units_per_em();
        assert!(
            em > 0,
            "the em itself has to survive for this to mean anything"
        );

        let scale = Ppem {
            ppem: 16.0,
            upem: em,
        };
        let metrics = glyph_metrics.scaled(scale);
        let ascender = F48Dot16::from_f64(em as f64 * 0.8);
        let empty = empty_glyph(&font);
        assert_eq!(
            metrics.v_origin_y(empty),
            scale.scale_y(F48Dot16::from_i32(em as i32)) / 2.0
        );
        let absent = |_: GlyphId| None;
        assert_eq!(
            metrics.with_glyph_extents(Some(&absent)).v_origin_y(empty),
            scale.scale_y(ascender)
        );
        // And the line it implies is a whole em tall, so a glyph with no
        // vertical metrics still advances by one.
        assert_eq!(
            metrics.v_advance(empty),
            scale.scale_y(ascender) - scale.scale_y(ascender - F48Dot16::from_i32(em as i32))
        );
    }

    /// A font serving every table but the ones named.
    fn font_without(hide: &'static [&[u8; 4]]) -> Font {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag: Tag| {
            if hide.iter().any(|hidden| Tag::new(hidden) == tag) {
                return None;
            }
            let font = crate::FontRef::new(STATIC).ok()?;
            Some(Blob::from(font.table_data(tag)?.as_bytes().to_vec()))
        });
        Font::new(source, 0).unwrap()
    }

    #[test]
    fn the_ends_of_the_line_are_signed_whatever_the_font_says() {
        // Fonts state these both ways round: some as positions, where the
        // descender runs down and is negative, and some as magnitudes, where
        // it is not. All three describe one line and have to measure as one.
        let stated = font_with_hhea_ends(1600, -400);
        let scale = Ppem {
            ppem: 16.0,
            upem: stated.units_per_em(),
        };
        let empty = empty_glyph(&stated);
        let ink = drawn_glyph(&stated);
        let expected = stated.glyph_metrics().scaled(scale);
        for (ascender, descender) in [(1600, 400), (-1600, 400), (-1600, -400)] {
            let font = font_with_hhea_ends(ascender, descender);
            let actual = font.glyph_metrics().scaled(scale);
            // The line itself, through the two rungs that read it.
            assert_eq!(
                actual.v_origin_y(empty),
                expected.v_origin_y(empty),
                "ascender {ascender}, descender {descender}"
            );
            assert_eq!(
                actual.v_origin_y(ink),
                expected.v_origin_y(ink),
                "ascender {ascender}, descender {descender}"
            );
            // And the height a glyph advances by, which is that line too.
            assert_eq!(
                actual.v_advance(ink),
                expected.v_advance(ink),
                "ascender {ascender}, descender {descender}"
            );
        }
        // And the line really is the one the font states, not a fallback that
        // would agree by accident.
        assert_eq!(
            expected.v_origin_y(empty),
            scale.scale_y(F48Dot16::from_i32(1000))
        );
    }

    #[test]
    fn a_supplied_line_is_taken_as_it_was_given() {
        // The signing is what HarfBuzz applies to what it reads from a font.
        // A caller stating its own line has already decided, so an ascender
        // below the baseline stays there.
        let (font, scale) = scaled(STATIC, 16.0);
        let metrics = font
            .glyph_metrics()
            .scaled(scale)
            .with_line_extents(Some(line(-100.0, 20.0)));
        let absent = |_: GlyphId| None;
        assert_eq!(
            metrics
                .with_glyph_extents(Some(&absent))
                .v_origin_y(empty_glyph(&font)),
            -100.0
        );
    }

    /// `STATIC` with the ends of its `hhea` line replaced, and no `OS/2` to
    /// be read ahead of it.
    fn font_with_hhea_ends(ascender: i16, descender: i16) -> Font {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag: Tag| {
            if tag == Tag::new(b"OS/2") {
                return None;
            }
            let font = crate::FontRef::new(STATIC).ok()?;
            let mut data = font.table_data(tag)?.as_bytes().to_vec();
            if tag == Tag::new(b"hhea") {
                // Both ends sit right after the version.
                data.get_mut(4..6)?.copy_from_slice(&ascender.to_be_bytes());
                data.get_mut(6..8)?
                    .copy_from_slice(&descender.to_be_bytes());
            }
            Some(Blob::from(data))
        });
        Font::new(source, 0).unwrap()
    }

    /// A present glyph of `font` with no outline and a zero box.
    fn empty_glyph(font: &Font) -> GlyphId {
        let metrics = font.glyph_metrics();
        (0..font.num_glyphs())
            .map(GlyphId::new)
            .find(|gid| metrics.extents(*gid) == Some(GlyphExtents::default()))
            .expect("no glyph has a zero box")
    }

    /// A glyph of `font` with ink to measure against.
    fn drawn_glyph(font: &Font) -> GlyphId {
        let metrics = font.glyph_metrics();
        (0..font.num_glyphs())
            .map(GlyphId::new)
            .find(|gid| metrics.extents(*gid).is_some())
            .expect("no glyph drew anything")
    }
}
