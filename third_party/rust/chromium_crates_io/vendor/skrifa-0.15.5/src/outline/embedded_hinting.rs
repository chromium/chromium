//! Support for applying embedded hinting instructions.

use super::{
    cff, AdjustedMetrics, DrawError, Hinting, LocationRef, NormalizedCoord, OutlineCollectionKind,
    OutlineGlyph, OutlineGlyphCollection, OutlineKind, OutlinePen, Size,
};

/// Modes for embedded hinting.
///
/// Only the `glyf` format supports all hinting modes.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum EmbeddedHinting {
    /// "Full" hinting mode. May generate rough outlines and poor horizontal
    /// spacing.
    Full,
    /// Light hinting mode. This prevents most movement in the horizontal
    /// direction with the exception of a per-font backward compatibility
    /// opt in.
    Light,
    /// Same as light, but with additional support for RGB subpixel rendering.
    LightSubpixel,
    /// Same as light subpixel, but always prevents adjustment in the
    /// horizontal direction. This is the default mode.
    VerticalSubpixel,
}

/// Hinting instance that uses information embedded in the font to perform
/// grid-fitting.
#[derive(Clone)]
pub struct EmbeddedHintingInstance {
    size: Size,
    coords: Vec<NormalizedCoord>,
    mode: EmbeddedHinting,
    kind: HinterKind,
}

impl EmbeddedHintingInstance {
    /// Creates a new embedded hinting instance for the given outline
    /// collection, size, location in variation space and hinting mode.
    pub fn new<'a>(
        outline_glyphs: &OutlineGlyphCollection,
        size: Size,
        location: impl Into<LocationRef<'a>>,
        mode: EmbeddedHinting,
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
    pub fn mode(&self) -> EmbeddedHinting {
        self.mode
    }

    /// Resets the hinter state for a new font instance with the given
    /// outline collection and settings.
    pub fn reconfigure<'a>(
        &mut self,
        outlines: &OutlineGlyphCollection,
        size: Size,
        location: impl Into<LocationRef<'a>>,
        mode: EmbeddedHinting,
    ) -> Result<(), DrawError> {
        self.size = size;
        self.coords.clear();
        self.coords.extend_from_slice(location.into().coords());
        self.mode = mode;
        // Reuse memory if the font contains the same outline format
        let current_kind = core::mem::replace(&mut self.kind, HinterKind::None);
        match &outlines.kind {
            OutlineCollectionKind::Glyf(_) => {
                self.kind = HinterKind::Glyf;
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

    pub(super) fn draw(
        &self,
        glyph: &OutlineGlyph,
        memory: Option<&mut [u8]>,
        pen: &mut impl OutlinePen,
    ) -> Result<AdjustedMetrics, DrawError> {
        let ppem = self.size.ppem();
        let coords = self.coords.as_slice();
        match (&self.kind, &glyph.kind) {
            (HinterKind::Glyf, OutlineKind::Glyf(glyf, outline)) => {
                super::with_glyf_memory(outline, Hinting::Embedded, memory, |buf| {
                    let mem = outline
                        .memory_from_buffer(buf, Hinting::Embedded)
                        .ok_or(DrawError::InsufficientMemory)?;
                    let scaled_outline = glyf.draw(mem, outline, ppem, coords)?;
                    scaled_outline.to_path(pen)?;
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
    Glyf,
    Cff(Vec<cff::Subfont>),
}
