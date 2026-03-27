//! Support for rendering variable composite glyphs from the VARC table.

use read_fonts::{
    tables::{
        layout::Condition,
        varc::{
            DecomposedTransform, MultiItemVariationStore, SparseVariationRegionList, Varc,
            VarcComponent, VarcFlags,
        },
        variations::NO_VARIATION_INDEX,
    },
    types::{F2Dot14, GlyphId},
    FontRef, ReadError, TableProvider,
};

use crate::{
    collections::SmallVec,
    instance::Size,
    outline::{cff, glyf, metrics::GlyphHMetrics, pen::PathStyle, DrawError, OutlinePen},
    provider::MetadataProvider,
    GLYF_COMPOSITE_RECURSION_LIMIT,
};

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::CoreFloat;

use super::OutlineKind;

type GlyphStack = SmallVec<GlyphId, 8>;
type CoordVec = SmallVec<F2Dot14, 64>;
type AxisIndexVec = SmallVec<u16, 64>;
type AxisValueVec = SmallVec<f32, 64>;
type DeltaVec = SmallVec<f32, 64>;
type ScalarCacheVec = SmallVec<f32, 128>;
type Affine = [f32; 6];

struct Scratchpad {
    deltas: DeltaVec,
    axis_indices: AxisIndexVec,
    axis_values: AxisValueVec,
}

impl Scratchpad {
    fn new() -> Self {
        Self {
            deltas: DeltaVec::new(),
            axis_indices: AxisIndexVec::new(),
            axis_values: AxisValueVec::new(),
        }
    }
}

struct VarcSharedContext<'a, 'b> {
    font_coords: &'b [F2Dot14],
    size: Size,
    path_style: PathStyle,
    coverage: &'b read_fonts::tables::layout::CoverageTable<'a>,
    var_store: Option<&'b MultiItemVariationStore<'a>>,
    store_regions: Option<(
        &'b MultiItemVariationStore<'a>,
        &'b SparseVariationRegionList<'a>,
    )>,
}

#[derive(Clone)]
enum BaseOutlines<'a> {
    Glyf(glyf::Outlines<'a>),
    Cff(cff::Outlines<'a>),
}

impl<'a> BaseOutlines<'a> {
    fn glyph_count(&self) -> u32 {
        match self {
            Self::Glyf(glyf) => glyf.glyph_count() as u32,
            Self::Cff(cff) => cff.glyph_count() as u32,
        }
    }

    fn font(&self) -> &FontRef<'a> {
        match self {
            Self::Glyf(glyf) => &glyf.font,
            Self::Cff(cff) => &cff.font,
        }
    }

    fn base_outline_kind(&self, glyph_id: GlyphId) -> Option<OutlineKind<'a>> {
        match self {
            Self::Glyf(glyf) => Some(OutlineKind::Glyf(
                glyf.clone(),
                glyf.outline(glyph_id).ok()?,
            )),
            Self::Cff(cff) => Some(OutlineKind::Cff(
                cff.clone(),
                glyph_id,
                cff.subfont_index(glyph_id),
            )),
        }
    }

    fn base_outline_memory(&self, glyph_id: GlyphId) -> usize {
        match self {
            Self::Glyf(glyf) => glyf
                .outline(glyph_id)
                .ok()
                .map(|outline| outline.required_buffer_size(super::Hinting::None))
                .unwrap_or(0),
            Self::Cff(..) => 0,
        }
    }
}

#[derive(Clone)]
pub(crate) struct Outlines<'a> {
    varc: Varc<'a>,
    coverage: read_fonts::tables::layout::CoverageTable<'a>,
    var_store: Option<MultiItemVariationStore<'a>>,
    regions: Option<SparseVariationRegionList<'a>>,
    base: BaseOutlines<'a>,
    glyph_metrics: GlyphHMetrics<'a>,
    units_per_em: u16,
    axis_count: usize,
}

#[derive(Clone, Copy)]
pub(crate) struct Outline {
    pub(crate) glyph_id: GlyphId,
    pub(crate) coverage_index: u16,
    max_component_memory: usize,
}

impl Outline {
    pub fn required_buffer_size(&self) -> usize {
        self.max_component_memory
    }
}

impl<'a> Outlines<'a> {
    pub fn new(font: &FontRef<'a>) -> Option<Self> {
        let varc = font.varc().ok()?;
        if let Some(glyf) = glyf::Outlines::new(font) {
            return Self::from_base(font, varc, BaseOutlines::Glyf(glyf));
        }
        if let Some(cff) = cff::Outlines::new(font) {
            return Self::from_base(font, varc, BaseOutlines::Cff(cff));
        }
        None
    }

    fn from_base(font: &FontRef<'a>, varc: Varc<'a>, base: BaseOutlines<'a>) -> Option<Self> {
        let glyph_metrics = GlyphHMetrics::new(font)?;
        let units_per_em = font.head().ok()?.units_per_em();
        let axis_count = font.axes().len();
        let coverage = varc.coverage().ok()?;
        let var_store = varc.multi_var_store().transpose().ok()?;
        let regions = var_store
            .as_ref()
            .map(|s| s.region_list())
            .transpose()
            .ok()?;
        Some(Self {
            varc,
            coverage,
            var_store,
            regions,
            base,
            glyph_metrics,
            units_per_em,
            axis_count,
        })
    }

