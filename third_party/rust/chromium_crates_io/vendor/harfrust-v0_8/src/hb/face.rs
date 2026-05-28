use read_fonts::types::{F2Dot14, Fixed, GlyphId};
use read_fonts::{FontRef, TableProvider};
use smallvec::SmallVec;

use super::aat::AatTables;
use super::charmap::{cache_t as cmap_cache_t, Charmap};
use super::glyph_metrics::GlyphMetrics;
use super::glyph_names::GlyphNames;
use super::ot::{LayoutTable, OtCache, OtTables};
use super::ot_layout::TableIndex;
use super::ot_shape::OtShapeContext;
use crate::hb::aat::AatCache;
use crate::hb::buffer::hb_buffer_t;
use crate::hb::tables::TableRanges;
use crate::{script, Feature, GlyphBuffer, NormalizedCoord, ShapePlan, UnicodeBuffer, Variation};

/// Data required for shaping with a single font.
pub struct ShaperData {
    table_ranges: TableRanges,
    ot_cache: OtCache,
    aat_cache: AatCache,
    cmap_cache: cmap_cache_t,
}

impl ShaperData {
    /// Creates new cached shaper data for the given font.
    pub fn new(font: &FontRef) -> Self {
        let ot_cache = OtCache::new(font);
        let aat_cache = AatCache::new(font);
        let table_ranges = TableRanges::new(font);
        let cmap_cache = cmap_cache_t::new();
        Self {
            table_ranges,
            ot_cache,
            aat_cache,
            cmap_cache,
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
        let charmap = Charmap::new(&font, &self.data.table_ranges, &self.data.cmap_cache);
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
        hb_font_t {
            font,
            units_per_em,
            charmap,
            glyph_metrics,
            ot_tables,
            aat_tables,
        }
    }
}

/// Options which can be used to configure shaping.
#[derive(Default)]
pub struct ShapeOptions<'a> {
    plan: Option<&'a ShapePlan>,
    point_size: Option<f32>,
    features: &'a [Feature],
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
}

/// A configured shaper.
#[derive(Clone)]
pub struct hb_font_t<'a> {
    pub(crate) font: FontRef<'a>,
    pub(crate) units_per_em: u16,
    charmap: Charmap<'a>,
    glyph_metrics: GlyphMetrics<'a>,
    pub(crate) ot_tables: OtTables<'a>,
    pub(crate) aat_tables: AatTables<'a>,
}

impl<'a> crate::Shaper<'a> {
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
    pub fn shape(&self, buffer: UnicodeBuffer, options: ShapeOptions) -> GlyphBuffer {
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

    /// Shapes the buffer content using the provided font and plan.
    ///
    /// Consumes the buffer. You can then run [`GlyphBuffer::clear`] to get the [`UnicodeBuffer`] back
    /// without allocating a new one.
    ///
    /// It is up to the caller to ensure that the shape plan matches the properties of the provided
    /// buffer, otherwise the shaping result will likely be incorrect.
    ///
    /// # Panics
    ///
    /// Will panic when debugging assertions are enabled if the buffer and plan have mismatched
    /// properties.
    fn shape_with_plan(
        &self,
        plan: &ShapePlan,
        buffer: UnicodeBuffer,
        options: ShapeOptions,
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
            OtShapeContext {
                plan,
                face: self,
                buffer: &mut buffer,
                target_direction,
                features: options.features,
                point_size: options.point_size,
            }
            .shape_internal();
        }

        buffer.leave();

        GlyphBuffer(buffer)
    }

    pub(crate) fn has_glyph(&self, c: u32) -> bool {
        self.get_nominal_glyph(c).is_some()
    }

    pub(crate) fn get_nominal_glyph(&self, c: u32) -> Option<GlyphId> {
        self.charmap.map(c)
    }

    pub(crate) fn get_nominal_variant_glyph(&self, c: u32, vs: u32) -> Option<GlyphId> {
        self.charmap.map_variant(c, vs)
    }

    pub(crate) fn glyph_h_advance(&self, glyph: GlyphId) -> i32 {
        self.glyph_metrics
            .advance_width(glyph, self.ot_tables.coords)
            .unwrap_or_default()
    }
    pub(crate) fn glyph_h_advances(&self, buffer: &mut hb_buffer_t) {
        self.glyph_metrics
            .populate_advance_widths(buffer, self.ot_tables.coords);
    }

    pub(crate) fn glyph_v_advance(&self, glyph: GlyphId) -> i32 {
        -self
            .glyph_metrics
            .advance_height(glyph, self.ot_tables.coords)
            .unwrap_or(self.units_per_em as i32)
    }

    pub(crate) fn glyph_h_origin(&self, glyph: GlyphId) -> i32 {
        self.glyph_h_advance(glyph) / 2
    }

    pub(crate) fn glyph_v_origin(&self, glyph: GlyphId) -> i32 {
        self.glyph_metrics
            .v_origin(glyph, self.ot_tables.coords)
            .unwrap_or_default()
    }

    pub(crate) fn glyph_extents(
        &self,
        glyph: GlyphId,
        glyph_extents: &mut hb_glyph_extents_t,
    ) -> bool {
        if let Some(extents) = self.glyph_metrics.extents(glyph, self.ot_tables.coords) {
            glyph_extents.x_bearing = extents.x_min;
            glyph_extents.y_bearing = extents.y_max;
            glyph_extents.width = extents.x_max - extents.x_min;
            glyph_extents.height = extents.y_min - extents.y_max;
            true
        } else {
            false
        }
    }

    pub(crate) fn glyph_names(&self) -> GlyphNames<'a> {
        GlyphNames::new(&self.font)
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

#[derive(Clone, Copy, Default, bytemuck::Pod, bytemuck::Zeroable)]
#[repr(C)]
pub struct hb_glyph_extents_t {
    pub x_bearing: i32,
    pub y_bearing: i32,
    pub width: i32,
    pub height: i32,
}
