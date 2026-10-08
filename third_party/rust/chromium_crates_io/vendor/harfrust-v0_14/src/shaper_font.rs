//! The font data and callbacks visible to a shaping operation.

use alloc::boxed::Box;
use core::{mem::size_of, ops::Deref, ptr, slice};
use read_fonts::types::GlyphId;
use read_fonts::TableProvider;

use crate::aat::{AatCache, AatData, EMPTY_AAT_DATA};
use crate::buffer::{Buffer, GlyphInfo, GlyphPosition};
use crate::cache::Cache;
use crate::font::Font;
use crate::once::Once;
use crate::ot::{OtCache, OtData, EMPTY_OT_DATA};
use crate::scale::Scale;
use read_fonts::model::{
    charmap::Charmap,
    metrics::{GlyphMetrics, ScaledGlyphMetrics},
    name::GlyphName,
};

/// A font configured for shaping and font queries.
///
/// Uses the font's own character map and metrics by default. A scale and
/// custom [`FontFuncs`] can be set to change the queries used during shaping.
/// Dereferences to the underlying [`Font`] for direct font queries.
#[derive(Clone)]
pub struct ShaperFont<'a, 'f> {
    font: &'a Font,
    ot: OtData<'a>,
    aat: AatData<'a>,
    units_per_em: u16,
    apply_trak: bool,
    charmap: Charmap<'a>,
    glyph_metrics: Once<GlyphMetrics<'a>>,
    cmap_cache: Option<&'a CharmapCache>,
    symbol_font_page: Once<u16>,
    pub(crate) scale: Scale,
    funcs: Option<&'f dyn FontFuncs>,
}

impl<'a, 'f> ShaperFont<'a, 'f> {
    /// Prepares a font for shaping, reusing its cached layout data.
    pub fn new(font: &'a Font) -> Self {
        let cached = layout_cache(font);
        let (ot, aat, apply_trak, cmap_cache) = if let Some(cache) = cached {
            let tables = font.tables();
            let coords = font.normalized_coords();
            let feature_variations = if coords.is_empty() {
                [None; 2]
            } else {
                let variations = font.feature_variations();
                [variations.gsub, variations.gpos]
            };
            (
                OtData::from_tables(&tables, &cache.ot, coords, feature_variations),
                AatData::from_tables(&tables, &cache.aat),
                cache.apply_trak,
                Some(&cache.cmap),
            )
        } else {
            (EMPTY_OT_DATA.clone(), EMPTY_AAT_DATA.clone(), false, None)
        };
        let scale = Scale::default();
        Self {
            font,
            ot,
            aat,
            units_per_em: font.units_per_em(),
            apply_trak,
            charmap: font.charmap(),
            glyph_metrics: Once::new(),
            cmap_cache,
            symbol_font_page: Once::new(),
            scale,
            funcs: None,
        }
    }

    /// Returns the currently active normalized coordinates.
    pub fn normalized_coords(&self) -> &[crate::font::NormalizedCoord] {
        self.font.normalized_coords()
    }

    /// Sets the same scale on both axes.
    pub fn set_scale(&mut self, scale: i32) {
        self.set_scale_separate(scale, scale);
    }

    /// Returns a new shaping font with the same scale applied to both axes.
    pub fn with_scale(mut self, scale: i32) -> Self {
        self.set_scale(scale);
        self
    }

    /// Sets independent horizontal and vertical scales.
    pub fn set_scale_separate(&mut self, x_scale: i32, y_scale: i32) {
        self.scale = Scale::new(Some((x_scale, y_scale)), self.units_per_em as i32);
    }

    /// Returns a new shaping font with independent horizontal and vertical scales applied.
    pub fn with_scale_separate(mut self, x_scale: i32, y_scale: i32) -> Self {
        self.set_scale_separate(x_scale, y_scale);
        self
    }

    /// Replaces the callbacks used for effective font queries.
    pub fn set_font_funcs(&mut self, funcs: Option<&'f dyn FontFuncs>) {
        self.funcs = funcs;
    }

    /// Returns a new shaping font with the specified font function callbacks.
    pub fn with_font_funcs(mut self, funcs: Option<&'f dyn FontFuncs>) -> Self {
        self.set_font_funcs(funcs);
        self
    }