    pub fn units_per_em(&self) -> u16 {
        self.units_per_em
    }

    pub fn glyph_count(&self) -> u32 {
        self.base.glyph_count()
    }

    pub fn prefer_interpreter(&self) -> bool {
        false
    }

    pub fn fractional_size_hinting(&self) -> bool {
        false
    }

    pub fn font(&self) -> &FontRef<'a> {
        self.base.font()
    }

    pub(crate) fn fallback_outline_kind(&self, glyph_id: GlyphId) -> Option<OutlineKind<'a>> {
        self.base.base_outline_kind(glyph_id)
    }

    pub fn outline(&self, glyph_id: GlyphId) -> Result<Option<Outline>, ReadError> {
        let Some(coverage_index) = self.coverage.get(glyph_id) else {
            return Ok(None);
        };
        let max_component_memory = self.compute_max_component_memory(glyph_id, coverage_index)?;
        Ok(Some(Outline {
            glyph_id,
            coverage_index,
            max_component_memory,
        }))
    }

    /// Lightweight coverage lookup without computing max_component_memory.
    fn coverage_index(&self, glyph_id: GlyphId) -> Result<Option<u16>, ReadError> {
        Ok(self.coverage.get(glyph_id))
    }

    fn compute_max_component_memory(
        &self,
        glyph_id: GlyphId,
        coverage_index: u16,
    ) -> Result<usize, ReadError> {
        let mut stack = GlyphStack::new();
        self.max_component_memory_for_glyph(glyph_id, coverage_index, &mut stack)
    }

    fn max_component_memory_for_glyph(
        &self,
        glyph_id: GlyphId,
        coverage_index: u16,
        stack: &mut GlyphStack,
    ) -> Result<usize, ReadError> {
        if stack.contains(&glyph_id) {
            return Ok(0);
        }
        if stack.len() >= GLYF_COMPOSITE_RECURSION_LIMIT {
            return Ok(0);
        }
        stack.push(glyph_id);
        let mut max_memory = 0usize;
        let glyph = self.varc.glyph(coverage_index as usize)?;
        for component in glyph.components() {
            let component = component?;
            let component_gid = component.gid();
            let component_memory = if component_gid == glyph_id {
                self.base.base_outline_memory(component_gid)
            } else if let Some(coverage_index) = self.coverage_index(component_gid)? {
                self.max_component_memory_for_glyph(component_gid, coverage_index, stack)?
            } else {
                self.base.base_outline_memory(component_gid)
            };
            max_memory = max_memory.max(component_memory);
        }
        stack.pop();
        Ok(max_memory)
    }

    pub fn draw(
        &self,
        outline: &Outline,
        buf: &mut [u8],
        size: Size,
        coords: &[F2Dot14],
        path_style: PathStyle,
        pen: &mut impl OutlinePen,
    ) -> Result<(), DrawError> {
        let mut font_coords = CoordVec::new();
        expand_coords(&mut font_coords, self.axis_count, coords);
        let mut stack = GlyphStack::new();
        let pen: &mut dyn OutlinePen = pen;
        let mut scalar_cache = self
            .scalar_cache_from_store(self.var_store.as_ref())?
            .unwrap();
        let mut scratch = Scratchpad::new();
        let ctx = VarcSharedContext {
            font_coords: &font_coords,
            size,
            path_style,
            coverage: &self.coverage,
            var_store: self.var_store.as_ref(),
            store_regions: self.var_store.as_ref().zip(self.regions.as_ref()),
        };
        self.draw_glyph(
            outline.glyph_id,
            outline.coverage_index,
            &font_coords,
            IDENTITY_MATRIX,
            &ctx,
            buf,
            pen,
            &mut stack,
            &mut scalar_cache,
            &mut scratch,
        )
    }

    pub fn draw_unscaled(
        &self,
        outline: &Outline,
        buf: &mut [u8],
        coords: &[F2Dot14],
        pen: &mut impl OutlinePen,
    ) -> Result<i32, DrawError> {
        let size = Size::unscaled();
        self.draw(outline, buf, size, coords, PathStyle::default(), pen)?;
        Ok(self.glyph_metrics.advance_width(outline.glyph_id, coords))
    }

    #[allow(clippy::too_many_arguments)]
    fn draw_glyph(
        &self,
        glyph_id: GlyphId,
        coverage_index: u16,
        current_coords: &[F2Dot14],
        parent_matrix: Affine,
        ctx: &VarcSharedContext<'a, '_>,
        buf: &mut [u8],
        pen: &mut dyn OutlinePen,
        stack: &mut GlyphStack,
        scalar_cache: &mut ScalarCache,
        scratch: &mut Scratchpad,
    ) -> Result<(), DrawError> {
        if stack.len() >= GLYF_COMPOSITE_RECURSION_LIMIT {
            return Err(DrawError::RecursionLimitExceeded(glyph_id));
        }
        let glyph = self.varc.glyph(coverage_index as usize)?;
        stack.push(glyph_id);
        let coverage = ctx.coverage;
        let store_regions = ctx.store_regions;
        let mut component_coords_buffer = CoordVec::new();
        let mut child_scalar_cache: Option<ScalarCache> = None;
        for component in glyph.components() {
            let component = component?;
            if !self.component_condition_met(
                &component,
                current_coords,
                scalar_cache,
                scratch,
                store_regions,
            )? {
                continue;
            }
            let component_gid = component.gid();
            let flags = component.flags();

            let coords_the_same = !flags.contains(VarcFlags::HAVE_AXES)
                && !flags.contains(VarcFlags::RESET_UNSPECIFIED_AXES);

            let component_coords = if coords_the_same {
                current_coords
            } else {
                self.component_coords(
                    &component,
                    current_coords,
                    &mut component_coords_buffer,
                    scalar_cache,
                    scratch,
                    ctx.font_coords,
                    store_regions,
                )?;
                component_coords_buffer.as_slice()
            };

            let mut transform = *component.transform();
            self.apply_transform_variations(
                &component,
                current_coords,
                &mut transform,
                scalar_cache,
                scratch,
                store_regions,
            )?;
            let scale = ctx.size.linear_scale(self.units_per_em);
            let matrix = mul_matrix(parent_matrix, scale_matrix(transform.matrix(), scale));
            if component_gid != glyph_id {
                if let Some(coverage_index) = coverage.get(component_gid) {
                    if !stack.contains(&component_gid) {
                        // Optimization: if coordinates haven't changed, we can reuse the scalar cache.
                        if coords_the_same {
                            self.draw_glyph(
                                component_gid,
                                coverage_index,
                                current_coords,
                                matrix,
                                ctx,
                                buf,
                                pen,
                                stack,
                                scalar_cache,
                                scratch,
                            )?;
                        } else {
                            if let Some(ref mut cache) = child_scalar_cache {
                                cache.values.fill(ScalarCache::INVALID);
                            } else {
                                child_scalar_cache = self.scalar_cache_from_store(ctx.var_store)?;
                            }
                            self.draw_glyph(
                                component_gid,
                                coverage_index,
                                component_coords,
                                matrix,
                                ctx,
                                buf,
                                pen,
                                stack,
                                child_scalar_cache.as_mut().unwrap(),
                                scratch,
                            )?;
                        }
                        continue;
                    }
                }
            }
            let mut transform_pen = TransformPen::new(pen, matrix);
            self.draw_base_glyph(
                component_gid,
                component_coords,
                ctx.size,
                ctx.path_style,
                buf,
                &mut transform_pen,
            )?;
        }
        stack.pop();
        Ok(())
    }

    fn draw_base_glyph(
        &self,
        glyph_id: GlyphId,
        coords: &[F2Dot14],
        size: Size,
        path_style: PathStyle,
        buf: &mut [u8],
        pen: &mut impl OutlinePen,
    ) -> Result<(), DrawError> {
        let Some(kind) = self.base.base_outline_kind(glyph_id) else {
            return Err(DrawError::GlyphNotFound(glyph_id));
        };
        let settings =
            crate::outline::DrawSettings::unhinted(size, crate::instance::LocationRef::new(coords))
                .with_path_style(path_style)
                .with_memory(Some(buf));
        crate::outline::OutlineGlyph { kind }.draw(settings, pen)?;
        Ok(())
    }

    #[allow(clippy::too_many_arguments)]
    fn component_coords(
        &self,
        component: &VarcComponent<'_>,
        current_coords: &[F2Dot14],
        coords: &mut CoordVec,
        scalar_cache: &mut ScalarCache,
        scratch: &mut Scratchpad,
        font_coords: &[F2Dot14],
        store_regions: Option<(&MultiItemVariationStore<'a>, &SparseVariationRegionList<'a>)>,
    ) -> Result<(), DrawError> {
        let flags = component.flags();
        if flags.contains(VarcFlags::RESET_UNSPECIFIED_AXES) {
            expand_coords(coords, font_coords.len(), font_coords);
        } else {
            expand_coords(coords, current_coords.len(), current_coords);
        }

        if !flags.contains(VarcFlags::HAVE_AXES) {
            return Ok(());
        }

        let axis_indices_index = component
            .axis_indices_index()
            .ok_or(ReadError::MalformedData("Missing axisIndicesIndex"))?;
        let num_axes = self.axis_indices(axis_indices_index as usize, &mut scratch.axis_indices)?;

        self.axis_values(component, num_axes, &mut scratch.axis_values)?;
        if let Some(var_idx) = component.axis_values_var_index() {
            let (store, regions) = store_regions.ok_or(ReadError::NullOffset)?;
            compute_tuple_deltas(
                store,
                regions,
                var_idx,
                current_coords,
                scratch.axis_indices.len(),
                scalar_cache,
                &mut scratch.deltas,
            )?;
            for (value, delta) in scratch.axis_values.iter_mut().zip(scratch.deltas.iter()) {
                *value += *delta;
            }
        }

        for (axis_index, value) in scratch
            .axis_indices
            .iter()
            .zip(scratch.axis_values.iter().copied())
        {
            let Some(slot) = coords.get_mut(*axis_index as usize) else {
                return Err(DrawError::Read(ReadError::OutOfBounds));
            };
            let raw = value.round().clamp(i16::MIN as f32, i16::MAX as f32) as i16;
            *slot = F2Dot14::from_bits(raw);
        }
        Ok(())
    }

    fn axis_indices(&self, nth: usize, out: &mut AxisIndexVec) -> Result<usize, DrawError> {
        let packed = self.varc.axis_indices(nth)?;
        out.clear();
        for value in packed.iter() {
            out.push(value as u16);
        }
        Ok(out.len())
    }

    fn axis_values(
        &self,
        component: &VarcComponent<'_>,
        count: usize,
        out: &mut AxisValueVec,
    ) -> Result<(), DrawError> {
        let Some(packed) = component.axis_values() else {
            out.clear();
            return Ok(());
        };
        out.resize_and_fill(count, 0.0);
        for (slot, value) in out.iter_mut().zip(packed.iter().by_ref().take(count)) {
            *slot = value as f32;
        }
        Ok(())
    }

    fn apply_transform_variations(
        &self,
        component: &VarcComponent<'_>,
        coords: &[F2Dot14],
        transform: &mut DecomposedTransform,
        scalar_cache: &mut ScalarCache,
        scratch: &mut Scratchpad,
        store_regions: Option<(&MultiItemVariationStore<'a>, &SparseVariationRegionList<'a>)>,
    ) -> Result<(), DrawError> {
        let Some(var_idx) = component.transform_var_index() else {
            return Ok(());
        };

        let flags = component.flags();

        // Count transform fields using a mask + count_ones
        const TRANSFORM_MASK: VarcFlags = VarcFlags::from_bits_truncate(
            VarcFlags::HAVE_TRANSLATE_X.bits()
                | VarcFlags::HAVE_TRANSLATE_Y.bits()
                | VarcFlags::HAVE_ROTATION.bits()
                | VarcFlags::HAVE_SCALE_X.bits()
                | VarcFlags::HAVE_SCALE_Y.bits()
                | VarcFlags::HAVE_SKEW_X.bits()
                | VarcFlags::HAVE_SKEW_Y.bits()
                | VarcFlags::HAVE_TCENTER_X.bits()
                | VarcFlags::HAVE_TCENTER_Y.bits(),
        );
        let field_count = (flags.bits() & TRANSFORM_MASK.bits()).count_ones() as usize;
        if field_count == 0 {
            return Ok(());
        }

        let (store, regions) = store_regions.ok_or(ReadError::NullOffset)?;
        compute_tuple_deltas(
            store,
            regions,
            var_idx,
            coords,
            field_count,
            scalar_cache,
            &mut scratch.deltas,
        )?;

        // Apply deltas in flag order, consuming from iterator
        let mut delta_iter = scratch.deltas.iter().copied();

        if flags.contains(VarcFlags::HAVE_TRANSLATE_X) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_translate_x(transform.translate_x() + delta);
        }
        if flags.contains(VarcFlags::HAVE_TRANSLATE_Y) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_translate_y(transform.translate_y() + delta);
        }
        if flags.contains(VarcFlags::HAVE_ROTATION) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_rotation(transform.rotation() + delta / 4096.0);
        }
        if flags.contains(VarcFlags::HAVE_SCALE_X) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_scale_x(transform.scale_x() + delta / 1024.0);
        }
        if flags.contains(VarcFlags::HAVE_SCALE_Y) {
            let delta = delta_iter.next().unwrap_or(0.0);
            transform.set_scale_y(transform.scale_y() + delta / 1024.0);
        }
        const SKEW_OR_CENTER: VarcFlags = VarcFlags::from_bits_truncate(
            VarcFlags::HAVE_SKEW_X.bits()
                | VarcFlags::HAVE_SKEW_Y.bits()
                | VarcFlags::HAVE_TCENTER_X.bits()
                | VarcFlags::HAVE_TCENTER_Y.bits(),
        );
        if flags.intersects(SKEW_OR_CENTER) {
            if flags.contains(VarcFlags::HAVE_SKEW_X) {
                let delta = delta_iter.next().unwrap_or(0.0);
                transform.set_skew_x(transform.skew_x() + delta / 4096.0);
            }
            if flags.contains(VarcFlags::HAVE_SKEW_Y) {
                let delta = delta_iter.next().unwrap_or(0.0);
                transform.set_skew_y(transform.skew_y() + delta / 4096.0);
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_X) {
                let delta = delta_iter.next().unwrap_or(0.0);
                transform.set_center_x(transform.center_x() + delta);
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_Y) {
                let delta = delta_iter.next().unwrap_or(0.0);
                transform.set_center_y(transform.center_y() + delta);
            }
        }

        if !flags.contains(VarcFlags::HAVE_SCALE_Y) {
            transform.set_scale_y(transform.scale_x());
        }
        Ok(())
    }

    fn component_condition_met(
        &self,
        component: &VarcComponent<'_>,
        coords: &[F2Dot14],
        scalar_cache: &mut ScalarCache,
        scratch: &mut Scratchpad,
        store_regions: Option<(&MultiItemVariationStore<'a>, &SparseVariationRegionList<'a>)>,
    ) -> Result<bool, DrawError> {
        let Some(condition_index) = component.condition_index() else {
            return Ok(true);
        };
        let Some(condition_list) = self.varc.condition_list() else {
            return Err(DrawError::Read(ReadError::NullOffset));
        };
        let condition_list = condition_list?;
        let condition = condition_list.conditions().get(condition_index as usize)?;
        let (store, regions) = store_regions.ok_or(ReadError::NullOffset)?;
        Self::eval_condition(&condition, coords, store, regions, scalar_cache, scratch)
    }

    fn eval_condition(
        condition: &Condition<'a>,
        coords: &[F2Dot14],
        var_store: &MultiItemVariationStore<'a>,
        regions: &SparseVariationRegionList<'a>,
        scalar_cache: &mut ScalarCache,
        scratch: &mut Scratchpad,
    ) -> Result<bool, DrawError> {
        match condition {
            Condition::Format1AxisRange(condition) => {
                let axis_index = condition.axis_index() as usize;
                let coord = coords.get(axis_index).copied().unwrap_or(F2Dot14::ZERO);
                Ok(coord >= condition.filter_range_min_value()
                    && coord <= condition.filter_range_max_value())
            }
            Condition::Format2VariableValue(condition) => {
                let default_value = condition.default_value() as f32;
                let var_idx = condition.var_index();
                compute_tuple_deltas(
                    var_store,
                    regions,
                    var_idx,
                    coords,
                    1,
                    scalar_cache,
                    &mut scratch.deltas,
                )?;
                let delta = scratch.deltas.first().copied().unwrap_or(0.0);
                Ok(default_value + delta > 0.0)
            }
            Condition::Format3And(condition) => {
                for nested in condition.conditions().iter() {
                    let nested = nested?;
                    if !Self::eval_condition(
                        &nested,
                        coords,
                        var_store,
                        regions,
                        scalar_cache,
                        scratch,
                    )? {
                        return Ok(false);
                    }
                }
                Ok(true)
            }
            Condition::Format4Or(condition) => {
                for nested in condition.conditions().iter() {
                    let nested = nested?;
                    if Self::eval_condition(
                        &nested,
                        coords,
                        var_store,
                        regions,
                        scalar_cache,
                        scratch,
                    )? {
                        return Ok(true);
                    }
                }
                Ok(false)
            }
            Condition::Format5Negate(condition) => {
                let nested = condition.condition()?;
                Ok(!Self::eval_condition(
                    &nested,
                    coords,
                    var_store,
                    regions,
                    scalar_cache,
                    scratch,
                )?)
            }
        }
    }

    fn scalar_cache_from_store(
        &self,
        store: Option<&MultiItemVariationStore<'a>>,
    ) -> Result<Option<ScalarCache>, DrawError> {
        let Some(store) = store else {
            return Ok(None);
        };
        let region_count = store.region_list()?.region_count() as usize;
        Ok(Some(ScalarCache::new(region_count)))
    }
}

