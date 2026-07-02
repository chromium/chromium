use alloc::boxed::Box;
use read_fonts::types::{F2Dot14, Fixed};
use read_fonts::{FontRef, TableProvider};
use smallvec::SmallVec;

// libm used for f32::floor() and f32::ceil()
#[cfg(not(feature = "std"))]
#[allow(unused_imports)]
use core_maths::CoreFloat as _;

use super::aat::AatTables;
use super::charmap::{cache_t as cmap_cache_t, Charmap};
use super::font_funcs::FontFuncsDispatch;
use super::glyph_metrics::GlyphMetrics;
use super::glyph_names::GlyphNames;
use super::ot::{LayoutTable, OtCache, OtTables};
use super::ot_layout::TableIndex;
use super::ot_shape::OtShapeContext;
use crate::hb::aat::AatCache;
use crate::hb::tables::TableRanges;
use crate::{script, Feature, GlyphBuffer, NormalizedCoord, ShapePlan, UnicodeBuffer, Variation};

pub use super::font_funcs::{AdvanceWidthBatch, BuiltinFontFuncs, FontFuncs, RawAdvanceWidthBatch};

/// Data required for shaping with a single font.
pub struct ShaperData {
    table_ranges: TableRanges,
    ot_cache: OtCache,
    aat_cache: AatCache,
    cmap_cache: cmap_cache_t,
    // True if a font has both trak and STAT tables.
    apply_trak: bool,
}

impl ShaperData {
    /// Creates new cached shaper data for the given font.
    pub fn new(font: &FontRef) -> Self {
        let ot_cache = OtCache::new(font);
        let aat_cache = AatCache::new(font);
        let table_ranges = TableRanges::new(font);
        let cmap_cache = cmap_cache_t::new();
        let apply_trak = font.trak().is_ok() && font.stat().is_ok();
        Self {
            table_ranges,
            ot_cache,
            aat_cache,
            cmap_cache,
            apply_trak,
        }
    }

    fn from_font(font: &crate::font::Font) -> Self {
        let tables = font.tables();
        let ot_cache = OtCache::new(&tables);
        let aat_cache = AatCache::new(&tables);
        let table_ranges = TableRanges::from_tables(&tables);
        let cmap_cache = cmap_cache_t::new();
        let apply_trak = tables.trak_data().is_some() && tables.stat_data().is_some();
        Self {
            table_ranges,
            ot_cache,
            aat_cache,
            cmap_cache,
            apply_trak,
        }
    }

    /// Returns a builder for constructing a new shaper with the given
    /// font.
    pub fn shaper<'a>(&'a self, font: &FontRef<'a>) -> ShaperBuilder<'a> {
        ShaperBuilder {
            data: self,
            font: font.clone(),
            instance: None,
        }
    }
}

// Maximum number of coordinates to store inline before spilling to the
// heap.
//
// Any value between 5 and 11 yields a SmallVec footprint of 32 bytes.
const MAX_INLINE_COORDS: usize = 11;

/// An instance of a variable font.
#[derive(Clone, Default, Debug)]
pub struct ShaperInstance {
    coords: SmallVec<[F2Dot14; MAX_INLINE_COORDS]>,
    pub(crate) feature_variations: [Option<u32>; 2],
    // TODO: this is a good place to hang variation specific caches
}

impl ShaperInstance {
    /// Creates a new shaper instance for the given font from the specified
    /// list of variation settings.
    ///
    /// The setting values are in user space and the order is insignificant.
    pub fn from_variations<V>(font: &FontRef, variations: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        let mut this = Self::default();
        this.set_variations(font, variations);
        this
    }

    /// Creates a new shaper instance for the given font from the specified
    /// set of normalized coordinates.
    ///
    /// The sequence of coordinates is expected to be in axis order.
    pub fn from_coords(font: &FontRef, coords: impl IntoIterator<Item = NormalizedCoord>) -> Self {
        let mut this = Self::default();
        this.set_coords(font, coords);
        this
    }

    /// Creates a new shaper instance for the given font using the variation
    /// position from the named instance at the specified index.
    pub fn from_named_instance(font: &FontRef, index: usize) -> Self {
        let mut this = Self::default();
        this.set_named_instance(font, index);
        this
    }