    /// Returns the conversion from font units to the configured scale.
    pub fn scale(&self) -> Scale {
        self.scale
    }

    /// Returns the name stored for a glyph in the font, if any.
    pub fn glyph_name(&self, glyph: GlyphId) -> Option<GlyphName> {
        self.font.glyph_name(glyph)
    }

    /// Maps a character through the configured callbacks.
    pub fn nominal_glyph(&self, codepoint: u32) -> Option<GlyphId> {
        self.funcs.map_or_else(
            || self.default_nominal_glyph(codepoint),
            |f| f.nominal_glyph(self, codepoint),
        )
    }

    /// Maps a character using the font's own charmap.
    pub fn default_nominal_glyph(&self, codepoint: u32) -> Option<GlyphId> {
        if let Some(gid) = self.cmap_cache().and_then(|cache| cache.get(codepoint)) {
            Some(gid.into())
        } else if let Some(gid) = self.map_unicode(codepoint) {
            if let Some(cache) = self.cmap_cache() {
                cache.set(codepoint, gid.to_u32());
            }
            Some(gid)
        } else {
            None
        }
    }

    /// Maps a batch of codepoints through the configured callbacks.
    pub fn nominal_glyphs(&self, glyphs: NominalGlyphs<'_>) -> usize {
        if let Some(funcs) = self.funcs {
            funcs.nominal_glyphs(self, glyphs)
        } else {
            self.default_nominal_glyphs(glyphs)
        }
    }

    /// Maps a batch of codepoints using the font's own charmap.
    pub fn default_nominal_glyphs(&self, glyphs: NominalGlyphs<'_>) -> usize {
        let mut done = 0;
        for (codepoint, glyph) in glyphs {
            match self.default_nominal_glyph(codepoint) {
                Some(gid) => *glyph = gid,
                None => break,
            }
            done += 1;
        }
        done
    }

    /// Maps a character and variation selector through the configured callbacks.
    pub fn variation_glyph(&self, codepoint: u32, selector: u32) -> Option<GlyphId> {
        self.funcs.map_or_else(
            || self.default_variation_glyph(codepoint, selector),
            |f| f.variation_glyph(self, codepoint, selector),
        )
    }

    /// Maps a variation sequence using the font's own charmap.
    pub fn default_variation_glyph(&self, codepoint: u32, selector: u32) -> Option<GlyphId> {
        self.charmap.map_unicode_variant(codepoint, selector)
    }

    /// Returns the effective horizontal advance.
    pub fn glyph_h_advance(&self, glyph: GlyphId) -> i32 {
        self.funcs.map_or_else(
            || self.default_glyph_h_advance(glyph),
            |f| f.glyph_h_advance(self, glyph),
        )
    }

    /// Returns the scaled horizontal advance from the font's own tables.
    pub fn default_glyph_h_advance(&self, glyph: GlyphId) -> i32 {
        self.glyph_metrics().h_advance(glyph)
    }

    /// Writes effective horizontal advances for a batch of glyphs.
    pub fn glyph_h_advances(&self, advances: Advances<'_>) {
        if let Some(funcs) = self.funcs {
            funcs.glyph_h_advances(self, advances);
        } else {
            self.default_glyph_h_advances(advances);
        }
    }

    /// Writes scaled horizontal advances from the font's own tables.
    pub fn default_glyph_h_advances(&self, advances: Advances<'_>) {
        self.glyph_metrics()
            .h_advance_batched(|advance| advance, advances.into_iter());
    }

    /// Returns the effective vertical advance.
    pub fn glyph_v_advance(&self, glyph: GlyphId) -> i32 {
        self.funcs.map_or_else(
            || self.default_glyph_v_advance(glyph),
            |f| f.glyph_v_advance(self, glyph),
        )
    }

    /// Returns the scaled vertical advance from the font's own tables.
    pub fn default_glyph_v_advance(&self, glyph: GlyphId) -> i32 {
        self.glyph_metrics().v_advance(glyph).saturating_neg()
    }