struct ScalarCache {
    values: ScalarCacheVec,
}

impl ScalarCache {
    const INVALID: f32 = 2.0; // Scalars are in [0,1], so 2.0 means "not cached"

    fn new(count: usize) -> Self {
        Self {
            values: ScalarCacheVec::with_len(count, Self::INVALID),
        }
    }

    fn get(&self, index: usize) -> f32 {
        self.values.get(index).copied().unwrap_or(Self::INVALID)
    }

    fn set(&mut self, index: usize, value: f32) {
        if let Some(slot) = self.values.get_mut(index) {
            *slot = value;
        }
    }
}

fn expand_coords(out: &mut CoordVec, axis_count: usize, coords: &[F2Dot14]) {
    out.resize_and_fill(axis_count, F2Dot14::ZERO);
    for (slot, value) in out.iter_mut().zip(coords.iter().copied()) {
        *slot = value;
    }
}

fn compute_tuple_deltas(
    store: &MultiItemVariationStore,
    regions: &SparseVariationRegionList,
    var_idx: u32,
    coords: &[F2Dot14],
    tuple_len: usize,
    cache: &mut ScalarCache,
    out: &mut DeltaVec,
) -> Result<(), ReadError> {
    out.resize_and_fill(tuple_len, 0.0);
    if tuple_len == 0 || var_idx == NO_VARIATION_INDEX {
        return Ok(());
    }
    let outer = (var_idx >> 16) as usize;
    let inner = (var_idx & 0xFFFF) as usize;
    let data = store
        .variation_data()
        .get(outer)
        .map_err(|_| ReadError::InvalidCollectionIndex(outer as _))?;
    let region_indices = data.region_indices();
    let mut deltas = data.delta_set(inner)?.fetcher();
    let regions = regions.regions();
    let out_slice = out.as_mut_slice();

    let mut skip = 0;
    for region_index in region_indices.iter() {
        let region_idx = region_index.get() as usize;
        let mut scalar = cache.get(region_idx);
        if scalar >= 2.0 {
            scalar = regions.get(region_idx)?.compute_scalar_f32(coords);
            cache.set(region_idx, scalar);
        }
        // We skip lazily. Reduces work at the tail end.
        if scalar == 0.0 {
            skip += out_slice.len();
        } else {
            if skip != 0 {
                deltas.skip(skip)?;
                skip = 0;
            }
            deltas.add_to_f32_scaled(out_slice, scalar)?;
        }
    }

    Ok(())
}