    /// Returns the underlying set of normalized coordinates.
    pub fn coords(&self) -> &[F2Dot14] {
        &self.coords
    }

    /// Resets the instance for the given font and variation settings.
    pub fn set_variations<V>(&mut self, font: &FontRef, variations: V)
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        self.coords.clear();
        if let Ok(fvar) = font.fvar() {
            self.coords
                .resize(fvar.axis_count() as usize, F2Dot14::ZERO);
            fvar.user_to_normalized(
                font.avar().ok().as_ref(),
                variations
                    .into_iter()
                    .map(Into::into)
                    .map(|var| (var.tag, Fixed::from_f64(var.value as _))),
                self.coords.as_mut_slice(),
            );
            self.check_default();
            self.set_feature_variations(font);
        }
    }

    /// Resets the instance for the given font and normalized coordinates.
    pub fn set_coords(&mut self, font: &FontRef, coords: impl IntoIterator<Item = F2Dot14>) {
        self.coords.clear();
        if let Ok(fvar) = font.fvar() {
            let count = fvar.axis_count() as usize;
            self.coords.reserve(count);
            self.coords.extend(coords.into_iter().take(count));
            self.check_default();
            self.set_feature_variations(font);
        }
    }

    /// Resets the instance for the given font using the variation
    /// position from the named instance at the specified index.
    pub fn set_named_instance(&mut self, font: &FontRef, index: usize) {
        self.coords.clear();
        if let Ok(fvar) = font.fvar() {
            if let Ok((axes, instance)) = fvar
                .axis_instance_arrays()
                .and_then(|arrays| Ok((arrays.axes(), arrays.instances().get(index)?)))
            {
                self.set_variations(
                    font,
                    axes.iter()
                        .zip(instance.coordinates)
                        .map(|(axis, coord)| (axis.axis_tag(), coord.get().to_f32())),
                );
            }
        }
    }

    fn set_feature_variations(&mut self, font: &FontRef) {
        self.feature_variations = [None; 2];
        if self.coords.is_empty() {
            return;
        }
        self.feature_variations[0] = font
            .gsub()
            .ok()
            .and_then(|t| LayoutTable::Gsub(t).feature_variation_index(&self.coords));
        self.feature_variations[1] = font
            .gpos()
            .ok()
            .and_then(|t| LayoutTable::Gpos(t).feature_variation_index(&self.coords));
    }

    fn check_default(&mut self) {
        if self.coords.iter().all(|coord| *coord == F2Dot14::ZERO) {
            self.coords.clear();
        }
    }
}

/// Builder type for constructing a [`Shaper`](crate::Shaper).
pub struct ShaperBuilder<'a> {
    data: &'a ShaperData,
    font: FontRef<'a>,
    instance: Option<&'a ShaperInstance>,
}

impl<'a> ShaperBuilder<'a> {
    /// Sets an optional instance for the shaper.
    ///
    /// This defines the variable font configuration.
    pub fn instance(mut self, instance: Option<&'a ShaperInstance>) -> Self {
        self.instance = instance;
        self
    }

    /// Builds the shaper with the current configuration.
    pub fn build(self) -> crate::Shaper<'a> {
        let font = self.font;
        let units_per_em = self.data.table_ranges.units_per_em;
        let charmap = Charmap::new(&font, &self.data.table_ranges);
        let glyph_metrics = GlyphMetrics::new(&font, &self.data.table_ranges);
        let (coords, feature_variations) = self
            .instance
            .map(|instance| (instance.coords(), instance.feature_variations))
            .unwrap_or_default();
        let ot_tables = OtTables::new(
            &font,
            &self.data.ot_cache,
            &self.data.table_ranges,
            coords,
            feature_variations,
        );
        let aat_tables = AatTables::new(&font, &self.data.aat_cache, &self.data.table_ranges);
        let font = FontKind::FontRef(FontRefData {
            font,
            glyph_metrics,
            charmap,
        });
        hb_font_t {
            font,
            units_per_em,
            cmap_cache: &self.data.cmap_cache,
            ot_tables,
            aat_tables,
            apply_trak: self.data.apply_trak,
        }
    }
}

/// Options which can be used to configure shaping.
#[derive(Default)]
pub struct ShapeOptions<'a> {
    plan: Option<&'a ShapePlan>,
    scale: Option<(i32, i32)>,
    point_size: Option<f32>,
    features: &'a [Feature],
    font_funcs: Option<&'a mut (dyn FontFuncs + 'a)>,
}

