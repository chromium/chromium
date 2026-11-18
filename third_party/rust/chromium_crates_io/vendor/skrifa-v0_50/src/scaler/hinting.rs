//! Hinting configuration for a [`ScalerFont`](super::ScalerFont).

use crate::outline::{self, SmoothMode};

/// The rasterization target for a hinted outline.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Default, Debug)]
pub enum RasterTarget {
    /// One bit per pixel with no antialiasing. Hinting is strong and rounds
    /// in both directions.
    Mono,
    /// Grayscale antialiasing.
    #[default]
    Normal,
    /// Grayscale antialiasing with lighter hinting. There is less adjustment
    /// in the horizontal direction, and the automatic hinter moves points
    /// only vertically.
    Light,
    /// Subpixel rendering on a display whose subpixels run horizontally.
    Lcd,
    /// Subpixel rendering on a display whose subpixels run vertically.
    VerticalLcd,
}

/// The hinting engine a scaler runs.
///
/// [`HintOptions`] asks for an engine, and the font decides which one runs:
/// [`HintOptions::auto_fallback`] runs the font's own hints where they are
/// worth running and the automatic hinter otherwise, and a font that needs
/// its instructions for correct contours always runs them.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum HintEngine {
    /// The font's TrueType instructions.
    TrueType,
    /// The font's PostScript hints.
    PostScript,
    /// The automatic hinter.
    Auto,
}

/// The hinting engine requested by a configuration.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub(super) enum EngineRequest {
    /// The font's own instructions.
    Interpreter,
    /// The interpreter if the font has instructions, and the automatic hinter
    /// otherwise.
    AutoFallback,
    /// The automatic hinter.
    Auto,
}

/// Hinting configuration for a [`ScalerFont`](super::ScalerFont).
///
/// Create one with a constructor and adjust it with the `with_*` methods:
///
/// ```
/// use skrifa::{HintOptions, RasterTarget};
///
/// let hinting = HintOptions::interpreter()
///     .with_target(RasterTarget::Lcd)
///     .with_classic_cleartype(true);
/// assert_eq!(hinting.target(), RasterTarget::Lcd);
/// ```
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct HintOptions {
    engine: EngineRequest,
    target: RasterTarget,
    classic_cleartype: bool,
    symmetric_rendering: bool,
    preserve_linear_metrics: bool,
    pedantic: bool,
}

impl Default for HintOptions {
    fn default() -> Self {
        Self::auto_fallback()
    }
}

impl HintOptions {
    /// Creates a configuration that chooses the engine based on the font, with
    /// a grayscale target.
    ///
    /// Uses the font's own instructions when it has any, and the automatic
    /// hinter otherwise. A TrueType font with no glyph, font or control value
    /// program is autohinted.
    pub fn auto_fallback() -> Self {
        Self::with_engine(EngineRequest::AutoFallback)
    }

    /// Creates a configuration that uses the instructions embedded in the
    /// font.
    ///
    /// This runs the TrueType interpreter or the PostScript hinter, depending
    /// on which kind of instructions the font contains.
    pub fn interpreter() -> Self {
        Self::with_engine(EngineRequest::Interpreter)
    }

    /// Creates a configuration that uses the automatic hinter, which ignores
    /// the font's instructions.
    ///
    /// A TrueType font that needs its instructions to draw correct contours
    /// always runs the interpreter.
    ///
    /// The data the hinter derives from a font is stored with the font. All
    /// scalers for that font share it at every size.
    pub fn auto() -> Self {
        Self::with_engine(EngineRequest::Auto)
    }

    /// Creates the defaults for a grayscale target that uses `engine`.
    fn with_engine(engine: EngineRequest) -> Self {
        Self {
            engine,
            target: RasterTarget::Normal,
            classic_cleartype: false,
            symmetric_rendering: true,
            preserve_linear_metrics: false,
            pedantic: false,
        }
    }

    /// Sets the rasterization target.
    pub fn with_target(self, target: RasterTarget) -> Self {
        Self { target, ..self }
    }