#[inline(always)]
fn scale_matrix(m: Affine, s: f32) -> Affine {
    [m[0], m[1], m[2], m[3], m[4] * s, m[5] * s]
}

const IDENTITY_MATRIX: Affine = [1.0, 0.0, 0.0, 1.0, 0.0, 0.0];

#[inline(always)]
fn mul_matrix(a: Affine, b: Affine) -> Affine {
    [
        a[0] * b[0] + a[2] * b[1],
        a[1] * b[0] + a[3] * b[1],
        a[0] * b[2] + a[2] * b[3],
        a[1] * b[2] + a[3] * b[3],
        a[0] * b[4] + a[2] * b[5] + a[4],
        a[1] * b[4] + a[3] * b[5] + a[5],
    ]
}

struct TransformPen<'a, P: OutlinePen + ?Sized> {
    pen: &'a mut P,
    matrix: Affine,
}

impl<'a, P: OutlinePen + ?Sized> TransformPen<'a, P> {
    fn new(pen: &'a mut P, matrix: Affine) -> Self {
        Self { pen, matrix }
    }

    #[inline(always)]
    fn transform(&self, x: f32, y: f32) -> (f32, f32) {
        let [a, b, c, d, e, f] = self.matrix;
        (a * x + c * y + e, b * x + d * y + f)
    }
}