impl<'a> ShapeOptions<'a> {
    /// Creates a default set of shape options ready for configuration.
    pub fn new() -> Self {
        Self::default()
    }

    /// Sets the plan to use for shaping.
    ///
    /// The shape plan must be compatible with the properties of the buffer
    /// passed to shaping.
    pub fn plan(mut self, plan: Option<&'a ShapePlan>) -> Self {
        self.plan = plan;
        self
    }

    /// Sets the scale factor to use during shaping.
    ///
    /// The font scale is a number related to, but not the same as, font size.
    /// Typically the client establishes a scale factor to be used between the
    /// two. For example, 64, or 256, which would be the fractional-precision
    /// part of the font scale. This is necessary because position and metric
    /// values are integer types and you need to leave room for fractional
    /// values in there.
    ///
    /// For example, to set the font size to 20, with 64 levels of fractional
    /// precision you would call provide a scale of `20 * 64`.
    ///
    /// In the example above, even what font size 20 means is up to you. It
    /// might be 20 pixels, or 20 points, or 20 millimeters. HarfRust does
    /// not care about that.
    ///
    /// The choice of scale is yours but needs to be consistent between what
    /// you set here, and what you expect as output as well as the values
    /// returned by [font functions](FontFuncs).
    ///
    /// This defaults to `None` which means that no scale is applied-- positions
    /// and metrics will be returned in font units.
    pub fn scale(mut self, scale: Option<i32>) -> Self {
        self.scale = scale.map(|s| (s, s));
        self
    }

    /// Sets separate x- and y-scale factors to use during shaping.
    ///
    /// Each axis uses the same semantics as [`scale`](Self::scale).
    pub fn scale_separate(mut self, scale: Option<(i32, i32)>) -> Self {
        self.scale = scale;
        self
    }

    /// Sets the size used for application of the tracking table.
    pub fn point_size(mut self, point_size: Option<f32>) -> Self {
        self.point_size = point_size;
        self
    }

    /// Sets the features to apply during shaping.
    pub fn features(mut self, features: &'a [Feature]) -> Self {
        self.features = features;
        self
    }

    /// Sets optional font functions used for shaping.
    pub fn font_funcs(mut self, funcs: Option<&'a mut (dyn FontFuncs + 'a)>) -> Self {
        self.font_funcs = funcs;
        self
    }
}

#[derive(Copy, Clone)]
pub(crate) struct Scale {
    x_mult: i64,
    y_mult: i64,
    x_multf: f32,
    y_multf: f32,
}

impl Default for Scale {
    fn default() -> Self {
        Self {
            x_mult: 1 << 16,
            y_mult: 1 << 16,
            x_multf: 1.0,
            y_multf: 1.0,
        }
    }
}

// Various conversions between f32 and i32
#[allow(clippy::cast_precision_loss)]
impl Scale {
    pub(crate) fn new(scale: Option<(i32, i32)>, upem: i32) -> Self {
        let (Some((x_scale, y_scale)), true) = (scale, upem != 0) else {
            // When scale is not configured, or upem is zero, return results
            // in font units.
            return Self::default();
        };
        let [x_mult, y_mult] = [x_scale, y_scale].map(|s| Self::mult_from_scale(s, upem));
        let upem = upem as f32;
        Self {
            x_mult,
            y_mult,
            x_multf: x_scale as f32 / upem,
            y_multf: y_scale as f32 / upem,
        }
    }

    #[inline(always)]
    pub(crate) fn scale_x(&self, x: i32) -> i32 {
        Self::scale_by_mult(x, self.x_mult)
    }

    #[inline(always)]
    pub(crate) fn scale_y(&self, y: i32) -> i32 {
        Self::scale_by_mult(y, self.y_mult)
    }