    /// Returns the effective vertical origin.
    pub fn glyph_v_origin(&self, glyph: GlyphId) -> (i32, i32) {
        self.funcs.map_or_else(
            || self.default_glyph_v_origin(glyph),
            |f| f.glyph_v_origin(self, glyph),
        )
    }

    /// Returns the scaled vertical origin from the font's own tables.
    pub fn default_glyph_v_origin(&self, glyph: GlyphId) -> (i32, i32) {
        let y = if self.funcs.is_some() {
            let extents = |glyph| {
                self.glyph_extents(glyph)
                    .map(|extents| read_fonts::model::metrics::GlyphExtents {
                        x_bearing: extents.x_bearing,
                        y_bearing: extents.y_bearing,
                        width: extents.width,
                        height: extents.height.saturating_neg(),
                    })
            };
            self.glyph_metrics()
                .with_glyph_extents(Some(&extents))
                .v_origin_y(glyph)
        } else {
            self.glyph_metrics().v_origin_y(glyph)
        };
        (self.glyph_h_advance(glyph) / 2, y)
    }

    /// Returns the effective glyph extents.
    pub fn glyph_extents(&self, glyph: GlyphId) -> Option<GlyphExtents> {
        self.funcs.map_or_else(
            || self.default_glyph_extents(glyph),
            |f| f.glyph_extents(self, glyph),
        )
    }

    /// Returns the scaled glyph extents from the font's own tables.
    pub fn default_glyph_extents(&self, glyph: GlyphId) -> Option<GlyphExtents> {
        self.glyph_metrics()
            .extents(glyph)
            .map(|extents| GlyphExtents {
                x_bearing: extents.x_bearing,
                y_bearing: extents.y_bearing,
                width: extents.width,
                height: extents.height.saturating_neg(),
            })
    }
}

impl Deref for ShaperFont<'_, '_> {
    type Target = Font;

    fn deref(&self) -> &Self::Target {
        self.font
    }
}

/// Overrides character mapping and glyph metrics for a [`ShaperFont`].
///
/// Scalar methods default to queries on the underlying font; batch methods
/// call their scalar counterparts. Implement only the callbacks that need
/// different behavior. Callbacks take `&self`; mutable state can be managed
/// by the implementation.
///
/// # Metrics scaling
///
/// All font metrics returned by these callbacks must be consistent with the
/// scale factor configured on [`ShaperFont`].
///
/// If no scale is set, values must be in unscaled font units (i.e. the same
/// coordinate space as the font's `units_per_em`). If a scale is set —
/// for example `font_size * 64` for FreeType-style 26.6 — then all returned
/// values must already be in that scaled coordinate space.
pub trait FontFuncs {
    /// Nominal character-to-glyph mapping callback.
    fn nominal_glyph(&self, font: &ShaperFont, c: u32) -> Option<GlyphId> {
        font.default_nominal_glyph(c)
    }

    /// Batch nominal character-to-glyph mapping callback.
    ///
    /// Maps a run of codepoints to glyphs, stopping at the first
    /// codepoint the font has no glyph for. Returns the number of
    /// consecutive codepoints mapped.
    fn nominal_glyphs(&self, font: &ShaperFont, glyphs: NominalGlyphs<'_>) -> usize {
        let mut done = 0;
        for (codepoint, glyph) in glyphs {
            match self.nominal_glyph(font, codepoint) {
                Some(gid) => *glyph = gid,
                None => break,
            }
            done += 1;
        }
        done
    }

    /// Variation-selector mapping callback.
    fn variation_glyph(&self, font: &ShaperFont, c: u32, vs: u32) -> Option<GlyphId> {
        font.default_variation_glyph(c, vs)
    }

    /// Horizontal advance callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what value this method should return.
    fn glyph_h_advance(&self, font: &ShaperFont, glyph: GlyphId) -> i32 {
        font.default_glyph_h_advance(glyph)
    }

    /// Batch horizontal-advance callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what value this method should return.
    fn glyph_h_advances(&self, font: &ShaperFont, advances: Advances<'_>) {
        for (glyph, advance) in advances {
            *advance = self.glyph_h_advance(font, glyph);
        }
    }

    /// Vertical advance callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what value this method should return.
    fn glyph_v_advance(&self, font: &ShaperFont, glyph: GlyphId) -> i32 {
        font.default_glyph_v_advance(glyph)
    }

