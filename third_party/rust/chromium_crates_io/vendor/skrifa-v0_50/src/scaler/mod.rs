//! Scaled and hinted glyphs from a [`Font`].
//!
//! The crate root re-exports the types.

mod error;
mod hinting;
mod postscript;
mod truetype;

#[cfg(test)]
mod tests;

pub use crate::outline::glyf::{DropoutControl, RasterPolicy};
pub use error::ScaleError;
use hinting::EngineRequest;
pub use hinting::{HintEngine, HintOptions, RasterTarget};
pub use raw::model::glyph::outline::{DrawMemory, DrawnMetrics, OutlineSource, PathSink};
pub use raw::model::glyph::{GlyphSource, GlyphSourceOrder};

use crate::{
    font::Font,
    instance::Size,
    outline::{autohint, pen::PathStyle, AdjustedMetrics},
};
use alloc::boxed::Box;
#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;
use raw::{
    model::glyph::outline::{DrawOptions, OutlineGlyph, OutlineGlyphs, PathContourStart},
    model::metrics::{ScaleF32, ScaledGlyphMetrics, ScaledMetrics, ScaledStyleMetrics},
    ps::cff::CffFontRef,
    types::{Fixed, GlyphId},
};
use yoke::Yoke;

// Drawing takes `&self`, so a scaler is meant to be shared between threads.
const _: () = {
    fn shared<T: Send + Sync>() {}
    let _ = shared::<ScalerFont>;
};

impl From<AdjustedMetrics> for DrawnMetrics {
    fn from(metrics: AdjustedMetrics) -> Self {
        let mut computed = Self::default();
        computed.h_advance = metrics.advance_width;
        computed.x_bearing = metrics.lsb;
        computed.has_overlaps = metrics.has_overlaps;
        computed
    }
}

/// Draws glyphs from a [`Font`] at a size, with optional hinting.
///
/// Drawing takes `&self`, so a scaler is cheap to draw from and safe to
/// share. Working memory comes from the caller or from short lived scratch.
/// [`reconfigure`](Self::reconfigure) sets up a scaler for another font,
/// size, or hinting and reuses the existing memory. This lets a cache of
/// scalers reuse its entries.
pub struct ScalerFont {
    /// The font, with its outlines resolved once rather than for every draw.
    outlines: Yoke<OutlineGlyphs<'static>, Box<Font>>,
    /// The order the outline sources were resolved in, which glyph extents
    /// follow too.
    order: GlyphSourceOrder,
    /// Pixels per em, or `None` for font units.
    ppem: Option<f32>,
    hint_options: Option<HintOptions>,
    state: State,
}

/// A font and the order in which its outline sources are tried.
///
/// A [`Font`] converts to a source with [`GlyphSourceOrder::DEFAULT`], so the
/// [`ScalerFont`] constructors accept a font directly.
#[derive(Clone)]
pub struct ScalerSource {
    font: Font,
    order: GlyphSourceOrder,
}

impl ScalerSource {
    /// Creates a source that draws `font` from its outline sources in
    /// `order`.
    ///
    /// See [`Font::outline_glyphs_with`] for how the order selects them.
    pub fn new(font: Font, order: GlyphSourceOrder) -> Self {
        Self { font, order }
    }
}

impl core::fmt::Debug for ScalerSource {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ScalerSource")
            .field("order", &self.order)
            .finish_non_exhaustive()
    }
}

impl From<Font> for ScalerSource {
    fn from(font: Font) -> Self {
        Self::new(font, GlyphSourceOrder::DEFAULT)
    }
}

impl core::fmt::Debug for ScalerFont {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ScalerFont")
            .field("outline_source", &self.outline_source())
            .field("ppem", &self.ppem)
            .field("hint_options", &self.hint_options)
            .field("hint_engine", &self.hint_engine())
            .finish_non_exhaustive()
    }
}

/// The hinting engine configured for the font and size.
enum State {
    /// No hinting. Hinting was not requested, there is no size, or the font
    /// has no hints.
    Unhinted,
    /// The setup programs failed, so every draw fails too.
    /// Boxed because it is rare and would otherwise double the size of every
    /// state.
    Failed(Box<ScaleError>),
    TrueType(Box<truetype::TrueTypeHinter>),
    PostScript(postscript::PostScriptHinter),
    Auto(autohint::Instance),
}