    /// Scales glyph extents using HarfBuzz's corner-based float arithmetic:
    /// floor the origin corners and ceil the far corners before deriving the
    /// final width/height.
    /// hb_font_t::scale_glyph_extents: <https://github.com/harfbuzz/harfbuzz/blob/88adc6437ef561486a5adf1822410297ef4a852b/src/hb-font.hh#L201>'
    pub(crate) fn scale_extents(&self, mut extents: GlyphExtents) -> GlyphExtents {
        let x1 = extents.x_bearing as f32 * self.x_multf;
        let y1 = extents.y_bearing as f32 * self.y_multf;
        let x2 = (extents.x_bearing + extents.width) as f32 * self.x_multf;
        let y2 = (extents.y_bearing + extents.height) as f32 * self.y_multf;
        extents.x_bearing = x1.floor() as i32;
        extents.y_bearing = y1.floor() as i32;
        extents.width = x2.ceil() as i32 - extents.x_bearing;
        extents.height = y2.ceil() as i32 - extents.y_bearing;
        extents
    }

    #[inline(always)]
    fn mult_from_scale(scale: i32, upem: i32) -> i64 {
        if scale < 0 {
            -((-(scale as i64)) << 16) / upem as i64
        } else {
            ((scale as i64) << 16) / upem as i64
        }
    }

    #[inline(always)]
    fn scale_by_mult(value: i32, mult: i64) -> i32 {
        (((value as i64) * mult + 32768) >> 16) as i32
    }
}

/// Shapes the buffer content using provided options.
///
/// Consumes the buffer. You can then run [`GlyphBuffer::clear`] to get the [`UnicodeBuffer`] back
/// without allocating a new one.
///
/// If a plan is provided, it is up to the caller to ensure that the shape plan matches the
/// properties of the provided buffer, otherwise the shaping result will likely be incorrect.
///
/// # Panics
///
/// Will panic when debugging assertions are enabled if the buffer and plan have mismatched
/// properties.
pub fn shape(
    font: &crate::font::FontInstance,
    mut buffer: UnicodeBuffer,
    mut options: ShapeOptions<'_>,
) -> GlyphBuffer {
    let Some(hb_font) = hb_font_t::from_font(font) else {
        buffer.clear();
        return GlyphBuffer(buffer.0);
    };
    // If the user didn't request an explicit scale but the font instance
    // has a size, set the scale to that size with 16 fractional bits.
    if options.scale.is_none() {
        if let Some(ppem) = font.size() {
            options = options.scale(Some((ppem * 65536.0) as i32));
        }
    }
    hb_font.shape(buffer, options)
}

// This will go away completely when we drop the old API.
#[allow(clippy::large_enum_variant)]
#[derive(Clone)]
pub enum FontKind<'a> {
    FontRef(FontRefData<'a>),
    FontInstance(&'a crate::font::FontInstance, BasicFontMetrics),
}

#[derive(Clone)]
pub struct FontRefData<'a> {
    pub(crate) font: FontRef<'a>,
    pub(crate) glyph_metrics: GlyphMetrics<'a>,
    pub(crate) charmap: Charmap<'a>,
}

#[derive(Copy, Clone, Debug)]
pub struct BasicFontMetrics {
    pub units_per_em: u16,
    pub num_glyphs: u32,
    pub ascent: i16,
    pub descent: i16,
}

/// A configured shaper.
#[derive(Clone)]
pub struct hb_font_t<'a> {
    pub(crate) font: FontKind<'a>,
    pub(crate) units_per_em: u16,
    pub(crate) cmap_cache: &'a cmap_cache_t,
    pub(crate) ot_tables: OtTables<'a>,
    pub(crate) aat_tables: AatTables<'a>,
    pub(crate) apply_trak: bool,
}

impl<'a> crate::Shaper<'a> {
    pub(crate) fn from_font(font: &'a crate::font::FontInstance) -> Option<Self> {
        let data = crate::font::_font_interop::_get_or_init_shaping_data(font, || {
            Box::new(ShaperData::from_font(font))
        })
        .downcast_ref::<ShaperData>()?;
        let metrics = BasicFontMetrics {
            units_per_em: data.table_ranges.units_per_em,
            num_glyphs: data.table_ranges.num_glyphs,
            ascent: data.table_ranges.ascent,
            descent: data.table_ranges.descent,
        };
        let coords = font.normalized_coords();
        let feature_variations = font.feature_variations();
        let feature_variations = [feature_variations.gsub(), feature_variations.gpos()];
        let tables = font.tables();
        let ot_tables = OtTables::from_tables(&tables, &data.ot_cache, coords, feature_variations);
        let aat_tables = AatTables::from_tables(&tables, &ot_tables, &data.aat_cache);
        Some(Self {
            font: FontKind::FontInstance(font, metrics),
            units_per_em: data.table_ranges.units_per_em,
            cmap_cache: &data.cmap_cache,
            ot_tables,
            aat_tables,
            apply_trak: data.apply_trak,
        })
    }