    /// Vertical origin callback.
    ///
    /// Returns the (x, y) coordinates of the vertical origin for the given glyph.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what values this method should return.
    fn glyph_v_origin(&self, font: &ShaperFont, glyph: GlyphId) -> (i32, i32) {
        font.default_glyph_v_origin(glyph)
    }

    /// Glyph extents callback.
    ///
    /// See "Metrics scaling" in the [trait-level docs](FontFuncs) for details
    /// on what values this method should return.
    fn glyph_extents(&self, font: &ShaperFont, glyph: GlyphId) -> Option<GlyphExtents> {
        font.default_glyph_extents(glyph)
    }
}

/// The ink extents of a glyph.
///
/// Values use the configured font scale, or font units if no scale was set.
/// The bearings locate the top-left corner of the ink box relative to the
/// glyph origin. Height is usually negative in a Y-up coordinate system.
#[derive(Clone, Copy, Default, bytemuck::Pod, bytemuck::Zeroable)]
#[repr(C)]
pub struct GlyphExtents {
    /// Horizontal bearing from glyph origin to the left side of the ink box.
    pub x_bearing: i32,
    /// Vertical bearing from glyph origin to the top of the ink box.
    pub y_bearing: i32,
    /// Width of the glyph ink box.
    pub width: i32,
    /// Height of the glyph ink box.
    pub height: i32,
}

/// A batch of codepoints and writable nominal glyphs.
///
/// Nominal glyphs must be written for consecutive codepoints starting at
/// the first entry; mapping stops at the first codepoint the font has no
/// glyph for, and the number of glyphs written is returned from
/// [`FontFuncs::nominal_glyphs`](crate::FontFuncs::nominal_glyphs).
pub struct NominalGlyphs<'a> {
    pub(crate) infos: &'a mut [GlyphInfo],
}

impl<'a> NominalGlyphs<'a> {
    pub(crate) fn new(infos: &'a mut [GlyphInfo]) -> Self {
        Self { infos }
    }

    /// Returns the number of entries in the batch.
    pub fn len(&self) -> usize {
        self.infos.len()
    }

    /// Returns true if the batch is empty.
    pub fn is_empty(&self) -> bool {
        self.infos.is_empty()
    }
}

/// Iterates over codepoints and writable glyphs.
pub struct NominalGlyphsIter<'a> {
    infos: slice::IterMut<'a, GlyphInfo>,
}

impl<'a> Iterator for NominalGlyphsIter<'a> {
    type Item = (u32, &'a mut GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        let info = self.infos.next()?;
        let codepoint = info.glyph_id;
        let var_index = GlyphInfo::NORMALIZER_GLYPH_INDEX_VAR.var_index as usize - 1;
        Some((codepoint, bytemuck::cast_mut(&mut info.vars[var_index])))
    }
}

impl<'a> IntoIterator for NominalGlyphs<'a> {
    type Item = (u32, &'a mut GlyphId);
    type IntoIter = NominalGlyphsIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        NominalGlyphsIter {
            infos: self.infos.iter_mut(),
        }
    }
}

/// Raw C-style view over a batch of codepoints and output glyphs.
#[derive(Clone, Copy, Debug)]
pub struct RawNominalGlyphs {
    /// Number of batch entries.
    pub len: usize,
    /// Pointer to codepoints (read-only).
    pub codepoints: *const u32,
    /// Pointer to output glyphs (writable).
    pub glyphs: *mut u32,
    /// Byte stride between successive codepoints.
    pub codepoint_stride: isize,
    /// Byte stride between successive glyphs.
    pub glyph_stride: isize,
}

