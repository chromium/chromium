//! Support for applying embedded hinting instructions.

use raw::tables::glyf::ToPathStyle;

use super::{
    cff, glyf, AdjustedMetrics, DrawError, Hinting, LocationRef, NormalizedCoord,
    OutlineCollectionKind, OutlineGlyph, OutlineGlyphCollection, OutlineKind, OutlinePen, Size,
};
use crate::alloc::{boxed::Box, vec::Vec};

/// Modes that control hinting when using embedded instructions.
///
/// Only the TrueType interpreter supports all hinting modes.
///
/// # FreeType compatibility
///
/// The following table describes how to map FreeType hinting modes:
///
/// | FreeType mode         | Variant                                                                              |
/// |-----------------------|--------------------------------------------------------------------------------------|
/// | FT_LOAD_TARGET_MONO   | Strong                                                                               |
/// | FT_LOAD_TARGET_NORMAL | Smooth { lcd_subpixel: None, preserve_linear_metrics: false }                        |
/// | FT_LOAD_TARGET_LCD    | Smooth { lcd_subpixel: Some(LcdLayout::Horizontal), preserve_linear_metrics: false } |
/// | FT_LOAD_TARGET_LCD_V  | Smooth { lcd_subpixel: Some(LcdLayout::Vertical), preserve_linear_metrics: false }   |
///
/// Note: `FT_LOAD_TARGET_LIGHT` is equivalent to `FT_LOAD_TARGET_NORMAL` since
/// FreeType 2.7.
///
/// The default value of this type is equivalent to `FT_LOAD_TARGET_NORMAL`.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum HintingMode {
    /// Strong hinting mode that should only be used for aliased, monochromatic
    /// rasterization.
    ///
    /// Corresponds to `FT_LOAD_TARGET_MONO` in FreeType.
    Strong,
    /// Lighter hinting mode that is intended for anti-aliased rasterization.
    Smooth {
        /// If set, enables support for optimized hinting that takes advantage
        /// of subpixel layouts in LCD displays and corresponds to
        /// `FT_LOAD_TARGET_LCD` or `FT_LOAD_TARGET_LCD_V` in FreeType.
        ///
        /// If unset, corresponds to `FT_LOAD_TARGET_NORMAL` in FreeType.
        lcd_subpixel: Option<LcdLayout>,
        /// If true, prevents adjustment of the outline in the horizontal
        /// direction and preserves inter-glyph spacing.
        ///
        /// This is useful for performing layout without concern that hinting
        /// will modify the advance width of a glyph. Specifically, it means
        /// that layout will not require evaluation of glyph outlines.
        ///
        /// FreeType has no corresponding setting.
        preserve_linear_metrics: bool,
    },
}

impl Default for HintingMode {
    fn default() -> Self {
        Self::Smooth {
            lcd_subpixel: None,
            preserve_linear_metrics: false,
        }
    }
}

/// Specifies direction of pixel layout for LCD based subpixel hinting.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum LcdLayout {
    /// Subpixels are ordered horizontally.
    ///
    /// Corresponds to `FT_LOAD_TARGET_LCD` in FreeType.
    Horizontal,
    /// Subpixels are ordered vertically.
    ///
    /// Corresponds to `FT_LOAD_TARGET_LCD_V` in FreeType.
    Vertical,
}

/// Hinting instance that uses information embedded in the font to perform
/// grid-fitting.
#[derive(Clone)]
pub struct HintingInstance {
    size: Size,
    coords: Vec<NormalizedCoord>,
    mode: HintingMode,
    kind: HinterKind,
}