impl ScalerFont {
    /// Creates a scaler that draws `font` unhinted.
    ///
    /// `ppem` is the size in pixels per em, or `None` for font units.
    ///
    /// A TrueType font that needs its instructions to draw correct contours is
    /// hinted anyway when there is a size. It is hinted with
    /// [`HintOptions::interpreter`], which [`hint_options`](Self::hint_options)
    /// then returns. Use [`force_unhinted`](Self::force_unhinted) to draw
    /// such a font unhinted.
    pub fn unhinted(font: impl Into<ScalerSource>, ppem: Option<f32>) -> Self {
        let source = font.into();
        let tricky = ppem.is_some() && source.font.is_hint_reliant();
        let mut scaler = Self {
            order: source.order,
            outlines: resolve_outlines(source),
            ppem,
            hint_options: tricky.then(HintOptions::interpreter),
            state: State::Unhinted,
        };
        scaler.configure();
        scaler
    }

    /// Creates a scaler that draws `font` unhinted, even a font that needs
    /// its instructions to draw correct contours.
    ///
    /// `ppem` is the size in pixels per em, or `None` for font units.
    pub fn force_unhinted(font: impl Into<ScalerSource>, ppem: Option<f32>) -> Self {
        let source = font.into();
        Self {
            order: source.order,
            outlines: resolve_outlines(source),
            ppem,
            hint_options: None,
            state: State::Unhinted,
        }
    }

    /// Creates a scaler that draws `font` at `ppem` pixels per em, hinted as
    /// `hint_options` asks.
    ///
    /// Hinting fits an outline to a pixel grid, so it always requires a size.
    pub fn hinted(font: impl Into<ScalerSource>, ppem: f32, hint_options: HintOptions) -> Self {
        let source = font.into();
        let mut scaler = Self {
            order: source.order,
            outlines: resolve_outlines(source),
            ppem: Some(ppem),
            hint_options: Some(hint_options),
            state: State::Unhinted,
        };
        scaler.configure();
        scaler
    }

    /// Reconfigures the scaler to draw `font` at `ppem` pixels per em, hinted
    /// as `hint_options` asks.
    ///
    /// The memory from the previous setup is reused. This lets an entry in a
    /// cache of hinted scalers change its font, size, and hinting without
    /// creating a new scaler. Setup reruns the font's programs, so avoid
    /// calling this on a hot path. An unhinted scaler is cheap to create, so
    /// there is nothing to gain from reconfiguring one; this always leaves
    /// the scaler hinted.
    pub fn reconfigure(
        &mut self,
        font: impl Into<ScalerSource>,
        ppem: f32,
        hint_options: HintOptions,
    ) {
        let source = font.into();
        self.order = source.order;
        self.outlines = resolve_outlines(source);
        self.ppem = Some(ppem);
        self.hint_options = Some(hint_options);
        self.configure();
    }

    /// Returns the font, set to its variation location.
    pub fn font(&self) -> &Font {
        self.outlines.backing_cart()
    }

    /// Returns the table that provides the outlines.
    ///
    /// This is [`OutlineSource::Varc`] if `VARC` composites are drawn. The
    /// composites cover only some glyphs, and the rest come from the
    /// [`underlying_outline_source`](Self::underlying_outline_source).
    /// Returns `None` if the font has no outlines from the scaler's sources.
    pub fn outline_source(&self) -> Option<OutlineSource> {
        self.outlines.get().source()
    }

    /// Returns the table that draws the glyphs `VARC` does not cover and the
    /// components of those it does.
    ///
    /// This is the same as [`outline_source`](Self::outline_source) unless
    /// `VARC` composites are drawn, and is never [`OutlineSource::Varc`].
    /// Returns `None` if the font has no outlines from the scaler's sources.
    pub fn underlying_outline_source(&self) -> Option<OutlineSource> {
        self.outlines.get().underlying_source()
    }

    /// Returns the size in pixels per em, or `None` for font units.
    pub fn ppem(&self) -> Option<f32> {
        self.ppem
    }

    /// Returns the size glyphs are hinted at, in pixels per em.
    ///
    /// This is [`ppem`](Self::ppem), except for a TrueType font whose `head`
    /// table asks for whole pixel sizes. The interpreter then hints at the
    /// size rounded to a whole pixel, and draws at it too when the font turns
    /// hinting off.
    pub fn hinted_ppem(&self) -> Option<f32> {
        match &self.state {
            State::TrueType(hinter) => hinter.ppem().or(self.ppem),
            _ => self.ppem,
        }
    }

    /// Returns the hinting engine the scaler runs, or `None` if it does not
    /// hint.
    ///
    /// A scaler does not hint without hinting options or a size, or where
    /// setup failed, which [`hinting_error`](Self::hinting_error) reports. A
    /// TrueType font can turn its hinting off at some sizes, so the engine
    /// can be selected where [`is_hinted`](Self::is_hinted) is false.
    pub fn hint_engine(&self) -> Option<HintEngine> {
        match &self.state {
            State::TrueType(_) => Some(HintEngine::TrueType),
            State::PostScript(_) => Some(HintEngine::PostScript),
            State::Auto(_) => Some(HintEngine::Auto),
            State::Unhinted | State::Failed(_) => None,
        }
    }