impl NominalGlyphs<'_> {
    /// Byte offset of the output glyph id within a batch entry.
    const GLYPH_OFFSET: usize = core::mem::offset_of!(GlyphInfo, vars)
        + (GlyphInfo::NORMALIZER_GLYPH_INDEX_VAR.var_index as usize - 1) * size_of::<u32>();

    /// Returns a raw C-style view over this batch.
    pub fn into_raw(self) -> RawNominalGlyphs {
        if self.infos.is_empty() {
            return RawNominalGlyphs {
                len: 0,
                codepoints: ptr::null(),
                glyphs: ptr::null_mut(),
                codepoint_stride: size_of::<GlyphInfo>() as isize,
                glyph_stride: size_of::<GlyphInfo>() as isize,
            };
        }

        let base = self.infos.as_mut_ptr();
        RawNominalGlyphs {
            len: self.infos.len(),
            // `glyph_id` is the first field in `GlyphInfo` and holds the
            // codepoint before mapping.
            codepoints: base.cast::<u32>().cast_const(),
            // The normalizer glyph-index var.
            glyphs: base.wrapping_byte_add(Self::GLYPH_OFFSET).cast::<u32>(),
            codepoint_stride: size_of::<GlyphInfo>() as isize,
            glyph_stride: size_of::<GlyphInfo>() as isize,
        }
    }
}

/// A batch of glyphs and their writable advances.
pub struct Advances<'a> {
    pub(crate) infos: &'a [GlyphInfo],
    pub(crate) positions: &'a mut [GlyphPosition],
}

impl<'a> Advances<'a> {
    pub(crate) fn new(buffer: &'a mut Buffer) -> Self {
        let len = buffer.len;
        Self {
            infos: &buffer.info[..len],
            positions: &mut buffer.pos[..len],
        }
    }

    /// Returns the number of entries in the batch.
    pub fn len(&self) -> usize {
        self.infos.len()
    }

    /// Returns true if the batch is empty.
    pub fn is_empty(&self) -> bool {
        self.infos.is_empty()
    }
}

/// Iterates over glyphs and their writable advances.
pub struct AdvancesIter<'a> {
    infos: slice::Iter<'a, GlyphInfo>,
    positions: slice::IterMut<'a, GlyphPosition>,
}

impl<'a> Iterator for AdvancesIter<'a> {
    type Item = (GlyphId, &'a mut i32);

    fn next(&mut self) -> Option<Self::Item> {
        let info = self.infos.next()?;
        let pos = self.positions.next()?;
        Some((info.as_glyph(), &mut pos.x_advance))
    }
}

impl<'a> IntoIterator for Advances<'a> {
    type Item = (GlyphId, &'a mut i32);
    type IntoIter = AdvancesIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        AdvancesIter {
            infos: self.infos.iter(),
            positions: self.positions.iter_mut(),
        }
    }
}

/// Raw C-style view over a batch of glyphs and their writable advances.
#[derive(Clone, Copy, Debug)]
pub struct RawAdvances {
    /// Number of batch entries.
    pub len: usize,
    /// Pointer to glyphs (read-only).
    pub gids: *const u32,
    /// Pointer to horizontal advances (writable).
    ///
    /// See "Metrics scaling" in the [FontFuncs] for details
    /// on what value this method should return.
    pub advances: *mut i32,
    /// Byte stride between successive glyphs.
    pub gid_stride: isize,
    /// Byte stride between successive advances.
    pub advance_stride: isize,
}

impl Advances<'_> {
    /// Returns a raw C-style view over this batch.
    pub fn into_raw(self) -> RawAdvances {
        if self.infos.is_empty() {
            return RawAdvances {
                len: 0,
                gids: ptr::null(),
                advances: ptr::null_mut(),
                gid_stride: size_of::<GlyphInfo>() as isize,
                advance_stride: size_of::<GlyphPosition>() as isize,
            };
        }

        RawAdvances {
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

pub(crate) type CharmapCache = Cache<21, 19, 256, 32>;

/// Font-wide layout data shared by shapers at different variation positions.
pub(crate) struct LayoutCache {
    pub ot: OtCache,
    pub aat: AatCache,
    pub cmap: CharmapCache,
    /// True if the font has both `trak` and `STAT` tables.
    pub apply_trak: bool,
}

impl LayoutCache {
    pub(crate) fn new(font: &Font) -> Self {
        let tables = font.tables();
        let ot = OtCache::new(&tables);
        let aat = AatCache::new(&tables, &ot);
        let apply_trak = tables.trak_data().is_some() && tables.stat_data().is_some();
        Self {
            ot,
            aat,
            cmap: CharmapCache::new(),
            apply_trak,
        }
    }
}

/// Returns cached layout data, or `None` if the read-fonts interop slot
/// contains an unexpected type.
fn layout_cache(font: &Font) -> Option<&LayoutCache> {
    let data = crate::font::_font_interop::_get_or_init_shaping_data(font, || {
        Box::new(LayoutCache::new(font))
    });
    let cache = data.downcast_ref::<LayoutCache>()?;
    Some(cache)
}

/// Prepared, read-only data for one font and variation position.
///
/// The table views borrow a long-lived `LayoutCache`.
#[derive(Clone, Copy)]
pub(crate) struct LayoutData<'a> {
    pub ot: &'a OtData<'a>,
    pub aat: &'a AatData<'a>,
    pub units_per_em: u16,
    pub apply_trak: bool,
}

impl<'a> ShaperFont<'a, '_> {
    pub(crate) fn layout(&self) -> LayoutData<'_> {
        LayoutData {
            ot: &self.ot,
            aat: &self.aat,
            units_per_em: self.units_per_em,
            apply_trak: self.apply_trak,
        }
    }