    /// Sets whether the TrueType interpreter uses the classic ClearType
    /// behavior.
    ///
    /// By default, a version 1 `gasp` table selects symmetric smoothing and
    /// grid fitting per size, and glyphs that opt out of backward
    /// compatibility with `INSTCTRL` selector 3 keep fractional horizontal
    /// positions. With the classic behavior, every glyph is grid fitted, the
    /// symmetric rendering bit of `GETINFO` follows
    /// [`symmetric_rendering`](Self::symmetric_rendering) alone, and
    /// horizontal moves round to whole pixels.
    ///
    /// The setting also chooses when an advance comes from the `hdmx` table.
    /// By default, as in FreeType, the table is read at the size rounded to
    /// a whole pixel and ignored for a font that `post` marks as fixed
    /// pitch. With the classic behavior, it is read only at a whole pixel
    /// size, for any font.
    ///
    /// The classic behavior is that of
    /// [`HintingInstance`](crate::outline::HintingInstance). Defaults to off.
    ///
    /// See <https://github.com/googlefonts/fontations/issues/2112>.
    pub fn with_classic_cleartype(self, classic_cleartype: bool) -> Self {
        Self {
            classic_cleartype,
            ..self
        }
    }

    /// Sets whether TrueType bytecode may assume vertical supersampling
    /// during rasterization.
    ///
    /// This is the symmetric rendering bit of the `GETINFO` instruction.
    /// When it is set, ClearType fonts often generate wider horizontal stems
    /// that look blurry with an analytic area rasterizer. It is set by
    /// default. Unless [classic ClearType](Self::with_classic_cleartype) is
    /// on, the font's `gasp` table can also clear it per size. Only the TrueType interpreter reads it.
    /// See <https://github.com/googlefonts/fontations/issues/1080>.
    pub fn with_symmetric_rendering(self, symmetric_rendering: bool) -> Self {
        Self {
            symmetric_rendering,
            ..self
        }
    }

    /// Sets whether hinting leaves the outline unchanged horizontally.
    ///
    /// Hinting then moves points only vertically, so a glyph keeps its
    /// unhinted horizontal shape and spacing. The advance a draw reports is
    /// still rounded to a whole pixel, as for any hinted draw. The TrueType
    /// interpreter runs in backward compatibility mode, and the automatic
    /// hinter works as it does for a [`RasterTarget::Light`] target. Defaults
    /// to off.
    pub fn with_preserve_linear_metrics(self, preserve_linear_metrics: bool) -> Self {
        Self {
            preserve_linear_metrics,
            ..self
        }
    }

    /// Sets whether an error in a glyph's instructions fails the draw.
    ///
    /// When off, the draw keeps the outline as the instructions left it.
    /// Defaults to off.
    pub fn with_pedantic(self, pedantic: bool) -> Self {
        Self { pedantic, ..self }
    }

    /// Returns the hinting engine.
    pub(super) fn engine(&self) -> EngineRequest {
        self.engine
    }

    /// Returns the rasterization target.
    pub fn target(&self) -> RasterTarget {
        self.target
    }

    /// Returns whether the TrueType interpreter uses the classic ClearType
    /// behavior.
    pub fn classic_cleartype(&self) -> bool {
        self.classic_cleartype
    }

    /// Returns whether the `GETINFO` symmetric rendering bit may be set.
    pub fn symmetric_rendering(&self) -> bool {
        self.symmetric_rendering
    }

    /// Returns whether hinting preserves the font's advance widths.
    pub fn preserve_linear_metrics(&self) -> bool {
        self.preserve_linear_metrics
    }

    /// Returns whether an instruction error fails the draw.
    pub fn pedantic(&self) -> bool {
        self.pedantic
    }

    /// Returns the target in the form the hinting engines use.
    pub(super) fn outline_target(&self) -> outline::Target {
        let mode = match self.target {
            RasterTarget::Mono => return outline::Target::Mono,
            RasterTarget::Normal => SmoothMode::Normal,
            RasterTarget::Light => SmoothMode::Light,
            RasterTarget::Lcd => SmoothMode::Lcd,
            RasterTarget::VerticalLcd => SmoothMode::VerticalLcd,
        };
        outline::Target::Smooth {
            mode,
            symmetric_rendering: self.symmetric_rendering,
            preserve_linear_metrics: self.preserve_linear_metrics,
        }
    }
}
