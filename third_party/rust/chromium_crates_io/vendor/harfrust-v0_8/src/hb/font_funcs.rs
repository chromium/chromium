use core::mem::size_of;
use core::ptr;
use core::slice;

use read_fonts::types::GlyphId;

use crate::hb::face::Scale;

use super::buffer::{hb_buffer_t, GlyphInfo, GlyphPosition};
use super::face::{hb_font_t, GlyphExtents};

/// Raw C-style view over a batch of glyph ids and advance widths.
#[derive(Clone, Copy, Debug)]
pub struct RawAdvanceWidthBatch {
    /// Number of batch entries.
    pub len: usize,
    /// Pointer to glyph ids (read-only).
    pub gids: *const u32,
    /// Pointer to horizontal advances (writable).
    ///
    /// See "Metrics scaling" in the [FontFuncs] for details
    /// on what value this method should return.
    pub advances: *mut i32,
    /// Byte stride between successive glyph ids.
    pub gid_stride: isize,
    /// Byte stride between successive advances.
    pub advance_stride: isize,
}

/// Safe batch view for glyph id / horizontal-advance updates.
pub struct AdvanceWidthBatch<'a> {
    infos: &'a [GlyphInfo],
    positions: &'a mut [GlyphPosition],
}

impl<'a> AdvanceWidthBatch<'a> {
    pub(crate) fn new(buffer: &'a mut hb_buffer_t) -> Self {
        let len = buffer.len;
        let infos = &buffer.info[..len];
        let positions = &mut buffer.pos[..len];
        Self { infos, positions }
    }

    /// Returns the number of entries in the batch.
    pub fn len(&self) -> usize {
        self.infos.len()
    }

    /// Returns true if the batch is empty.
    pub fn is_empty(&self) -> bool {
        self.infos.is_empty()
    }

    /// Returns a raw C-style view over this batch.
    pub fn into_raw(self) -> RawAdvanceWidthBatch {
        if self.infos.is_empty() {
            return RawAdvanceWidthBatch {
                len: 0,
                gids: ptr::null(),
                advances: ptr::null_mut(),
                gid_stride: size_of::<GlyphInfo>() as isize,
                advance_stride: size_of::<GlyphPosition>() as isize,
            };
        }

        RawAdvanceWidthBatch {
            len: self.infos.len(),
            // `glyph_id` is the first field in `GlyphInfo`.
            gids: self.infos.as_ptr().cast::<u32>(),
            // `x_advance` is the first field in `GlyphPosition`.
            advances: self.positions.as_mut_ptr().cast::<i32>(),
            gid_stride: size_of::<GlyphInfo>() as isize,
            advance_stride: size_of::<GlyphPosition>() as isize,
        }
    }
}

pub struct AdvanceWidthBatchIter<'a> {
    infos: slice::Iter<'a, GlyphInfo>,
    positions: slice::IterMut<'a, GlyphPosition>,
}

impl<'a> Iterator for AdvanceWidthBatchIter<'a> {
    type Item = (GlyphId, &'a mut i32);

    fn next(&mut self) -> Option<Self::Item> {
        let info = self.infos.next()?;
        let pos = self.positions.next()?;
        Some((info.as_glyph(), &mut pos.x_advance))
    }
}

impl<'a> IntoIterator for AdvanceWidthBatch<'a> {
    type Item = (GlyphId, &'a mut i32);
    type IntoIter = AdvanceWidthBatchIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        AdvanceWidthBatchIter {
            infos: self.infos.iter(),
            positions: self.positions.iter_mut(),
        }
    }
}

/// Default implementations backed by font tables.
pub struct BuiltinFontFuncs<'a> {
    face: &'a hb_font_t<'a>,
}

impl<'a> BuiltinFontFuncs<'a> {
    pub(crate) fn new(face: &'a hb_font_t<'a>) -> Self {
        Self { face }
    }

    /// Maps a Unicode scalar value to a nominal glyph.
    pub fn nominal_glyph(&self, c: u32) -> Option<GlyphId> {
        self.face.get_nominal_glyph(c)
    }