    pub(crate) fn has_glyph(&self, codepoint: u32) -> bool {
        self.nominal_glyph(codepoint).is_some()
    }

    fn cmap_cache(&self) -> Option<&CharmapCache> {
        self.cmap_cache
    }

    fn symbol_font_page(&self) -> u16 {
        *self.symbol_font_page.get_or_init(|| {
            if self.charmap.unicode_is_symbol() {
                legacy_symbol_font_page(self.font.tables().os2().ok().as_ref())
            } else {
                0
            }
        })
    }

    fn glyph_metrics(&self) -> ScaledGlyphMetrics<'a, 'static, Scale> {
        (*self.glyph_metrics.get_or_init(|| self.font.glyph_metrics())).scaled(self.scale)
    }

    fn map_unicode(&self, codepoint: u32) -> Option<GlyphId> {
        self.charmap.map_unicode(codepoint).or_else(|| {
            let mapped = match self.symbol_font_page() {
                0xB200 => arabic_pua_map(codepoint, true),
                0xB300 => arabic_pua_map(codepoint, false),
                _ => 0,
            };
            (mapped != 0)
                .then(|| self.charmap.map_unicode(mapped))
                .flatten()
        })
    }
}

fn legacy_symbol_font_page(os2: Option<&read_fonts::tables::os2::Os2<'_>>) -> u16 {
    let Some(os2) = os2.filter(|os2| os2.version() == 0) else {
        return 0;
    };
    os2.offset_data()
        .read_at::<u16>(os2.fs_selection_byte_range().start)
        .unwrap_or_default()
        & 0xFF00
}