    /// Returns the hinting configuration, if any.
    pub fn hint_options(&self) -> Option<HintOptions> {
        self.hint_options
    }

    /// Returns true if glyphs are hinted at this size.
    ///
    /// Returns false if no hinting is configured, if the size is in font
    /// units, or if the font has nothing to hint with. Also returns false if
    /// the font disables hinting at this size through its control value
    /// program or `gasp` table.
    ///
    /// A `VARC` composite is drawn unhinted unless the automatic hinter is
    /// in use, even where this returns true.
    pub fn is_hinted(&self) -> bool {
        match &self.state {
            State::TrueType(hinter) => hinter.is_enabled(),
            State::PostScript(_) | State::Auto(_) => true,
            State::Unhinted | State::Failed(_) => false,
        }
    }

    /// Returns the error that prevented hinting setup, if any.
    ///
    /// Every draw fails with this error until the size or hinting changes.
    pub fn hinting_error(&self) -> Option<&ScaleError> {
        match &self.state {
            State::Failed(error) => Some(error),
            _ => None,
        }
    }

    /// Returns the smoothing and grid fitting the font requests at this size.
    ///
    /// Returns `None` unless the TrueType interpreter hints the font. With
    /// [classic ClearType](HintOptions::with_classic_cleartype), this is the
    /// default policy rather than the font's.
    pub fn raster_policy(&self) -> Option<RasterPolicy> {
        match &self.state {
            State::TrueType(hinter) => Some(hinter.raster_policy()),
            _ => None,
        }
    }

    /// Returns the metrics describing the font as a whole, in pixels at
    /// [`ppem`](Self::ppem), or in font units without a size.
    ///
    /// These are scaled linearly; hinting does not change them.
    pub fn metrics(&self) -> ScaledMetrics<'_, ScaleF32> {
        let metrics = self.font().metrics();
        metrics.scaled_f32(self.ppem.unwrap_or(metrics.units_per_em as f32))
    }

    /// Returns the font-wide style measurements, in pixels at
    /// [`ppem`](Self::ppem), or in font units without a size.
    ///
    /// These are scaled linearly; hinting does not change them.
    pub fn style_metrics(&self) -> ScaledStyleMetrics<'_, ScaleF32> {
        let style = self.font().style_metrics();
        style.scaled_f32(self.ppem.unwrap_or(style.units_per_em as f32))
    }

    /// Returns the glyph metrics, in pixels at [`ppem`](Self::ppem), or in
    /// font units without a size.
    ///
    /// These are scaled linearly; hinting does not change them. For a hinted
    /// glyph's advance, draw its outline: [`ScalerOutline::draw`] returns it
    /// in [`DrawnMetrics::h_advance`].
    ///
    /// Glyph extents are measured from the sources the scaler draws from, in
    /// the same order.
    pub fn linear_glyph_metrics(&self) -> ScaledGlyphMetrics<'_, 'static, ScaleF32> {
        let font = self.font();
        let ppem = self.ppem.unwrap_or(font.units_per_em() as f32);
        font.glyph_metrics()
            .with_extents_source_order(self.order)
            .scaled_f32(ppem)
    }

    /// Returns the outline of `glyph`, resolved for drawing.
    ///
    /// Returns `None` if the font has no outline for the glyph.
    pub fn outline(&self, glyph: GlyphId) -> Option<ScalerOutline<'_>> {
        let outline = self.outlines.get().get(glyph)?;
        Some(ScalerOutline {
            scaler: self,
            outline,
        })
    }

    /// Sets the state up for the font, size and hinting.
    fn configure(&mut self) {
        // Reuse the memory from the last configuration where the engine is
        // the same.
        let previous = core::mem::replace(&mut self.state, State::Unhinted);
        let (Some(hinting), Some(ppem)) = (&self.hint_options, self.ppem) else {
            return;
        };
        self.state = match EngineChoice::new(self.font(), self.outlines.get(), hinting) {
            EngineChoice::TrueType => {
                let mut hinter = match previous {
                    State::TrueType(hinter) => hinter,
                    _ => Box::default(),
                };
                match hinter.reconfigure(self.font(), ppem, hinting) {
                    Ok(()) => State::TrueType(hinter),
                    Err(error) => State::Failed(Box::new(error)),
                }
            }
            EngineChoice::PostScript(cff) => {
                let mut hinter = match previous {
                    State::PostScript(hinter) => hinter,
                    _ => Default::default(),
                };
                hinter.reconfigure(cff, self.font().normalized_coords(), ppem);
                State::PostScript(hinter)
            }
            // The font keeps what the automatic hinter derives from it, so
            // every scaler of the font at this location shares it.
            EngineChoice::Auto => State::Auto(autohint::Instance::from_state(
                autohint::LocatedState::of_outlines(self.font(), *self.outlines.get()),
                hinting.outline_target(),
            )),
            EngineChoice::None => State::Unhinted,
        };
    }

    /// Draws `outline` unhinted at `ppem`.
    ///
    /// This is the scaler's size, except when the TrueType interpreter runs
    /// at a rounded size.
    fn draw_unhinted(
        &self,
        outline: &OutlineGlyph<'_>,
        ppem: Option<f32>,
        memory: Option<&mut [u8]>,
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, ScaleError> {
        let glyph = outline.glyph();
        let options = draw_options(ppem);
        let supplied = memory.as_ref().map_or(0, |memory| memory.len());
        let memory = memory.map_or(DrawMemory::Scratch, DrawMemory::Buffer);
        let metrics = outline.draw(&options, memory, sink).map_err(|error| {
            ScaleError::from_outline(error, outline.required_memory_size(), supplied)
        })?;
        let mut h_advance = metrics.h_advance;
        // A charstring that states no width, in a subfont with no default,
        // falls back to the metrics tables, as FreeType does.
        if let (None, OutlineGlyph::Cff(charstring)) = (h_advance, outline) {
            // In whole units, as FreeType rounds them.
            let advance = self.font().glyph_metrics().h_advance_exact(glyph).round();
            let advance = Fixed::from_i32(advance.to_i32());
            let transform = charstring
                .cff_font()
                .transform(charstring.subfont(), options.ppem);
            h_advance = Some(transform.transform_h_metric(advance).to_f32());
        }
        // A charstring can state a width below zero. Report none, as when
        // hinting, and leave the caller to its linear metrics.
        if let OutlineGlyph::Cff(_) = outline {
            h_advance = h_advance.filter(|advance| *advance >= 0.0);
        }
        // When hinting is asked for, FreeType rounds the advance even where
        // nothing ran, which keeps a font's unhinted glyphs in step with its
        // hinted ones. A composite states no advance to round.
        if self.hint_options.is_some() && !matches!(outline, OutlineGlyph::Varc(_)) {
            h_advance = h_advance.map(|advance| advance.round());
        }
        let mut metrics = metrics;
        metrics.h_advance = h_advance;
        Ok(metrics)
    }
}