    /// Maps a Unicode scalar value and variation selector to a glyph.
    pub fn variant_glyph(&self, c: u32, vs: u32) -> Option<GlyphId> {
        self.face.get_nominal_variant_glyph(c, vs)
    }

    /// Returns the horizontal advance for a glyph.
    pub fn advance_width(&self, glyph: GlyphId) -> i32 {
        self.face.glyph_h_advance(glyph)
    }

    /// Returns the vertical advance for a glyph.
    pub fn advance_height(&self, glyph: GlyphId) -> i32 {
        self.face.glyph_v_advance(glyph)
    }

    /// Returns the vertical origin for a glyph.
    pub fn vertical_origin(&self, glyph: GlyphId) -> (i32, i32) {
        (
            self.advance_width(glyph) / 2,
            self.face.glyph_v_origin(glyph),
        )
    }

    /// Returns extents for a glyph if available.
    pub fn extents(&self, glyph: GlyphId) -> Option<GlyphExtents> {
        let mut extents = GlyphExtents::default();
        if self.face.glyph_extents(glyph, &mut extents) {
            Some(extents)
        } else {
            None
        }
    }

    /// Populates horizontal advances for all entries in the batch.
    pub fn populate_advance_widths(&self, batch: AdvanceWidthBatch<'_>) {
        for (glyph, advance) in batch {
            *advance = self.face.glyph_h_advance(glyph);
        }
    }
}

/// Customizable font callback surface.
///
/// # Metrics scaling
///
/// All font metrics returned by these callbacks must be consistent with the
/// scale factor configured via
/// [`ShapeOptions::scale`](crate::ShapeOptions::scale).
///
/// If no scale is set, values must be in unscaled font units (i.e. the same
/// coordinate space as the font's `units_per_em`). If a scale is set —
/// for example `font_size * 64` for FreeType-style 26.6 — then all returned
/// values must already be in that scaled coordinate space.
pub trait FontFuncs {
    /// Nominal character-to-glyph mapping callback.
    fn nominal_glyph(&mut self, builtin: &BuiltinFontFuncs, c: u32) -> Option<GlyphId> {
        builtin.nominal_glyph(c)
    }

    /// Variation-selector mapping callback.
    fn variant_glyph(&mut self, builtin: &BuiltinFontFuncs, c: u32, vs: u32) -> Option<GlyphId> {
        builtin.variant_glyph(c, vs)
    }

    /// Horizontal advance callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what value this method should return.
    fn advance_width(&mut self, builtin: &BuiltinFontFuncs, glyph: GlyphId) -> i32 {
        builtin.advance_width(glyph)
    }

    /// Batch horizontal-advance callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what value this method should return.
    fn populate_advance_widths(
        &mut self,
        builtin: &BuiltinFontFuncs,
        batch: AdvanceWidthBatch<'_>,
    ) {
        for (glyph, advance) in batch {
            *advance = self.advance_width(builtin, glyph);
        }
    }

    /// Vertical advance callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what value this method should return.
    fn advance_height(&mut self, builtin: &BuiltinFontFuncs, glyph: GlyphId) -> i32 {
        builtin.advance_height(glyph)
    }

    /// Vertical origin callback.
    ///
    /// Returns the (x, y) coordinates of the vertical origin for the given glyph.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what values this method should return.
    fn vertical_origin(&mut self, builtin: &BuiltinFontFuncs, glyph: GlyphId) -> (i32, i32) {
        builtin.vertical_origin(glyph)
    }

    /// Glyph extents callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what values this method should return.
    fn extents(&mut self, builtin: &BuiltinFontFuncs, glyph: GlyphId) -> Option<GlyphExtents> {
        builtin.extents(glyph)
    }
}

pub(crate) struct FontFuncsDispatch<'a, 'u> {
    builtin: BuiltinFontFuncs<'a>,
    scale: Scale,
    funcs: Option<&'u mut (dyn FontFuncs + 'u)>,
}