fn arabic_pua_map(codepoint: u32, simplified: bool) -> u32 {
    let Ok(codepoint) = usize::try_from(codepoint) else {
        return 0;
    };
    let mapped = if simplified {
        crate::ot::shaper::arabic_pua::arabic_pua_simp_map(codepoint)
    } else {
        crate::ot::shaper::arabic_pua::arabic_pua_trad_map(codepoint)
    };
    u32::from(mapped)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{aat::AatData, ot::OtData, Tag};
    use core::sync::atomic::{AtomicUsize, Ordering};
    use read_fonts::{FontRef, TableProvider};

    use alloc::sync::Arc;
    use read_fonts::model::{Blob, TableFunction};

    #[test]
    fn cached_glyph_metrics_are_sync() {
        fn assert_send_sync<T: Send + Sync>() {}
        assert_send_sync::<Once<GlyphMetrics<'static>>>();
    }

    #[test]
    fn cached_table_disposition_skips_missing_tables() {
        let source = FontRef::new(include_bytes!("../benches/fonts/Roboto-Regular.ttf")).unwrap();
        let loads = Arc::new(AtomicUsize::new(0));
        let load_count = loads.clone();
        let tables = TableFunction::new(Arc::new(move |tag| {
            load_count.fetch_add(1, Ordering::Relaxed);
            source
                .data_for_tag(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        }));
        let font = Font::new(tables, 0).unwrap();
        let cache = LayoutCache::new(&font);

        loads.store(0, Ordering::Relaxed);
        let tables = font.tables();
        let ot_data = OtData::from_tables(&tables, &cache.ot, &[], [None; 2]);
        let aat_data = AatData::from_tables(&tables, &cache.aat);

        assert!(ot_data.gsub.is_some());
        assert!(ot_data.gpos.is_some());
        assert!(ot_data.gdef.table.is_some());
        assert!(aat_data.morx.is_none());
        assert!(aat_data.mort.is_none());
        assert!(aat_data.ankr.is_none());
        assert!(aat_data.kern.is_none());
        assert!(aat_data.kerx.is_none());
        assert!(aat_data.trak.is_none());
        assert!(aat_data.feat.is_none());
        assert!(aat_data.ltag.is_none());
        // The layout cache has already loaded these tables through Font.
        assert_eq!(loads.load(Ordering::Relaxed), 0);
    }

    #[test]
    fn charmap_and_glyph_metrics_load_on_first_query() {
        let source = FontRef::new(include_bytes!("../benches/fonts/Roboto-Regular.ttf")).unwrap();
        let cmap_loads = Arc::new(AtomicUsize::new(0));
        let hmtx_loads = Arc::new(AtomicUsize::new(0));
        let cmap_count = cmap_loads.clone();
        let hmtx_count = hmtx_loads.clone();
        let tables = TableFunction::new(Arc::new(move |tag| {
            if tag == Tag::new(b"cmap") {
                cmap_count.fetch_add(1, Ordering::Relaxed);
            } else if tag == Tag::new(b"hmtx") {
                hmtx_count.fetch_add(1, Ordering::Relaxed);
            }
            source
                .data_for_tag(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        }));
        let font = Font::new(tables, 0).unwrap();
        let shaper = ShaperFont::new(&font);
        assert_eq!(cmap_loads.load(Ordering::Relaxed), 0);
        assert_eq!(hmtx_loads.load(Ordering::Relaxed), 0);

        assert!(shaper.default_nominal_glyph('A' as u32).is_some());
        assert_eq!(cmap_loads.load(Ordering::Relaxed), 1);
        assert_eq!(hmtx_loads.load(Ordering::Relaxed), 0);

        let _ = shaper.default_glyph_h_advance(GlyphId::new(1));
        assert_eq!(hmtx_loads.load(Ordering::Relaxed), 1);
    }

    #[test]
    fn maps_legacy_arabic_symbol_fonts() {
        let simplified = Font::new(
            include_bytes!("../tests/fonts/in-house/SimpArabicTest.ttf").to_vec(),
            0,
        )
        .unwrap();
        assert_eq!(
            ShaperFont::new(&simplified).default_nominal_glyph(0x0627),
            Some(GlyphId::new(45))
        );

        let traditional = Font::new(
            include_bytes!("../tests/fonts/in-house/TradArabicTest.ttf").to_vec(),
            0,
        )
        .unwrap();
        assert_eq!(
            ShaperFont::new(&traditional).default_nominal_glyph(0x0627),
            Some(GlyphId::new(65))
        );
    }

    #[test]
    fn extents_scale_from_corners_like_harfbuzz() {
        // HarfBuzz scales corners in floating point, floors the bearings,
        // ceils the far corners, and then derives width/height from them.
        let scale = Scale::new(Some((1500, 1500)), 1000);
        let extents = GlyphExtents {
            x_bearing: 1,
            y_bearing: 4,
            width: 3,
            height: -2,
        };
        let scaled = scale.scale_extents(extents);
        assert_eq!(scaled.x_bearing, 1);
        assert_eq!(scaled.y_bearing, 6);
        assert_eq!(scaled.width, 5);
        assert_eq!(scaled.height, -3);
    }

    #[test]
    fn full_range_extents_saturate() {
        let extents = GlyphExtents {
            x_bearing: i32::MAX,
            y_bearing: i32::MIN,
            width: i32::MAX,
            height: i32::MIN,
        };

        let scaled = Scale::default().scale_extents(extents);
        assert_eq!(scaled.x_bearing, i32::MAX);
        assert_eq!(scaled.y_bearing, i32::MIN);
        assert_eq!(scaled.width, i32::MAX);
        assert_eq!(scaled.height, i32::MIN);
    }
}