/// The engine chosen to hint a font.
enum EngineChoice<'a> {
    /// The TrueType interpreter.
    TrueType,
    /// The PostScript hinter for these charstrings.
    PostScript(&'a CffFontRef<'a>),
    /// The automatic hinter.
    Auto,
    /// No engine. Type 1 fonts and fonts without outlines are not hinted.
    None,
}

impl<'a> EngineChoice<'a> {
    /// Chooses the engine that `hinting` requests for `outlines`, the
    /// outlines of `font` that the scaler draws.
    ///
    /// Requesting the interpreter runs the font's own instructions, whichever
    /// kind it contains. Requesting the FreeType fallback runs the TrueType
    /// interpreter only when the font's programs are worth running, and
    /// always runs a PostScript font's own hints. A TrueType font that needs
    /// its instructions for correct contours always runs them.
    fn new(font: &'a Font, outlines: &OutlineGlyphs<'a>, hinting: &HintOptions) -> Self {
        // The engine suits the outlines a composite is drawn from. Only the
        // automatic hinter hints a `VARC` composite itself; the others draw
        // it unhinted, since hints fitted to the grid would not survive the
        // components' transforms.
        let source = outlines.underlying_source();
        // A font that needs its instructions to draw correct contours is
        // always run through them, as FreeType runs one.
        if source == Some(OutlineSource::Glyf) && font.is_hint_reliant() {
            return Self::TrueType;
        }
        let instructions = match hinting.engine() {
            EngineRequest::Interpreter => true,
            EngineRequest::Auto => false,
            EngineRequest::AutoFallback => match source {
                Some(OutlineSource::Glyf) => truetype::prefer_interpreter(font.tables()),
                _ => true,
            },
        };
        if !instructions {
            return Self::Auto;
        }
        match source {
            Some(OutlineSource::Glyf) => Self::TrueType,
            Some(OutlineSource::Cff2 | OutlineSource::Cff) => {
                outlines.cff_font().map_or(Self::None, Self::PostScript)
            }
            _ => Self::None,
        }
    }
}