impl HintingInstance {
    /// Creates a new embedded hinting instance for the given outline
    /// collection, size, location in variation space and hinting mode.
    pub fn new<'a>(
        outline_glyphs: &OutlineGlyphCollection,
        size: Size,
        location: impl Into<LocationRef<'a>>,
        mode: HintingMode,
    ) -> Result<Self, DrawError> {
        let mut hinter = Self {
            size: Size::unscaled(),
            coords: vec![],
            mode,
            kind: HinterKind::None,
        };
        hinter.reconfigure(outline_glyphs, size, location, mode)?;
        Ok(hinter)
    }

    /// Returns the currently configured size.
    pub fn size(&self) -> Size {
        self.size
    }

    /// Returns the currently configured normalized location in variation space.
    pub fn location(&self) -> LocationRef {
        LocationRef::new(&self.coords)
    }

    /// Returns the currently configured hinting mode.
    pub fn mode(&self) -> HintingMode {
        self.mode
    }

    /// Resets the hinter state for a new font instance with the given
    /// outline collection and settings.
    pub fn reconfigure<'a>(
        &mut self,
        outlines: &OutlineGlyphCollection,
        size: Size,
        location: impl Into<LocationRef<'a>>,
        mode: HintingMode,
    ) -> Result<(), DrawError> {
        self.size = size;
        self.coords.clear();
        self.coords.extend_from_slice(location.into().coords());
        self.mode = mode;
        // Reuse memory if the font contains the same outline format
        let current_kind = core::mem::replace(&mut self.kind, HinterKind::None);
        match &outlines.kind {
            OutlineCollectionKind::Glyf(glyf) => {
                let mut hint_instance = match current_kind {
                    HinterKind::Glyf(instance) => instance,
                    _ => Box::<glyf::HintInstance>::default(),
                };
                let ppem = size.ppem();
                let scale = glyf.compute_scale(ppem).1.to_bits();
                hint_instance.reconfigure(
                    glyf,
                    scale,
                    ppem.unwrap_or_default() as i32,
                    mode,
                    &self.coords,
                )?;
                self.kind = HinterKind::Glyf(hint_instance);
            }
            OutlineCollectionKind::Cff(cff) => {
                let mut subfonts = match current_kind {
                    HinterKind::Cff(subfonts) => subfonts,
                    _ => vec![],
                };
                subfonts.clear();
                let ppem = size.ppem();
                for i in 0..cff.subfont_count() {
                    subfonts.push(cff.subfont(i, ppem, &self.coords)?);
                }
                self.kind = HinterKind::Cff(subfonts);
            }
            OutlineCollectionKind::None => {}
        }
        Ok(())
    }

    /// Returns true if hinting should actually be applied for this instance.
    ///
    /// Some TrueType fonts disable hinting dynamically based on the instance
    /// configuration.
    pub fn is_enabled(&self) -> bool {
        match &self.kind {
            HinterKind::Glyf(instance) => instance.is_enabled(),
            HinterKind::Cff(_) => true,
            _ => false,
        }
    }

    pub(super) fn draw(
        &self,
        glyph: &OutlineGlyph,
        memory: Option<&mut [u8]>,
        path_style: ToPathStyle,
        pen: &mut impl OutlinePen,
        is_pedantic: bool,
    ) -> Result<AdjustedMetrics, DrawError> {
        let ppem = self.size.ppem();
        let coords = self.coords.as_slice();
        match (&self.kind, &glyph.kind) {
            (HinterKind::Glyf(instance), OutlineKind::Glyf(glyf, outline)) => {
                super::with_glyf_memory(outline, Hinting::Embedded, memory, |buf| {
                    let mem = outline
                        .memory_from_buffer(buf, Hinting::Embedded)
                        .ok_or(DrawError::InsufficientMemory)?;
                    let scaled_outline =
                        glyf.draw_hinted(mem, outline, ppem, coords, instance, is_pedantic)?;
                    scaled_outline.to_path(path_style, pen)?;
                    Ok(AdjustedMetrics {
                        has_overlaps: outline.has_overlaps,
                        lsb: Some(scaled_outline.adjusted_lsb().to_f32()),
                        advance_width: Some(scaled_outline.adjusted_advance_width().to_f32()),
                    })
                })
            }
            (HinterKind::Cff(subfonts), OutlineKind::Cff(cff, glyph_id, subfont_ix)) => {
                let Some(subfont) = subfonts.get(*subfont_ix as usize) else {
                    return Err(DrawError::NoSources);
                };
                cff.draw(subfont, *glyph_id, &self.coords, true, pen)?;
                Ok(AdjustedMetrics::default())
            }
            _ => Err(DrawError::NoSources),
        }
    }
}

#[derive(Clone)]
enum HinterKind {
    /// Represents a hinting instance that is associated with an empty outline
    /// collection.
    None,
    Glyf(Box<glyf::HintInstance>),
    Cff(Vec<cff::Subfont>),
}