impl<'a, 'u> FontFuncsDispatch<'a, 'u> {
    pub(crate) fn new(
        face: &'a hb_font_t<'a>,
        scale: Scale,
        funcs: Option<&'u mut (dyn FontFuncs + 'u)>,
    ) -> Self {
        Self {
            builtin: BuiltinFontFuncs::new(face),
            scale,
            funcs,
        }
    }

    #[inline(always)]
    pub(crate) fn font(&self) -> &'a hb_font_t<'a> {
        self.builtin.face
    }

    #[inline(always)]
    pub(crate) fn scale(&self) -> &Scale {
        &self.scale
    }

    #[inline(always)]
    fn scale_x(&self, value: i32) -> i32 {
        self.scale.scale_x(value)
    }

    #[inline(always)]
    fn scale_y(&self, value: i32) -> i32 {
        self.scale.scale_y(value)
    }

    #[inline(always)]
    fn scale_point(&self, point: (i32, i32)) -> (i32, i32) {
        (self.scale_x(point.0), self.scale_y(point.1))
    }

    #[inline(always)]
    fn scale_extents(&self, extents: GlyphExtents) -> GlyphExtents {
        self.scale.scale_extents(extents)
    }

    #[inline(always)]
    pub(crate) fn nominal_glyph(&mut self, c: u32) -> Option<GlyphId> {
        let cache = self.builtin.face.cmap_cache;
        if let Some(gid) = cache.get(c) {
            Some(gid.into())
        } else if let Some(funcs) = &mut self.funcs {
            if let Some(gid) = funcs.nominal_glyph(&self.builtin, c) {
                cache.set(c, gid.to_u32());
                Some(gid)
            } else {
                None
            }
        } else if let Some(gid) = self.builtin.nominal_glyph(c) {
            cache.set(c, gid.to_u32());
            Some(gid)
        } else {
            None
        }
    }

    #[inline(always)]
    pub(crate) fn has_glyph(&mut self, c: u32) -> bool {
        self.nominal_glyph(c).is_some()
    }

    #[inline(always)]
    pub(crate) fn variant_glyph(&mut self, c: u32, vs: u32) -> Option<GlyphId> {
        if let Some(funcs) = &mut self.funcs {
            funcs.variant_glyph(&self.builtin, c, vs)
        } else {
            self.builtin.variant_glyph(c, vs)
        }
    }

    #[inline(always)]
    pub(crate) fn advance_width(&mut self, glyph: GlyphId) -> i32 {
        if let Some(funcs) = &mut self.funcs {
            funcs.advance_width(&self.builtin, glyph)
        } else {
            self.scale_x(self.builtin.advance_width(glyph))
        }
    }

    #[inline(always)]
    pub(crate) fn advance_height(&mut self, glyph: GlyphId) -> i32 {
        if let Some(funcs) = &mut self.funcs {
            funcs.advance_height(&self.builtin, glyph)
        } else {
            self.scale_y(self.builtin.advance_height(glyph))
        }
    }

    #[inline(always)]
    pub(crate) fn vertical_origin(&mut self, glyph: GlyphId) -> (i32, i32) {
        if let Some(funcs) = &mut self.funcs {
            funcs.vertical_origin(&self.builtin, glyph)
        } else {
            self.scale_point(self.builtin.vertical_origin(glyph))
        }
    }

    #[inline(always)]
    pub(crate) fn extents(&mut self, glyph: GlyphId) -> Option<GlyphExtents> {
        if let Some(funcs) = &mut self.funcs {
            funcs.extents(&self.builtin, glyph)
        } else {
            Some(self.scale_extents(self.builtin.extents(glyph)?))
        }
    }

    pub(crate) fn populate_advance_widths(&mut self, batch: AdvanceWidthBatch<'_>) {
        if let Some(funcs) = &mut self.funcs {
            funcs.populate_advance_widths(&self.builtin, batch);
        } else {
            let font = self.font();
            font.glyph_metrics.populate_advance_widths(
                batch.infos,
                batch.positions,
                font.coords(),
                self.scale,
            );
        }
    }
}