/// An outline from a [`ScalerFont`], resolved for drawing.
///
/// Holds the outline in its source format. A caller can size its own
/// working memory and then draw without looking up the glyph again.
#[derive(Clone)]
pub struct ScalerOutline<'a> {
    scaler: &'a ScalerFont,
    outline: OutlineGlyph<'a>,
}

impl core::fmt::Debug for ScalerOutline<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ScalerOutline")
            .field("glyph", &self.glyph())
            .field("source", &self.source())
            .finish()
    }
}

impl ScalerOutline<'_> {
    /// Returns the glyph identifier.
    pub fn glyph(&self) -> GlyphId {
        self.outline.glyph()
    }

    /// Returns the table that contains the outline.
    pub fn source(&self) -> OutlineSource {
        self.outline.source()
    }

    /// Returns the size in bytes of the working memory needed by
    /// [`draw`](Self::draw).
    ///
    /// Returns zero for the automatic hinter, which manages its own memory.
    pub fn required_memory_size(&self) -> usize {
        match (&self.scaler.state, &self.outline) {
            (State::Auto(_), _) => 0,
            (State::TrueType(hinter), OutlineGlyph::Glyf(glyf)) if hinter.is_enabled() => {
                hinter.required_memory_size(glyf)
            }
            _ => self.outline.required_memory_size(),
        }
    }

    /// Draws the outline into `sink` with the scaler's size and hinting.
    ///
    /// With [`DrawMemory::Scratch`], working memory comes from short lived
    /// scratch, which may be allocated on the heap for a large glyph program.
    /// Supplied `memory` must hold at least
    /// [`required_memory_size`](Self::required_memory_size) bytes, or the
    /// draw fails with [`ScaleError::InsufficientMemory`].
    pub fn draw(
        &self,
        memory: DrawMemory<'_>,
        sink: &mut impl PathSink,
    ) -> Result<DrawnMetrics, ScaleError> {
        let memory = match memory {
            DrawMemory::Buffer(buffer) => Some(buffer),
            _ => None,
        };
        self.draw_dyn(memory, sink)
    }

    /// Dispatches a draw through a trait object so the engines are compiled
    /// once rather than once per sink type.
    fn draw_dyn(
        &self,
        memory: Option<&mut [u8]>,
        sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, ScaleError> {
        let scaler = self.scaler;
        match (&scaler.state, &self.outline) {
            (State::Failed(error), _) => Err(ScaleError::clone(error)),
            (State::Auto(instance), _) => {
                let context = autohint::FontContext::for_glyph(
                    scaler.font(),
                    *scaler.outlines.get(),
                    &self.outline,
                );
                let size = scaler.ppem.map_or_else(Size::unscaled, Size::new);
                let mut sink = sink;
                let mut metrics = DrawnMetrics::from(
                    instance
                        .draw(&context, size, self.glyph(), PathStyle::FreeType, &mut sink)
                        .map_err(|error| {
                            let supplied = memory.as_ref().map_or(0, |memory| memory.len());
                            ScaleError::from_draw_error(error, 0, supplied)
                        })?,
                );
                // A composite states no advance, whatever hints it.
                if matches!(self.outline, OutlineGlyph::Varc(_)) {
                    metrics.h_advance = None;
                }
                Ok(metrics)
            }
            (State::TrueType(hinter), OutlineGlyph::Glyf(glyf)) if hinter.is_enabled() => {
                hinter.draw(glyf, memory, sink)
            }
            (State::PostScript(hinter), OutlineGlyph::Cff(charstring)) => {
                hinter.draw(scaler.font(), charstring, sink)
            }
            (State::TrueType(hinter), OutlineGlyph::Glyf(_)) => {
                let ppem = hinter.program_unhinted_ppem().or(scaler.ppem);
                scaler.draw_unhinted(&self.outline, ppem, memory, sink)
            }
            _ => scaler.draw_unhinted(&self.outline, scaler.ppem, memory, sink),
        }
    }
}

/// Keeps the font with its outlines, resolved once for every draw.
fn resolve_outlines(source: ScalerSource) -> Yoke<OutlineGlyphs<'static>, Box<Font>> {
    let order = source.order;
    Yoke::attach_to_cart(Box::new(source.font), |font| {
        font.outline_glyphs_with(order)
    })
}

/// Returns the read-fonts draw options for a size, with FreeType's contour
/// start points.
fn draw_options(ppem: Option<f32>) -> DrawOptions {
    DrawOptions {
        ppem,
        contour_start: PathContourStart::ScanBackward,
    }
}