    /// Returns font's units per EM.
    #[inline]
    pub fn units_per_em(&self) -> i32 {
        self.units_per_em as i32
    }

    /// Returns the currently active normalized coordinates.
    pub fn coords(&self) -> &'a [NormalizedCoord] {
        self.ot_tables.coords
    }

    /// Shapes the buffer content using provided options.
    ///
    /// Consumes the buffer. You can then run [`GlyphBuffer::clear`] to get the [`UnicodeBuffer`] back
    /// without allocating a new one.
    ///
    /// If a plan is provided, it is up to the caller to ensure that the shape plan matches the
    /// properties of the provided buffer, otherwise the shaping result will likely be incorrect.
    ///
    /// # Panics
    ///
    /// Will panic when debugging assertions are enabled if the buffer and plan have mismatched
    /// properties.    
    pub fn shape(&self, buffer: UnicodeBuffer, options: ShapeOptions<'_>) -> GlyphBuffer {
        if let Some(plan) = options.plan {
            self.shape_with_plan(plan, buffer, options)
        } else {
            let plan = ShapePlan::new(
                self,
                buffer.0.direction,
                buffer.0.script,
                buffer.0.language.as_ref(),
                options.features,
            );
            self.shape_with_plan(&plan, buffer, options)
        }
    }

    fn shape_with_plan(
        &self,
        plan: &ShapePlan,
        buffer: UnicodeBuffer,
        options: ShapeOptions<'_>,
    ) -> GlyphBuffer {
        let mut buffer = buffer.0;
        buffer.enter();

        assert_eq!(
            buffer.direction, plan.direction,
            "Buffer direction does not match plan direction: {:?} != {:?}",
            buffer.direction, plan.direction
        );
        assert_eq!(
            buffer.script.unwrap_or(script::UNKNOWN),
            plan.script.unwrap_or(script::UNKNOWN),
            "Buffer script does not match plan script: {:?} != {:?}",
            buffer.script.unwrap_or(script::UNKNOWN),
            plan.script.unwrap_or(script::UNKNOWN)
        );

        if buffer.len > 0 {
            // Save the original direction, we use it later.
            let target_direction = buffer.direction;
            let scale = Scale::new(options.scale, self.units_per_em as i32);
            let mut font_funcs = FontFuncsDispatch::new(self, scale, options.font_funcs);
            OtShapeContext {
                plan,
                face: self,
                buffer: &mut buffer,
                target_direction,
                features: options.features,
                point_size: options.point_size,
                font_funcs: &mut font_funcs,
            }
            .shape_internal();
        }

        buffer.leave();

        GlyphBuffer(buffer)
    }

    pub(crate) fn glyph_names(&self) -> GlyphNames<'a> {
        GlyphNames::new(&self.font)
    }

    pub(crate) fn glyph_metrics(&self) -> GlyphMetrics<'a> {
        match &self.font {
            FontKind::FontRef(data) => data.glyph_metrics.clone(),
            FontKind::FontInstance(instance, metrics) => {
                GlyphMetrics::from_tables(&instance.tables(), metrics)
            }
        }
    }

    pub(crate) fn layout_table(&self, table_index: TableIndex) -> Option<LayoutTable<'a>> {
        match table_index {
            TableIndex::GSUB => self
                .ot_tables
                .gsub
                .as_ref()
                .map(|table| LayoutTable::Gsub(table.table.clone())),
            TableIndex::GPOS => self
                .ot_tables
                .gpos
                .as_ref()
                .map(|table| LayoutTable::Gpos(table.table.clone())),
        }
    }

    pub(crate) fn layout_tables(&self) -> impl Iterator<Item = (TableIndex, LayoutTable<'a>)> + '_ {
        TableIndex::iter().filter_map(move |idx| self.layout_table(idx).map(|table| (idx, table)))
    }
}

/// Glyph ink extents in font units.
///
/// This matches HarfBuzz's glyph extents layout and semantics.
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

#[cfg(test)]
mod tests {
    use super::*;

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
}