impl<P: OutlinePen + ?Sized> OutlinePen for TransformPen<'_, P> {
    fn move_to(&mut self, x: f32, y: f32) {
        let (x, y) = self.transform(x, y);
        self.pen.move_to(x, y);
    }

    fn line_to(&mut self, x: f32, y: f32) {
        let (x, y) = self.transform(x, y);
        self.pen.line_to(x, y);
    }

    fn quad_to(&mut self, cx0: f32, cy0: f32, x: f32, y: f32) {
        let (cx0, cy0) = self.transform(cx0, cy0);
        let (x, y) = self.transform(x, y);
        self.pen.quad_to(cx0, cy0, x, y);
    }

    fn curve_to(&mut self, cx0: f32, cy0: f32, cx1: f32, cy1: f32, x: f32, y: f32) {
        let (cx0, cy0) = self.transform(cx0, cy0);
        let (cx1, cy1) = self.transform(cx1, cy1);
        let (x, y) = self.transform(x, y);
        self.pen.curve_to(cx0, cy0, cx1, cy1, x, y);
    }

    fn close(&mut self) {
        self.pen.close();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::outline::pen::PathElement;
    use read_fonts::{FontRef, TableProvider};

    fn coord(value: f32) -> F2Dot14 {
        F2Dot14::from_f32(value)
    }

    fn assert_close(actual: f32, expected: f32) {
        let diff = (actual - expected).abs();
        assert!(
            diff <= 1e-6,
            "expected {expected}, got {actual}, diff {diff}"
        );
    }

    fn path_head_signature(path: &[PathElement], n: usize) -> Vec<String> {
        path.iter()
            .take(n)
            .map(|el| match *el {
                PathElement::MoveTo { x, y } => format!("M{:.2},{:.2}", x, y),
                PathElement::LineTo { x, y } => format!("L{:.2},{:.2}", x, y),
                PathElement::QuadTo { cx0, cy0, x, y } => {
                    format!("Q{:.2},{:.2} {:.2},{:.2}", cx0, cy0, x, y)
                }
                PathElement::CurveTo {
                    cx0,
                    cy0,
                    cx1,
                    cy1,
                    x,
                    y,
                } => format!(
                    "C{:.2},{:.2} {:.2},{:.2} {:.2},{:.2}",
                    cx0, cy0, cx1, cy1, x, y
                ),
                PathElement::Close => "Z".to_string(),
            })
            .collect()
    }

    #[test]
    fn expand_coords_pads_and_truncates() {
        let mut out = CoordVec::new();
        expand_coords(&mut out, 4, &[coord(0.25), coord(-0.5)]);
        assert_eq!(
            out.as_slice(),
            &[coord(0.25), coord(-0.5), F2Dot14::ZERO, F2Dot14::ZERO]
        );

        expand_coords(&mut out, 1, &[coord(0.25), coord(-0.5)]);
        assert_eq!(out.as_slice(), &[coord(0.25)]);
    }

    #[test]
    fn scale_matrix_only_scales_translation() {
        let matrix = [1.0, 2.0, 3.0, 4.0, 5.0, -6.0];
        assert_eq!(
            scale_matrix(matrix, 10.0),
            [1.0, 2.0, 3.0, 4.0, 50.0, -60.0]
        );
    }

    #[test]
    fn mul_matrix_identity_and_known_product() {
        let a = [0.5, 1.0, -2.0, 0.25, 7.0, -3.0];
        assert_eq!(mul_matrix(IDENTITY_MATRIX, a), a);
        assert_eq!(mul_matrix(a, IDENTITY_MATRIX), a);

        let translate = [1.0, 0.0, 0.0, 1.0, 10.0, 20.0];
        let scale = [2.0, 0.0, 0.0, 3.0, 0.0, 0.0];
        assert_eq!(
            mul_matrix(translate, scale),
            [2.0, 0.0, 0.0, 3.0, 10.0, 20.0]
        );
        assert_eq!(
            mul_matrix(scale, translate),
            [2.0, 0.0, 0.0, 3.0, 20.0, 60.0]
        );
    }

    #[test]
    fn compute_tuple_deltas_no_variation_index_is_noop_after_resize() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let mut cache = ScalarCache::new(regions.region_count() as usize);
        let mut out = DeltaVec::new();
        out.push(42.0);

        compute_tuple_deltas(
            &store,
            &regions,
            NO_VARIATION_INDEX,
            &[coord(0.0)],
            3,
            &mut cache,
            &mut out,
        )
        .unwrap();
        assert_eq!(out.as_slice(), &[0.0, 0.0, 0.0]);

        compute_tuple_deltas(
            &store,
            &regions,
            NO_VARIATION_INDEX,
            &[coord(0.0)],
            0,
            &mut cache,
            &mut out,
        )
        .unwrap();
        assert!(out.is_empty());
    }

    #[test]
    fn compute_tuple_deltas_invalid_outer_index_errors() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let mut cache = ScalarCache::new(regions.region_count() as usize);
        let mut out = DeltaVec::new();

        let err = compute_tuple_deltas(&store, &regions, 0xFFFF_0000, &[], 1, &mut cache, &mut out)
            .unwrap_err();
        assert!(matches!(err, ReadError::InvalidCollectionIndex(_)));
    }

    #[test]
    fn compute_tuple_deltas_matches_manual_decode() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let region_list = regions.regions();
        let coords = [coord(0.5); 8];

        let mut tried = 0usize;
        for (outer, data) in store.variation_data().iter().enumerate() {
            let data = data.unwrap();
            let region_count = data.region_indices().len();
            if region_count == 0 {
                continue;
            }
            let delta_set_count = data.delta_sets().unwrap().count() as usize;
            for inner in 0..delta_set_count.min(3) {
                let decoded = data.delta_set(inner).unwrap().iter().collect::<Vec<_>>();
                if decoded.is_empty() || decoded.len() % region_count != 0 {
                    continue;
                }
                let tuple_len = decoded.len() / region_count;
                let var_idx = ((outer as u32) << 16) | inner as u32;

                let mut cache = ScalarCache::new(regions.region_count() as usize);
                let mut actual = DeltaVec::new();
                compute_tuple_deltas(
                    &store,
                    &regions,
                    var_idx,
                    &coords,
                    tuple_len,
                    &mut cache,
                    &mut actual,
                )
                .unwrap();
                let first = actual.as_slice().to_vec();

                // Same cache after population should not alter results.
                compute_tuple_deltas(
                    &store,
                    &regions,
                    var_idx,
                    &coords,
                    tuple_len,
                    &mut cache,
                    &mut actual,
                )
                .unwrap();
                assert_eq!(actual.as_slice(), first.as_slice());

                let mut expected = vec![0.0f32; tuple_len];
                for (region_order, region_idx) in data.region_indices().iter().enumerate() {
                    let scalar = region_list
                        .get(region_idx.get() as usize)
                        .unwrap()
                        .compute_scalar_f32(&coords);
                    if scalar == 0.0 {
                        continue;
                    }
                    let base = region_order * tuple_len;
                    for (i, slot) in expected.iter_mut().enumerate() {
                        *slot += decoded[base + i] as f32 * scalar;
                    }
                }
                assert_eq!(actual.len(), expected.len());
                for (a, e) in actual.iter().zip(expected.iter()) {
                    assert_close(*a, *e);
                }
                tried += 1;
            }
        }
        assert!(tried > 0, "expected at least one tuple to be exercised");
    }

    fn apply_transform_variations_reference(
        component: &VarcComponent<'_>,
        coords: &[F2Dot14],
        transform: &mut DecomposedTransform,
        var_store: Option<&MultiItemVariationStore<'_>>,
        regions: Option<&SparseVariationRegionList<'_>>,
        scalar_cache: &mut ScalarCache,
        deltas: &mut DeltaVec,
    ) -> Result<(), DrawError> {
        let Some(var_idx) = component.transform_var_index() else {
            return Ok(());
        };
        let flags = component.flags();
        const TRANSFORM_MASK: VarcFlags = VarcFlags::from_bits_truncate(
            VarcFlags::HAVE_TRANSLATE_X.bits()
                | VarcFlags::HAVE_TRANSLATE_Y.bits()
                | VarcFlags::HAVE_ROTATION.bits()
                | VarcFlags::HAVE_SCALE_X.bits()
                | VarcFlags::HAVE_SCALE_Y.bits()
                | VarcFlags::HAVE_SKEW_X.bits()
                | VarcFlags::HAVE_SKEW_Y.bits()
                | VarcFlags::HAVE_TCENTER_X.bits()
                | VarcFlags::HAVE_TCENTER_Y.bits(),
        );
        let field_count = (flags.bits() & TRANSFORM_MASK.bits()).count_ones() as usize;
        if field_count == 0 {
            return Ok(());
        }

        let store = var_store.ok_or(ReadError::NullOffset)?;
        let regions = regions.ok_or(ReadError::NullOffset)?;
        compute_tuple_deltas(
            store,
            regions,
            var_idx,
            coords,
            field_count,
            scalar_cache,
            deltas,
        )?;

        let mut delta_iter = deltas.iter().copied();
        if flags.contains(VarcFlags::HAVE_TRANSLATE_X) {
            transform.set_translate_x(transform.translate_x() + delta_iter.next().unwrap_or(0.0));
        }
        if flags.contains(VarcFlags::HAVE_TRANSLATE_Y) {
            transform.set_translate_y(transform.translate_y() + delta_iter.next().unwrap_or(0.0));
        }
        if flags.contains(VarcFlags::HAVE_ROTATION) {
            transform
                .set_rotation(transform.rotation() + delta_iter.next().unwrap_or(0.0) / 4096.0);
        }
        if flags.contains(VarcFlags::HAVE_SCALE_X) {
            transform.set_scale_x(transform.scale_x() + delta_iter.next().unwrap_or(0.0) / 1024.0);
        }
        if flags.contains(VarcFlags::HAVE_SCALE_Y) {
            transform.set_scale_y(transform.scale_y() + delta_iter.next().unwrap_or(0.0) / 1024.0);
        }
        const SKEW_OR_CENTER: VarcFlags = VarcFlags::from_bits_truncate(
            VarcFlags::HAVE_SKEW_X.bits()
                | VarcFlags::HAVE_SKEW_Y.bits()
                | VarcFlags::HAVE_TCENTER_X.bits()
                | VarcFlags::HAVE_TCENTER_Y.bits(),
        );
        if flags.intersects(SKEW_OR_CENTER) {
            if flags.contains(VarcFlags::HAVE_SKEW_X) {
                transform
                    .set_skew_x(transform.skew_x() + delta_iter.next().unwrap_or(0.0) / 4096.0);
            }
            if flags.contains(VarcFlags::HAVE_SKEW_Y) {
                transform
                    .set_skew_y(transform.skew_y() + delta_iter.next().unwrap_or(0.0) / 4096.0);
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_X) {
                transform.set_center_x(transform.center_x() + delta_iter.next().unwrap_or(0.0));
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_Y) {
                transform.set_center_y(transform.center_y() + delta_iter.next().unwrap_or(0.0));
            }
        }
        if !flags.contains(VarcFlags::HAVE_SCALE_Y) {
            transform.set_scale_y(transform.scale_x());
        }
        Ok(())
    }

    #[test]
    fn apply_transform_variations_matches_reference_path() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = Outlines::new(&font).unwrap();
        let coverage = outlines.varc.coverage().unwrap();
        let var_store = outlines.varc.multi_var_store().transpose().unwrap();
        let regions = var_store
            .as_ref()
            .map(|s| s.region_list())
            .transpose()
            .unwrap();
        let region_count = regions
            .as_ref()
            .map(|r| r.region_count() as usize)
            .unwrap_or(0);

        let mut coords = CoordVec::new();
        coords.resize_and_fill(outlines.axis_count, F2Dot14::ZERO);
        for (i, c) in coords.iter_mut().enumerate() {
            *c = match i % 4 {
                0 => coord(0.5),
                1 => coord(-0.5),
                2 => coord(0.25),
                _ => coord(-0.25),
            };
        }

        let mut tested = 0usize;
        for gid16 in coverage.iter() {
            let gid: GlyphId = gid16.into();
            let coverage_index = coverage.get(gid).unwrap() as usize;
            let glyph = outlines.varc.glyph(coverage_index).unwrap();
            for component in glyph.components() {
                let component = component.unwrap();
                if component.transform_var_index().is_none() {
                    continue;
                }

                let mut transform_new = *component.transform();
                let mut transform_ref = *component.transform();
                let mut cache_new = ScalarCache::new(region_count);
                let mut cache_ref = ScalarCache::new(region_count);
                let mut deltas_ref = DeltaVec::new();
                let mut scratch = Scratchpad::new();
                let store_regions = var_store.as_ref().zip(regions.as_ref());

                outlines
                    .apply_transform_variations(
                        &component,
                        &coords,
                        &mut transform_new,
                        &mut cache_new,
                        &mut scratch,
                        store_regions,
                    )
                    .unwrap();
                apply_transform_variations_reference(
                    &component,
                    &coords,
                    &mut transform_ref,
                    var_store.as_ref(),
                    regions.as_ref(),
                    &mut cache_ref,
                    &mut deltas_ref,
                )
                .unwrap();

                assert_close(transform_new.translate_x(), transform_ref.translate_x());
                assert_close(transform_new.translate_y(), transform_ref.translate_y());
                assert_close(transform_new.rotation(), transform_ref.rotation());
                assert_close(transform_new.scale_x(), transform_ref.scale_x());
                assert_close(transform_new.scale_y(), transform_ref.scale_y());
                assert_close(transform_new.skew_x(), transform_ref.skew_x());
                assert_close(transform_new.skew_y(), transform_ref.skew_y());
                assert_close(transform_new.center_x(), transform_ref.center_x());
                assert_close(transform_new.center_y(), transform_ref.center_y());
                tested += 1;
                if tested >= 32 {
                    break;
                }
            }
            if tested >= 32 {
                break;
            }
        }
        assert!(tested > 0, "expected at least one transformed component");
    }

    #[test]
    fn draw_varc_6868_freetype_path_head_snapshot() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let outlines = Outlines::new(&font).unwrap();
        let gid = font.cmap().unwrap().map_codepoint(0x6868_u32).unwrap();
        let outline = outlines.outline(gid).unwrap().unwrap();
        let mut memory = vec![0u8; outline.required_buffer_size()];
        let mut pen = Vec::<PathElement>::new();
        outlines
            .draw(
                &outline,
                &mut memory,
                Size::unscaled(),
                &[],
                PathStyle::FreeType,
                &mut pen,
            )
            .unwrap();

        let head = path_head_signature(&pen, 8);
        assert_eq!(
            head,
            vec![
                "M454.56,574.77".to_string(),
                "Q477.58,585.56 499.80,598.25".to_string(),
                "Q522.02,610.95 543.36,625.16".to_string(),
                "Q564.71,639.37 584.54,655.19".to_string(),
                "Q604.38,671.02 623.03,688.41".to_string(),
                "Q641.67,705.80 658.41,724.46".to_string(),
                "Q675.14,743.12 689.79,763.21".to_string(),
                "Q704.43,783.31 717.03,804.56".to_string(),
            ]
        );
    }
}
